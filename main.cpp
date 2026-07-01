#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <typeinfo>

#include "Generator.h"
#include "Parser.h"
#include "nodes/JSON/parser/JSONParser.h"
#include "nodes/wavmarker/Patcher.h"
#include "nodes/wavmarker/WavFile.h"
#include "util/Error.h"
#include "version.h"

namespace {

void handle_terminate() {
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
		<< "  " << executable << " --set <bext.field>=<json-value> <input.wav>\n"
		<< "  " << executable << " --get <bext.field> <input.wav>\n"
		<< "  " << executable << " --copy-sample-loops <source.wav> <target.wav> [--no-labels]\n"
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

/// Parses a CLI value according to the reflected BEXT field type.
std::unique_ptr<JSONValue> parse_bext_value(const std::string& field_path, const std::string& text) {
	const std::type_index field_type = Patcher::bext_field_type(field_path);
	if (field_type == typeid(std::string) && (text.empty() || (text.front() != '"' && text.front() != '\''))) {
		// Shell syntax such as "originator"="Sonuscore" reaches us without quotes.
		return std::make_unique<JSONString>(text);
	}

	JSONParser parser;
	auto value = parser.parse(text, "CLI --set");
	if (field_type == typeid(std::string) && !dynamic_cast<JSONString*>(value.get())) {
		throw RuntimeError("BEXT field expects a string value: " + field_path, "Parsing --set value");
	}
	if (field_type == typeid(std::vector<uint8_t>) && !dynamic_cast<JSONArray*>(value.get())) {
		throw RuntimeError("BEXT field expects a JSON byte array: " + field_path, "Parsing --set value");
	}
	if (field_type != typeid(std::string) && field_type != typeid(std::vector<uint8_t>) &&
		!dynamic_cast<JSONInt*>(value.get()) && !dynamic_cast<JSONString*>(value.get())) {
		throw RuntimeError("BEXT field expects an integer value: " + field_path, "Parsing --set value");
	}
	return value;
}

void set_bext_field(const std::string& assignment, const std::string& input_path) {
	const size_t separator = assignment.find('=');
	if (separator == std::string::npos || separator == 0 || separator + 1 == assignment.size()) {
		throw RuntimeError("Expected <bext.field>=<json-value>, got: " + assignment, "Parsing --set");
	}

	const std::string field_path = assignment.substr(0, separator);
	auto value = parse_bext_value(field_path, assignment.substr(separator + 1));
	auto container = Parser::parse_file(input_path);
	auto& wav = require_wav(*container);
	Patcher patcher(wav);
	auto patch = Patcher::create_bext_patch(field_path, std::move(value));
	if (!patcher.apply_patch(patch)) {
		throw RuntimeError("Could not set BEXT field: " + field_path, "Applying --set");
	}

	Generator::write_file(wav, input_path);
	std::cout << "Set " << field_path << " in " << input_path << "\n";
}

void get_bext_field(const std::string& field_path, const std::string& input_path) {
	auto container = Parser::parse_file(input_path);
	auto& wav = require_wav(*container);
	std::cout << Patcher(wav).get_bext_value(field_path)->get_string() << "\n";
}

/// Copies sample-loop metadata into target_path and overwrites that WAV in place.
void copy_sample_loops(const std::string& source_path, const std::string& target_path,
	const bool include_labels) {
	auto source_container = Parser::parse_file(source_path);
	auto target_container = Parser::parse_file(target_path);
	auto& source = require_wav(*source_container);
	auto& target = require_wav(*target_container);

	target.copy_sample_loops_from(source, include_labels);
	Generator::write_file(target, target_path);

	std::cout << "Copied " << target.sample_loops().size() << " sample loop(s) from "
		<< source_path << " to " << target_path << "\n";
}

} // namespace

void debug() {
	auto project_dir = std::filesystem::path(PROJECT_DIR);
	auto samples_dir = project_dir / "samples";

	auto file_path = samples_dir / "Violins1_HarmonicSustain_BleedOrch_1-127_rr1_A4.wav";

	auto container = Parser::parse_file(file_path.string());
	auto& wav = require_wav(*container);
	print_info(wav);
	Generator::write_file(*container, (project_dir / "out.wav").string());

	auto output_path = project_dir / "out.json";
	Generator::dump_json(wav, output_path.string());

}

int main(int argc, const char* argv[]) {
	std::set_terminate(handle_terminate);

#ifndef NDEBUG
	debug();
	return 0;
#endif

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
		if (first_arg == "--set") {
			if (argc != 4) {
				print_usage(argv[0]);
				return 1;
			}
			set_bext_field(argv[2], argv[3]);
			return 0;
		}
		if (first_arg == "--get") {
			if (argc != 4) {
				print_usage(argv[0]);
				return 1;
			}
			get_bext_field(argv[2], argv[3]);
			return 0;
		}
		if (first_arg == "--copy-sample-loops") {
			if (argc != 4 && argc != 5) {
				print_usage(argv[0]);
				return 1;
			}
			bool include_labels = true;
			if (argc == 5) {
				if (std::string(argv[4]) != "--no-labels") {
					std::cerr << "Unknown option: " << argv[4] << "\n";
					print_usage(argv[0]);
					return 1;
				}
				include_labels = false;
			}
			copy_sample_loops(argv[2], argv[3], include_labels);
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
			if (std::filesystem::path(output_path).extension() == ".json") {
				Generator::dump_json(wav, output_path);
			} else {
				Generator::write_file(wav, output_path);
			}
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
