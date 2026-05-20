#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "nodes/Container.h"

class FileInputStream;
class FileOutputStream;

namespace WavMarker {

struct FormatChunk {
	uint16_t audio_format = 0;
	uint16_t channels = 0;
	uint32_t sample_rate = 0;
	uint32_t byte_rate = 0;
	uint16_t block_align = 0;
	uint16_t bits_per_sample = 0;
	std::vector<uint8_t> extra_bytes;

	void parse(FileInputStream& in);
	void write(FileOutputStream& out) const;
};

struct CuePoint {
	uint32_t id = 0;
	uint32_t position = 0;
	std::string data_chunk_id = "data";
	uint32_t chunk_start = 0;
	uint32_t block_start = 0;
	uint32_t sample_offset = 0;

	void parse(FileInputStream& in);
	void write(FileOutputStream& out) const;
};

struct Label {
	uint32_t cue_id = 0;
	std::string text;

	void parse(FileInputStream& in);
	void write(FileOutputStream& out) const;
};

struct ListSubChunk {
	std::string id;
	std::vector<uint8_t> payload;
	std::optional<Label> label;

	void parse(FileInputStream& in, const std::string& list_type);
	void write(FileOutputStream& out) const;
};

struct ListChunk {
	std::string type;
	std::vector<ListSubChunk> subchunks;
	std::vector<uint8_t> trailing_data;

	void parse(FileInputStream& in);
	void write(FileOutputStream& out) const;
};

struct SampleLoop {
	uint32_t cue_point_id = 0;
	uint32_t type = 0;
	uint32_t start = 0;
	uint32_t end = 0;
	uint32_t fraction = 0;
	uint32_t play_count = 0;

	void parse(FileInputStream& in);
	void write(FileOutputStream& out) const;
};

struct SamplerChunk {
	uint32_t manufacturer = 0;
	uint32_t product = 0;
	uint32_t sample_period = 0;
	uint32_t midi_unity_note = 0;
	uint32_t midi_pitch_fraction = 0;
	uint32_t smpte_format = 0;
	uint32_t smpte_offset = 0;
	uint32_t sampler_data = 0;
	std::vector<SampleLoop> loops;
	std::vector<uint8_t> trailing_data;

	void parse(FileInputStream& in);
	void write(FileOutputStream& out) const;
};

enum class ChunkKind {
	Raw,
	Format,
	Data,
	Cue,
	List,
	Sampler
};

struct Chunk {
	std::string id;
	ChunkKind kind = ChunkKind::Raw;
	std::vector<uint8_t> raw_payload;
	FormatChunk format;
	std::vector<uint8_t> audio_data;
	std::vector<CuePoint> cue_points;
	ListChunk list;
	SamplerChunk sampler;

	void parse(FileInputStream& in);
	void write(FileOutputStream& out) const;
};

} // namespace WavMarker

class WavFile final : public Container {
	std::string m_riff_id = "RIFF";
	std::string m_wave_id = "WAVE";
	std::vector<WavMarker::Chunk> m_chunks;

public:
	void parse(FileInputStream& in) override;
	void write(FileOutputStream& out) override;

	[[nodiscard]] const std::string& riff_id() const { return m_riff_id; }
	[[nodiscard]] const std::string& wave_id() const { return m_wave_id; }
	[[nodiscard]] const std::vector<WavMarker::Chunk>& chunks() const { return m_chunks; }
	[[nodiscard]] std::vector<WavMarker::Chunk>& chunks() { return m_chunks; }

	[[nodiscard]] const WavMarker::FormatChunk* format() const;
	[[nodiscard]] std::vector<WavMarker::CuePoint> cue_points() const;
	[[nodiscard]] std::vector<WavMarker::Label> labels() const;
	[[nodiscard]] std::vector<WavMarker::SampleLoop> sample_loops() const;
	[[nodiscard]] const std::vector<uint8_t>* audio_data() const;
};
