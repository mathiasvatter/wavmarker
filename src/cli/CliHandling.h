//
// Created by Mathias Vatter on 02.07.26.
//

#pragma once

#include "CliApplication.h"
#include "../wavmarker/Patcher.h"
#include "../wavmarker/WavFile.h"
#include "../JSON/parser/JSONParser.h"
#include "src/Generator.h"
#include "src/Parser.h"


namespace WavMarkerCli {

/// casts a container object to its WavFile subclass -> throws error if it is not a WavFile
inline WavFile& require_wav(Container& container) {
	auto* wav = dynamic_cast<WavFile*>(&container);
	if (!wav) {
		throw RuntimeError("Parsed container is not a WavFile.", "CLI");
	}
	return *wav;
}

inline void print_info(const WavFile& wav) {
	std::cout << "Container: " << wav.riff_id() << '/' << wav.wave_id() << '\n'
		<< "Chunks: " << wav.chunks().size() << '\n';

	if (const auto* format = wav.format()) {
		std::cout << "Format:\n"
			<< "  Audio format: " << format->audio_format << '\n'
			<< "  Channels: " << format->channels << '\n'
			<< "  Sample rate: " << format->sample_rate << '\n'
			<< "  Byte rate: " << format->byte_rate << '\n'
			<< "  Block align: " << format->block_align << '\n'
			<< "  Bits per sample: " << format->bits_per_sample << '\n';
	}

	if (const auto* audio = wav.audio_data()) {
		std::cout << "Audio bytes: " << audio->size() << '\n';
	}
	std::cout << "Cue points: " << wav.cue_points().size() << '\n'
		<< "Labels: " << wav.labels().size() << '\n'
		<< "Sample loops: " << wav.sample_loops().size() << '\n';
}


inline std::unique_ptr<JSONValue> parse_bext_value(const std::string& field_path, const std::string& text) {
	const std::type_index field_type = Patcher::bext_field_type(field_path);
	if (field_type == typeid(std::string) && (text.empty() || (text.front() != '"' && text.front() != '\''))) {
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
	if (field_type != typeid(std::string) && field_type != typeid(std::vector<uint8_t>)
		&& !dynamic_cast<JSONInt*>(value.get()) && !dynamic_cast<JSONString*>(value.get())) {
		throw RuntimeError("BEXT field expects an integer value: " + field_path, "Parsing --set value");
	}
	return value;
}

inline void set_bext_field(Container& container, const std::string& assignment) {
	const size_t separator = assignment.find('=');
	if (separator == std::string::npos || separator == 0 || separator + 1 == assignment.size()) {
		throw RuntimeError("Expected <bext.field>=<json-value>, got: " + assignment, "Parsing --set");
	}

	const std::string field_path = assignment.substr(0, separator);
	auto value = parse_bext_value(field_path, assignment.substr(separator + 1));
	Patcher patcher(require_wav(container));
	auto patch = Patcher::create_bext_patch(field_path, std::move(value));
	if (!patcher.apply_patch(patch)) {
		throw RuntimeError("Could not set BEXT field: " + field_path, "Applying --set");
	}
	std::cout << "Set " << field_path << '\n';
}

inline void copy_markers(Container& target_container, const std::string& source_path,
	const bool include_labels) {
	auto source_container = Parser::parse_file(source_path);
	auto& source = require_wav(*source_container);
	auto& target = require_wav(target_container);
	target.copy_markers_from(source, include_labels);
	std::cout << "Copied " << target.cue_points().size() << " marker(s) from "
		<< source_path;
	if (include_labels) {
		std::cout << " with " << target.labels().size() << " label(s)";
	}
	std::cout << '\n';
}

inline void remove_markers(Container& container) {
	auto& wav = require_wav(container);
	const auto marker_count = wav.cue_points().size();
	const auto loop_count = wav.sample_loops().size();
	wav.remove_markers();
	std::cout << "Removed " << marker_count << " marker(s) and "
		<< loop_count << " sample loop(s)\n";
}

/// add main cli options
inline void add_options(CliApp::CliParser& parser) {
	auto include_labels = std::make_shared<bool>(true);

	parser.add_option({
		"-i", "--info", "Print WAV container and format information.", "",
		[&parser](const std::string&, const std::optional<std::string>&) {
			parser.add_command(CliApp::Command([](Container& container) {
				print_info(require_wav(container));
			}));
			return true;
		}
	});

	parser.add_option({
		"-s", "--set", "Set a BEXT field to a JSON value.", "<bext.field>=<json-value>",
		[&parser](const std::string& option_name, const std::optional<std::string>& value) {
			const auto separator = value->find('=');
			if (separator == std::string::npos || separator == 0 || separator + 1 == value->size()) {
				std::cerr << "Error: Invalid format for " << option_name
					<< ". Expected <bext.field>=<json-value>, got: " << *value << '\n';
				return false;
			}
			parser.add_command(CliApp::Command([assignment = *value](Container& container) {
				set_bext_field(container, assignment);
			}, true));
			return true;
		}
	});

	parser.add_option({
		"-g", "--get", "Print a BEXT field as JSON.", "<bext.field>",
		[&parser](const std::string&, const std::optional<std::string>& value) {
			parser.add_command(CliApp::Command([field_path = *value](Container& container) {
				std::cout << Patcher(require_wav(container)).get_bext_value(field_path)->get_string() << '\n';
			}));
			return true;
		}
	});

	parser.add_option({
		"", "--copy-markers", "Copy wav markers (cue points) from the source WAV into the input WAV.", "<source.wav>",
		[&parser, include_labels](const std::string&, const std::optional<std::string>& value) {
			parser.add_command(CliApp::Command(
				[source_path = *value, include_labels](Container& container) {
					copy_markers(container, source_path, *include_labels);
				}, true));
			return true;
		}
	});

	parser.add_option({
		"", "--no-labels", "Do not copy labels with --copy-markers.", "",
		[include_labels](const std::string&, const std::optional<std::string>&) {
			*include_labels = false;
			return true;
		}
	});

	parser.add_option({
		"", "--remove-markers", "Remove all cue markers, labels, and sample loops.", "",
		[&parser](const std::string&, const std::optional<std::string>&) {
			parser.add_command(CliApp::Command([](Container& container) {
				remove_markers(container);
			}, true));
			return true;
		}
	});
}

inline int handle(const int argc, const char* argv[]) {
	try {
		CliApp::CliParser cli_parser("wavmarker");
		add_options(cli_parser);
		if (!cli_parser.parse_arguments(argc, argv)) {
			return 1;
		}
		if (cli_parser.should_print_help()) {
			cli_parser.print_usage();
			return 0;
		}
		if (cli_parser.should_print_version()) {
			CliApp::CliParser::print_version();
			return 0;
		}

		auto container = Parser::parse_file(*cli_parser.get_input_filepath());
		if (!cli_parser.has_commands()) {
			print_info(require_wav(*container));
		}
		cli_parser.execute_commands(*container);

		if (cli_parser.has_mutating_commands() || cli_parser.has_explicit_output()) {
			const auto output_path = *cli_parser.get_effective_output_filepath();
			if (std::filesystem::path(output_path).extension() == ".json") {
				Generator::dump_json(require_wav(*container), output_path);
			} else {
				Generator::write_file(*container, output_path);
			}
			std::cout << "Wrote: " << output_path << '\n';
		}
		return 0;
	} catch (const BaseError& error) {
		std::cerr << error.get_detailed_description() << '\n';
		return 1;
	} catch (const std::exception& error) {
		std::cerr << "Error: " << error.what() << '\n';
		return 1;
	}
}
}
