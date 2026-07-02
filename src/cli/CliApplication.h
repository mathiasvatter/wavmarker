#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

struct Container;

namespace CliApp {

/**
 * A deferred CLI operation.
 *
 * Option handlers create commands while parsing arguments. The commands are
 * executed later, after main() has loaded the input container. This keeps
 * argument parsing independent from container lifetime and removes the need
 * for a central command-type switch.
 */
class Command {
public:
	/**
	 * @param action Operation to execute against the loaded container.
	 * @param modifies_container Whether the operation requires writing the
	 *        container back to the output file after all commands have run.
	 */
	explicit Command(std::function<void(Container&)> action, bool modifies_container = false);

	void execute(Container& container) const;
	[[nodiscard]] bool modifies_container() const;

private:
	std::function<void(Container&)> m_action;
	bool m_modifies_container;
};

/**
 * Parses command-line options and builds an ordered list of deferred commands.
 *
 * The parser owns CLI metadata, input/output paths and command ordering.
 * Container loading and final serialization remain the responsibility of
 * main().
 */
class CliParser {
public:
	/// Static option metadata plus the parser callback invoked for that option.
	struct Option {
		std::string short_name;
		std::string long_name;
		std::string description;
		std::string value_placeholder;
		std::function<bool(const std::string&, const std::optional<std::string>&)> handler;
		bool hidden = false;

		[[nodiscard]] bool takes_value() const {
			return !value_placeholder.empty();
		}
	};

private:
	// Commands preserve CLI order; option indices provide fast alias lookup.
	std::vector<Command> m_commands;
	std::vector<Option> m_options;
	std::unordered_map<std::string, std::size_t> m_option_indices;
	std::string m_program_name;
	std::optional<std::string> m_input_filepath;
	std::optional<std::string> m_output_filepath;
	bool m_print_help = false;
	bool m_print_version = false;

	void add_default_options();
	void set_input_filepath(std::string path);
	void set_output_filepath(std::string path);

public:
	explicit CliParser(std::string name);

	void add_option(Option option);
	void add_command(Command command);
	/// Parses argv and creates commands without accessing an input container.
	bool parse_arguments(int argc, const char* argv[]);
	/// Executes parsed commands in the same order in which they appeared.
	void execute_commands(Container& container) const;

	[[nodiscard]] const std::optional<std::string>& get_input_filepath() const;
	/// Returns the explicit output path, or the input path for in-place writes.
	[[nodiscard]] std::optional<std::string> get_effective_output_filepath() const;
	[[nodiscard]] bool has_commands() const;
	/// True when at least one command changes the loaded container.
	[[nodiscard]] bool has_mutating_commands() const;
	[[nodiscard]] bool has_explicit_output() const;
	[[nodiscard]] bool should_print_help() const;
	[[nodiscard]] bool should_print_version() const;

	void print_usage() const;
	static void print_version();

};

} // namespace CliApp
