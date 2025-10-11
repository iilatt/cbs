struct Config {
	ctk::Time time;
	f64 start_time;
	ctk::ar<u8> res_path;
	ctk::gar<ctk::ar<u8>> modules;
	ctk::gar<ctk::ar<u8>> libraries;
	ctk::ar<u8> out_path;
	Mode mode;
	
	static void assert_value_count(size_t count, size_t target_count) {
		if (count == 0) {
			panic("A value is required");
		}
		if (target_count != 0 && count != target_count) {
			panic("Invalid number of values (got %zu, need %zu)", count, target_count);
		}
	}

	void create(this auto& self, const char* file_path, Mode mode) {
		self.time.create();
		self.start_time = self.time.get();
		std::printf("Build started\n");
		self.res_path = ctk::ar<u8>::empty();
		self.modules.create_auto();
		self.libraries.create_auto();
		self.out_path = ctk::ar<u8>::empty();
		self.mode = mode;
		ctk::ar<u8> file_data = ctk::load_file(file_path);
		enum class State {
			Key,
			Value,
		};
		State state = State::Key;
		ctk::gar<u8> current_str;
		current_str.create_auto();
		ctk::ar<u8> current_key;
		ctk::gar<ctk::ar<u8>> current_values;
		current_values.create_auto();
		for (size_t a = 0; a <= file_data.len; ++a) {
			u8 codepoint = a >= file_data.len ? '\0' : file_data[a];
			if (codepoint == ' ' || codepoint == '\t' || codepoint == '\r') {
				continue;
			}
			if (codepoint == '|') {
				if (state == State::Key) {
					current_key = current_str.to_ar().clone();
					current_str.clear();
					state = State::Value;
				} else {
					panic("Unexpected '|'");
				}
			} else if (codepoint == ',') {
				if (state == State::Value) {
					current_values.push(current_str.to_ar().clone());
					current_str.clear();
				} else {
					panic("Unexpected ','");
				}
			} else if (codepoint == '\n' || codepoint == '\0') {
				if (state == State::Value) {
					current_values.push(current_str.to_ar().clone());
					current_str.clear();
					self.handle_config_line(current_key, current_values.to_ar());
					current_key.destroy();
					current_values.clear();
					state = State::Key;
				} else {
					if (codepoint == '\n') {
						panic("Unexpected new line");
					} else {
						panic("Unexpected end of file");
					}
				}
			} else {
				current_str.push(codepoint);
			}
		}
		current_values.destroy();
		current_str.destroy();
		file_data.destroy();
	}

	void destroy(this auto& self) {
		self.res_path.destroy();
		for (size_t a = 0; a < self.modules.len; ++a) {
			self.modules[a].destroy();
		}
		self.modules.destroy();
		for (size_t a = 0; a < self.libraries.len; ++a) {
			self.libraries[a].destroy();
		}
		self.libraries.destroy();
		self.out_path.destroy();
	}

	void handle_config_line(this auto& self, ctk::ar<u8> current_key, ctk::ar<ctk::ar<u8>> current_values) {
		if (ctk::ar<const u8>::compare(current_key, AR_STR("d3d"))) {
			assert_value_count(current_values.len, 2);
			self.build_shader(current_values[0], current_values[1], true);
			self.build_shader(current_values[0], current_values[1], false);
			current_values[0].destroy();
			current_values[1].destroy();
		} else if (ctk::ar<const u8>::compare(current_key, AR_STR("adb"))) {
			assert_value_count(current_values.len, 3);
			ADB_Packer adb_packer;
			adb_packer.create(current_values[1], current_values[2]);
			adb_packer.pack_assets_dir(current_values[0], nullptr);
			adb_packer.destroy();
		} else if (ctk::ar<const u8>::compare(current_key, AR_STR("res"))) {
			if (self.res_path.buf != nullptr) {
				panic("Key 'res' already set");
			}
			assert_value_count(current_values.len, 1);
			self.res_path = current_values[0];
		} else if (ctk::ar<const u8>::compare(current_key, AR_STR("mod"))) {
			assert_value_count(current_values.len, 1);
			self.modules.join(current_values);
		} else if (ctk::ar<const u8>::compare(current_key, AR_STR("lib"))) {
			assert_value_count(current_values.len, 0);
			self.libraries.join(current_values);
		} else if (ctk::ar<const u8>::compare(current_key, AR_STR("out"))) {
			if (self.out_path.buf != nullptr) {
				panic("Key 'out' already set");
			}
			assert_value_count(current_values.len, 1);
			self.out_path = current_values[0];
		} else {
			panic("Invalid key");
		}
	}

	void build(this auto& self) {
		for (size_t a = 0; a < self.modules.len; ++a) {
			self.build_module(self.modules[a]);
		}
		self.build_main();
	}

	void build_shader(this auto& self, ctk::ar<u8> shader_path, ctk::ar<u8> out_path, bool vertex) {
		shader_path = shader_path.resize_clone(shader_path.len + 1);
		shader_path[shader_path.len - 1] = '\0';

		const wchar_t* entry_point = vertex ? L"vs_main" : L"ps_main";
		const wchar_t* target = vertex ? L"vs_6_0" : L"ps_6_0";

		// Load shader file
		ctk::ar<u8> shader_data = ctk::load_file((const char*)shader_path.buf);

		// Initialize Dxc compiler
		CComPtr<IDxcCompiler3> compiler;
		CComPtr<IDxcUtils> utils;
		DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils));
		DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));

		// Create blob from memory
		DxcBuffer sourceBuffer;
		sourceBuffer.Ptr = shader_data.buf;
		sourceBuffer.Size = shader_data.len;
		sourceBuffer.Encoding = DXC_CP_UTF8;

		setlocale(LC_ALL, "en_US.UTF-8");

		size_t len = mbstowcs(NULL, (const char*)shader_path.buf, 0);
		if (len == (size_t)-1) {
			perror("UTF-8 to wide string conversion failed");
			return;
		}

		// Prepare arguments
		wchar_t* shaderFileW = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
		mbstowcs(shaderFileW, (const char*)shader_path.buf, len + 1);
		LPCWSTR args[] = {
			shaderFileW,
			L"-E", entry_point,
			L"-T", target,
			L"-Zi", L"-Qembed_debug",
			L"-Od" // disable optimization for debug
		};

		CComPtr<IDxcIncludeHandler> includeHandler;
		utils->CreateDefaultIncludeHandler(&includeHandler);

		// Compile
		CComPtr<IDxcResult> result;
		compiler->Compile(
			&sourceBuffer,
			args, _countof(args),
			includeHandler,
			IID_PPV_ARGS(&result)
		);

		// Check for errors
		CComPtr<IDxcBlobUtf8> errors;
		result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
		if (errors && errors->GetStringLength() > 0) {
			printf("[%ls] compile error:\n%s\n", entry_point, errors->GetStringPointer());
		}

		HRESULT status;
		result->GetStatus(&status);
		if (FAILED(status)) {
			panic("[%ls] shader compilation failed (0x%08X)", entry_point, status);
		}

		// Get compiled shader
		CComPtr<IDxcBlob> shaderBlob;
		result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);

		// Build output path (add .vs or .ps)
		out_path = out_path.resize_clone(out_path.len + 4);
		out_path[out_path.len - 4] = '.';
		out_path[out_path.len - 3] = vertex ? 'v' : 'p';
		out_path[out_path.len - 2] = 's';
		out_path[out_path.len - 1] = '\0';

		// Write compiled blob to file
		FILE* fp = fopen((const char*)out_path.buf, "wb");
		if (fp != nullptr) {
			fwrite(shaderBlob->GetBufferPointer(), 1, shaderBlob->GetBufferSize(), fp);
			fclose(fp);
		} else {
			panic("Failed to write output file: %s", (const char*)out_path.buf);
		}
	}

	void build_module(this auto& self, ctk::ar<u8> module_name) {
		ctk::ar<u8> gcc_command = ctk::alloc_format("x86_64-w64-mingw32-g++ -std=c++23 -Wall -c include/%.*s/main.cpp -o temp/%.*s.o %s", module_name.len, (const char*)module_name.buf, module_name.len, (const char*)module_name.buf, self.mode == Mode::Debug ? "-O0 -g": "-O3 -s");
		if (::system((const char*)gcc_command.buf)) {
			panic("GCC error");
		}
		gcc_command.destroy();
	}
	
	void build_main(this auto& self) {
		if (self.out_path.buf == nullptr) {
			panic("Missing 'out' key");
		}
		ctk::gar<u8> modules_str;
		modules_str.create_auto();
		for (size_t a = 0; a < self.modules.len; ++a) {
			modules_str.join(AR_STR("temp/"));
			modules_str.join(self.modules[a]);
			modules_str.join(AR_STR(".o "));
		}
		if (self.res_path.buf != nullptr) {
			ctk::ar<u8> windres_command = ctk::alloc_format("x86_64-w64-mingw32-windres -i %.*s -o temp/res.o", self.res_path.len, (const char*)self.res_path.buf);
			if (::system((const char*)windres_command.buf)) {
				panic("Windres error");
			}
			windres_command.destroy();
			modules_str.join(AR_STR("temp/res.o"));
		}
		ctk::gar<u8> libraries_str;
		libraries_str.create_auto();
		for (size_t a = 0; a < self.libraries.len; ++a) {
			libraries_str.join(AR_STR("-l"));
			libraries_str.join(self.libraries[a]);
			libraries_str.push((u8)' ');
		}
		ctk::ar<u8> gcc_command = ctk::alloc_format("x86_64-w64-mingw32-g++ -std=c++23 -static -Wall src/main.cpp -o %.*s -Iinclude -Itemp -Llib %.*s %.*s %s", self.out_path.len, (const char*)self.out_path.buf, modules_str.len, (const char*)modules_str.buf, libraries_str.len, (const char*)libraries_str.buf, self.mode == Mode::Debug ? "-O0 -g -mconsole": "-O3 -s -mwindows");
		modules_str.destroy();
		libraries_str.destroy();
		if (::system((const char*)gcc_command.buf)) {
			panic("GCC error");
		}
		gcc_command.destroy();
		f64 end_time = self.time.get();
		std::printf("Build finished (took %.2fs)\n", end_time - self.start_time);
		if (self.mode == Mode::Debug) {
			// ctk::ar<u8> gdb_command = ctk::alloc_format("start %.*s/gdb -ex \"set pagination off\" -ex \"break debug.cpp:27\" -ex \"run\" %.*s", self.gcc_path.len, (const char*)self.gcc_path.buf, self.out_path.len, (const char*)self.out_path.buf);
			// ::system((const char*)gdb_command.buf);
			// gdb_command.destroy();
		} else if (self.mode == Mode::Run) {
			ctk::ar<u8> start_command = ctk::alloc_format("start %.*s", self.out_path.len, (const char*)self.out_path.buf);
			::system((const char*)start_command.buf);
			start_command.destroy();
		}
	}
};