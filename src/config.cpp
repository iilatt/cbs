struct Config {
	ctk::Time time;
	f64 start_time;
	ctk::ar<u8> gcc_path;
	ctk::ar<u8> fxc_path;
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
		self.gcc_path = ctk::ar<u8>::empty();
		self.fxc_path = ctk::ar<u8>::empty();
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
		self.gcc_path.destroy();
		self.fxc_path.destroy();
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
		if (ctk::ar<const u8>::compare(current_key, AR_STR("gcc"))) {
			if (self.gcc_path.buf != nullptr) {
				panic("Key 'res' already set");
			}
			assert_value_count(current_values.len, 1);
			self.gcc_path = current_values[0];
		} else if (ctk::ar<const u8>::compare(current_key, AR_STR("fxc"))) {
			if (self.fxc_path.buf != nullptr) {
				panic("Key 'fxc' already set");
			}
			assert_value_count(current_values.len, 1);
			self.fxc_path = current_values[0];
		} else if (ctk::ar<const u8>::compare(current_key, AR_STR("d3d"))) {
			assert_value_count(current_values.len, 2);
			self.build_shader(current_values[0], current_values[1]);
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
		} else if (ctk::ar<const u8>::compare(current_key, AR_STR("dbg_lib"))) {
			assert_value_count(current_values.len, 2);
			if (self.mode == Mode::Debug) {
				self.libraries.push(current_values[1]);
			} else {
				self.libraries.push(current_values[0]);
			}
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

	void build_shader(this auto& self, ctk::ar<u8> shader_path, ctk::ar<u8> out_path) {
		if (self.fxc_path.buf == nullptr) {
			panic("Missing 'fxc' key");
		}

		ctk::ar<u8> fxc_vs_command = ctk::alloc_format("%.*s\\fxc.exe /nologo /O3 /T vs_4_0 /E vs_main /Fo %.*s.vs %.*s.hlsl", self.fxc_path.len, (const char*)self.fxc_path.buf, out_path.len, (const char*)out_path.buf, shader_path.len, (const char*)shader_path.buf);
		if (::system((const char*)fxc_vs_command.buf)) {
			panic("fxc error");
		}
		fxc_vs_command.destroy();

		ctk::ar<u8> fxc_ps_command = ctk::alloc_format("%.*s\\fxc.exe /nologo /O3 /T ps_4_0 /E ps_main /Fo %.*s.ps %.*s.hlsl", self.fxc_path.len, (const char*)self.fxc_path.buf, out_path.len, (const char*)out_path.buf, shader_path.len, (const char*)shader_path.buf);
		if (::system((const char*)fxc_ps_command.buf)) {
			panic("fxc error");
		}
		fxc_ps_command.destroy();
	}

	void build_module(this auto& self, ctk::ar<u8> module_name) {
		ctk::ar<u8> out_path = ctk::alloc_format("temp\\%.*s.o", module_name.len, (const char*)module_name.buf);
		if (ctk::File::exists((const char*)out_path.buf)) {
			return;
		}
		ctk::ar<u8> gcc_command = ctk::alloc_format("%.*s\\g++.exe -std=c++23 -Wall -c include\\%.*s\\main.cpp -o %s %s", self.gcc_path.len, (const char*)self.gcc_path.buf, module_name.len, (const char*)module_name.buf, (const char*)out_path.buf, self.mode == Mode::Debug ? "-DCBS_DEBUG -O0 -g": "-O3 -s");
		std::printf("Building module %.*s\n", (int)module_name.len, (const char*)module_name.buf);
		if (::system((const char*)gcc_command.buf)) {
			panic("GCC error");
		}
		gcc_command.destroy();
	}
	
	void build_main(this auto& self) {
		if (self.gcc_path.buf == nullptr) {
			panic("Missing 'gcc' key");
		}
		if (self.out_path.buf == nullptr) {
			panic("Missing 'out' key");
		}
		ctk::gar<u8> modules_str;
		modules_str.create_auto();
		for (size_t a = 0; a < self.modules.len; ++a) {
			modules_str.join(AR_STR("temp\\"));
			modules_str.join(self.modules[a]);
			modules_str.join(AR_STR(".o "));
		}
		if (self.res_path.buf != nullptr) {
			ctk::ar<u8> windres_command = ctk::alloc_format("%.*s\\windres.exe -i %.*s -o temp\\res.o", self.gcc_path.len, (const char*)self.gcc_path.buf, self.res_path.len, (const char*)self.res_path.buf);
			if (::system((const char*)windres_command.buf)) {
				panic("Windres error");
			}
			windres_command.destroy();
			modules_str.join(AR_STR("temp\\res.o"));
		}
		ctk::gar<u8> libraries_str;
		libraries_str.create_auto();
		for (size_t a = 0; a < self.libraries.len; ++a) {
			libraries_str.join(AR_STR("-l"));
			libraries_str.join(self.libraries[a]);
			libraries_str.push((u8)' ');
		}
		ctk::ar<u8> gcc_command = ctk::alloc_format("%.*s\\g++.exe -std=c++23 -static -Wall src\\main.cpp -o %.*s -Iinclude -Itemp -Llib %.*s %.*s %s", self.gcc_path.len, (const char*)self.gcc_path.buf, self.out_path.len, (const char*)self.out_path.buf, modules_str.len, (const char*)modules_str.buf, libraries_str.len, (const char*)libraries_str.buf, self.mode == Mode::Debug ? "-DCBS_DEBUG -O0 -g -mconsole": "-O3 -s -mwindows");
		modules_str.destroy();
		libraries_str.destroy();
		std::printf("Building main\n");
		if (::system((const char*)gcc_command.buf)) {
			panic("GCC error");
		}
		gcc_command.destroy();
		f64 end_time = self.time.get();
		std::printf("Build finished (took %.2fs)\n", end_time - self.start_time);
		if (self.mode == Mode::Debug) {
			ctk::ar<u8> gdb_command = ctk::alloc_format("start %.*s\\gdb -ex \"set pagination off\" -ex \"run\" %.*s", self.gcc_path.len, (const char*)self.gcc_path.buf, self.out_path.len, (const char*)self.out_path.buf);
			::system((const char*)gdb_command.buf);
			gdb_command.destroy();
		} else if (self.mode == Mode::Run) {
			ctk::ar<u8> start_command = ctk::alloc_format("start %.*s", self.out_path.len, (const char*)self.out_path.buf);
			::system((const char*)start_command.buf);
			start_command.destroy();
		}
	}
};