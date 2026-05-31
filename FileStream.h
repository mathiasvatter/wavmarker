//
// Created by Mathias Vatter on 10.05.25.
//

#pragma once

#include <istream>
#include <ostream>
#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "util/Error.h"

class FileStream {
protected:
	std::string m_filename;
public:

	explicit FileStream(std::string filename) : m_filename(std::move(filename)) {}
	virtual ~FileStream() = default;

	virtual long long offset() = 0;
	[[nodiscard]] virtual bool is_open() = 0;
	[[nodiscard]] virtual bool good() = 0;
	[[nodiscard]] virtual bool fail() = 0;
	[[nodiscard]] virtual bool bad() = 0;
	virtual void clear() = 0;

	[[nodiscard]] explicit operator bool() {
		return !this->fail(); // Gibt true zurück, wenn der Stream NICHT im Fehlerzustand ist
	}

	[[nodiscard]] std::string& filename() { return m_filename; }
};


class FileInputStream : public FileStream {
	std::ifstream m_file_stream;

protected:
	explicit FileInputStream(std::string path, bool)
		: FileStream(std::move(path)) {}

public:
	// Constructor opens the file
	explicit FileInputStream(const std::string& path, std::ios_base::openmode mode = std::ios::binary)
		: FileStream(path) {
		m_file_stream.open(path, mode);
		// if (!m_file_stream.is_open()) {
		// 	// Throw a ReadError if the file couldn't be opened.
		// 	// No offset applicable here as opening failed.
		// 	throw ReadError("Failed to open input file", *this, "Opening file");
		// }
	}

	~FileInputStream() override = default;
	// Prevent copying and assignment if managing a unique resource like a file stream
	FileInputStream(const FileInputStream&) = delete;
	FileInputStream& operator=(const FileInputStream&) = delete;
	FileInputStream(FileInputStream&&)  noexcept = default; // Allow moving
	FileInputStream& operator=(FileInputStream&&)  noexcept = default; // Allow moving

	virtual std::istream& stream() { return m_file_stream; }
	[[nodiscard]] long long offset() override {return m_file_stream.tellg();}
	[[nodiscard]] virtual size_t remaining_bytes() {
		auto& input = stream();
		const auto current = input.tellg();
		if (current == std::istream::pos_type(-1)) {
			return 0;
		}

		input.seekg(0, std::ios::end);
		const auto end = input.tellg();
		if (end == std::istream::pos_type(-1) || end < current) {
			input.clear();
			input.seekg(current);
			return 0;
		}

		input.seekg(current);
		if (!input) {
			input.clear();
			return 0;
		}

		return static_cast<size_t>(end - current);
	}
	std::string content() {
		return {std::istreambuf_iterator<char>(stream()), std::istreambuf_iterator<char>()};
	}

	virtual void seekg(const long long offset, const std::ios_base::seekdir dir) {
		stream().seekg(offset, dir);
		if (!stream()) {
			throw ReadError("Failed to seek in input stream", *this, "Seeking to offset");
		}
	}
	bool is_open() override { return m_file_stream.is_open(); }
	[[nodiscard]] bool good() override { return stream().good(); }
	// Expose eof, good, fail, bad for convenience
	[[nodiscard]] virtual bool eof() { return stream().eof(); }
	[[nodiscard]] bool fail() override { return stream().fail(); }
	[[nodiscard]] bool bad() override { return stream().bad(); }
	void clear() override { stream().clear(); } // Clear error flags

	// Liest ein einzelnes Zeichen und gibt es als int zurück (oder EOF bei Fehler/Ende).
	virtual int get() { return stream().get(); }
	virtual FileInputStream& get(char& s) { stream().get(s); return *this; }
	virtual FileInputStream& read(char* buffer, std::streamsize size) { stream().read(buffer, size); return *this; }
	virtual std::streamsize gcount() { return stream().gcount(); }
	virtual std::streambuf* rdbuf() { return stream().rdbuf(); }
	virtual std::streampos tellg() { return stream().tellg(); }
};


class FileInputSubStream final : public FileInputStream {
	long long m_parent_stream_start_offset; // Offset in the conceptual parent stream
	std::istringstream m_internal_istream;  // Internal stream using an internal copy of the data

public:
	FileInputSubStream(const std::vector<std::uint8_t>& data, FileInputStream& parent_stream)
		: FileInputStream(parent_stream.filename(), true),
		  m_parent_stream_start_offset(parent_stream.offset()),
		  m_internal_istream(std::string(data.begin(), data.end()), std::ios::binary) {}

	~FileInputSubStream() override = default;

	// Copying is deleted because std::istringstream is not copyable.
	FileInputSubStream(const FileInputSubStream&) = delete;
	FileInputSubStream& operator=(const FileInputSubStream&) = delete;
	FileInputSubStream(FileInputSubStream&& other) noexcept = default;
	FileInputSubStream& operator=(FileInputSubStream&& other) noexcept = default;

	// Provide access to the internal istream
	std::istream& stream() override { return m_internal_istream; }
	[[nodiscard]] long long offset() override {
		std::streampos current_sub_pos = m_internal_istream.tellg();
		return m_parent_stream_start_offset + static_cast<long long>(current_sub_pos);
	}

	[[nodiscard]] bool is_open() override { return true; }
	size_t size() const { return m_internal_istream.view().size(); }

	std::vector<std::uint8_t> data() const {
		std::string data = m_internal_istream.str();
		return std::vector<std::uint8_t>(data.begin(), data.end());
	}

};



class FileOutputStream : public FileStream {
	std::ofstream m_file_stream;

protected:
	explicit FileOutputStream(std::string path, bool)
		: FileStream(std::move(path)) {}

public:
	// Constructor opens the file
	explicit FileOutputStream(const std::string& path, std::ios_base::openmode mode = std::ios::binary | std::ios::trunc)
		: FileStream(path) {
		m_file_stream.open(path, mode);
		if (!m_file_stream.is_open()) {
			// Throw a WriteError if the file couldn't be opened.
			throw WriteError("Failed to open output file", *this, "Opening file");
		}
	}
	~FileOutputStream() override = default;
	// Prevent copying and assignment
	FileOutputStream(const FileOutputStream&) = delete;
	FileOutputStream& operator=(const FileOutputStream&) = delete;
	FileOutputStream(FileOutputStream&&)  noexcept = default; // Allow moving
	FileOutputStream& operator=(FileOutputStream&&)  noexcept = default; // Allow moving

	virtual std::ostream& stream() { return m_file_stream; }
	[[nodiscard]] long long offset() override {return m_file_stream.tellp();}

	virtual void seekp(const long long offset, const std::ios_base::seekdir dir) {
		stream().seekp(offset, dir);
		if (!stream()) {
			throw WriteError("Failed to seek in output stream", *this, "Seeking to offset");
		}
	}

	virtual FileOutputStream& write(const char* buffer, std::streamsize size) {
		stream().write(buffer, size);
		return *this;
	}
	virtual FileOutputStream& put(char c) {
		stream().put(c);
		return *this;
	}

	// Expose good, fail, bad for convenience
	bool is_open() override { return m_file_stream.is_open(); }
	[[nodiscard]] bool good() override { return stream().good(); }
	[[nodiscard]] bool fail() override { return stream().fail(); }
	[[nodiscard]] bool bad() override  { return stream().bad(); }
	void clear() override { stream().clear(); } // Clear error flags

};

class FileOutputSubStream final : public FileOutputStream {
	long long m_parent_stream_start_offset; // Offset in the conceptual parent stream
	std::ostringstream m_internal_ostream;

public:
	explicit FileOutputSubStream(FileOutputStream& parent_stream)
		: FileOutputStream(parent_stream.filename(), true),
		  m_parent_stream_start_offset(parent_stream.offset()) {}

	explicit FileOutputSubStream() : FileOutputStream("", true), m_parent_stream_start_offset(0) {}

	~FileOutputSubStream() override = default;

	// Copying is deleted because std::istringstream is not copyable.
	FileOutputSubStream(const FileOutputSubStream&) = delete;
	FileOutputSubStream& operator=(const FileOutputSubStream&) = delete;
	FileOutputSubStream(FileOutputSubStream&& other) noexcept = default;
	FileOutputSubStream& operator=(FileOutputSubStream&& other) noexcept = default;

	// Provide access to the internal istream
	std::ostream& stream() override { return m_internal_ostream; }
	[[nodiscard]] long long offset() override {
		std::streampos current_sub_pos = m_internal_ostream.tellp();
		return m_parent_stream_start_offset + static_cast<long long>(current_sub_pos);
	}

	[[nodiscard]] bool is_open() override { return true; }

	size_t size() const { return m_internal_ostream.view().size(); }

	std::vector<std::uint8_t> data() const {
		std::string data = m_internal_ostream.str();
		return std::vector<std::uint8_t>(data.begin(), data.end());
	}

};
