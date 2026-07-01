#pragma once

#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <ranges>
#include <string>
#include <vector>

#include "../util/FileStream.h"
#include "Container.h"
#include "wavmarker/WavFile.h"
#include "../util/StringUtils.h"

class Parser {
public:
	using ParseFunction = std::function<std::unique_ptr<Container>(FileInputStream&)>;

private:
	static std::map<std::string, ParseFunction> s_parsers;
	static std::once_flag s_parsers_initialized_flag;

	static void initialize_parsers() {
		s_parsers[".wav"] = &Parser::parse_wav;
		s_parsers[".wave"] = &Parser::parse_wav;
	}

public:
	static std::unique_ptr<Container> parse_file(const std::string& file_path) {
		std::call_once(s_parsers_initialized_flag, &Parser::initialize_parsers);

		std::filesystem::path path(file_path);
		FileInputStream in(path.string());
		if (!in.is_open()) {
			throw ReadError("Failed to open input file", in, "Opening file");
		}

		auto ext = StringUtils::to_lower(path.extension().string());
		if (auto it = s_parsers.find(ext); it != s_parsers.end()) {
			return it->second(in);
		}

		throw ParseError("No parser found for file extension: " + ext,
			in,
			"File Extension",
			"Supported extensions: " + StringUtils::join(
				{std::ranges::begin(s_parsers | std::ranges::views::keys), std::ranges::end(s_parsers | std::ranges::views::keys)},
				','
			),
			ext,
			std::vector<uint8_t>(ext.begin(), ext.end()),
			"Parsing file"
		);
	}

	static std::unique_ptr<Container> parse_wav(FileInputStream& in) {
		auto root = std::make_unique<WavFile>();
		root->parse(in);
		return root;
	}
};

inline std::map<std::string, Parser::ParseFunction> Parser::s_parsers;
inline std::once_flag Parser::s_parsers_initialized_flag;
