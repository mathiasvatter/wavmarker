#include "WavFile.h"

#include "../../util/FileStream.h"
#include "util/StreamUtils.h"

constexpr bool kLittleEndian = false;

void check_for_numeric_limits(const ChunkKind kind, size_t size, FileOutputStream& out, const std::string& id) {
	if (size > std::numeric_limits<uint32_t>::max()) {
		throw WriteError(get_chunk_kind_string(kind) + "chunk payload exceeds uint32 size field.", out, id);
	}
}

std::unique_ptr<JSONValue> byte_preview_to_json(const std::vector<uint8_t>& bytes, const size_t max_size = 32) {
	auto preview = std::make_unique<JSONArray>();
	for (size_t index = 0; index < std::min(bytes.size(), max_size); ++index) {
		preview->add(std::make_unique<JSONInt>(bytes[index]));
	}
	return preview;
}

std::unique_ptr<JSONObject> chunk_json_base(const Chunk& chunk) {
	auto json = std::make_unique<JSONObject>();
	json->add("id", std::make_unique<JSONString>(chunk.id));
	json->add("kind", std::make_unique<JSONString>(get_chunk_kind_string(chunk.kind)));
	return json;
}

Chunk::Chunk(const ChunkKind kind, std::string id)
	: id(id.empty() ? get_chunk_kind_id(kind) : std::move(id)), kind(kind) {}

void Chunk::write_chunk(FileOutputStream& out, const std::vector<uint8_t>& payload) const {
	StreamUtils::write_ascii(out, id, 4);

	check_for_numeric_limits(kind, payload.size(), out, id);

	StreamUtils::write_unsigned32(out, static_cast<uint32_t>(payload.size()), kLittleEndian);
	StreamUtils::write_bytes(out, payload);
	if ((payload.size() & 1u) != 0u) {
		out.put('\0');
	}
}

std::unique_ptr<JSONValue> Chunk::to_json() const {
	auto json = chunk_json_base(*this);
	json->add("payload_size", std::make_unique<JSONInt>(static_cast<long long>(payload_size())));
	return json;
}

RawChunk::RawChunk() : Chunk(ChunkKind::Raw) {}

RawChunk::RawChunk(std::string id) : Chunk(ChunkKind::Raw, std::move(id)) {}

void RawChunk::parse(FileInputStream& in) {
	raw_payload = StreamUtils::read_all_remaining_bytes(in);
}

void RawChunk::write(FileOutputStream& out) const {
	write_chunk(out, raw_payload);
}

std::unique_ptr<JSONValue> RawChunk::to_json() const {
	auto json = chunk_json_base(*this);
	json->add("payload_size", std::make_unique<JSONInt>(static_cast<long long>(raw_payload.size())));
	if (id == "JUNK" || id == "junk") {
		json->add("kind", std::make_unique<JSONString>("junk"));
		json->add("purpose", std::make_unique<JSONString>("padding"));
		// json->add("byte_preview", byte_preview_to_json(raw_payload));
	} else {
		json->add("raw_payload", convert_to_json(raw_payload));
	}
	return json;
}

FormatChunk::FormatChunk() : Chunk(ChunkKind::Format) {}

void FormatChunk::parse(FileInputStream& in) {
	if (in.remaining_bytes() < 16) {
		throw ParseError("fmt chunk is too short.", in, "fmt ", "At least 16 bytes");
	}

	audio_format = StreamUtils::read_unsigned16(in, kLittleEndian);
	channels = StreamUtils::read_unsigned16(in, kLittleEndian);
	sample_rate = StreamUtils::read_unsigned32(in, kLittleEndian);
	byte_rate = StreamUtils::read_unsigned32(in, kLittleEndian);
	block_align = StreamUtils::read_unsigned16(in, kLittleEndian);
	bits_per_sample = StreamUtils::read_unsigned16(in, kLittleEndian);
	extra_bytes = StreamUtils::read_all_remaining_bytes(in);
}

void FormatChunk::write(FileOutputStream& out) const {
	FileOutputSubStream payload(out);
	StreamUtils::write_unsigned16(payload, audio_format, kLittleEndian);
	StreamUtils::write_unsigned16(payload, channels, kLittleEndian);
	StreamUtils::write_unsigned32(payload, sample_rate, kLittleEndian);
	StreamUtils::write_unsigned32(payload, byte_rate, kLittleEndian);
	StreamUtils::write_unsigned16(payload, block_align, kLittleEndian);
	StreamUtils::write_unsigned16(payload, bits_per_sample, kLittleEndian);
	StreamUtils::write_bytes(payload, extra_bytes);
	write_chunk(out, payload.data());
}

std::unique_ptr<JSONValue> FormatChunk::to_json() const {
	auto json = chunk_json_base(*this);
	json->add("payload_size", std::make_unique<JSONInt>(static_cast<long long>(payload_size())));
	json->add("format", Reflectable::to_json());
	return json;
}

DataChunk::DataChunk() : Chunk(ChunkKind::Data) {}

void DataChunk::parse(FileInputStream& in) {
	audio_data = StreamUtils::read_all_remaining_bytes(in);
}

void DataChunk::write(FileOutputStream& out) const {
	write_chunk(out, audio_data);
}

std::unique_ptr<JSONValue> DataChunk::to_json() const {
	auto json = chunk_json_base(*this);
	json->add("audio_data_size", std::make_unique<JSONInt>(static_cast<long long>(audio_data.size())));
	return json;
}

void CuePoint::parse(FileInputStream& in) {
	id = StreamUtils::read_unsigned32(in, kLittleEndian);
	position = StreamUtils::read_unsigned32(in, kLittleEndian);
	data_chunk_id = StreamUtils::read_ascii(in, 4);
	chunk_start = StreamUtils::read_unsigned32(in, kLittleEndian);
	block_start = StreamUtils::read_unsigned32(in, kLittleEndian);
	sample_offset = StreamUtils::read_unsigned32(in, kLittleEndian);
}

void CuePoint::write(FileOutputStream& out) const {
	StreamUtils::write_unsigned32(out, id, kLittleEndian);
	StreamUtils::write_unsigned32(out, position, kLittleEndian);
	StreamUtils::write_ascii(out, data_chunk_id.empty() ? get_chunk_kind_id(ChunkKind::Data) : data_chunk_id, 4);
	StreamUtils::write_unsigned32(out, chunk_start, kLittleEndian);
	StreamUtils::write_unsigned32(out, block_start, kLittleEndian);
	StreamUtils::write_unsigned32(out, sample_offset, kLittleEndian);
}

CuePoint CuePoint::with_id(const uint32_t new_id) const {
	auto copy = *this;
	copy.id = new_id;
	return copy;
}

void Label::parse(FileInputStream& in) {
	cue_id = StreamUtils::read_unsigned32(in, kLittleEndian);
	text = StreamUtils::read_remaining_ascii_until_null(in);
}

void Label::write(FileOutputStream& out) const {
	StreamUtils::write_unsigned32(out, cue_id, kLittleEndian);
	StreamUtils::write_from(out, text.data(), text.size(), "Writing labl text");
	out.put('\0');
}

Label Label::with_cue_id(const uint32_t new_cue_id) const {
	auto copy = *this;
	copy.cue_id = new_cue_id;
	return copy;
}

void ListSubChunk::parse(FileInputStream& in, const std::string& list_type) {
	id = StreamUtils::read_ascii(in, 4);
	const uint32_t size = StreamUtils::read_unsigned32(in, kLittleEndian);
	payload = StreamUtils::read_n_bytes(in, size);

	if ((size & 1u) != 0u && in.remaining_bytes() > 0) {
		(void)StreamUtils::read_byte(in);
	}

	if (list_type == "adtl" && id == "labl" && payload.size() >= 4) {
		FileInputSubStream payload_stream(payload, in);
		Label parsed_label;
		parsed_label.parse(payload_stream);
		label = std::move(parsed_label);
	}
}

void ListSubChunk::write(FileOutputStream& out) const {
	StreamUtils::write_ascii(out, id, 4);

	if (label) {
		const size_t payload_size = 4 + label->text.size() + 1;
		check_for_numeric_limits(ChunkKind::List, payload.size(), out, id);
		StreamUtils::write_unsigned32(out, payload_size, kLittleEndian);
		label->write(out);
		if ((payload_size & 1u) != 0u) {
			out.put('\0');
		}
	} else {
		check_for_numeric_limits(ChunkKind::List, payload.size(), out, id);
		StreamUtils::write_unsigned32(out, static_cast<uint32_t>(payload.size()), kLittleEndian);
		StreamUtils::write_bytes(out, payload);
		if ((payload.size() & 1u) != 0u) {
			out.put('\0');
		}
	}
}

std::unique_ptr<JSONValue> ListSubChunk::to_json() const {
	auto obj = std::make_unique<JSONObject>();
	obj->add("id", std::make_unique<JSONString>(id));
	obj->add("label", label->to_json());
	return obj;
}

ListChunk::ListChunk() : Chunk(ChunkKind::List) {}

std::vector<Label> ListChunk::labels_for_cue_ids(const std::unordered_set<uint32_t>& ids) const {
	std::vector<Label> labels;
	if (type != "adtl") {
		return labels;
	}
	for (const auto& subchunk : subchunks) {
		if (subchunk.id == "labl" && subchunk.label && ids.contains(subchunk.label->cue_id)) {
			labels.push_back(*subchunk.label);
		}
	}
	return labels;
}

void ListChunk::append_remapped_labels(const std::vector<Label>& labels,
	const CueIdMap& cue_id_map) {
	type = "adtl";
	for (const auto& label : labels) {
		ListSubChunk subchunk;
		subchunk.id = "labl";
		subchunk.label = label.with_cue_id(cue_id_map.at(label.cue_id));
		subchunks.push_back(std::move(subchunk));
	}
}

void ListChunk::parse(FileInputStream& in) {
	if (in.remaining_bytes() < 4) {
		throw ParseError("LIST chunk is too short.", in, "LIST", "At least 4 bytes");
	}

	type = StreamUtils::read_ascii(in, 4);
	subchunks.clear();
	trailing_data.clear();

	while (in.remaining_bytes() >= 8) {
		ListSubChunk subchunk;
		subchunk.parse(in, type);
		subchunks.push_back(std::move(subchunk));
	}

	if (in.remaining_bytes() > 0) {
		trailing_data = StreamUtils::read_all_remaining_bytes(in);
	}
}

void ListChunk::write(FileOutputStream& out) const {
	FileOutputSubStream payload(out);
	StreamUtils::write_ascii(payload, type.empty() ? "adtl" : type, 4);
	for (const auto& subchunk : subchunks) {
		subchunk.write(payload);
	}
	StreamUtils::write_bytes(payload, trailing_data);
	write_chunk(out, payload.data());
}

size_t ListChunk::payload_size() const {
	size_t size = 4 + trailing_data.size();
	for (const auto& subchunk : subchunks) {
		size += 8;
		if (subchunk.label) {
			size += 4 + subchunk.label->text.size() + 1;
			if (((4 + subchunk.label->text.size() + 1) & 1u) != 0u) {
				++size;
			}
		} else {
			size += subchunk.payload.size();
			if ((subchunk.payload.size() & 1u) != 0u) {
				++size;
			}
		}
	}
	return size;
}

std::unique_ptr<JSONValue> ListChunk::to_json() const {
	auto json = chunk_json_base(*this);
	json->add("payload_size", std::make_unique<JSONInt>(static_cast<long long>(payload_size())));
	json->add("list", Reflectable::to_json());
	return json;
}

void SampleLoop::parse(FileInputStream& in) {
	cue_point_id = StreamUtils::read_unsigned32(in, kLittleEndian);
	type = StreamUtils::read_unsigned32(in, kLittleEndian);
	start = StreamUtils::read_unsigned32(in, kLittleEndian);
	end = StreamUtils::read_unsigned32(in, kLittleEndian);
	fraction = StreamUtils::read_unsigned32(in, kLittleEndian);
	play_count = StreamUtils::read_unsigned32(in, kLittleEndian);
}

void SampleLoop::write(FileOutputStream& out) const {
	StreamUtils::write_unsigned32(out, cue_point_id, kLittleEndian);
	StreamUtils::write_unsigned32(out, type, kLittleEndian);
	StreamUtils::write_unsigned32(out, start, kLittleEndian);
	StreamUtils::write_unsigned32(out, end, kLittleEndian);
	StreamUtils::write_unsigned32(out, fraction, kLittleEndian);
	StreamUtils::write_unsigned32(out, play_count, kLittleEndian);
}

void SampleLoop::remap_cue_id(const CueIdMap& cue_id_map) {
	if (const auto mapped_id = cue_id_map.find(cue_point_id); mapped_id != cue_id_map.end()) {
		cue_point_id = mapped_id->second;
	}
}

SamplerChunk::SamplerChunk() : Chunk(ChunkKind::Sampler) {}

std::vector<uint32_t> SamplerChunk::loop_cue_ids_in_order() const {
	std::vector<uint32_t> ids;
	std::unordered_set<uint32_t> seen;
	// Cue points are emitted in the order in which loops first reference them.
	for (const auto& loop : loops) {
		if (seen.insert(loop.cue_point_id).second) {
			ids.push_back(loop.cue_point_id);
		}
	}
	return ids;
}

void SamplerChunk::validate_loop_cues(const CueChunk& cue_chunk) const {
	for (const uint32_t cue_id : loop_cue_ids_in_order()) {
		if (!cue_chunk.cue_point_map.contains(cue_id)) {
			throw RuntimeError("Source sample loop references a missing cue point id: " + std::to_string(cue_id),
				"Copying sample loops");
		}
	}
}

void SamplerChunk::copy_from(const SamplerChunk& source, const CueIdMap& cue_id_map) {
	*this = source;
	for (auto& loop : loops) {
		loop.remap_cue_id(cue_id_map);
	}
}

void SamplerChunk::parse(FileInputStream& in) {
	if (in.remaining_bytes() < 36) {
		throw ParseError("smpl chunk is too short.", in, "smpl", "At least 36 bytes");
	}

	manufacturer = StreamUtils::read_unsigned32(in, kLittleEndian);
	product = StreamUtils::read_unsigned32(in, kLittleEndian);
	sample_period = StreamUtils::read_unsigned32(in, kLittleEndian);
	midi_unity_note = StreamUtils::read_unsigned32(in, kLittleEndian);
	midi_pitch_fraction = StreamUtils::read_unsigned32(in, kLittleEndian);
	smpte_format = StreamUtils::read_unsigned32(in, kLittleEndian);
	smpte_offset = StreamUtils::read_unsigned32(in, kLittleEndian);

	const uint32_t loop_count = StreamUtils::read_unsigned32(in, kLittleEndian);
	sampler_data = StreamUtils::read_unsigned32(in, kLittleEndian);
	if (in.remaining_bytes() < static_cast<size_t>(loop_count) * 24u) {
		throw ParseError("smpl chunk does not contain all declared loops.", in, "smpl");
	}

	loops.clear();
	loops.reserve(loop_count);
	for (uint32_t index = 0; index < loop_count; ++index) {
		SampleLoop loop;
		loop.parse(in);
		loops.push_back(loop);
	}

	trailing_data = StreamUtils::read_all_remaining_bytes(in);
}

void SamplerChunk::write(FileOutputStream& out) const {
	FileOutputSubStream payload(out);
	StreamUtils::write_unsigned32(payload, manufacturer, kLittleEndian);
	StreamUtils::write_unsigned32(payload, product, kLittleEndian);
	StreamUtils::write_unsigned32(payload, sample_period, kLittleEndian);
	StreamUtils::write_unsigned32(payload, midi_unity_note, kLittleEndian);
	StreamUtils::write_unsigned32(payload, midi_pitch_fraction, kLittleEndian);
	StreamUtils::write_unsigned32(payload, smpte_format, kLittleEndian);
	StreamUtils::write_unsigned32(payload, smpte_offset, kLittleEndian);
	StreamUtils::write_unsigned32(payload, static_cast<uint32_t>(loops.size()), kLittleEndian);
	StreamUtils::write_unsigned32(payload, sampler_data, kLittleEndian);
	for (const auto& loop : loops) {
		loop.write(payload);
	}
	StreamUtils::write_bytes(payload, trailing_data);
	write_chunk(out, payload.data());
}

std::unique_ptr<JSONValue> SamplerChunk::to_json() const {
	auto json = chunk_json_base(*this);
	json->add("payload_size", std::make_unique<JSONInt>(static_cast<long long>(payload_size())));
	json->add("sampler", Reflectable::to_json());
	return json;
}

BextChunk::BextChunk() : Chunk(ChunkKind::BroadcastExtension) {}

void BextChunk::parse(FileInputStream& in) {
	if (in.remaining_bytes() < 602) {
		throw ParseError("bext chunk is too short.", in, "bext", "At least 602 bytes");
	}

	description = StreamUtils::read_fixed_ascii(in, 256);
	originator = StreamUtils::read_fixed_ascii(in, 32);
	originator_reference = StreamUtils::read_fixed_ascii(in, 32);
	origination_date = StreamUtils::read_fixed_ascii(in, 10);
	origination_time = StreamUtils::read_fixed_ascii(in, 8);
	time_reference = StreamUtils::read_unsigned64(in, kLittleEndian);
	version = StreamUtils::read_unsigned16(in, kLittleEndian);
	umid = StreamUtils::read_n_bytes(in, 64);
	loudness_value = StreamUtils::read_unsigned16(in, kLittleEndian);
	loudness_range = StreamUtils::read_unsigned16(in, kLittleEndian);
	max_true_peak_level = StreamUtils::read_unsigned16(in, kLittleEndian);
	max_momentary_loudness = StreamUtils::read_unsigned16(in, kLittleEndian);
	max_short_term_loudness = StreamUtils::read_unsigned16(in, kLittleEndian);
	reserved = StreamUtils::read_n_bytes(in, 180);
	coding_history = StreamUtils::read_utf8(in);
}

void BextChunk::write(FileOutputStream& out) const {
	FileOutputSubStream payload(out);
	StreamUtils::write_ascii(payload, description, 256);
	StreamUtils::write_ascii(payload, originator, 32);
	StreamUtils::write_ascii(payload, originator_reference, 32);
	StreamUtils::write_ascii(payload, origination_date, 10);
	StreamUtils::write_ascii(payload, origination_time, 8);
	StreamUtils::write_unsigned64(payload, time_reference, kLittleEndian);
	StreamUtils::write_unsigned16(payload, version, kLittleEndian);

	StreamUtils::write_padded_bytes(payload, umid, 64, "Writing bext UMID");

	StreamUtils::write_unsigned16(payload, loudness_value, kLittleEndian);
	StreamUtils::write_unsigned16(payload, loudness_range, kLittleEndian);
	StreamUtils::write_unsigned16(payload, max_true_peak_level, kLittleEndian);
	StreamUtils::write_unsigned16(payload, max_momentary_loudness, kLittleEndian);
	StreamUtils::write_unsigned16(payload, max_short_term_loudness, kLittleEndian);

	StreamUtils::write_padded_bytes(payload, reserved, 180, "Writing bext reserved field");

	StreamUtils::write_from(payload, coding_history.data(), coding_history.size(), "Writing bext coding history");
	write_chunk(out, payload.data());
}

std::unique_ptr<JSONValue> BextChunk::to_json() const {
	auto json = chunk_json_base(*this);
	json->add("payload_size", std::make_unique<JSONInt>(static_cast<long long>(payload_size())));

	auto payload = std::make_unique<JSONObject>();
	payload->add("description", std::make_unique<JSONString>(description));
	payload->add("originator", std::make_unique<JSONString>(originator));
	payload->add("originator_reference", std::make_unique<JSONString>(originator_reference));
	payload->add("origination_date", std::make_unique<JSONString>(origination_date));
	payload->add("origination_time", std::make_unique<JSONString>(origination_time));
	payload->add("time_reference", std::make_unique<JSONInt>(static_cast<long long>(time_reference)));
	payload->add("version", std::make_unique<JSONInt>(version));

	if (std::ranges::any_of(umid, [](const uint8_t byte) { return byte != 0; })) {
		payload->add("umid", convert_to_json(umid));
	}
	if (loudness_value != 0 || loudness_range != 0 || max_true_peak_level != 0 ||
		max_momentary_loudness != 0 || max_short_term_loudness != 0) {
		auto loudness = std::make_unique<JSONObject>();
		loudness->add("value", std::make_unique<JSONInt>(loudness_value));
		loudness->add("range", std::make_unique<JSONInt>(loudness_range));
		loudness->add("max_true_peak_level", std::make_unique<JSONInt>(max_true_peak_level));
		loudness->add("max_momentary_loudness", std::make_unique<JSONInt>(max_momentary_loudness));
		loudness->add("max_short_term_loudness", std::make_unique<JSONInt>(max_short_term_loudness));
		payload->add("loudness", std::move(loudness));
	}
	if (!coding_history.empty()) {
		payload->add("coding_history", std::make_unique<JSONString>(coding_history));
	}

	json->add("broadcast_extension", std::move(payload));
	return json;
}

CueChunk::CueChunk() : Chunk(ChunkKind::Cue) {}

CueIdMap CueChunk::create_cue_id_map(const std::vector<uint32_t>& source_ids,
	const std::unordered_set<uint32_t>& reserved_ids) const {
	std::unordered_set<uint32_t> used_ids = reserved_ids;
	for (const auto& cue_point : cue_points) {
		used_ids.insert(cue_point.id);
	}

	CueIdMap cue_id_map;
	uint32_t next_id = 1;
	for (const uint32_t source_id : source_ids) {
		if (next_id == 0) {
			throw RuntimeError("Cue point id space exhausted.", "Copying markers");
		}
		while (used_ids.contains(next_id)) {
			if (next_id == std::numeric_limits<uint32_t>::max()) {
				throw RuntimeError("Cue point id space exhausted.", "Copying markers");
			}
			++next_id;
		}
		cue_id_map.emplace(source_id, next_id);
		used_ids.insert(next_id);
		++next_id;
	}
	return cue_id_map;
}

void CueChunk::append_remapped_cues(const CueChunk& source, const std::vector<uint32_t>& source_ids, const CueIdMap& cue_id_map) {
	for (const uint32_t source_id : source_ids) {
		cue_points.push_back(source.cue_point_map.at(source_id)->with_id(cue_id_map.at(source_id)));
	}
	rebuild_cue_point_map();
}

void CueChunk::parse(FileInputStream& in) {
	const uint32_t count = StreamUtils::read_unsigned32(in, kLittleEndian);
	if (in.remaining_bytes() < static_cast<size_t>(count) * 24u) {
		throw ParseError("cue chunk does not contain all declared cue points.", in, get_chunk_kind_id(ChunkKind::Cue));
	}
	cue_points.clear();
	cue_points.reserve(count);
	for (uint32_t index = 0; index < count; ++index) {
		CuePoint cue_point;
		cue_point.parse(in);
		cue_points.push_back(std::move(cue_point));
	}
	rebuild_cue_point_map();
}

void CueChunk::write(FileOutputStream& out) const {
	FileOutputSubStream payload(out);
	StreamUtils::write_unsigned32(payload, static_cast<uint32_t>(cue_points.size()), kLittleEndian);
	for (const auto& cue_point : cue_points) {
		cue_point.write(payload);
	}
	write_chunk(out, payload.data());
}

std::unique_ptr<JSONValue> CueChunk::to_json() const {
	auto json = chunk_json_base(*this);
	json->add("payload_size", std::make_unique<JSONInt>(static_cast<long long>(payload_size())));
	json->add("cue_points", convert_to_json(cue_points));
	return json;
}

std::unique_ptr<Chunk> WavFile::create_empty_chunk(const ChunkKind kind) {
	switch (kind) {
		case ChunkKind::Format:
			return std::make_unique<FormatChunk>();
		case ChunkKind::Data:
			return std::make_unique<DataChunk>();
		case ChunkKind::Cue:
			return std::make_unique<CueChunk>();
		case ChunkKind::List:
			return std::make_unique<ListChunk>();
		case ChunkKind::Sampler:
			return std::make_unique<SamplerChunk>();
		case ChunkKind::BroadcastExtension:
			return std::make_unique<BextChunk>();
		case ChunkKind::Raw:
		default:
			return std::make_unique<RawChunk>();
	}
}

std::unique_ptr<Chunk> WavFile::parse_chunk(FileInputStream& in) {
	const std::string id = StreamUtils::read_ascii(in, 4);
	const uint32_t size = StreamUtils::read_unsigned32(in, kLittleEndian);
	auto payload = StreamUtils::read_n_bytes(in, size);

	if ((size & 1u) != 0u && in.remaining_bytes() > 0) {
		(void)StreamUtils::read_byte(in);
	}

	FileInputSubStream payload_stream(payload, in);
	std::unique_ptr<Chunk> chunk = create_empty_chunk(get_chunk_kind_from_id(id));

	chunk->id = id;
	chunk->parse(payload_stream);
	return chunk;
}

void WavFile::parse(FileInputStream& in) {
	m_chunks.clear();

	m_riff_id = StreamUtils::read_ascii(in, 4);
	if (m_riff_id != "RIFF") {
		throw ParseError("Not a RIFF WAV file.", in, "RIFF Header", "RIFF", m_riff_id);
	}

	const uint32_t riff_payload_size = StreamUtils::read_unsigned32(in, kLittleEndian);
	const uint64_t riff_end_offset = 8ull + riff_payload_size;

	m_wave_id = StreamUtils::read_ascii(in, 4);
	if (m_wave_id != "WAVE") {
		throw ParseError("Not a WAVE file.", in, "RIFF Type", "WAVE", m_wave_id);
	}

	while (!in.eof() && static_cast<uint64_t>(in.offset()) < riff_end_offset) {
		if (static_cast<uint64_t>(in.offset()) + 8ull > riff_end_offset) {
			throw ParseError("Trailing bytes before RIFF end cannot form a chunk header.", in, "Chunk Header");
		}

		auto chunk = parse_chunk(in);
		if (static_cast<uint64_t>(in.offset()) > riff_end_offset) {
			throw ParseError("WAV chunk exceeds RIFF boundary.", in, chunk->id);
		}
		m_chunks.push_back(std::move(chunk));
	}
}

void WavFile::write(FileOutputStream& out) {
	FileOutputSubStream body(out);
	StreamUtils::write_ascii(body, m_wave_id, 4);
	for (const auto& chunk : m_chunks) {
		chunk->write(body);
	}

	const auto bytes = body.data();
	if (bytes.size() > std::numeric_limits<uint32_t>::max()) {
		throw WriteError("WAV file exceeds RIFF uint32 size field.", out, "Writing RIFF header");
	}

	StreamUtils::write_ascii(out, m_riff_id, 4);
	StreamUtils::write_unsigned32(out, static_cast<uint32_t>(bytes.size()), kLittleEndian);
	StreamUtils::write_bytes(out, bytes);
}

std::unique_ptr<JSONValue> WavFile::to_json() const {
	auto json = std::make_unique<JSONObject>();
	json->add("riff_id", std::make_unique<JSONString>(m_riff_id));
	json->add("wave_id", std::make_unique<JSONString>(m_wave_id));

	auto chunks = std::make_unique<JSONArray>();
	for (const auto& chunk : m_chunks) {
		chunks->add(chunk->to_json());
	}
	json->add("chunks", std::move(chunks));

	return json;
}

const FormatChunk* WavFile::format() const {
	return find_chunk_as<FormatChunk>(ChunkKind::Format);
}

std::vector<CuePoint> WavFile::cue_points() const {
	std::vector<CuePoint> out;
	for (const auto& chunk : m_chunks) {
		if (const auto* cue_chunk = dynamic_cast<const CueChunk*>(chunk.get())) {
			out.insert(out.end(), cue_chunk->cue_points.begin(), cue_chunk->cue_points.end());
		}
	}
	return out;
}

std::vector<Label> WavFile::labels() const {
	std::vector<Label> out;
	for (const auto& chunk : m_chunks) {
		const auto* list_chunk = dynamic_cast<const ListChunk*>(chunk.get());
		if (!list_chunk || list_chunk->type != "adtl") {
			continue;
		}
		for (const auto& subchunk : list_chunk->subchunks) {
			if (subchunk.label) {
				out.push_back(*subchunk.label);
			}
		}
	}
	return out;
}

std::vector<SampleLoop> WavFile::sample_loops() const {
	std::vector<SampleLoop> out;
	for (const auto& chunk : m_chunks) {
		if (const auto* sampler_chunk = dynamic_cast<const SamplerChunk*>(chunk.get())) {
			out.insert(out.end(), sampler_chunk->loops.begin(), sampler_chunk->loops.end());
		}
	}
	return out;
}

std::vector<Label> WavFile::labels_for_cue_ids(const std::unordered_set<uint32_t>& ids) const {
	std::vector<Label> labels;
	for (const auto& chunk : m_chunks) {
		const auto* list_chunk = dynamic_cast<const ListChunk*>(chunk.get());
		if (!list_chunk) {
			continue;
		}
		auto matching_labels = list_chunk->labels_for_cue_ids(ids);
		labels.insert(labels.end(), matching_labels.begin(), matching_labels.end());
	}
	return labels;
}

ListChunk& WavFile::ensure_adtl_chunk() {
	for (auto& chunk : m_chunks) {
		auto* list_chunk = dynamic_cast<ListChunk*>(chunk.get());
		if (list_chunk && list_chunk->type == "adtl") {
			return *list_chunk;
		}
	}

	auto list_chunk = std::make_unique<ListChunk>();
	list_chunk->type = "adtl";
	auto* result = list_chunk.get();
	m_chunks.push_back(std::move(list_chunk));
	return *result;
}

std::unordered_set<uint32_t> WavFile::collect_all_cue_ids() const {
	std::unordered_set<uint32_t> cue_ids;
	for (const auto& chunk : m_chunks) {
		if (const auto* cue_chunk = dynamic_cast<const CueChunk*>(chunk.get())) {
			for (const auto& cue_point : cue_chunk->cue_points) {
				cue_ids.insert(cue_point.id);
			}
		}
	}
	return cue_ids;
}

void WavFile::copy_markers_from(const WavFile& source, const bool include_labels) {
	if (this == &source) {
		return;
	}

	const auto* source_cues = source.find_chunk_as<CueChunk>(ChunkKind::Cue);
	if (!source_cues || source_cues->cue_points.empty()) {
		return; // fail silently
		throw RuntimeError("Source WAV contains no cue markers.", "Copying markers");
	}

	std::vector<uint32_t> source_cue_ids;
	source_cue_ids.reserve(source_cues->cue_points.size());
	std::unordered_set<uint32_t> source_cue_id_set;
	for (const auto& cue_point : source_cues->cue_points) {
		source_cue_ids.push_back(cue_point.id);
		source_cue_id_set.insert(cue_point.id);
	}
	const auto source_labels = include_labels
		? source.labels_for_cue_ids(source_cue_id_set)
		: std::vector<Label>{};
	const auto* source_sampler = source.find_chunk_as<SamplerChunk>(ChunkKind::Sampler);
	std::unordered_set<uint32_t> reserved_cue_ids;
	if (source_sampler) {
		for (const uint32_t loop_id : source_sampler->cue_ids_for_loops()) {
			if (!source_cue_id_set.contains(loop_id)) {
				reserved_cue_ids.insert(loop_id);
			}
		}
	} else {
		if (const auto* target_sampler = find_chunk_as<SamplerChunk>(ChunkKind::Sampler)) {
			reserved_cue_ids = target_sampler->cue_ids_for_loops();
		}
	}

	remove_markers();
	auto& target_cue_chunk = this->ensure_chunk_as<CueChunk>(ChunkKind::Cue);
	const auto cue_id_map = target_cue_chunk.create_cue_id_map(source_cue_ids, reserved_cue_ids);
	target_cue_chunk.append_remapped_cues(*source_cues, source_cue_ids, cue_id_map);

	if (source_sampler) {
		auto& target_sampler = ensure_chunk_as<SamplerChunk>(ChunkKind::Sampler);
		target_sampler.copy_from(*source_sampler, cue_id_map);
	}

	if (!include_labels || source_labels.empty()) {
		return;
	}

	ensure_adtl_chunk().append_remapped_labels(source_labels, cue_id_map);
}

void WavFile::remove_markers() {
	auto cue_ids = collect_all_cue_ids();

	for (auto& chunk : m_chunks) {
		auto* list_chunk = dynamic_cast<ListChunk*>(chunk.get());
		if (list_chunk && list_chunk->type == "adtl") {
			list_chunk->remove_labels_by_cue_id(cue_ids);
		}
		if (auto* sampler_chunk = dynamic_cast<SamplerChunk*>(chunk.get())) {
			sampler_chunk->loops.clear();
		}
	}

	std::erase_if(m_chunks, [](const std::unique_ptr<Chunk>& chunk) {
		if (chunk->kind == ChunkKind::Cue) {
			return true;
		}
		const auto* list_chunk = dynamic_cast<const ListChunk*>(chunk.get());
		return list_chunk && list_chunk->type == "adtl"
			&& list_chunk->subchunks.empty() && list_chunk->trailing_data.empty();
	});
}

const std::vector<uint8_t>* WavFile::audio_data() const {
	const auto* data_chunk = find_chunk_as<DataChunk>(ChunkKind::Data);
	return data_chunk ? &data_chunk->audio_data : nullptr;
}
