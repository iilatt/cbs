struct Config {
#if CBS_WIN32
	static constexpr char path_seperator = '\\';
	static constexpr const char* linker = "lld";
	static constexpr const char* system_define = "CBS_WIN32";
	static constexpr const char* debug_args = "-mconsole";
	static constexpr const char* release_args = "-mwindows";
#endif
#if CBS_LINUX
	static constexpr char path_seperator = '/';
	static constexpr const char* linker = "mold";
	static constexpr const char* system_define = "CBS_LINUX";
	static constexpr const char* debug_args = "";
	static constexpr const char* release_args = "";
#endif

	ctk::Time time;
	f64 start_time;
	ctk::ar<u8> gcc_path;
	ctk::ar<u8> fxc_path;
	ctk::ar<u8> res_path;
	ctk::gar<ctk::ar<u8>> modules;
	ctk::gar<ctk::ar<u8>> libraries;
	ctk::gar<ctk::ar<u8>> includes;
	ctk::ar<u8> out_path;
	Mode mode;
	
	static void assert_value_count(size_t count, size_t target_count) {
		if (count == 0) {
			ctk::panic("A value is required");
		}
		if (target_count != 0 && count != target_count) {
			ctk::panic("Invalid number of values (got %zu, need %zu)", count, target_count);
		}
	}

	void create(this auto& self, const char* file_path, Mode mode) {
		self.time.create();
		self.start_time = self.time.get();
		ctk::log("Build started");
		self.gcc_path = ctk::ar<u8>::empty();
		self.fxc_path = ctk::ar<u8>::empty();
		self.res_path = ctk::ar<u8>::empty();
		self.modules.create_auto();
		self.libraries.create_auto();
		self.includes.create_auto();
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
					ctk::panic("Unexpected '|'");
				}
			} else if (codepoint == ',') {
				if (state == State::Value) {
					current_values.push(current_str.to_ar().clone());
					current_str.clear();
				} else {
					ctk::panic("Unexpected ','");
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
						ctk::panic("Unexpected new line");
					} else {
						ctk::panic("Unexpected end of file");
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
		for (size_t a = 0; a < self.includes.len; ++a) {
			self.includes[a].destroy();
		}
		self.includes.destroy();
		self.out_path.destroy();
	}

	void handle_config_line(this auto& self, ctk::ar<u8> current_key, ctk::ar<ctk::ar<u8>> current_values) {
		if (ctk::ar<const u8>::compare(current_key, AR_STR("gcc"))) {
			if (self.gcc_path.buf != nullptr) {
				ctk::panic("Key 'res' already set");
			}
			assert_value_count(current_values.len, 1);
			self.gcc_path = current_values[0];
		} else if (ctk::ar<const u8>::compare(current_key, AR_STR("fxc"))) {
			if (self.fxc_path.buf != nullptr) {
				ctk::panic("Key 'fxc' already set");
			}
			assert_value_count(current_values.len, 1);
			self.fxc_path = current_values[0];
		} else if (ctk::ar<const u8>::compare(current_key, AR_STR("d3d"))) {
			assert_value_count(current_values.len, 2);
			self.build_shader("vs", current_values[0], current_values[1]);
			self.build_shader("ps", current_values[0], current_values[1]);
			current_values[0].destroy();
			current_values[1].destroy();
		} else if (ctk::ar<const u8>::compare(current_key, AR_STR("d3d_vs"))) {
			assert_value_count(current_values.len, 2);
			self.build_shader("vs", current_values[0], current_values[1]);
			current_values[0].destroy();
			current_values[1].destroy();
		} else if (ctk::ar<const u8>::compare(current_key, AR_STR("d3d_ps"))) {
			assert_value_count(current_values.len, 2);
			self.build_shader("ps", current_values[0], current_values[1]);
			current_values[0].destroy();
			current_values[1].destroy();
		} else if (ctk::ar<const u8>::compare(current_key, AR_STR("d3d_cs"))) {
			assert_value_count(current_values.len, 2);
			self.build_shader("cs", current_values[0], current_values[1]);
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
				ctk::panic("Key 'res' already set");
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
		} else if (ctk::ar<const u8>::compare(current_key, AR_STR("inc"))) {
			assert_value_count(current_values.len, 0);
			self.includes.join(current_values);
		} else if (ctk::ar<const u8>::compare(current_key, AR_STR("out"))) {
			if (self.out_path.buf != nullptr) {
				ctk::panic("Key 'out' already set");
			}
			assert_value_count(current_values.len, 1);
			self.out_path = current_values[0];
		} else {
			ctk::panic("Invalid key");
		}
	}

	bool should_rebuild(this auto& self, ctk::ar<const u8> dir_path, u64 out_write_time) {
		ctk::DirWalk dir_walk;
		dir_walk.create((const char*)dir_path.buf);
		while (true) {
			ctk::DirWalk::Entity ent = dir_walk.get_next(true);
			if (ent.type == ctk::DirWalk::Entity::Type::None) {
				break;
			}
			ctk::ar<u8> file_path = ctk::alloc_format("%s/%s", dir_path.buf, ent.path);
			if (ent.type == ctk::DirWalk::Entity::Type::Dir) {
				bool dir_dirty = self.should_rebuild(file_path, out_write_time);
				if (dir_dirty) {
					return true;
				}
			} else {
				if (ent.write_time > out_write_time) {
					return true;
				}
			}
			file_path.destroy();
		}
		dir_walk.destroy();
		return false;
	}

	void build(this auto& self) {
		for (size_t a = 0; a < self.modules.len; ++a) {
			self.build_module(self.modules[a]);
		}
		self.build_main();
	}

	void build_shader(this auto& self, const char* type, ctk::ar<const u8> src_path, ctk::ar<const u8> out_path) {
		if (self.fxc_path.buf == nullptr) {
			ctk::panic("Missing 'fxc' key");
		}
		ctk::ar<u8> full_src_path = ctk::alloc_format("%.*s.hlsl", src_path.len, (const char*)src_path.buf);
		if (ctk::File::exists((const char*)full_src_path.buf) == false) {
			ctk::panic("Shader %.*s not found", full_src_path.len, (const char*)full_src_path.buf);
		}
		ctk::ar<u8> full_out_path = ctk::alloc_format("%.*s.%s", out_path.len, (const char*)out_path.buf, type);
		if (ctk::File::get_write_time((const char*)full_src_path.buf) <= ctk::File::get_write_time((const char*)full_out_path.buf)) {
			return;
		}
		ctk::ar<u8> fxc_vs_command = ctk::alloc_format("%.*s\\fxc.exe /nologo /O3 /T %s_5_0 /E %s_main /Fo %.*s %.*s", self.fxc_path.len, (const char*)self.fxc_path.buf, type, type, full_out_path.len, (const char*)full_out_path.buf, full_src_path.len, (const char*)full_src_path.buf);
		if (::system((const char*)fxc_vs_command.buf)) {
			ctk::panic("fxc error");
		}
		full_src_path.destroy();
		full_out_path.destroy();
		fxc_vs_command.destroy();
	}

	void build_module(this auto& self, ctk::ar<const u8> module_name) {
		ctk::ar<u8> mod_dir_path = ctk::alloc_format("include%c%.*s", path_seperator, module_name.len, (const char*)module_name.buf);
		ctk::ar<u8> mod_cpp_path = ctk::alloc_format("%.*s%cmod.cpp", mod_dir_path.len, (const char*)mod_dir_path.buf, path_seperator);
		if (ctk::File::exists((const char*)mod_dir_path.buf) == false || ctk::File::exists((const char*)mod_cpp_path.buf) == false) {
			ctk::panic("Module %.*s not found", mod_cpp_path.len, (const char*)mod_cpp_path.buf);
		}
		ctk::ar<u8> out_path = ctk::alloc_format("temp%c%.*s_%s.o", path_seperator, module_name.len, (const char*)module_name.buf, self.mode == Mode::Debug ? "dbg": "rls");
		if (self.should_rebuild(mod_dir_path, ctk::File::get_write_time((const char*)out_path.buf)) == false) {
			return;
		}
		mod_dir_path.destroy();
		ctk::ar<u8> gcc_command = ctk::alloc_format("%.*s%cbin%cg++ -fuse-ld=%s -std=c++23 -Wall -Wextra -Wno-unused-parameter -c %.*s -o %s -Iinclude %s -DCBS_MOD -D%s -DWINVER=_WIN32_WINNT_WIN10", self.gcc_path.len, (const char*)self.gcc_path.buf, path_seperator, path_seperator, linker, mod_cpp_path.len, (const char*)mod_cpp_path.buf, (const char*)out_path.buf, self.mode == Mode::Debug ? "-DCBS_DEBUG -Og -g": "-O3 -s", system_define);
		mod_cpp_path.destroy();
		ctk::log("Building module %.*s", (int)module_name.len, (const char*)module_name.buf);
		if (::system((const char*)gcc_command.buf)) {
			ctk::panic("GCC error");
		}
		gcc_command.destroy();
	}
	
	void build_main(this auto& self) {
		if (self.gcc_path.buf == nullptr) {
			ctk::panic("Missing 'gcc' key");
		}
		if (self.out_path.buf == nullptr) {
			ctk::panic("Missing 'out' key");
		}
		ctk::gar<u8> modules_str;
		modules_str.create_auto();
		for (size_t a = 0; a < self.modules.len; ++a) {
			modules_str.join(AR_STR("temp"));
			modules_str.push(path_seperator);
			modules_str.join(self.modules[a]);
			modules_str.join(self.mode == Mode::Debug ? AR_STR("_dbg") : AR_STR("_rls"));
			modules_str.join(AR_STR(".o "));
		}
		if (self.res_path.buf != nullptr) {
			ctk::ar<u8> res_src_path = ctk::alloc_format("%.*s.rc", self.res_path.len, (const char*)self.res_path.buf);
			if (ctk::File::exists((const char*)res_src_path.buf) == false) {
				ctk::panic("Resource %.*s not found", res_src_path.len, (const char*)res_src_path.buf);
			}
			const char* res_out_path = "temp\\res.o";
			if (ctk::File::get_write_time((const char*)res_src_path.buf) > ctk::File::get_write_time(res_out_path)) {
				ctk::ar<u8> windres_command = ctk::alloc_format("%.*s\\windres.exe -i %.*s -o %s", self.gcc_path.len, (const char*)self.gcc_path.buf, res_src_path.len, (const char*)res_src_path.buf, res_out_path);
				if (::system((const char*)windres_command.buf)) {
					ctk::panic("Windres error");
				}
				windres_command.destroy();
				ctk::log("Building resource %.*s", (int)res_src_path.len, (const char*)res_src_path.buf);
			}
			modules_str.join(AR_STR(res_out_path));
		}
		ctk::gar<u8> libraries_str;
		libraries_str.create_auto();
		for (size_t a = 0; a < self.libraries.len; ++a) {
			libraries_str.join(AR_STR("-l"));
			libraries_str.join(self.libraries[a]);
			libraries_str.push((u8)' ');
		}
		ctk::gar<u8> includes_str;
		includes_str.create_auto();
		for (size_t a = 0; a < self.includes.len; ++a) {
			includes_str.join(AR_STR("-I"));
			includes_str.join(self.includes[a]);
			includes_str.push((u8)' ');
		}
		ctk::ar<u8> gcc_command = ctk::alloc_format("%.*s%cbin%cg++ -fuse-ld=%s -std=c++23 -static -Wall -Wextra -Wno-unused-parameter source%cmain.cpp -o %.*s -Itemp -Llibrary -D%s -DWINVER=_WIN32_WINNT_WIN10 %.*s %.*s %.*s %s %s", self.gcc_path.len, (const char*)self.gcc_path.buf, path_seperator, path_seperator, linker, path_seperator, self.out_path.len, (const char*)self.out_path.buf, system_define, modules_str.len, (const char*)modules_str.buf, libraries_str.len, (const char*)libraries_str.buf, includes_str.len, (const char*)includes_str.buf, self.mode == Mode::Debug ? "-DCBS_DEBUG -Og -g" : "-O3 -s", self.mode == Mode::Debug ? debug_args : release_args);
		modules_str.destroy();
		libraries_str.destroy();
		includes_str.destroy();
		ctk::log("Building main");
		if (::system((const char*)gcc_command.buf)) {
			ctk::panic("GCC error");
		}
		gcc_command.destroy();
		f64 end_time = self.time.get();
		ctk::log("Build finished (took %.2fs)", end_time - self.start_time);
		if (self.mode == Mode::Debug) {
			ctk::ar<u8> gdb_command = ctk::alloc_format("start %.*s\\gdb.exe -ex \"set pagination off\" -ex \"run\" %.*s", self.gcc_path.len, (const char*)self.gcc_path.buf, self.out_path.len, (const char*)self.out_path.buf);
			::system((const char*)gdb_command.buf);
			gdb_command.destroy();
		} else if (self.mode == Mode::Run) {
			ctk::ar<u8> start_command = ctk::alloc_format("start %.*s", self.out_path.len, (const char*)self.out_path.buf);
			::system((const char*)start_command.buf);
			start_command.destroy();
		}
	}
};