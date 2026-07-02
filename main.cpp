#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <typeinfo>

#include "src/cli/CliHandling.h"
#include "src/wavmarker/Patcher.h"
#include "src/wavmarker/WavFile.h"
#include "util/Error.h"

namespace {

void handle_terminate() {
	const std::exception_ptr current_exception = std::current_exception();
	if (current_exception) {
		try {
			std::rethrow_exception(current_exception);
		} catch (const BaseError& error) {
			std::cerr << "--------------------------------------------------\n"
				<< "ERROR DETAILS (BaseError):\n"
				<< error.get_detailed_description() << '\n'
				<< "--------------------------------------------------\n";
		} catch (const std::exception& error) {
			std::cerr << "--------------------------------------------------\n"
				<< "ERROR DETAILS (std::exception):\n"
				<< "  Type: " << typeid(error).name() << '\n'
				<< "  Message: " << error.what() << '\n'
				<< "--------------------------------------------------\n";
		} catch (...) {
			std::cerr << "An unknown error occurred.\n";
		}
	}
	std::abort();
}



void debug() {
	auto project_dir = std::filesystem::path(PROJECT_DIR);
	auto samples_dir = project_dir / "samples";

	auto file_path = samples_dir / "Violins1_HarmonicSustain_BleedOrch_1-127_rr1_A4.wav";

	auto container = Parser::parse_file(file_path.string());
	auto& wav = WavMarkerCli::require_wav(*container);
	WavMarkerCli::print_info(wav);
	Generator::write_file(*container, (project_dir / "out.wav").string());

	auto output_path = project_dir / "out.json";
	Generator::dump_json(wav, output_path.string());

}

} // namespace

int main(const int argc, const char* argv[]) {
	std::set_terminate(handle_terminate);

#ifndef NDEBUG
	debug();
	return 0;
#endif

	WavMarkerCli::handle(argc, argv);
}
