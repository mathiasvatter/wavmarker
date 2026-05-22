#pragma once

#include <filesystem>
#include <string>

#include "FileStream.h"
#include "nodes/Container.h"
#include "nodes/Reflection.h"

class Generator {
public:
	static bool write_file(Container& root, const std::string& file_path) {
		FileOutputStream out(file_path);
		root.write(out);
		return true;
	}

	static bool dump_json(const Reflectable& root, const std::string& file_path) {
		FileOutputStream out(file_path, std::ios::out | std::ios::trunc);
		const auto json = root.to_json_string();
		out.write(json.data(), static_cast<std::streamsize>(json.size()));
		out.put('\n');
		if (out.fail()) {
			throw WriteError("Failed to write JSON file", out, "Writing JSON");
		}
		return true;
	}
};
