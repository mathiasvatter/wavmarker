#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <iomanip>

// Helper function to generate a hex string for a byte vector
static std::string bytes_to_hex_string(const std::vector<uint8_t>& bytes, size_t max_bytes_to_show = 32) {
	if (bytes.empty()) {
		return "[empty]";
	}
	std::ostringstream oss;
	oss << std::hex << std::setfill('0');
	for (size_t i = 0; i < std::min(bytes.size(), max_bytes_to_show); ++i) {
		oss << std::setw(2) << static_cast<int>(bytes[i]) << " ";
	}
	if (bytes.size() > max_bytes_to_show) {
		oss << "... (" << bytes.size() - max_bytes_to_show << " more bytes)";
	}
	return oss.str();
}

class FileStream;
// Base class for all custom errors.
// It inherits from std::exception to allow standard error handling.
class BaseError : public std::exception {
protected:
	std::string m_message;          // The main error message.
	std::string m_operation;        // The operation that caused the error (e.g., "File read", "Chunk parse").
	std::string m_filename;         // The filename, if relevant.
	int m_line_number = -1;         // The line number, if relevant (-1 means not applicable).
	long long m_file_offset = -1;    // Byte offset in the file, if relevant.

	// Cache for the message returned by what().
	// mutable so it can be initialized in the const method what().
	mutable std::string m_what_message_cache;

public:

	// Constructor for BaseError.
	explicit BaseError(std::string msg, FileStream& file_stream, std::string operation = "", int line = -1);
	explicit BaseError(std::string msg, std::string operation = "", std::string filename = "", int line = -1, long long offset = -1);
	~BaseError() noexcept override = default;

	// Overrides std::exception::what().
	const char* what() const noexcept override;

	// Returns a more detailed, formatted description of the error.
	virtual std::string get_detailed_description() const;

	// Pure virtual method for error type prefix.
	virtual std::string get_error_type_prefix() const = 0;

	// Getters
	const std::string& get_message() const { return m_message; }
	const std::string& get_operation() const { return m_operation; }
	const std::string& get_filename() const { return m_filename; }
	int get_line_number() const { return m_line_number; }
	long long get_file_offset() const { return m_file_offset; }
};

// Specialized error class for file operations.
class FileOperationError : public BaseError {
public:
	FileOperationError(std::string msg, FileStream& file_stream, const std::string& operation_prefix, int line = -1);
	explicit FileOperationError(std::string msg, const std::string& operation_prefix, std::string filename,
								int line = -1, long long offset = -1);
};

// Error during file reading.
class ReadError final : public FileOperationError {
public:
	ReadError(std::string msg, FileStream& file_stream, const std::string &operation = "", int line = -1);
	explicit ReadError(std::string msg, std::string filename,
					   long long offset = -1, // Offset is more common for read errors in binary
					   const std::string& operation_details = "",
					   int line = -1) // Line less common, but kept for flexibility
	;

	std::string get_error_type_prefix() const override {
		return "Read Error";
	}
};

// Error during file writing.
class WriteError final : public FileOperationError {
public:
	WriteError(std::string msg, FileStream& file_stream, const std::string& operation_details = "", int line = -1):
		FileOperationError(std::move(msg), file_stream, "File write" + (operation_details.empty() ? "" : " - " + operation_details), line) {}
	explicit WriteError(std::string msg,
						std::string filename,
						long long offset = -1, // Offset where write error might have occurred
						const std::string& operation_details = "")
		: FileOperationError(std::move(msg),
							  "File write" + (operation_details.empty() ? "" : " - " + operation_details),
							  std::move(filename),
							  -1, // Line number usually not applicable for write errors
							  offset) {}

	std::string get_error_type_prefix() const override {
		return "Write Error";
	}
};

// Error during data parsing.
class ParseError final : public BaseError {
	std::string m_parse_context;         // General textual context.
	std::string m_chunk_or_struct_name;    // Name of the NIContainer chunk/struct being parsed.
	std::string m_expected_content;      // What was expected (e.g., magic number, specific value).
	std::string m_actual_content;        // What was actually found.
	std::vector<uint8_t> m_byte_context; // Bytes around the error for hex dump.

public:
	ParseError(std::string msg, FileStream& file_stream,
					std::string chunk_or_struct_name = "",
					std::string expected_content = "",
					std::string actual_content = "",
					std::vector<uint8_t> byte_context = {},
					std::string operation = "Data parse",
					std::string general_parse_context = "" // For the original m_parseContext
					)
						: BaseError(std::move(msg), file_stream,
								  std::move(operation),-1),
						  m_parse_context(std::move(general_parse_context)),
						  m_chunk_or_struct_name(std::move(chunk_or_struct_name)),
						  m_expected_content(std::move(expected_content)),
						  m_actual_content(std::move(actual_content)),
						  m_byte_context(std::move(byte_context)) {}
	explicit ParseError(std::string msg,
						std::string filename = "",
						long long offset = -1,
						std::string chunk_or_struct_name = "",
						std::string expected_content = "",
						std::string actual_content = "",
						std::vector<uint8_t> byte_context = {},
						std::string operation = "Data parse",
						std::string general_parse_context = "" // For the original m_parseContext
						)
		: BaseError(std::move(msg), std::move(operation), std::move(filename), -1, offset),
		  m_parse_context(std::move(general_parse_context)),
		  m_chunk_or_struct_name(std::move(chunk_or_struct_name)),
		  m_expected_content(std::move(expected_content)),
		  m_actual_content(std::move(actual_content)),
		  m_byte_context(std::move(byte_context)) {}

	std::string get_error_type_prefix() const override {
		return "Parse Error";
	}

	std::string get_detailed_description() const override {
		std::ostringstream oss;
		oss << BaseError::get_detailed_description(); // Base details (message, file, offset)
		if (!m_chunk_or_struct_name.empty()) {
			oss << "  Structure/Chunk: " << m_chunk_or_struct_name << "\n";
		}
		if (!m_expected_content.empty()) {
			oss << "  Expected:  " << m_expected_content << "\n";
		}
		if (!m_actual_content.empty()) {
			oss << "  Actual:    " << m_actual_content << "\n";
		}
		if (!m_byte_context.empty()) {
			oss << "  Byte Context: " << bytes_to_hex_string(m_byte_context) << "\n";
		}
		if (!m_parse_context.empty()) { // Original general context
			oss << "  Context:   " << m_parse_context << "\n";
		}
		return oss.str();
	}

	// Getters for ParseError specific fields
	const std::string& get_chunk_or_struct_name() const { return m_chunk_or_struct_name; }
	const std::string& get_expected_content() const { return m_expected_content; }
	const std::string& get_actual_content() const { return m_actual_content; }
	const std::vector<uint8_t>& get_byte_context() const { return m_byte_context; }
	const std::string& get_general_parse_context() const { return m_parse_context; }
};

// General runtime error for other situations.
class RuntimeError final : public BaseError {
public:
	explicit RuntimeError(std::string msg, std::string operation = "Runtime operation")
		: BaseError(std::move(msg), std::move(operation)) {}

	std::string get_error_type_prefix() const override {
		return "Runtime Error";
	}
};


