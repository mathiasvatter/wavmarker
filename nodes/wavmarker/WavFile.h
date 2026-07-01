#pragma once

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "nodes/Container.h"
#include "nodes/Reflection.h"
#include "ChunkKind.h"

class FileInputStream;
class FileOutputStream;
struct CueChunk;

/// Maps cue point ids from a source WAV to their ids in a destination WAV.
using CueIdMap = std::unordered_map<uint32_t, uint32_t>;

struct Chunk : Reflectable {
	std::string id;
	ChunkKind kind = ChunkKind::Raw;

	explicit Chunk(ChunkKind kind = ChunkKind::Raw, std::string id = {});
	virtual ~Chunk() = default;

	virtual void parse(FileInputStream& in) = 0;
	virtual void write(FileOutputStream& out) const = 0;
	[[nodiscard]] virtual size_t payload_size() const = 0;
	[[nodiscard]] std::unique_ptr<JSONValue> to_json() const override;
	DECLARE_REFLECTABLE()

protected:
	void write_chunk(FileOutputStream& out, const std::vector<uint8_t>& payload) const;
};
DEFINE_REFLECTABLE_MEMBERS(Chunk, id, kind)

struct RawChunk final : Chunk {
	std::vector<uint8_t> raw_payload;

	RawChunk();
	explicit RawChunk(std::string id);

	void parse(FileInputStream& in) override;
	void write(FileOutputStream& out) const override;
	[[nodiscard]] size_t payload_size() const override { return raw_payload.size(); }
	[[nodiscard]] std::unique_ptr<JSONValue> to_json() const override;
	DECLARE_REFLECTABLE()
};
DEFINE_REFLECTABLE_MEMBERS(RawChunk, raw_payload)

struct FormatChunk final : Chunk {
	uint16_t audio_format = 0;
	uint16_t channels = 0;
	uint32_t sample_rate = 0;
	uint32_t byte_rate = 0;
	uint16_t block_align = 0;
	uint16_t bits_per_sample = 0;
	std::vector<uint8_t> extra_bytes;

	FormatChunk();

	void parse(FileInputStream& in) override;
	void write(FileOutputStream& out) const override;
	[[nodiscard]] size_t payload_size() const override { return 16 + extra_bytes.size(); }
	[[nodiscard]] std::unique_ptr<JSONValue> to_json() const override;
	DECLARE_REFLECTABLE()
};
DEFINE_REFLECTABLE_MEMBERS(FormatChunk, audio_format, channels, sample_rate, byte_rate, block_align, bits_per_sample, extra_bytes)

struct DataChunk final : Chunk {
	std::vector<uint8_t> audio_data;

	DataChunk();

	void parse(FileInputStream& in) override;
	void write(FileOutputStream& out) const override;
	[[nodiscard]] size_t payload_size() const override { return audio_data.size(); }
	[[nodiscard]] std::unique_ptr<JSONValue> to_json() const override;
	DECLARE_REFLECTABLE()
};
DEFINE_REFLECTABLE_MEMBERS(DataChunk, audio_data)

struct CuePoint : Reflectable {
	uint32_t id = 0;
	uint32_t position = 0;
	std::string data_chunk_id = "data";
	uint32_t chunk_start = 0;
	uint32_t block_start = 0;
	uint32_t sample_offset = 0;

	void parse(FileInputStream& in);
	void write(FileOutputStream& out) const;
	/// Returns a copy of this cue point with a different id.
	[[nodiscard]] CuePoint with_id(uint32_t new_id) const;
	DECLARE_REFLECTABLE()
};
DEFINE_REFLECTABLE_MEMBERS(CuePoint, id, position, data_chunk_id, chunk_start, block_start, sample_offset)

struct Label : Reflectable {
	uint32_t cue_id = 0;
	std::string text;

	void parse(FileInputStream& in);
	void write(FileOutputStream& out) const;
	/// Returns a copy of this label referencing a different cue point.
	[[nodiscard]] Label with_cue_id(uint32_t new_cue_id) const;
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

struct ListChunk final : Chunk {
	std::string type;
	std::vector<ListSubChunk> subchunks;
	std::vector<uint8_t> trailing_data;

	ListChunk();

	/// Removes label subchunks that reference one of the supplied cue point ids.
	void remove_labels_by_cue_id(const std::unordered_set<uint32_t>& ids) {
		std::erase_if(subchunks, [&ids](const ListSubChunk& subchunk) {
			return subchunk.label && ids.contains(subchunk.label->cue_id);
		});
	}
	/// Returns labels for the supplied cue ids. Non-adtl LIST chunks return no labels.
	[[nodiscard]] std::vector<Label> labels_for_cue_ids(const std::unordered_set<uint32_t>& ids) const;
	/// Appends labels after replacing their source cue ids according to cue_id_map.
	void append_remapped_labels(const std::vector<Label>& labels, const CueIdMap& cue_id_map);

	void parse(FileInputStream& in) override;
	void write(FileOutputStream& out) const override;
	[[nodiscard]] size_t payload_size() const override;
	[[nodiscard]] std::unique_ptr<JSONValue> to_json() const override;
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
	/// Replaces cue_point_id with its destination id. Throws if no mapping exists.
	void remap_cue_id(const CueIdMap& cue_id_map);
	DECLARE_REFLECTABLE()
};
DEFINE_REFLECTABLE_MEMBERS(SampleLoop, cue_point_id, type, start, end, fraction, play_count)

struct SamplerChunk final : Chunk {
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

	SamplerChunk();

	/// Returns the distinct cue point ids referenced by the sample loops.
	std::unordered_set<uint32_t> cue_ids_for_loops() const {
		std::unordered_set<uint32_t> ids;
		for (const auto& loop : loops) {
			ids.insert(loop.cue_point_id);
		}
		return ids;
	}
	/// Returns distinct loop cue ids while preserving their first occurrence order.
	[[nodiscard]] std::vector<uint32_t> loop_cue_ids_in_order() const;
	/// Verifies that every loop references a cue point contained in cue_chunk.
	void validate_loop_cues(const CueChunk& cue_chunk) const;
	/// Copies all sampler data from source and remaps every loop cue reference.
	void copy_from(const SamplerChunk& source, const CueIdMap& cue_id_map);

	void parse(FileInputStream& in) override;
	void write(FileOutputStream& out) const override;
	[[nodiscard]] size_t payload_size() const override { return 36 + loops.size() * 24 + trailing_data.size(); }
	[[nodiscard]] std::unique_ptr<JSONValue> to_json() const override;
	DECLARE_REFLECTABLE()
};
DEFINE_REFLECTABLE_MEMBERS(SamplerChunk, manufacturer, product, sample_period, midi_unity_note, midi_pitch_fraction, smpte_format, smpte_offset, sampler_data, loops, trailing_data)

struct BextChunk final : Chunk {
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

	BextChunk();

	void parse(FileInputStream& in) override;
	void write(FileOutputStream& out) const override;
	[[nodiscard]] size_t payload_size() const override { return 602 + coding_history.size(); }
	[[nodiscard]] std::unique_ptr<JSONValue> to_json() const override;
	DECLARE_REFLECTABLE()
};
DEFINE_REFLECTABLE_MEMBERS(BextChunk, description, originator, originator_reference, origination_date, origination_time, time_reference, version, umid, loudness_value, loudness_range, max_true_peak_level, max_momentary_loudness, max_short_term_loudness, reserved, coding_history)

struct CueChunk final : Chunk {
	std::vector<CuePoint> cue_points;
	std::map<uint32_t, CuePoint*> cue_point_map;

	CueChunk();

	/// Rebuilds the non-owning id lookup after cue_points has changed.
	void rebuild_cue_point_map() {
		cue_point_map.clear();
		for (auto& cue_point : cue_points) {
			cue_point_map[cue_point.id] = &cue_point;
		}
	}

	/// Removes cue points with a matching id and refreshes the lookup map.
	void remove_cues_by_id(const std::unordered_set<uint32_t>& ids) {
		std::erase_if(cue_points, [&ids](const CuePoint& cue_point) {
			return ids.contains(cue_point.id);
		});
		rebuild_cue_point_map();
	}
	/**
	 * Assigns consecutive, currently unused destination ids to source_ids.
	 *
	 * @throws RuntimeError if the uint32 id space is exhausted.
	 */
	[[nodiscard]] CueIdMap create_cue_id_map(const std::vector<uint32_t>& source_ids) const;
	/// Copies the selected cue points from source, applies cue_id_map, and refreshes the lookup map.
	void append_remapped_cues(const CueChunk& source, const std::vector<uint32_t>& source_ids,
		const CueIdMap& cue_id_map);

	void parse(FileInputStream& in) override;
	void write(FileOutputStream& out) const override;
	[[nodiscard]] size_t payload_size() const override { return 4 + cue_points.size() * 24; }
	[[nodiscard]] std::unique_ptr<JSONValue> to_json() const override;
	DECLARE_REFLECTABLE()
};
DEFINE_REFLECTABLE_MEMBERS(CueChunk, cue_points)


class WavFile final : public Container {
	std::string m_riff_id = "RIFF";
	std::string m_wave_id = "WAVE";
	std::vector<std::unique_ptr<Chunk>> m_chunks;

public:
	void parse(FileInputStream& in) override;
	void write(FileOutputStream& out) override;
	[[nodiscard]] std::unique_ptr<JSONValue> to_json() const override;

	[[nodiscard]] const std::string& riff_id() const { return m_riff_id; }
	[[nodiscard]] const std::string& wave_id() const { return m_wave_id; }
	[[nodiscard]] const std::vector<std::unique_ptr<Chunk>>& chunks() const { return m_chunks; }

	[[nodiscard]] const FormatChunk* format() const;
	[[nodiscard]] std::vector<CuePoint> cue_points() const;
	[[nodiscard]] std::vector<Label> labels() const;
	[[nodiscard]] std::vector<SampleLoop> sample_loops() const;
	[[nodiscard]] const std::vector<uint8_t>* audio_data() const;
	/**
	 * Replaces this WAV's sample loops and their cue points with those from source.
	 * Existing cues and labels belonging to the replaced loops are removed; unrelated
	 * metadata remains intact. Source cue ids are remapped to unused destination ids.
	 *
	 * @param source WAV providing the sampler, cue, and optional label data.
	 * @param include_labels Whether labels belonging to the source loops are copied.
	 * @throws RuntimeError if source has no loops, lacks a cue chunk, references a
	 *         missing cue point, or the destination cue id space is exhausted.
	 */
	void copy_sample_loops_from(const WavFile& source, bool include_labels = true);

	Chunk* find_chunk(const ChunkKind kind) noexcept {
		const auto it = std::ranges::find_if(m_chunks, [kind](const std::unique_ptr<Chunk>& chunk) {
			return chunk->kind == kind;
		});
		return it == m_chunks.end() ? nullptr : it->get();
	}

	[[nodiscard]] const Chunk* find_chunk(const ChunkKind kind) const noexcept {
		const auto it = std::ranges::find_if(m_chunks, [kind](const std::unique_ptr<Chunk>& chunk) {
			return chunk->kind == kind;
		});
		return it == m_chunks.end() ? nullptr : it->get();
	}

	template<typename T>
	T* find_chunk_as(const ChunkKind kind) noexcept {
		return dynamic_cast<T*>(find_chunk(kind));
	}

	template<typename T>
	[[nodiscard]] const T* find_chunk_as(const ChunkKind kind) const noexcept {
		return dynamic_cast<const T*>(find_chunk(kind));
	}

	/// Returns the requested chunk, creating and appending it when absent.
	Chunk& ensure_chunk(const ChunkKind kind) noexcept {
		if (auto* chunk = find_chunk(kind)) {
			return *chunk;
		}

		m_chunks.push_back(create_empty_chunk(kind));
		return *m_chunks.back();
	}

	template<typename T>
	T& ensure_chunk_as(const ChunkKind kind) noexcept {
		return static_cast<T&>(ensure_chunk(kind));
	}

	DECLARE_REFLECTABLE()

private:
	/// Collects matching labels from all adtl LIST chunks.
	[[nodiscard]] std::vector<Label> labels_for_cue_ids(const std::unordered_set<uint32_t>& ids) const;
	/// Removes cue points and adtl labels associated with the supplied ids.
	void remove_cues_and_labels(const std::unordered_set<uint32_t>& cue_ids);
	/// Returns the existing adtl LIST chunk or appends a new one.
	ListChunk& ensure_adtl_chunk();
	static std::unique_ptr<Chunk> create_empty_chunk(ChunkKind kind);
	static std::unique_ptr<Chunk> parse_chunk(FileInputStream& in);
};

DEFINE_REFLECTABLE_MEMBERS(WavFile, m_riff_id, m_wave_id, m_chunks)
