#pragma once

#include <filesystem>
#include <string>

#include "FileStream.h"
#include "nodes/Container.h"

class Generator {
public:
	static bool write_file(Container& root, const std::string& file_path) {
		FileOutputStream out(file_path);
		root.write(out);
		return true;
	}
};
