//
// Created by Mathias Vatter on 29.05.26.
//

#pragma once

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