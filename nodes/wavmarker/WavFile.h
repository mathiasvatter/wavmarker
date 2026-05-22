#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "nodes/Container.h"
#include "nodes/Reflection.h"

class FileInputStream;
class FileOutputStream;

namespace WavMarker {

struct FormatChunk : Reflectable {
	uint16_t audio_format = 0;
	uint16_t channels = 0;
	uint32_t sample_rate = 0;
	uint32_t byte_rate = 0;
	uint16_t block_align = 0;
	uint16_t bits_per_sample = 0;
	std::vector<uint8_t> extra_bytes;

	void parse(FileInputStream& in);
	void write(FileOutputStream& out) const;
	DECLARE_REFLECTABLE()
};

struct CuePoint : Reflectable {
	uint32_t id = 0;
	uint32_t position = 0;
	std::string data_chunk_id = "data";
	uint32_t chunk_start = 0;
	uint32_t block_start = 0;
	uint32_t sample_offset = 0;

	void parse(FileInputStream& in);
	void write(FileOutputStream& out) const;
	DECLARE_REFLECTABLE()
};

struct Label : Reflectable {
	uint32_t cue_id = 0;
	std::string text;

	void parse(FileInputStream& in);
	void write(FileOutputStream& out) const;
	DECLARE_REFLECTABLE()
};

struct ListSubChunk : Reflectable {
	std::string id;
	std::vector<uint8_t> payload;
	std::optional<Label> label;

	void parse(FileInputStream& in, const std::string& list_type);
	void write(FileOutputStream& out) const;
	DECLARE_REFLECTABLE()
};

struct ListChunk : Reflectable {
	std::string type;
	std::vector<ListSubChunk> subchunks;
	std::vector<uint8_t> trailing_data;

	void parse(FileInputStream& in);
	void write(FileOutputStream& out) const;
	DECLARE_REFLECTABLE()
};

struct SampleLoop : Reflectable {
	uint32_t cue_point_id = 0;
	uint32_t type = 0;
	uint32_t start = 0;
	uint32_t end = 0;
	uint32_t fraction = 0;
	uint32_t play_count = 0;

	void parse(FileInputStream& in);
	void write(FileOutputStream& out) const;
	DECLARE_REFLECTABLE()
};

struct SamplerChunk : Reflectable {
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
	DECLARE_REFLECTABLE()
};

enum class ChunkKind {
	Raw,
	Format,
	Data,
	Cue,
	List,
	Sampler
};

struct Chunk : Reflectable {
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
	DECLARE_REFLECTABLE()
};

DEFINE_REFLECTABLE_MEMBERS(FormatChunk, audio_format, channels, sample_rate, byte_rate, block_align, bits_per_sample, extra_bytes)
DEFINE_REFLECTABLE_MEMBERS(CuePoint, id, position, data_chunk_id, chunk_start, block_start, sample_offset)
DEFINE_REFLECTABLE_MEMBERS(Label, cue_id, text)
DEFINE_REFLECTABLE_MEMBERS(ListSubChunk, id, payload, label)
DEFINE_REFLECTABLE_MEMBERS(ListChunk, type, subchunks, trailing_data)
DEFINE_REFLECTABLE_MEMBERS(SampleLoop, cue_point_id, type, start, end, fraction, play_count)
DEFINE_REFLECTABLE_MEMBERS(SamplerChunk, manufacturer, product, sample_period, midi_unity_note, midi_pitch_fraction, smpte_format, smpte_offset, sampler_data, loops, trailing_data)
DEFINE_REFLECTABLE_MEMBERS(Chunk, id, kind, raw_payload, format, audio_data, cue_points, list, sampler)

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
	DECLARE_REFLECTABLE()
};

DEFINE_REFLECTABLE_MEMBERS(WavFile, m_riff_id, m_wave_id, m_chunks)
