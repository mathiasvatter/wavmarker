#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "nodes/Container.h"
#include "nodes/Reflection.h"
#include "ChunkKind.h"

class FileInputStream;
class FileOutputStream;

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
DEFINE_REFLECTABLE_MEMBERS(FormatChunk, audio_format, channels, sample_rate, byte_rate, block_align, bits_per_sample, extra_bytes)

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
DEFINE_REFLECTABLE_MEMBERS(CuePoint, id, position, data_chunk_id, chunk_start, block_start, sample_offset)

struct Label : Reflectable {
	uint32_t cue_id = 0;
	std::string text;

	void parse(FileInputStream& in);
	void write(FileOutputStream& out) const;
	DECLARE_REFLECTABLE()
};
DEFINE_REFLECTABLE_MEMBERS(Label, cue_id, text)

struct ListSubChunk : Reflectable {
	std::string id;
	std::vector<uint8_t> payload;
	std::optional<Label> label;

	void parse(FileInputStream& in, const std::string& list_type);
	void write(FileOutputStream& out) const;
	[[nodiscard]] std::unique_ptr<JSONValue> to_json() const override;
	DECLARE_REFLECTABLE()
};
DEFINE_REFLECTABLE_MEMBERS(ListSubChunk, id, payload, label)

struct ListChunk : Reflectable {
	std::string type;
	std::vector<ListSubChunk> subchunks;
	std::vector<uint8_t> trailing_data;

	void parse(FileInputStream& in);
	void write(FileOutputStream& out) const;
	DECLARE_REFLECTABLE()
};
DEFINE_REFLECTABLE_MEMBERS(ListChunk, type, subchunks, trailing_data)

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
DEFINE_REFLECTABLE_MEMBERS(SampleLoop, cue_point_id, type, start, end, fraction, play_count)

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
DEFINE_REFLECTABLE_MEMBERS(SamplerChunk, manufacturer, product, sample_period, midi_unity_note, midi_pitch_fraction, smpte_format, smpte_offset, sampler_data, loops, trailing_data)

struct BextChunk : Reflectable {
	std::string description;
	std::string originator;
	std::string originator_reference;
	std::string origination_date;
	std::string origination_time;
	uint64_t time_reference = 0;
	uint16_t version = 0;
	std::vector<uint8_t> umid;
	uint16_t loudness_value = 0;
	uint16_t loudness_range = 0;
	uint16_t max_true_peak_level = 0;
	uint16_t max_momentary_loudness = 0;
	uint16_t max_short_term_loudness = 0;
	std::vector<uint8_t> reserved;
	std::string coding_history;

	void parse(FileInputStream& in);
	void write(FileOutputStream& out) const;
	[[nodiscard]] std::unique_ptr<JSONValue> to_json() const override;
	DECLARE_REFLECTABLE()
};
DEFINE_REFLECTABLE_MEMBERS(BextChunk, description, originator, originator_reference, origination_date, origination_time, time_reference, version, umid, loudness_value, loudness_range, max_true_peak_level, max_momentary_loudness, max_short_term_loudness, reserved, coding_history)

struct Chunk : Reflectable {
	std::string id;
	ChunkKind kind = ChunkKind::Raw;
	std::vector<uint8_t> raw_payload;
	FormatChunk format;
	std::vector<uint8_t> audio_data;
	std::vector<CuePoint> cue_points;
	ListChunk list;
	SamplerChunk sampler;
	BextChunk bext;

	void parse(FileInputStream& in);
	void write(FileOutputStream& out) const;
	[[nodiscard]] std::unique_ptr<JSONValue> to_json() const override;
	DECLARE_REFLECTABLE()
};
DEFINE_REFLECTABLE_MEMBERS(Chunk, id, kind, raw_payload, format, audio_data, cue_points, list, sampler, bext)


class WavFile final : public Container {
	std::string m_riff_id = "RIFF";
	std::string m_wave_id = "WAVE";
	std::vector<Chunk> m_chunks;

public:
	void parse(FileInputStream& in) override;
	void write(FileOutputStream& out) override;
	[[nodiscard]] std::unique_ptr<JSONValue> to_json() const override;

	[[nodiscard]] const std::string& riff_id() const { return m_riff_id; }
	[[nodiscard]] const std::string& wave_id() const { return m_wave_id; }
	[[nodiscard]] const std::vector<Chunk>& chunks() const { return m_chunks; }

	[[nodiscard]] const FormatChunk* format() const;
	[[nodiscard]] std::vector<CuePoint> cue_points() const;
	[[nodiscard]] std::vector<Label> labels() const;
	[[nodiscard]] std::vector<SampleLoop> sample_loops() const;
	[[nodiscard]] const std::vector<uint8_t>* audio_data() const;

	[[nodiscard]] const Chunk& chunk(const ChunkKind &kind) const {
		for (const auto& chunk : m_chunks) {
			if (chunk.kind == kind) {
				return chunk;
			}
		}
		throw ParseError(
			"Requested chunk kind not found in WAV file: " + get_chunk_kind_string(kind),
			"get_chunk"
		);
	}

	DECLARE_REFLECTABLE()
};

DEFINE_REFLECTABLE_MEMBERS(WavFile, m_riff_id, m_wave_id, m_chunks)
