//
// Created by Mathias Vatter on 10.05.25.
//

#include "Error.h"
#include "FileStream.h"

BaseError::BaseError(std::string msg, FileStream &file_stream, std::string operation, int line)
	: m_message(std::move(msg)), m_operation(std::move(operation)),
m_filename(file_stream.filename()), m_line_number(line), m_file_offset(file_stream.offset()) {}


BaseError::BaseError(std::string msg, std::string operation, std::string filename,
	int line, long long offset)
: m_message(std::move(msg)), m_operation(std::move(operation)),
m_filename(std::move(filename)), m_line_number(line), m_file_offset(offset) {}

const char * BaseError::what() const noexcept {
	try {
		if (m_what_message_cache.empty()) {
			m_what_message_cache = get_error_type_prefix() + ": " + m_message;
			if (!m_filename.empty()) {
				m_what_message_cache += " (File: " + m_filename;
				if (m_file_offset != -1) {
					m_what_message_cache += ", Offset: 0x" + [](long long val) {
						std::ostringstream oss;
						oss << std::hex << val;
						return oss.str();
					}(m_file_offset);
				}
				if (m_line_number != -1) { // Still include if provided
					m_what_message_cache += ", Line: " + std::to_string(m_line_number);
				}
				m_what_message_cache += ")";
			} else if (m_file_offset != -1) {
				m_what_message_cache += " (Offset: 0x" + [](long long val) {
					std::ostringstream oss;
					oss << std::hex << val;
					return oss.str();
				}(m_file_offset) + ")";
			}
		}
		return m_what_message_cache.c_str();
	} catch (...) {
		if (!m_message.empty()) {
			return m_message.c_str();
		}
		return "An unspecified error occurred.";
	}
}

std::string BaseError::get_detailed_description() const {
	std::ostringstream oss;
	oss << "--- " << get_error_type_prefix() << " ---\n";
	oss << "  Message:   " << m_message << "\n";
	if (!m_operation.empty()) {
		oss << "  Operation: " << m_operation << "\n";
	}
	if (!m_filename.empty()) {
		oss << "  File:      " << m_filename << "\n";
	}
	if (m_file_offset != -1) {
		oss << "  Offset:    0x" << std::hex << m_file_offset << " (Decimal: " << std::dec << m_file_offset << ")\n";
	}
	if (m_line_number != -1) { // Still include if provided
		oss << "  Line:      " << std::dec << m_line_number << "\n";
	}
	return oss.str();
}

FileOperationError::FileOperationError(std::string msg,
                                       FileStream &file_stream,
                                       const std::string &operation_prefix,
                                       int line)
	: BaseError(std::move(msg), file_stream, operation_prefix, line) {}

FileOperationError::FileOperationError(std::string msg,
                                       const std::string &operation_prefix,
                                       std::string filename,
                                       int line,
                                       long long offset):
BaseError(std::move(msg), operation_prefix, std::move(filename), line, offset) {}

ReadError::ReadError(std::string msg, FileStream &file_stream, const std::string &operation, int line):
FileOperationError(std::move(msg), file_stream,
"File read" + (operation.empty() ? "" : " - " + operation),
	line) {}

ReadError::ReadError(std::string msg,
                     std::string filename,
                     long long offset,
                     const std::string &operation_details,
                     int line)
: FileOperationError(std::move(msg),
"File read" + (operation_details.empty() ? "" : " - " + operation_details),
std::move(filename), line, offset) {}

