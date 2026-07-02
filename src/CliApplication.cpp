#include "CliApplication.h"

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "version.h"

namespace CliApp {

Command::Command(std::function<void(Container&)> action, const bool modifies_container)
	: m_action(std::move(action)), m_modifies_container(modifies_container) {}

void Command::execute(Container& container) const {
	m_action(container);
}

bool Command::modifies_container() const {
	return m_modifies_container;
}

CliParser::CliParser(std::string name) : m_program_name(std::move(name)) {
	add_default_options();
}

bool CliParser::parse_arguments(const int argc, const char* argv[]) {
	// m_program_name = argc > 0 ? std::filesystem::path(argv[0]).filename().string() : "wavmarker";

	if (argc == 1) {
		m_print_help = true;
		return true;
	}

	std::vector<std::string> positional_arguments;
	for (int i = 1; i < argc; ++i) {
		const std::string argument = argv[i];
		if (!argument.starts_with('-')) {
			positional_arguments.push_back(argument);
			continue;
		}

		const auto option_it = m_option_indices.find(argument);
		if (option_it == m_option_indices.end()) {
			std::cerr << "Error: Unknown option: " << argument << '\n';
			print_usage();
			return false;
		}

		const auto& option = m_options[option_it->second];
		std::optional<std::string> value;
		if (option.takes_value()) {
			if (i + 1 >= argc || argv[i + 1][0] == '-') {
				std::cerr << "Error: Option " << argument << " requires a value ("
					<< option.value_placeholder << ").\n";
				print_usage();
				return false;
			}
			value = argv[++i];
		}

		if (!option.handler(argument, value)) {
			return false;
		}
		if (m_print_help || m_print_version) {
			return true;
		}
	}

	if (positional_arguments.size() > 2) {
		std::cerr << "Error: Expected an input path and optional output path, but found "
			<< positional_arguments.size() << " positional arguments.\n";
		print_usage();
		return false;
	}
	if (!positional_arguments.empty()) {
		set_input_filepath(std::move(positional_arguments[0]));
	}
	if (positional_arguments.size() == 2) {
		if (m_output_filepath) {
			std::cerr << "Error: Output path was specified both positionally and with --output.\n";
			return false;
		}
		set_output_filepath(std::move(positional_arguments[1]));
	}

	if (!m_input_filepath) {
		std::cerr << "Error: An input file path is required.\n";
		print_usage();
		return false;
	}

	return true;
}

void CliParser::execute_commands(Container& container) const {
	for (const auto& command : m_commands) {
		command.execute(container);
	}
}

const std::optional<std::string>& CliParser::get_input_filepath() const {
	return m_input_filepath;
}

std::optional<std::string> CliParser::get_effective_output_filepath() const {
	return m_output_filepath ? m_output_filepath : m_input_filepath;
}

bool CliParser::has_commands() const {
	return !m_commands.empty();
}

bool CliParser::has_mutating_commands() const {
	return std::ranges::any_of(m_commands, &Command::modifies_container);
}

bool CliParser::has_explicit_output() const {
	return m_output_filepath.has_value();
}

bool CliParser::should_print_help() const {
	return m_print_help;
}

bool CliParser::should_print_version() const {
	return m_print_version;
}

void CliParser::print_usage() const {
	std::cout << m_program_name << " " << PROJECT_VERSION << "\n"
		<< "Usage: " << m_program_name << " [options] <input.wav> [output.wav|output.json]\n\n"
		<< "Description:\n"
		<< "  Inspect and modify WAV metadata while preserving unknown chunks.\n\n"
		<< "Options:\n";

	for (const auto& option : m_options) {
		if (option.hidden) {
			continue;
		}
		std::ostringstream names;
		names << "  ";
		if (!option.short_name.empty()) {
			names << option.short_name;
			if (!option.long_name.empty()) {
				names << ", ";
			}
		}
		names << option.long_name;
		if (option.takes_value()) {
			names << ' ' << option.value_placeholder;
		}
		std::cout << std::left << std::setw(44) << names.str() << option.description << '\n';
	}

	std::cout << "\nExamples:\n"
		<< "  " << m_program_name << " --info input.wav\n"
		<< "  " << m_program_name << " input.wav output.wav\n"
		<< "  " << m_program_name << " --set 'bext.originator=\"WAVMARKER\"' input.wav\n"
		<< "  " << m_program_name << " --get bext.originator input.wav\n"
		<< "  " << m_program_name << " --copy-markers source.wav target.wav --no-labels\n";
}

void CliParser::print_version() {
	std::cout << "wavmarker " << PROJECT_VERSION << '\n';
}

void CliParser::add_option(Option option) {
	if (option.short_name.empty() && option.long_name.empty()) {
		throw std::invalid_argument("CLI option requires at least one name.");
	}

	const auto index = m_options.size();
	if (!option.short_name.empty()) {
		m_option_indices[option.short_name] = index;
	}
	if (!option.long_name.empty()) {
		m_option_indices[option.long_name] = index;
	}
	m_options.push_back(std::move(option));
}

void CliParser::add_command(Command command) {
	m_commands.push_back(std::move(command));
}

void CliParser::set_input_filepath(std::string path) {
	if (!std::filesystem::exists(std::filesystem::absolute(path))) {
		throw std::invalid_argument("Path \"" + path + "\" does not exist.");
	}
	m_input_filepath = std::move(path);
}

void CliParser::set_output_filepath(std::string path) {
	m_output_filepath = std::move(path);
}

void CliParser::add_default_options() {
	add_option({
		"-v", "--version", "Show the current program version.", "",
		[this](const std::string&, const std::optional<std::string>&) {
			m_print_version = true;
			return true;
		}
	});

	add_option({
		"-h", "--help", "Show this help message.", "",
		[this](const std::string&, const std::optional<std::string>&) {
			m_print_help = true;
			return true;
		}
	});

	add_option({
		"-o", "--output", "Write to this path instead of overwriting the input.", "<filepath>",
		[this](const std::string&, const std::optional<std::string>& value) {
			set_output_filepath(*value);
			return true;
		}
	});
}

} // namespace CliApp
