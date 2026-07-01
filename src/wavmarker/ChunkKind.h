//
// Created by Mathias Vatter on 29.05.26.
//

#pragma once

#include <string>
#include <unordered_map>

enum class ChunkKind {
	Raw,
	Format,
	Data,
	Cue,
	List,
	Sampler,
	BroadcastExtension
};

static std::unordered_map<ChunkKind, std::string> chunk_kind_string_map = {
	{ChunkKind::Raw, "raw"},
	{ChunkKind::Format, "format"},
	{ChunkKind::Data, "data"},
	{ChunkKind::Cue, "cue"},
	{ChunkKind::List, "list"},
	{ChunkKind::Sampler, "sampler"},
	{ChunkKind::BroadcastExtension, "broadcast_extension"},
};
inline std::string get_chunk_kind_string(const ChunkKind& chunk_kind) {
	return chunk_kind_string_map.at(chunk_kind);
}

/// a map on how the data chunk ids are actually formatted as strings
static std::unordered_map<ChunkKind, std::string> chunk_kind_id_map = {
	{ChunkKind::Raw, "raw"},
	{ChunkKind::Format, "fmt "},
	{ChunkKind::Data, "data"},
	{ChunkKind::Cue, "cue "},
	{ChunkKind::List, "LIST"},
	{ChunkKind::Sampler, "smpl"},
	{ChunkKind::BroadcastExtension, "bext"},
};
inline std::string get_chunk_kind_id(const ChunkKind& chunk_kind) {
	return chunk_kind_id_map.at(chunk_kind);
}

/// a map that returns the string ids for a provided ChunkKind enum member
static inline std::unordered_map<std::string, ChunkKind> chunk_kind_from_id_map = [] {
	std::unordered_map<std::string, ChunkKind> chunk_kind_from_id_map;
	for (const auto& [chunk_kind, chunk_id] : chunk_kind_id_map) {
		chunk_kind_from_id_map[chunk_id] = chunk_kind;
	}
	return chunk_kind_from_id_map;
}();

inline ChunkKind get_chunk_kind_from_id(const std::string& id) {
	if (auto it = chunk_kind_from_id_map.find(id); it != chunk_kind_from_id_map.end()) {
		return it->second;
	}
	return ChunkKind::Raw;
}
