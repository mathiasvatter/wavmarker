//
// Created by Mathias Vatter on 29.05.26.
//

#pragma once

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
	std::vector<FieldPatch*> patches;

public:
	explicit Patcher(WavFile& file) : file(file) {}

	[[nodiscard]] bool apply_patch(const FieldPatch& patch) const {
		auto chunk = file.chunk(patch.kind);
		return chunk.set_member_value(patch.field_name, patch.value.get());
	}

};
