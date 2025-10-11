void panic(const char* format, ...);

// #define CTK_ARR_CHECK
// #define CTK_MEM_CHECK
#define CTK_PANIC panic
#include <ctk-0.34/main.cpp>

enum class Mode {
	Build,
	Run,
	Debug,
};

#include "adbp.cpp"
#include "config.cpp"

void panic(const char* format, ...) {
	va_list args;
	va_start(args, format);
	std::vfprintf(stderr, format, args);
	va_end(args);
	std::putchar('\n');
	std::exit(1);
}

int main(int argc, char** argv) {
	if (argc != 3) {
		panic("Need 2 arguments [config_path, 'build'/'run'/'debug']");
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
		panic("Invalid mode");
	}
	Config config;
	config.create(config_file_path, mode);
	config.build();
	config.destroy();
	return 0;
}