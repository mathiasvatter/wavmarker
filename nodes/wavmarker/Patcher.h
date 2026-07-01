//
// Created by Mathias Vatter on 29.05.26.
//

#pragma once

#include <limits>
#include <string_view>
#include <typeindex>

#include "WavFile.h"

struct FieldPatch {
	ChunkKind kind; // chunk id to patch
	std::string field_name; // keyname of the field to patch
	std::unique_ptr<JSONValue> value; // new value to set
};

/**
 * A class that patches one or multiple fields in the serialized WavFile class reflectables
 */
class Patcher {
	WavFile& file;

	/// Resolves supported short and qualified BEXT field paths.
	[[nodiscard]] static std::string resolve_bext_field(const std::string_view path) {
		constexpr std::string_view bext_prefix = "bext.";
		constexpr std::string_view broadcast_extension_prefix = "broadcast_extension.";

		std::string field_name;
		if (path.starts_with(bext_prefix)) {
			field_name = path.substr(bext_prefix.size());
		} else if (path.starts_with(broadcast_extension_prefix)) {
			field_name = path.substr(broadcast_extension_prefix.size());
		} else if (path.find('.') == std::string_view::npos) {
			field_name = path;
		} else {
			throw RuntimeError("Unsupported field path: " + std::string(path), "Resolving patch field");
		}

		if (!BextChunk::get_descriptors().contains(field_name)) {
			throw RuntimeError("Unknown BEXT field: " + field_name, "Resolving patch field");
		}
		return field_name;
	}

	/// Rejects values that Reflection cannot assign safely to the selected BEXT field.
	static void validate_bext_value(const std::string& field_name, const JSONValue* value) {
		const std::type_index field_type = BextChunk::get_descriptors().at(field_name).type;
		if (field_type == typeid(std::string)) {
			const auto* string = dynamic_cast<const JSONString*>(value);
			if (!string) {
				throw RuntimeError("BEXT field expects a string: " + field_name, "Validating patch value");
			}
			size_t maximum_size = std::numeric_limits<size_t>::max();
			if (field_name == "description") maximum_size = 256;
			else if (field_name == "originator" || field_name == "originator_reference") maximum_size = 32;
			else if (field_name == "origination_date") maximum_size = 10;
			else if (field_name == "origination_time") maximum_size = 8;
			if (string->value.size() > maximum_size) {
				throw RuntimeError("String exceeds the fixed size of BEXT field: " + field_name,
					"Validating patch value");
			}
			return;
		}

		if (field_type == typeid(std::vector<uint8_t>)) {
			const auto* array = dynamic_cast<const JSONArray*>(value);
			if (!array) {
				throw RuntimeError("BEXT field expects a JSON byte array: " + field_name, "Validating patch value");
			}
			for (const auto& element : array->elements) {
				const auto* integer = dynamic_cast<const JSONInt*>(element.get());
				if (!integer || integer->value < 0 || integer->value > 255) {
					throw RuntimeError("BEXT byte array contains a value outside 0..255: " + field_name,
						"Validating patch value");
				}
			}
			const size_t maximum_size = field_name == "umid" ? 64 : 180;
			if (array->elements.size() > maximum_size) {
				throw RuntimeError("Byte array exceeds the fixed size of BEXT field: " + field_name,
					"Validating patch value");
			}
			return;
		}

		long long integer_value;
		if (const auto* integer = dynamic_cast<const JSONInt*>(value)) {
			integer_value = integer->value;
		} else if (const auto* string = dynamic_cast<const JSONString*>(value)) {
			try {
				integer_value = std::stoll(string->value);
			} catch (const std::exception&) {
				throw RuntimeError("BEXT field expects an integer: " + field_name, "Validating patch value");
			}
		} else {
			throw RuntimeError("BEXT field expects an integer: " + field_name, "Validating patch value");
		}

		if (integer_value < 0 ||
			(field_type == typeid(uint16_t) && integer_value > std::numeric_limits<uint16_t>::max()) ||
			(field_type == typeid(uint32_t) && integer_value > std::numeric_limits<uint32_t>::max())) {
			throw RuntimeError("Integer is outside the range of BEXT field: " + field_name,
				"Validating patch value");
		}
	}

public:
	explicit Patcher(WavFile& file) : file(file) {}

	/// Creates a patch for a BEXT field path such as "bext.originator" or "originator".
	[[nodiscard]] static FieldPatch create_bext_patch(const std::string_view path,
		std::unique_ptr<JSONValue> value) {
		const std::string field_name = resolve_bext_field(path);
		validate_bext_value(field_name, value.get());
		return {ChunkKind::BroadcastExtension, field_name, std::move(value)};
	}

	/// Returns the reflected C++ type of a supported BEXT field.
	[[nodiscard]] static std::type_index bext_field_type(const std::string_view path) {
		return BextChunk::get_descriptors().at(resolve_bext_field(path)).type;
	}

	/// Applies a patch, creating the target chunk when it does not exist yet.
	[[nodiscard]] bool apply_patch(const FieldPatch& patch) const {
		auto& chunk = file.ensure_chunk(patch.kind);
		return chunk.set_member_value(patch.field_name, patch.value.get());
	}

	/// Reads a BEXT field without creating a missing BEXT chunk.
	[[nodiscard]] std::unique_ptr<JSONValue> get_bext_value(const std::string_view path) const {
		const std::string field_name = resolve_bext_field(path);
		const auto* chunk = file.find_chunk(ChunkKind::BroadcastExtension);
		if (!chunk) {
			throw RuntimeError("WAV file does not contain a BEXT chunk.", "Reading BEXT field");
		}

		auto value = chunk->get_member_value(field_name);
		if (!value) {
			throw RuntimeError("Could not read BEXT field: " + field_name, "Reading BEXT field");
		}
		return value;
	}
};
