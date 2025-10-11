struct ADB_Packer {
	std::FILE* blob_file;
	std::FILE* cpp_file;
	size_t current_pos;
	size_t asset_count;

	void create(this auto& self, ctk::ar<u8> blob_path, ctk::ar<u8> cpp_path) {
		blob_path = blob_path.resize_clone(blob_path.len + 1);
		blob_path[blob_path.len - 1] = '\0';
		self.blob_file = std::fopen((const char*)blob_path.buf, "wb");
		if (self.blob_file == nullptr) {
			panic("failed to open output file");
		}
		cpp_path = cpp_path.resize_clone(cpp_path.len + 1);
		cpp_path[cpp_path.len - 1] = '\0';
		self.cpp_file = std::fopen((const char*)cpp_path.buf, "w");
		if (self.cpp_file == nullptr) {
			panic("failed to open output file");
		}
		self.current_pos = 0;
		self.asset_count = 0;
		std::fprintf(self.cpp_file, "namespace adb {\nconst u8* blob;\n");
	}

	void destroy(this auto& self) {
		std::fprintf(self.cpp_file, "}");
		std::fclose(self.blob_file);
		std::fclose(self.cpp_file);
	}

	void pack_assets_dir(this auto& self, ctk::ar<u8> dir_path, const char* asset_id) {
		dir_path = dir_path.resize_clone(dir_path.len + 1);
		dir_path[dir_path.len - 1] = '\0';
		ctk::Directory dir;
		dir.create((const char*)dir_path.buf);
		while (true) {
			ctk::Directory::Entity ent = dir.get_next(true);
			if (ent.type == ctk::Directory::Entity::Type::None) {
				break;
			}
			ctk::ar<u8> file_path = ctk::alloc_format("%s/%s", dir_path.buf, ent.path);
			ctk::ar<u8> new_asset_id;
			if (asset_id == nullptr) {
				new_asset_id = ctk::alloc_format("%s", ent.path);
			} else {
				new_asset_id = ctk::alloc_format("%s/%s", asset_id, ent.path);
			}
			for (size_t a = 0; a < new_asset_id.len; ++a) {
				if (new_asset_id[a] == '/' || new_asset_id[a] == '.') {
					new_asset_id[a] = '_';
				}
			}
			if (ent.type == ctk::Directory::Entity::Type::Dir) {
				self.pack_assets_dir(file_path, (const char*)new_asset_id.buf);
			} else {
				self.pack_asset_file((const char*)file_path.buf, (const char*)new_asset_id.buf);
			}
			file_path.destroy();
			new_asset_id.destroy();
		}
		dir.destroy();
	}

	void pack_asset_file(this auto& self, const char* asset_path, const char* asset_id) {
		ctk::ar<u8> asset_data = ctk::load_file(asset_path);
		if (std::fwrite(asset_data.buf, 1, asset_data.len, self.blob_file) != asset_data.len) {
			panic("failed to write to output file");
		}
		std::fprintf(self.cpp_file, "ctk::ar<const u8> %s() { return ctk::ar<const u8>(&adb::blob[%zu], %zu); }\n", asset_id, self.current_pos, asset_data.len);
		self.current_pos += asset_data.len;
		self.asset_count += 1;
	}
};