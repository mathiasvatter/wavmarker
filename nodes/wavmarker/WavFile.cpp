#include "WavFile.h"

#include "FileStream.h"
#include "util/StreamUtils.h"

constexpr bool kLittleEndian = false;

std::vector<uint8_t> write_to_bytes(const auto& value, FileOutputStream& parent_stream) {
	FileOutputSubStream out(parent_stream);
	value.write(out);
	return out.data();
}

std::unique_ptr<JSONValue> byte_preview_to_json(const std::vector<uint8_t>& bytes, const size_t max_size = 32) {
	auto preview = std::make_unique<JSONArray>();
	for (size_t index = 0; index < std::min(bytes.size(), max_size); ++index) {
		preview->add(std::make_unique<JSONInt>(bytes[index]));
	}
	return preview;
}


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
	StreamUtils::write_unsigned16(out, audio_format, kLittleEndian);
	StreamUtils::write_unsigned16(out, channels, kLittleEndian);
	StreamUtils::write_unsigned32(out, sample_rate, kLittleEndian);
	StreamUtils::write_unsigned32(out, byte_rate, kLittleEndian);
	StreamUtils::write_unsigned16(out, block_align, kLittleEndian);
	StreamUtils::write_unsigned16(out, bits_per_sample, kLittleEndian);
	StreamUtils::write_bytes(out, extra_bytes);
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

void Label::parse(FileInputStream& in) {
	cue_id = StreamUtils::read_unsigned32(in, kLittleEndian);
	text = StreamUtils::read_remaining_ascii_until_null(in);
}

void Label::write(FileOutputStream& out) const {
	StreamUtils::write_unsigned32(out, cue_id, kLittleEndian);
	StreamUtils::write_from(out, text.data(), text.size(), "Writing labl text");
	out.put('\0');
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

	std::vector<uint8_t> bytes;
	if (label) {
		bytes = write_to_bytes(*label, out);
	} else {
		bytes = payload;
	}

	StreamUtils::write_unsigned32(out, static_cast<uint32_t>(bytes.size()), kLittleEndian);
	StreamUtils::write_bytes(out, bytes);
	if ((bytes.size() & 1u) != 0u) {
		out.put('\0');
	}
}

std::unique_ptr<JSONValue> ListSubChunk::to_json() const {
	auto obj = std::make_unique<JSONObject>();
	obj->add("id", std::make_unique<JSONString>(id));
	obj->add("label", label->to_json());
	return obj;
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
	StreamUtils::write_ascii(out, type.empty() ? "adtl" : type, 4);
	for (const auto& subchunk : subchunks) {
		subchunk.write(out);
	}
	StreamUtils::write_bytes(out, trailing_data);
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
	StreamUtils::write_unsigned32(out, manufacturer, kLittleEndian);
	StreamUtils::write_unsigned32(out, product, kLittleEndian);
	StreamUtils::write_unsigned32(out, sample_period, kLittleEndian);
	StreamUtils::write_unsigned32(out, midi_unity_note, kLittleEndian);
	StreamUtils::write_unsigned32(out, midi_pitch_fraction, kLittleEndian);
	StreamUtils::write_unsigned32(out, smpte_format, kLittleEndian);
	StreamUtils::write_unsigned32(out, smpte_offset, kLittleEndian);
	StreamUtils::write_unsigned32(out, static_cast<uint32_t>(loops.size()), kLittleEndian);
	StreamUtils::write_unsigned32(out, sampler_data, kLittleEndian);
	for (const auto& loop : loops) {
		loop.write(out);
	}
	StreamUtils::write_bytes(out, trailing_data);
}

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
	StreamUtils::write_ascii(out, description, 256);
	StreamUtils::write_ascii(out, originator, 32);
	StreamUtils::write_ascii(out, originator_reference, 32);
	StreamUtils::write_ascii(out, origination_date, 10);
	StreamUtils::write_ascii(out, origination_time, 8);
	StreamUtils::write_unsigned64(out, time_reference, kLittleEndian);
	StreamUtils::write_unsigned16(out, version, kLittleEndian);

	std::vector<uint8_t> fixed_umid = umid;
	fixed_umid.resize(64);
	StreamUtils::write_bytes(out, fixed_umid);

	StreamUtils::write_unsigned16(out, loudness_value, kLittleEndian);
	StreamUtils::write_unsigned16(out, loudness_range, kLittleEndian);
	StreamUtils::write_unsigned16(out, max_true_peak_level, kLittleEndian);
	StreamUtils::write_unsigned16(out, max_momentary_loudness, kLittleEndian);
	StreamUtils::write_unsigned16(out, max_short_term_loudness, kLittleEndian);

	std::vector<uint8_t> fixed_reserved = reserved;
	fixed_reserved.resize(180);
	StreamUtils::write_bytes(out, fixed_reserved);

	StreamUtils::write_from(out, coding_history.data(), coding_history.size(), "Writing bext coding history");
}

std::unique_ptr<JSONValue> BextChunk::to_json() const {
	auto json = std::make_unique<JSONObject>();
	json->add("description", std::make_unique<JSONString>(description));
	json->add("originator", std::make_unique<JSONString>(originator));
	json->add("originator_reference", std::make_unique<JSONString>(originator_reference));
	json->add("origination_date", std::make_unique<JSONString>(origination_date));
	json->add("origination_time", std::make_unique<JSONString>(origination_time));
	json->add("time_reference", std::make_unique<JSONInt>(static_cast<long long>(time_reference)));
	json->add("version", std::make_unique<JSONInt>(version));

	if (std::ranges::any_of(umid, [](const uint8_t byte) { return byte != 0; })) {
		json->add("umid", convert_to_json(umid));
	}
	if (loudness_value != 0 || loudness_range != 0 || max_true_peak_level != 0 ||
		max_momentary_loudness != 0 || max_short_term_loudness != 0) {
		auto loudness = std::make_unique<JSONObject>();
		loudness->add("value", std::make_unique<JSONInt>(loudness_value));
		loudness->add("range", std::make_unique<JSONInt>(loudness_range));
		loudness->add("max_true_peak_level", std::make_unique<JSONInt>(max_true_peak_level));
		loudness->add("max_momentary_loudness", std::make_unique<JSONInt>(max_momentary_loudness));
		loudness->add("max_short_term_loudness", std::make_unique<JSONInt>(max_short_term_loudness));
		json->add("loudness", std::move(loudness));
	}
	if (!coding_history.empty()) {
		json->add("coding_history", std::make_unique<JSONString>(coding_history));
	}

	return json;
}

void Chunk::parse(FileInputStream& in) {
	id = StreamUtils::read_ascii(in, 4);
	const uint32_t size = StreamUtils::read_unsigned32(in, kLittleEndian);
	auto payload = StreamUtils::read_n_bytes(in, size);

	if ((size & 1u) != 0u && in.remaining_bytes() > 0) {
		(void)StreamUtils::read_byte(in);
	}

	FileInputSubStream payload_stream(payload, in);
	raw_payload = payload;

	if (id == get_chunk_kind_id(ChunkKind::Format)) {
		kind = ChunkKind::Format;
		format.parse(payload_stream);
	} else if (id == get_chunk_kind_id(ChunkKind::Data)) {
		kind = ChunkKind::Data;
		audio_data = std::move(payload);
		raw_payload.clear();
	} else if (id == get_chunk_kind_id(ChunkKind::Cue)) {
		kind = ChunkKind::Cue;
		const uint32_t count = StreamUtils::read_unsigned32(payload_stream, kLittleEndian);
		if (payload_stream.remaining_bytes() < static_cast<size_t>(count) * 24u) {
			throw ParseError("cue chunk does not contain all declared cue points.", payload_stream, get_chunk_kind_id(ChunkKind::Cue));
		}
		cue_points.clear();
		cue_points.reserve(count);
		for (uint32_t index = 0; index < count; ++index) {
			CuePoint cue_point;
			cue_point.parse(payload_stream);
			cue_points.push_back(std::move(cue_point));
		}
		rebuild_cue_point_map();
	} else if (id == get_chunk_kind_id(ChunkKind::List)) {
		kind = ChunkKind::List;
		list.parse(payload_stream);
	} else if (id == get_chunk_kind_id(ChunkKind::Sampler)) {
		kind = ChunkKind::Sampler;
		sampler.parse(payload_stream);
	} else if (id == get_chunk_kind_id(ChunkKind::BroadcastExtension)) {
		kind = ChunkKind::BroadcastExtension;
		bext.parse(payload_stream);
	} else {
		kind = ChunkKind::Raw;
	}
}

void Chunk::write(FileOutputStream& out) const {
	StreamUtils::write_ascii(out, id, 4);

	std::vector<uint8_t> payload;
	switch (kind) {
		case ChunkKind::Format:
			payload = write_to_bytes(format, out);
			break;
		case ChunkKind::Data:
			payload = audio_data;
			break;
		case ChunkKind::Cue:
			{
				FileOutputSubStream payload_stream(out);
				StreamUtils::write_unsigned32(payload_stream, static_cast<uint32_t>(cue_points.size()), kLittleEndian);
				for (const auto& cue_point : cue_points) {
					cue_point.write(payload_stream);
				}
				payload = payload_stream.data();
			}
			break;
		case ChunkKind::List:
			payload = write_to_bytes(list, out);
			break;
		case ChunkKind::Sampler:
			payload = write_to_bytes(sampler, out);
			break;
		case ChunkKind::BroadcastExtension:
			payload = write_to_bytes(bext, out);
			break;
		case ChunkKind::Raw:
		default:
			payload = raw_payload;
			break;
	}

	if (payload.size() > std::numeric_limits<uint32_t>::max()) {
		throw WriteError("WAV chunk payload exceeds uint32 size field.", out, id);
	}

	StreamUtils::write_unsigned32(out, static_cast<uint32_t>(payload.size()), kLittleEndian);
	StreamUtils::write_bytes(out, payload);
	if ((payload.size() & 1u) != 0u) {
		out.put('\0');
	}
}

std::unique_ptr<JSONValue> Chunk::to_json() const {
	auto json = std::make_unique<JSONObject>();
	json->add("id", std::make_unique<JSONString>(id));
	json->add("kind", std::make_unique<JSONString>(get_chunk_kind_string(kind)));
	switch (kind) {
		case ChunkKind::Format:
			json->add("payload_size", std::make_unique<JSONInt>(static_cast<long long>(16 + format.extra_bytes.size())));
			json->add("format", format.to_json());
			break;
		case ChunkKind::Data:
			json->add("audio_data_size", std::make_unique<JSONInt>(static_cast<long long>(audio_data.size())));
			break;
		case ChunkKind::Cue:
			json->add("payload_size", std::make_unique<JSONInt>(static_cast<long long>(4 + cue_points.size() * 24)));
			json->add("cue_points", convert_to_json(cue_points));
			break;
		case ChunkKind::List:
			json->add("list", list.to_json());
			break;
		case ChunkKind::Sampler:
			json->add("payload_size", std::make_unique<JSONInt>(static_cast<long long>(36 + sampler.loops.size() * 24 + sampler.trailing_data.size())));
			json->add("sampler", sampler.to_json());
			break;
		case ChunkKind::BroadcastExtension:
			json->add("payload_size", std::make_unique<JSONInt>(static_cast<long long>(602 + bext.coding_history.size())));
			json->add("broadcast_extension", bext.to_json());
			break;
		case ChunkKind::Raw:
		default:
			json->add("payload_size", std::make_unique<JSONInt>(static_cast<long long>(raw_payload.size())));
			if (id == "JUNK" || id == "junk") {
				json->add("kind", std::make_unique<JSONString>("junk"));
				json->add("purpose", std::make_unique<JSONString>("padding"));
				// json->add("byte_preview", byte_preview_to_json(raw_payload));
			} else {
				json->add("raw_payload", convert_to_json(raw_payload));
			}
			break;
	}

	return json;
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

		Chunk chunk;
		chunk.parse(in);
		if (static_cast<uint64_t>(in.offset()) > riff_end_offset) {
			throw ParseError("WAV chunk exceeds RIFF boundary.", in, chunk.id);
		}
		m_chunks.push_back(std::move(chunk));
	}
}

void WavFile::write(FileOutputStream& out) {
	FileOutputSubStream body(out);
	StreamUtils::write_ascii(body, m_wave_id, 4);
	for (const auto& chunk : m_chunks) {
		chunk.write(body);
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
		chunks->add(chunk.to_json());
	}
	json->add("chunks", std::move(chunks));

	return json;
}

const FormatChunk* WavFile::format() const {
	for (const auto& chunk : m_chunks) {
		if (chunk.kind == ChunkKind::Format) {
			return &chunk.format;
		}
	}
	return nullptr;
}

std::vector<CuePoint> WavFile::cue_points() const {
	std::vector<CuePoint> out;
	for (const auto& chunk : m_chunks) {
		if (chunk.kind == ChunkKind::Cue) {
			out.insert(out.end(), chunk.cue_points.begin(), chunk.cue_points.end());
		}
	}
	return out;
}

std::vector<Label> WavFile::labels() const {
	std::vector<Label> out;
	for (const auto& chunk : m_chunks) {
		if (chunk.kind != ChunkKind::List || chunk.list.type != "adtl") {
			continue;
		}
		for (const auto& subchunk : chunk.list.subchunks) {
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
		if (chunk.kind == ChunkKind::Sampler) {
			out.insert(out.end(), chunk.sampler.loops.begin(), chunk.sampler.loops.end());
		}
	}
	return out;
}

WavFile::SampleLoopCueReferences WavFile::validate_sample_loop_cues() const {
	SampleLoopCueReferences references;

	references.sampler_chunk = find_chunk(ChunkKind::Sampler);
	if (!references.sampler_chunk || references.sampler_chunk->sampler.loops.empty()) {
		throw RuntimeError("Source WAV does not contain sample loops.", "Copying sample loops");
	}

	references.cue_chunk = find_chunk(ChunkKind::Cue);
	if (!references.cue_chunk) {
		throw RuntimeError("Source WAV contains sample loops but no cue chunk.", "Copying sample loops");
	}

	for (const auto& loop : references.sampler_chunk->sampler.loops) {
		if (references.cue_ids.insert(loop.cue_point_id).second) {
			references.cue_ids_in_order.push_back(loop.cue_point_id);
		}
	}

	for (const auto source_cue_id : references.cue_ids_in_order) {
		if (!references.cue_chunk->cue_point_map.contains(source_cue_id)) {
			throw RuntimeError("Source sample loop references a missing cue point id: " + std::to_string(source_cue_id),
				"Copying sample loops");
		}
	}

	return references;
}

void WavFile::copy_sample_loops_from(WavFile& source, const bool include_labels) {
	const auto source_loop_cues = source.validate_sample_loop_cues();

	std::vector<Label> source_labels;
	if (include_labels) {
		for (const auto& chunk : source.m_chunks) {
			if (chunk.kind != ChunkKind::List || chunk.list.type != "adtl") {
				continue;
			}
			for (const auto& subchunk : chunk.list.subchunks) {
				if (subchunk.id == "labl" && subchunk.label && source_loop_cues.cue_ids.contains(subchunk.label->cue_id)) {
					source_labels.push_back(*subchunk.label);
				}
			}
		}
	}

	std::unordered_set<uint32_t> old_target_loop_cue_ids;
	if (const auto* target_sampler_chunk = this->find_chunk(ChunkKind::Sampler)) {
		old_target_loop_cue_ids = target_sampler_chunk->sampler.cue_ids_for_loops();
	}

	if (!old_target_loop_cue_ids.empty()) {
		if (auto* target_cue_chunk = this->find_chunk(ChunkKind::Cue)) {
			target_cue_chunk->remove_cues_by_id(old_target_loop_cue_ids);
		}
		for (auto& chunk : m_chunks) {
			if (chunk.kind == ChunkKind::List && chunk.list.type == "adtl") {
				chunk.list.remove_labels_by_cue_id(old_target_loop_cue_ids);
			}
		}
	}

	std::unordered_map<uint32_t, uint32_t> cue_id_map;
	uint32_t next_cue_id = this->next_free_cue_id();
	for (const auto source_cue_id : source_loop_cues.cue_ids_in_order) {
		if (next_cue_id == 0) {
			throw RuntimeError("Cue point id overflow while copying sample loops.", "Copying sample loops");
		}
		cue_id_map.emplace(source_cue_id, next_cue_id++);
	}

	auto& target_cue_chunk = this->ensure_chunk(ChunkKind::Cue);
	for (const auto source_cue_id : source_loop_cues.cue_ids_in_order) {
		const auto target_cue_id = cue_id_map.at(source_cue_id);
		auto cue_point = *source_loop_cues.cue_chunk->cue_point_map.at(source_cue_id);
		cue_point.id = target_cue_id;
		target_cue_chunk.cue_points.push_back(std::move(cue_point));
	}
	target_cue_chunk.rebuild_cue_point_map();

	auto& target_sampler_chunk = this->ensure_chunk(ChunkKind::Sampler);
	target_sampler_chunk.sampler = source_loop_cues.sampler_chunk->sampler;
	for (auto& loop : target_sampler_chunk.sampler.loops) {
		loop.cue_point_id = cue_id_map.at(loop.cue_point_id);
	}

	if (!include_labels || source_labels.empty()) {
		return;
	}

	auto& target_list_chunk = this->ensure_chunk(ChunkKind::List);
	target_list_chunk.list.type = "adtl";
	for (auto label : source_labels) {
		label.cue_id = cue_id_map.at(label.cue_id);

		ListSubChunk subchunk;
		subchunk.id = "labl";
		subchunk.label = std::move(label);
		target_list_chunk.list.subchunks.push_back(std::move(subchunk));
	}
}

const std::vector<uint8_t>* WavFile::audio_data() const {
	for (const auto& chunk : m_chunks) {
		if (chunk.kind == ChunkKind::Data) {
			return &chunk.audio_data;
		}
	}
	return nullptr;
}
