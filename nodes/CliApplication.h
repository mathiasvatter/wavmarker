//
// Created by Mathias Vatter on 11.06.26.
//

#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <memory>    // For std::unique_ptr
#include <stdexcept> // For std::runtime_error
#include <algorithm> // For std::find_if
#include <sstream>   // For std::ostringstream
#include <optional>  // For std::optional
#include <functional> // For std::function
#include <map>       // For storing options
#include <filesystem>
#include <version.h>

namespace CliApp {
/// Represents a parsed high-level command operation
struct Command {
	enum class Type { SET, GET, CLEAR, HELP, VERSION, BATCH_SET_FROM_FILE, EXPORT_JSON, CONFIG_ONLY, UNKNOWN }; // CONFIG_ONLY for options like --output

	Type type = Type::UNKNOWN;
	std::string path; // Used by SET, GET, CLEAR, EXPORT_JSON + BATCH_SET_FROM_TILE (this is the output path then)
	// std::unique_ptr<JSONValue> value; // Only used for SET commands

	// Constructor for HELP, UNKNOWN, CONFIG_ONLY
	explicit Command(Type t) : type(t) {}
	// Constructor for GET, CLEAR, SET
	Command(Type t, std::string p) : type(t), path(std::move(p)) {}
	// Constructor for SET
	// Command(Type t, std::string p, std::unique_ptr<JSONValue> v) : type(t), path(std::move(p)), value(std::move(v)) {}
};

class CliParser;

/// Definition of a command line option
struct Option {
	std::string short_name; // e.g., "-h" (can be empty)
	std::string long_name;  // e.g., "--help" (should not be empty)
	std::string description;
	bool takes_value;       // Does this option expect an argument after it?
	std::string value_placeholder; // e.g., "<path>" or "<path>=<json_value>"
	// Processor function: takes the option name triggered and its string value (if any),
	// returns an optional Command. If std::nullopt, it's an error or the option
	// doesn't directly map to a single Command (could set a flag in parser etc.).
	std::function<std::optional<Command>(CliParser* parser, const std::string& option_name, const std::optional<std::string>& value_str)> processor;
};

class CliParser {
	std::vector<Command> m_parsed_commands;
	std::map<std::string, Option> m_options; // Stores canonical option definitions by primary key
	std::map<std::string, std::string> m_option_lookup_map; // Maps all names (-s, --set) to primary key
	std::string m_program_name;
	std::optional<std::string> m_input_filepath;
	std::optional<std::string> m_output_filepath;

public:

	explicit CliParser() {
		// Register default options/commands
		add_default_options();
	}

	/**
	 * @brief Adds a definition for a command-line option.
	 * @param opt The Option structure defining the new option.
	 */
	void add_option(Option opt) {
		// Ensure long_name is primary if available, otherwise short_name
		std::string primary_key = opt.long_name.empty() ? opt.short_name : opt.long_name;
		if (primary_key.empty()) {
			// Should not happen with well-defined options
			std::cerr << "Warning: Attempting to add an option with no name." << std::endl;
			return;
		}
		m_options[primary_key] = opt; // Store by a primary key

		// Create mappings for a quick lookup
		if (!opt.long_name.empty()) {
			m_option_lookup_map[opt.long_name] = primary_key;
		}
		if (!opt.short_name.empty()) {
			m_option_lookup_map[opt.short_name] = primary_key;
		}
	}

	bool parse_arguments(int argc, const char* argv[]) {
		if (argc < 1) {
			m_program_name = "cli_tool";
		} else {
			m_program_name = argv[0];
			size_t last_slash = m_program_name.find_last_of("/\\");
			if (last_slash != std::string::npos) {
				m_program_name = m_program_name.substr(last_slash + 1);
			}
		}

		if (argc == 1) { // No arguments provided, show help
			m_parsed_commands.emplace_back(Command::Type::HELP);
			return true;
		}

		std::vector<std::string> positional_args;
		int i = 1;
		while (i < argc) {
			std::string arg_str = argv[i];

			if (arg_str.rfind('-', 0) == 0) { // It's an option
				auto lookup_it = m_option_lookup_map.find(arg_str);
				if (lookup_it == m_option_lookup_map.end()) {
					std::cerr << "Error: Unknown option: " << arg_str << std::endl;
					print_usage();
					return false;
				}

				const Option& opt_def = m_options.at(lookup_it->second); // Get canonical option
				std::optional<std::string> value_for_option = std::nullopt;

				if (opt_def.takes_value) {
					if (i + 1 < argc) {
						// Check if the next argument is also an option; if so, value is missing
						if (argv[i+1][0] == '-') {
							std::cerr << "Error: Option " << arg_str << " requires a value (" << opt_def.value_placeholder << "), but found another option '" << argv[i+1] << "' instead." << std::endl;
							print_usage();
							return false;
						}
						value_for_option = argv[++i];
					} else {
						std::cerr << "Error: Option " << arg_str << " requires a value (" << opt_def.value_placeholder << ")." << std::endl;
						print_usage();
						return false;
					}
				}

				if (opt_def.processor) {
					auto command_opt = opt_def.processor(this, arg_str, value_for_option);
					if (command_opt) {
						// Only add if it's not a "config only" pseudo-command that just sets parser state
						if (command_opt.value().type != Command::Type::CONFIG_ONLY) {
							m_parsed_commands.push_back(command_opt.value());
						}
						if (command_opt.value().type == Command::Type::HELP || command_opt.value().type == Command::Type::VERSION) return true; // Stop after help/version
					} else {
						// Processor indicated an error.
						return false;
					}
				} else {
					std::cerr << "Error: No processor defined for option " << arg_str << std::endl;
					return false;
				}
			} else { // It's a positional argument
				positional_args.push_back(arg_str);
			}
			i++;
		}

		// Process positional arguments (expecting one for input file)
		if (!positional_args.empty()) {
			if (positional_args.size() == 1) {
				m_input_filepath = positional_args[0];
			} else {
				std::cerr << "Error: Expected one input file path, but found " << positional_args.size() << " positional arguments: ";
				for(const auto& pa : positional_args) std::cerr << "'" << pa << "' ";
				std::cerr << std::endl;
				print_usage();
				return false;
			}
		}

		// If no actual commands were parsed (e.g., only --output was given) and no input file,
		// it might be an incomplete command line. However, if help was requested, it's fine.
		bool has_action_command = false;
		for(const auto& cmd : m_parsed_commands) {
			if(cmd.type != Command::Type::HELP && cmd.type != Command::Type::CONFIG_ONLY && cmd.type != Command::Type::UNKNOWN) {
				has_action_command = true;
				break;
			}
		}

		if (!has_action_command && !m_input_filepath.has_value() && argc > 1) {
			bool only_config_options = true;
			for(const auto& cmd : m_parsed_commands){
				if(cmd.type != Command::Type::CONFIG_ONLY && cmd.type != Command::Type::HELP){
					only_config_options = false;
					break;
				}
			}
			if(m_parsed_commands.empty() || only_config_options){
				// If only config options like --output were given, but no action command or input file.
				// This might be an error depending on requirements. For now, allow it if output was set.
				if(!m_output_filepath.has_value()){ // If output wasn't even set, it's likely an error.
					std::cerr << "Error: No action specified (e.g., --set, --get) and no input file provided." << std::endl;
					print_usage();
					return false;
				}
			}
		}
		if (has_action_command && !m_input_filepath.has_value()) {
			// If commands like --set, --get, --clear are present, an input file is typically expected.
			// This logic can be adjusted based on whether the input file is always mandatory for these commands.
			std::cerr << "Error: An input file path is required for the specified command(s)." << std::endl;
			print_usage();
			return false;
		}

		return true;
	}

	[[nodiscard]] const std::vector<Command>& get_parsed_commands() const {
		return m_parsed_commands;
	}

	[[nodiscard]] std::optional<std::string> get_input_filepath() const {
		return m_input_filepath;
	}

	[[nodiscard]] std::optional<std::string> get_output_filepath() const {
		return m_output_filepath;
	}

	/**
	 * @brief Gets the effective output filepath. If an output path was specified via -o/--output,
	 * that is returned. Otherwise, if an input path is available, a default output path is
	 * generated (e.g., input "dir/file.txt" -> default output "dir/out.txt").
	 * @return An optional string containing the output filepath. Returns std::nullopt if
	 * no output path is specified and no input path is available to derive a default.
	 */
	[[nodiscard]] std::optional<std::string> get_effective_output_filepath() const {
		if (m_output_filepath.has_value()) {
			return m_output_filepath;
		}
		if (m_input_filepath.has_value()) {
			// std::filesystem::path input_fs_path(m_input_filepath.value());
			// std::filesystem::path output_fs_path = input_fs_path.parent_path() / "out";
			// if (input_fs_path.has_extension()) {
			//     output_fs_path += input_fs_path.extension();
			// }
			// return output_fs_path.string();
			return m_input_filepath.value();
		}
		return std::nullopt; // No explicit output, no input to derive default from
	}

	/// Public setter for output filepath, used by the --output option's processor
	void set_output_filepath(const std::string& path) {
		m_output_filepath = path;
	}

	void print_usage() const {
		std::cout << "Usage: " << m_program_name << " [options] [input_file]" << std::endl;
		std::cout << std::endl;
		std::cout << "Description:" << std::endl;
		std::cout << "  A Small Cli tool for parsing WAV files into structs and writing them back."
					 " Accepted file types are *.wav. More to be added." << std::endl;
		std::cout << std::endl;
		std::cout << "Positional Arguments:" << std::endl;
		std::cout << std::left << std::setw(40) << "  input_file" << "The main file to operate on." << std::endl;
		std::cout << std::endl;
		std::cout << "Options:" << std::endl;

		std::vector<const Option*> unique_options_to_print;
		std::map<std::string, bool> printed_long_names;

		for(const auto& pair : m_options){
			// Use the canonical Option object stored by its primary key
			const Option* opt = &pair.second;
			// Ensure we only print each unique option once, preferring the long_name version if available
			std::string key_for_uniqueness = opt->long_name.empty() ? opt->short_name : opt->long_name;
			if(printed_long_names.find(key_for_uniqueness) == printed_long_names.end()){
				unique_options_to_print.push_back(opt);
				printed_long_names[key_for_uniqueness] = true;
			}
		}
		// Sort options for consistent display, e.g., alphabetically by long_name or short_name
		std::sort(unique_options_to_print.begin(), unique_options_to_print.end(), [](const Option* a, const Option* b){
			const std::string& name_a = !a->long_name.empty() ? a->long_name : a->short_name;
			const std::string& name_b = !b->long_name.empty() ? b->long_name : b->short_name;
			return name_a < name_b;
		});


		for (const auto* opt : unique_options_to_print) {
			std::ostringstream line;
			line << "  ";
			if (!opt->short_name.empty()) {
				line << opt->short_name;
				if (!opt->long_name.empty()) {
					line << ", ";
				}
			}
			if (!opt->long_name.empty()) {
				line << opt->long_name;
			}
			if (opt->takes_value && !opt->value_placeholder.empty()) {
				line << " " << opt->value_placeholder;
			}
			std::cout << std::left << std::setw(40) << line.str() << opt->description << std::endl;
		}
		std::cout << std::endl;
		std::cout << "Examples:" << std::endl;
		// std::cout << "  " << m_program_name << " --set object.name=\"My Object\" myfile.nki" << std::endl;
		// std::cout << "  " << m_program_name << " --set object.count=10 myfile.nki -o output.nki" << std::endl;
		// std::cout << "  " << m_program_name << " --get object.name myfile.nki" << std::endl;
		// std::cout << "  " << m_program_name << " --to-json exported.json myfile.nki" << std::endl;
		// std::cout << "  " << m_program_name << " --from-json batch.json myfile.nki" << std::endl;
	}

	static void print_version() {
		std::cout << "nicontainer version " << PROJECT_VERSION << std::endl;
	}

private:
	void add_default_options() {
		add_option({
			"-s", "--set", "Set a property at <path> to <json_value>.", true, "<path>=<json_value>",
			[this](CliParser* parser, const std::string& opt_name, const std::optional<std::string>& value_str_opt) -> std::optional<CliApp::Command> {
				if (!value_str_opt) {
					std::cerr << "Error: " << opt_name << " requires a value in <path>=<json_value> format." << std::endl;
					return std::nullopt;
				}
				const std::string& path_value_pair = value_str_opt.value();
				size_t equals_pos = path_value_pair.find('=');
				if (equals_pos == std::string::npos || equals_pos == 0 || equals_pos == path_value_pair.length() - 1) {
					std::cerr << "Error: Invalid format for " << opt_name << ". Expected <path>=<json_value>, got: " << path_value_pair << std::endl;
					return std::nullopt;
				}
				return CliApp::Command(Command::Type::SET, value_str_opt.value());
			}
		});

		add_option({
			"-g", "--get", "Get the property at <path>.", true, "<path>",
			[](CliParser* parser, const std::string& opt_name, const std::optional<std::string>& value_str_opt) -> std::optional<CliApp::Command> {
				if (!value_str_opt || value_str_opt.value().empty()) {
					std::cerr << "Error: " << opt_name << " requires a <path> argument." << std::endl;
					return std::nullopt;
				}
				return CliApp::Command(Command::Type::GET, value_str_opt.value());
			}
		});

		// add_option({
		//     "-c", "--clear", "Clear/remove the property at <path>.", true, "<path>",
		//     [](CliParser* parser, const std::string& opt_name, const std::optional<std::string>& value_str_opt) -> std::optional<CliApp::Command> {
		//         if (!value_str_opt || value_str_opt.value().empty()) {
		//             std::cerr << "Error: " << opt_name << " requires a <path> argument." << std::endl;
		//             return std::nullopt;
		//         }
		//         return CliApp::Command(Command::Type::CLEAR, value_str_opt.value());
		//     }
		// });

		add_option({
			"-v", "--version", "Show current program version.", false, "",
			[](CliParser* parser, const std::string& opt_name, const std::optional<std::string>& value_str_opt) -> std::optional<CliApp::Command> {
				return CliApp::Command(Command::Type::VERSION);
			}
		});

		add_option({
			"-h", "--help", "Show this help message.", false, "",
			[](CliParser* parser, const std::string& opt_name, const std::optional<std::string>& value_str_opt) -> std::optional<CliApp::Command> {
				return CliApp::Command(Command::Type::HELP);
			}
		});

		add_option({
			"-o", "--output", "Specify output file path. If none is specified, the program will overwrite the input file.", true, "<filepath>",
			[](CliParser* parser, const std::string& opt_name, const std::optional<std::string>& value_str_opt) -> std::optional<CliApp::Command> {
				if (!value_str_opt || value_str_opt.value().empty()) {
					std::cerr << "Error: " << opt_name << " requires a <filepath> argument." << std::endl;
					return std::nullopt;
				}
				parser->set_output_filepath(value_str_opt.value());
				// This option configures the parser but doesn't represent an action command itself.
				return CliApp::Command(Command::Type::CONFIG_ONLY);
			}
		});

		//     add_option({
		//         "-j", "--to-json", "Export the input file to the specified path in JSON format.", true, "<output_filepath>",
		//         [](CliParser* parser, const std::string& opt_name, const std::optional<std::string>& value_str_opt) -> std::optional<CliApp::Command> {
		//             if (!value_str_opt || value_str_opt.value().empty()) {
		//                 std::cerr << "Error: " << opt_name << " requires an <output_filepath> argument." << std::endl;
		//                 return std::nullopt;
		//             }
		//             if (!check_path_for_extension(value_str_opt.value(), ".json")) {
		//                 return std::nullopt;
		//             }
		//             // The output path for the JSON export is stored in the Command's path field.
		//             return CliApp::Command(Command::Type::EXPORT_JSON, value_str_opt.value());
		//         }
		//     });
		//
		//     add_option({
		//         "-f", "--from-json", "Set multiple properties from a JSON file.", true, "<batch_filepath>",
		//         [](CliParser* parser, const std::string& opt_name, const std::optional<std::string>& value_str_opt) -> std::optional<CliApp::Command> {
		//             if (!value_str_opt || value_str_opt.value().empty()) {
		//                 std::cerr << "Error: " << opt_name << " requires a <batch_filepath> argument." << std::endl;
		//                 return std::nullopt;
		//             }
		//             if (!check_path_for_extension(value_str_opt.value(), ".json")) {
		//                 return std::nullopt;
		//             }
		//             // The path to the batch JSON file is stored in the Command's path field.
		//             // The actual parsing and creation of multiple SET commands will happen in the main application logic.
		//             return CliApp::Command(Command::Type::BATCH_SET_FROM_FILE, value_str_opt.value());
		//         }
		//     });
		// }



	}

};
}
