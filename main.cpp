#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <typeinfo>

#include "Generator.h"
#include "Parser.h"
#include "nodes/wavmarker/WavFile.h"
#include "util/Error.h"
#include "version.h"

namespace {

void terminate_handler() {
	std::exception_ptr current_ex = std::current_exception();
	if (current_ex) {
		try {
			std::rethrow_exception(current_ex);
		} catch (const BaseError& e) {
			std::cerr << "--------------------------------------------------\n";
			std::cerr << "ERROR DETAILS (BaseError):\n";
			std::cerr << e.get_detailed_description() << std::endl;
			std::cerr << "--------------------------------------------------\n";
		} catch (const std::exception& e) {
			std::cerr << "--------------------------------------------------\n";
			std::cerr << "ERROR DETAILS (std::exception):\n";
			std::cerr << "  Type: " << typeid(e).name() << "\n";
			std::cerr << "  Message: " << e.what() << std::endl;
			std::cerr << "--------------------------------------------------\n";
		} catch (...) {
			std::cerr << "An unknown error occurred." << std::endl;
		}
	}
	std::abort();
}

void print_usage(const char* executable) {
	std::cout << "wavmarker " << PROJECT_VERSION << "\n"
		<< "Usage:\n"
		<< "  " << executable << " <input.wav> [output.wav]\n"
		<< "  " << executable << " --info <input.wav>\n"
		<< "  " << executable << " --version\n";
}

void print_info(const WavFile& wav) {
	std::cout << "Container: " << wav.riff_id() << "/" << wav.wave_id() << "\n";
	std::cout << "Chunks: " << wav.chunks().size() << "\n";

	if (const auto* format = wav.format()) {
		std::cout << "Format:\n";
		std::cout << "  Audio format: " << format->audio_format << "\n";
		std::cout << "  Channels: " << format->channels << "\n";
		std::cout << "  Sample rate: " << format->sample_rate << "\n";
		std::cout << "  Byte rate: " << format->byte_rate << "\n";
		std::cout << "  Block align: " << format->block_align << "\n";
		std::cout << "  Bits per sample: " << format->bits_per_sample << "\n";
	}

	if (const auto* audio = wav.audio_data()) {
		std::cout << "Audio bytes: " << audio->size() << "\n";
	}

	const auto cues = wav.cue_points();
	const auto labels = wav.labels();
	const auto loops = wav.sample_loops();
	std::cout << "Cue points: " << cues.size() << "\n";
	std::cout << "Labels: " << labels.size() << "\n";
	std::cout << "Sample loops: " << loops.size() << "\n";
}

WavFile& require_wav(Container& container) {
	auto* wav = dynamic_cast<WavFile*>(&container);
	if (!wav) {
		throw RuntimeError("Parsed container is not a WavFile.", "CLI");
	}
	return *wav;
}

} // namespace

int main(int argc, const char* argv[]) {
	std::set_terminate(terminate_handler);

	try {
		if (argc < 2) {
			print_usage(argv[0]);
			return 1;
		}

		const std::string first_arg = argv[1];
		if (first_arg == "--help" || first_arg == "-h") {
			print_usage(argv[0]);
			return 0;
		}
		if (first_arg == "--version" || first_arg == "-v") {
			std::cout << PROJECT_VERSION << "\n";
			return 0;
		}

		bool info_only = false;
		std::string input_path;
		std::string output_path;
		if (first_arg == "--info") {
			if (argc != 3) {
				print_usage(argv[0]);
				return 1;
			}
			info_only = true;
			input_path = argv[2];
		} else {
			input_path = first_arg;
			if (argc >= 3) {
				output_path = argv[2];
			}
		}

		auto container = Parser::parse_file(input_path);
		auto& wav = require_wav(*container);
		print_info(wav);

		if (!info_only && !output_path.empty()) {
			Generator::write_file(wav, output_path);
			std::cout << "Wrote: " << output_path << "\n";
		}

		return 0;
	} catch (const BaseError& e) {
		std::cerr << e.get_detailed_description() << std::endl;
		return 1;
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
}
