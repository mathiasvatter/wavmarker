#pragma once

#include <array>
#include <algorithm>
#include <bit>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <istream>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "FileStream.h"

namespace StreamUtils {

	// ----------------------------
	// Fast helpers
	// ----------------------------

	// Reads raw bytes into an existing buffer. No allocations.
	static void read_into(FileInputStream& in, void* dst, size_t num_bytes, const char* ctx) {
		const auto off = in.offset();
		in.read(reinterpret_cast<char*>(dst), static_cast<std::streamsize>(num_bytes));
		if (!in || in.gcount() != static_cast<std::streamsize>(num_bytes)) {
			const auto got = static_cast<size_t>(in.gcount());
			throw ReadError(
				std::string("read_into: expected ") + std::to_string(num_bytes) + " but got " + std::to_string(got),
				in.filename(),
				off,
				ctx
			);
		}
	}

	// Writes raw bytes from an existing buffer. One call, one check.
	static void write_from(FileOutputStream& out, const void* src, size_t num_bytes, const char* ctx) {
		const auto off = out.offset();
		out.write(reinterpret_cast<const char*>(src), static_cast<std::streamsize>(num_bytes));
		if (!out) {
			throw WriteError(
				std::string("write_from: failed to write ") + std::to_string(num_bytes) + " bytes.",
				out.filename(),
				off,
				ctx
			);
		}
	}

	static constexpr uint16_t bswap16(uint16_t x) noexcept {
		return static_cast<uint16_t>((x << 8) | (x >> 8));
	}
	static constexpr uint32_t bswap32(uint32_t x) noexcept {
		return (x << 24)
			| ((x << 8) & 0x00FF0000u)
			| ((x >> 8) & 0x0000FF00u)
			| (x >> 24);
	}
	static constexpr uint64_t bswap64(uint64_t x) noexcept {
		return (x << 56)
			| ((x << 40) & 0x00FF000000000000ull)
			| ((x << 24) & 0x0000FF0000000000ull)
			| ((x << 8)  & 0x000000FF00000000ull)
			| ((x >> 8)  & 0x00000000FF000000ull)
			| ((x >> 24) & 0x0000000000FF0000ull)
			| ((x >> 40) & 0x000000000000FF00ull)
			| (x >> 56);
	}

	template <typename T>
	static constexpr T maybe_bswap(T v, bool is_big_endian_on_disk) noexcept {
		// Assumes T is unsigned 16/32/64.
		constexpr bool host_be = (std::endian::native == std::endian::big);

		// If disk endianness differs from host, swap.
		const bool disk_be = is_big_endian_on_disk;
		if (disk_be == host_be) return v;

		if constexpr (sizeof(T) == 2) return static_cast<T>(bswap16(static_cast<uint16_t>(v)));
		if constexpr (sizeof(T) == 4) return static_cast<T>(bswap32(static_cast<uint32_t>(v)));
		if constexpr (sizeof(T) == 8) return static_cast<T>(bswap64(static_cast<uint64_t>(v)));
		return v;
	}

	// ----------------------------
	// Original API, faster internals
	// ----------------------------

	// reads a single byte and returns it as int (0–255) or -1 at eof/error
	static int read_byte(FileInputStream& in) {
		int byte = in.get();
		if (byte == std::istream::traits_type::eof()) return -1;
		return static_cast<unsigned char>(byte);
	}

	// reads exactly num_bytes bytes (kept signature)
	static std::vector<uint8_t> read_n_bytes(FileInputStream& in, const size_t num_bytes) {
		std::vector<uint8_t> buf;
		buf.resize(num_bytes);
		read_into(in, buf.data(), num_bytes, "read_n_bytes");
		return buf;
	}

	// New: allocation-free variant (use this in hot paths)
	static void read_n_bytes_into(FileInputStream& in, uint8_t* dst, size_t num_bytes) {
		read_into(in, dst, num_bytes, "read_n_bytes_into");
	}

	template <typename T>
	static T read_exact(FileInputStream& in) {
		static_assert(std::is_trivially_copyable_v<T>,
					  "read_exact<T> requires trivially copyable type");

		T value{};
		read_into(in, &value, sizeof(T), "read_exact");
		return value;
	}

	/**
	 * @brief Reads all remaining bytes from the current position (single allocation where possible).
	 */
	static std::vector<uint8_t> read_all_remaining_bytes(FileInputStream& in) {
		std::istream& s = in.stream();
		const auto start_off = in.offset();

		// Try fast path for seekable streams.
		auto cur = s.tellg();
		if (cur != std::istream::pos_type(-1)) {
			s.seekg(0, std::ios::end);
			auto end = s.tellg();
			if (end != std::istream::pos_type(-1) && end >= cur) {
				const auto remaining = static_cast<size_t>(end - cur);
				s.seekg(cur);

				std::vector<uint8_t> out;
				out.resize(remaining);
				s.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(remaining));
				if (!s || s.gcount() != static_cast<std::streamsize>(remaining)) {
					throw ReadError("Stream error occurred while reading all remaining bytes.",
						in.filename(), start_off, "read_all_remaining_bytes");
				}
				return out;
			}
			// If seekg failed, reset state and fall back.
			s.clear();
			s.seekg(cur);
		}

		// Fallback: streambuf iterator (may do multiple internal reads, but avoids double-copy).
		std::vector<uint8_t> out;
		std::istreambuf_iterator<char> it(s), end;
		for (; it != end; ++it) out.push_back(static_cast<uint8_t>(*it));
		if (s.bad()) {
			throw ReadError("Stream error occurred while reading all remaining bytes.",
				in.filename(), start_off, "read_all_remaining_bytes");
		}
		return out;
	}

	static std::string read_remaining_ascii_until_null(FileInputStream& in) {
		std::istream& s = in.stream();
		const auto start_off = in.offset();

		auto cur = s.tellg();
		if (cur != std::istream::pos_type(-1)) {
			s.seekg(0, std::ios::end);
			auto end = s.tellg();
			if (end != std::istream::pos_type(-1) && end >= cur) {
				const auto remaining = static_cast<size_t>(end - cur);
				s.seekg(cur);

				std::string out;
				out.resize(remaining);
				s.read(out.data(), static_cast<std::streamsize>(remaining));
				if (!s || s.gcount() != static_cast<std::streamsize>(remaining)) {
					throw ReadError("Stream error occurred while reading remaining text.",
						in.filename(), start_off, "read_remaining_ascii_until_null");
				}
				if (const auto null_pos = out.find('\0'); null_pos != std::string::npos) {
					out.resize(null_pos);
				}
				return out;
			}
			s.clear();
			s.seekg(cur);
		}

		std::string out;
		out.reserve(4096);
		char c;
		while (s.get(c)) {
			if (c == '\0') {
				while (s.get(c)) {}
				break;
			}
			out.push_back(c);
		}
		if (s.bad()) {
			throw ReadError("Stream error occurred while reading remaining text.",
				in.filename(), start_off, "read_remaining_ascii_until_null");
		}
		return out;
	}

	// Writes a vector of bytes (kept signature)
	static void write_bytes(FileOutputStream& out, const std::vector<uint8_t>& data) {
		write_from(out, data.data(), data.size(), "write_bytes");
	}

	// writes num_bits of value in be or le (faster: build buffer + one write)
	static void write_unsigned_bits(FileOutputStream& out, uint64_t value, int num_bits, bool is_big_endian) {
		const int num_bytes_to_write = num_bits / 8;
		if (num_bytes_to_write <= 0 || num_bytes_to_write > 8) {
			throw RuntimeError("write_unsigned_bits: invalid num_bits.", "Writing unsigned bits");
		}

		std::array<uint8_t, 8> buf{};
		for (int i = 0; i < num_bytes_to_write; ++i) {
			const int shift = is_big_endian
				? ((num_bytes_to_write - 1 - i) * 8)
				: (i * 8);
			buf[static_cast<size_t>(i)] = static_cast<uint8_t>((value >> shift) & 0xFFu);
		}
		write_from(out, buf.data(), static_cast<size_t>(num_bytes_to_write), "write_unsigned_bits");
	}

	// ----------------------------
	// Fast endian-aware integer reads/writes (no vectors)
	// ----------------------------

	static uint16_t read_unsigned16(FileInputStream& in, bool is_big_endian) {
		uint16_t v = read_exact<uint16_t>(in);
		return maybe_bswap<uint16_t>(v, is_big_endian);
	}
	static int16_t read_signed16(FileInputStream& in, bool is_big_endian) {
		return static_cast<int16_t>(read_unsigned16(in, is_big_endian));
	}
	static void write_unsigned16(FileOutputStream& out, uint16_t v, bool is_big_endian) {
		const uint16_t on_disk = maybe_bswap<uint16_t>(v, is_big_endian);
		write_from(out, &on_disk, sizeof(on_disk), "write_unsigned16");
	}
	static void write_signed16(FileOutputStream& out, int16_t v, bool is_big_endian) {
		write_unsigned16(out, static_cast<uint16_t>(v), is_big_endian);
	}

	static uint32_t read_unsigned24(FileInputStream& in, bool is_big_endian) {
		std::array<uint8_t, 3> b{};
		read_n_bytes_into(in, b.data(), b.size());

		if (is_big_endian) {
			return (static_cast<uint32_t>(b[0]) << 16)
				 | (static_cast<uint32_t>(b[1]) << 8)
				 |  static_cast<uint32_t>(b[2]);
		} else {
			return (static_cast<uint32_t>(b[2]) << 16)
				 | (static_cast<uint32_t>(b[1]) << 8)
				 |  static_cast<uint32_t>(b[0]);
		}
	}
	static void write_unsigned24(FileOutputStream& out, uint32_t v, bool is_big_endian) {
		std::array<uint8_t, 3> b{};
		if (is_big_endian) {
			b[0] = static_cast<uint8_t>((v >> 16) & 0xFFu);
			b[1] = static_cast<uint8_t>((v >> 8)  & 0xFFu);
			b[2] = static_cast<uint8_t>(v & 0xFFu);
		} else {
			b[0] = static_cast<uint8_t>(v & 0xFFu);
			b[1] = static_cast<uint8_t>((v >> 8)  & 0xFFu);
			b[2] = static_cast<uint8_t>((v >> 16) & 0xFFu);
		}
		write_from(out, b.data(), b.size(), "write_unsigned24");
	}

	static uint32_t read_unsigned32(FileInputStream& in, bool is_big_endian) {
		uint32_t v = read_exact<uint32_t>(in);
		return maybe_bswap<uint32_t>(v, is_big_endian);
	}
	static int32_t read_signed32(FileInputStream& in, bool is_big_endian) {
		return static_cast<int32_t>(read_unsigned32(in, is_big_endian));
	}
	static void write_unsigned32(FileOutputStream& out, uint32_t v, bool is_big_endian) {
		const uint32_t on_disk = maybe_bswap<uint32_t>(v, is_big_endian);
		write_from(out, &on_disk, sizeof(on_disk), "write_unsigned32");
	}
	static void write_signed32(FileOutputStream& out, int32_t v, bool is_big_endian) {
		write_unsigned32(out, static_cast<uint32_t>(v), is_big_endian);
	}

	static uint64_t read_unsigned64(FileInputStream& in, bool is_big_endian) {
		uint64_t v = read_exact<uint64_t>(in);
		return maybe_bswap<uint64_t>(v, is_big_endian);
	}
	static void write_unsigned64(FileOutputStream& out, uint64_t v, bool is_big_endian) {
		const uint64_t on_disk = maybe_bswap<uint64_t>(v, is_big_endian);
		write_from(out, &on_disk, sizeof(on_disk), "write_unsigned64");
	}

	// --- block64 ---
	static std::vector<uint8_t> read_block64(FileInputStream& in, bool is_big_endian) {
		const auto offset = in.offset();
		const uint64_t size = read_unsigned64(in, is_big_endian);

		if (size < 8) {
			throw ParseError("Invalid block64 size: " + std::to_string(size) + " (must be >= 8).",
				in.filename(), offset, "Block64 Size Field", "Size >= 8", std::to_string(size), {}, "Validating block64 size");
		}

		const uint64_t payload_size = size - 8;
		const uint64_t max_sane_payload_size = 1024ULL * 1024ULL * 1024ULL;

		if (payload_size > max_sane_payload_size) {
			throw ParseError("Block64 payload size " + std::to_string(payload_size) + " exceeds sanity limit.",
				in.filename(), offset, "Block64 Size Field",
				"Payload size <= " + std::to_string(max_sane_payload_size),
				std::to_string(payload_size), {}, "Validating block64 size");
		}

		std::vector<uint8_t> payload;
		payload.resize(static_cast<size_t>(payload_size));
		read_into(in, payload.data(), static_cast<size_t>(payload_size), "read_block64 payload");
		return payload;
	}

	static void write_block64(FileOutputStream& out, const std::vector<uint8_t>& data, bool is_big_endian) {
		write_unsigned64(out, static_cast<uint64_t>(data.size()) + 8u, is_big_endian);
		write_from(out, data.data(), data.size(), "write_block64 payload");
	}

	// --- 7-bit lsb number ---
	static void write_7bit_number_lsb(FileOutputStream& out, int value) {
		if (value < 0) {
			throw RuntimeError("Cannot write negative value as 7-bit LSB number.", "Writing 7-bit number");
		}
		// Important: value == 0 must still emit one byte.
		uint32_t n = static_cast<uint32_t>(value);
		do {
			uint8_t b = static_cast<uint8_t>(n & 0x7Fu);
			n >>= 7;
			if (n) b |= 0x80u;
			out.put(static_cast<char>(b));
		} while (n);
	}

	static std::pair<int,int> read_7bit_number_le(FileInputStream& in) {
		int result = 0, count = 0;
		while (true) {
			int b = read_byte(in);
			if (b < 0) break;
			result |= (b & 0x7F) << (7 * count);
			++count;
			if (!(b & 0x80)) break;
		}
		return { result, count };
	}

	// --- byte conversion ---
	static uint32_t from_bytes_le(const std::vector<uint8_t>& data) {
		uint32_t x = 0;
		for (size_t i = 0; i < data.size(); ++i)
			x |= static_cast<uint32_t>(data[i]) << (8 * i);
		return x;
	}

	// --- floating point ---
	static float read_float_le(FileInputStream& in) {
		// Reads 4 bytes little-endian on disk, converts if host is BE.
		uint32_t bits = read_unsigned32(in, /*is_big_endian=*/false);
		float f;
		std::memcpy(&f, &bits, sizeof(f));
		return f;
	}

	static float read_float_le_from_bytes(const std::vector<uint8_t>& b) {
		if (b.size() < 4) {
			throw RuntimeError("Too few bytes to read float_le from bytes. Expected at least 4, got " + std::to_string(b.size()) + ".",
				"Converting bytes to float_le");
		}
		uint32_t v = from_bytes_le(b);
		float f;
		std::memcpy(&f, &v, sizeof(f));
		return f;
	}

	static void write_float_le(FileOutputStream& out, float f) {
		uint32_t v;
		std::memcpy(&v, &f, sizeof(v));
		write_unsigned32(out, v, /*is_big_endian=*/false);
	}

	// --- double ---
	static double read_double(FileInputStream& in, bool is_big_endian) {
		uint64_t bits = read_unsigned64(in, is_big_endian);
		double d;
		std::memcpy(&d, &bits, sizeof(d));
		return d;
	}
	static void write_double(FileOutputStream& out, double d, bool is_big_endian) {
		uint64_t bits;
		std::memcpy(&bits, &d, sizeof(bits));
		write_unsigned64(out, bits, is_big_endian);
	}

	// --- utf-8 / ascii ---
	static std::string read_utf8(FileInputStream& in) {
		// Note: This reads until EOF from current position.
		std::string s;
		s.reserve(4096);
		char c;
		while (in.get(c)) s.push_back(c);
		if (s.rfind("\xEF\xBB\xBF", 0) == 0) s.erase(0, 3);
		return s;
	}

	static std::string read_ascii(FileInputStream& in, size_t len) {
		std::string s;
		s.resize(len);
		read_into(in, s.data(), len, "read_ascii");
		return s;
	}

	static std::string read_fixed_ascii(FileInputStream& in, size_t len, bool trim_spaces = true) {
		std::string s = read_ascii(in, len);
		while (!s.empty() && (s.back() == '\0' || (trim_spaces && s.back() == ' '))) {
			s.pop_back();
		}
		return s;
	}

	static void write_ascii(FileOutputStream& out, const std::string& txt, size_t len, bool reverse = false) {
		std::string buf(len, '\0');
		if (!reverse) {
			std::memcpy(buf.data(), txt.data(), std::min(txt.size(), len));
		} else {
			const size_t l = std::min(txt.size(), len);
			for (size_t i = 0; i < l; ++i) buf[l - 1 - i] = txt[i];
		}
		write_from(out, buf.data(), len, "write_ascii");
	}

	static std::string read_with_4_byte_length_ascii(FileInputStream& in) {
		const uint32_t len = read_unsigned32(in, /*is_big_endian=*/false);
		if (len == 0) return {};
		return read_ascii(in, len);
	}

	// --- utf-16 (BMP only, as in your original; still much less overhead) ---
	// static std::string read_utf16_string(FileInputStream& in, bool is_big_endian) {
	// 	const uint32_t count = read_unsigned32(in, is_big_endian);
	// 	if (count == 0) return {};
	//
	// 	std::vector<uint8_t> bytes;
	// 	bytes.resize(static_cast<size_t>(count) * 2u);
	// 	read_into(in, bytes.data(), bytes.size(), "read_utf16_string payload");
	//
	// 	// Decode UTF-16 code units (stop at NUL) to UTF-8 (BMP only).
	// 	std::string out;
	// 	out.reserve(count); // Often ASCII-ish.
	//
	// 	for (size_t i = 0; i + 1 < bytes.size(); i += 2) {
	// 		uint16_t wc = is_big_endian
	// 			? static_cast<uint16_t>((bytes[i] << 8) | bytes[i + 1])
	// 			: static_cast<uint16_t>((bytes[i + 1] << 8) | bytes[i]);
	// 		if (wc == 0) break;
	//
	// 		if (wc <= 0x7Fu) {
	// 			out.push_back(static_cast<char>(wc));
	// 		} else if (wc <= 0x7FFu) {
	// 			out.push_back(static_cast<char>(0xC0 | (wc >> 6)));
	// 			out.push_back(static_cast<char>(0x80 | (wc & 0x3F)));
	// 		} else {
	// 			out.push_back(static_cast<char>(0xE0 | (wc >> 12)));
	// 			out.push_back(static_cast<char>(0x80 | ((wc >> 6) & 0x3F)));
	// 			out.push_back(static_cast<char>(0x80 | (wc & 0x3F)));
	// 		}
	// 	}
	// 	return out;
	// }

	static std::string read_utf16_string(FileInputStream& in, bool be) {
		const uint32_t count = StreamUtils::read_unsigned32(in, be);
		if (count == 0) return {};

		std::string ascii;
		ascii.reserve(count);

		bool ascii_only = true;

		for (uint32_t i = 0; i < count; ++i) {
			uint16_t wc = StreamUtils::read_unsigned16(in, be);
			if (wc == 0) break;

			if (wc <= 0x7F) {
				ascii.push_back(static_cast<char>(wc));
			// } else {
			// 	ascii_only = false;
			// 	// fallback: full conversion
			// 	// rewind stream and call slow path
			// 	in.seekg(-static_cast<std::streamoff>((i + 1) * 2), std::ios::cur);
			// 	return read_utf16_string_slow(in, be, count);
			}
		}

		return ascii;
	}

	static void write_utf16_string(FileOutputStream& out, const std::string& utf8, bool is_big_endian) {
		// Note: This mimics your original "BMP only / naive" behavior (1 byte -> 1 code unit).
		std::u16string u;
		u.reserve(utf8.size());
		for (unsigned char c : utf8) u.push_back(static_cast<char16_t>(c));

		write_unsigned32(out, static_cast<uint32_t>(u.size()), is_big_endian);

		for (char16_t wc : u) {
			uint16_t v = static_cast<uint16_t>(wc);
			v = maybe_bswap<uint16_t>(v, is_big_endian);
			write_from(out, &v, sizeof(v), "write_utf16_string codeunit");
		}
	}

	// --- timestamp ---
	static std::chrono::system_clock::time_point read_timestamp(FileInputStream& in, bool is_big_endian) {
		const uint32_t seconds = read_unsigned32(in, is_big_endian);
		return std::chrono::system_clock::time_point{ std::chrono::seconds{ seconds } };
	}

	static void write_timestamp(FileOutputStream& out,
		const std::chrono::system_clock::time_point& timestamp,
		bool is_big_endian)
	{
		const auto secs64 = std::chrono::duration_cast<std::chrono::seconds>(
			timestamp.time_since_epoch()
		).count();

		if (secs64 < 0 || secs64 > static_cast<long long>(std::numeric_limits<uint32_t>::max())) {
			throw std::runtime_error("write_timestamp: timestamp out of range for uint32 seconds since epoch");
		}

		write_unsigned32(out, static_cast<uint32_t>(secs64), is_big_endian);
	}

} // namespace StreamUtils
