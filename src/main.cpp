// #define CTK_MEM_CHECK
#include <ctk-0.45/mod.cpp>

enum class Mode {
	Build,
	Run,
	Debug,
};

#include "adbp.cpp"
#include "config.cpp"

int main(int argc, char** argv) {
	if (argc != 3) {
		ctk::panic("Need 2 arguments [config_path, 'build'/'run'/'debug']");
	}
	const char* config_file_path = argv[1];
	ctk::ar<const u8> mode_str = AR_STR(argv[2]);
	Mode mode;
	if (ctk::ar<const u8>::compare(mode_str, AR_STR("b")) || ctk::ar<const u8>::compare(mode_str, AR_STR("build"))) {
		mode = Mode::Build;
	} else if (ctk::ar<const u8>::compare(mode_str, AR_STR("r")) || ctk::ar<const u8>::compare(mode_str, AR_STR("run"))) {
		mode = Mode::Run;
	} else if (ctk::ar<const u8>::compare(mode_str, AR_STR("d")) || ctk::ar<const u8>::compare(mode_str, AR_STR("debug"))) {
		mode = Mode::Debug;
	} else {
		ctk::panic("Invalid mode");
	}
	if (ctk::File::exists("temp") == false) {
		if (ctk::File::create_dir("temp") == ctk::File::CreateResult::Error) {
			ctk::panic("ctk::File::create_dir failed");
		}
	}
	Config config;
	config.create(config_file_path, mode);
	config.build();
	config.destroy();
	return 0;
}