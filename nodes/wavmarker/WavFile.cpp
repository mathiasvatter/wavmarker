#include "nodes/wavmarker/WavFile.h"

#include <algorithm>
#include <limits>

#include "FileStream.h"
#include "util/StreamUtils.h"

namespace {

constexpr bool kLittleEndian = false;

std::string read_fourcc(FileInputStream& in) {
	return StreamUtils::read_ascii(in, 4);
}

void write_fourcc(FileOutputStream& out, const std::string& id) {
	if (id.size() != 4) {
		throw WriteError("Invalid fourcc length for '" + id + "'", out, "Writing fourcc");
	}
	StreamUtils::write_ascii(out, id, 4);
}

size_t remaining_bytes(FileInputStream& in) {
	auto& stream = in.stream();
	const auto current = stream.tellg();
	if (current == std::istream::pos_type(-1)) {
		return 0;
	}

	stream.seekg(0, std::ios::end);
	const auto end = stream.tellg();
	stream.seekg(current);
	if (end == std::istream::pos_type(-1) || end < current) {
		return 0;
	}

	return static_cast<size_t>(end - current);
}

std::vector<uint8_t> write_to_bytes(const auto& value, FileOutputStream& parent_stream) {
	FileOutputSubStream out(parent_stream);
	value.write(out);
	return out.data();
}

} // namespace

namespace WavMarker {

void FormatChunk::parse(FileInputStream& in) {
	if (remaining_bytes(in) < 16) {
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
	data_chunk_id = read_fourcc(in);
	chunk_start = StreamUtils::read_unsigned32(in, kLittleEndian);
	block_start = StreamUtils::read_unsigned32(in, kLittleEndian);
	sample_offset = StreamUtils::read_unsigned32(in, kLittleEndian);
}

void CuePoint::write(FileOutputStream& out) const {
	StreamUtils::write_unsigned32(out, id, kLittleEndian);
	StreamUtils::write_unsigned32(out, position, kLittleEndian);
	write_fourcc(out, data_chunk_id.empty() ? "data" : data_chunk_id);
	StreamUtils::write_unsigned32(out, chunk_start, kLittleEndian);
	StreamUtils::write_unsigned32(out, block_start, kLittleEndian);
	StreamUtils::write_unsigned32(out, sample_offset, kLittleEndian);
}

void Label::parse(FileInputStream& in) {
	cue_id = StreamUtils::read_unsigned32(in, kLittleEndian);
	auto bytes = StreamUtils::read_all_remaining_bytes(in);
	auto end = std::find(bytes.begin(), bytes.end(), 0);
	text.assign(bytes.begin(), end);
}

void Label::write(FileOutputStream& out) const {
	StreamUtils::write_unsigned32(out, cue_id, kLittleEndian);
	StreamUtils::write_from(out, text.data(), text.size(), "Writing labl text");
	out.put('\0');
}

void ListSubChunk::parse(FileInputStream& in, const std::string& list_type) {
	id = read_fourcc(in);
	const uint32_t size = StreamUtils::read_unsigned32(in, kLittleEndian);
	payload = StreamUtils::read_n_bytes(in, size);

	if ((size & 1u) != 0u && remaining_bytes(in) > 0) {
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
	write_fourcc(out, id);

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

void ListChunk::parse(FileInputStream& in) {
	if (remaining_bytes(in) < 4) {
		throw ParseError("LIST chunk is too short.", in, "LIST", "At least 4 bytes");
	}

	type = read_fourcc(in);
	subchunks.clear();
	trailing_data.clear();

	while (remaining_bytes(in) >= 8) {
		ListSubChunk subchunk;
		subchunk.parse(in, type);
		subchunks.push_back(std::move(subchunk));
	}

	if (remaining_bytes(in) > 0) {
		trailing_data = StreamUtils::read_all_remaining_bytes(in);
	}
}

void ListChunk::write(FileOutputStream& out) const {
	write_fourcc(out, type.empty() ? "adtl" : type);
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
	if (remaining_bytes(in) < 36) {
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
	if (remaining_bytes(in) < static_cast<size_t>(loop_count) * 24u) {
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

void Chunk::parse(FileInputStream& in) {
	id = read_fourcc(in);
	const uint32_t size = StreamUtils::read_unsigned32(in, kLittleEndian);
	auto payload = StreamUtils::read_n_bytes(in, size);

	if ((size & 1u) != 0u && remaining_bytes(in) > 0) {
		(void)StreamUtils::read_byte(in);
	}

	FileInputSubStream payload_stream(payload, in);
	raw_payload = payload;

	if (id == "fmt ") {
		kind = ChunkKind::Format;
		format.parse(payload_stream);
	} else if (id == "data") {
		kind = ChunkKind::Data;
		audio_data = std::move(payload);
		raw_payload.clear();
	} else if (id == "cue ") {
		kind = ChunkKind::Cue;
		const uint32_t count = StreamUtils::read_unsigned32(payload_stream, kLittleEndian);
		if (remaining_bytes(payload_stream) < static_cast<size_t>(count) * 24u) {
			throw ParseError("cue chunk does not contain all declared cue points.", payload_stream, "cue ");
		}
		cue_points.clear();
		cue_points.reserve(count);
		for (uint32_t index = 0; index < count; ++index) {
			CuePoint cue_point;
			cue_point.parse(payload_stream);
			cue_points.push_back(std::move(cue_point));
		}
	} else if (id == "LIST") {
		kind = ChunkKind::List;
		list.parse(payload_stream);
	} else if (id == "smpl") {
		kind = ChunkKind::Sampler;
		sampler.parse(payload_stream);
	} else {
		kind = ChunkKind::Raw;
	}
}

void Chunk::write(FileOutputStream& out) const {
	write_fourcc(out, id);

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

} // namespace WavMarker

void WavFile::parse(FileInputStream& in) {
	m_chunks.clear();

	m_riff_id = read_fourcc(in);
	if (m_riff_id != "RIFF") {
		throw ParseError("Not a RIFF WAV file.", in, "RIFF Header", "RIFF", m_riff_id);
	}

	const uint32_t riff_payload_size = StreamUtils::read_unsigned32(in, kLittleEndian);
	const uint64_t riff_end_offset = 8ull + riff_payload_size;

	m_wave_id = read_fourcc(in);
	if (m_wave_id != "WAVE") {
		throw ParseError("Not a WAVE file.", in, "RIFF Type", "WAVE", m_wave_id);
	}

	while (!in.eof() && static_cast<uint64_t>(in.offset()) < riff_end_offset) {
		if (static_cast<uint64_t>(in.offset()) + 8ull > riff_end_offset) {
			throw ParseError("Trailing bytes before RIFF end cannot form a chunk header.", in, "Chunk Header");
		}

		WavMarker::Chunk chunk;
		chunk.parse(in);
		if (static_cast<uint64_t>(in.offset()) > riff_end_offset) {
			throw ParseError("WAV chunk exceeds RIFF boundary.", in, chunk.id);
		}
		m_chunks.push_back(std::move(chunk));
	}
}

void WavFile::write(FileOutputStream& out) {
	FileOutputSubStream body(out);
	write_fourcc(body, m_wave_id);
	for (const auto& chunk : m_chunks) {
		chunk.write(body);
	}

	const auto bytes = body.data();
	if (bytes.size() > std::numeric_limits<uint32_t>::max()) {
		throw WriteError("WAV file exceeds RIFF uint32 size field.", out, "Writing RIFF header");
	}

	write_fourcc(out, m_riff_id);
	StreamUtils::write_unsigned32(out, static_cast<uint32_t>(bytes.size()), kLittleEndian);
	StreamUtils::write_bytes(out, bytes);
}

const WavMarker::FormatChunk* WavFile::format() const {
	for (const auto& chunk : m_chunks) {
		if (chunk.kind == WavMarker::ChunkKind::Format) {
			return &chunk.format;
		}
	}
	return nullptr;
}

std::vector<WavMarker::CuePoint> WavFile::cue_points() const {
	std::vector<WavMarker::CuePoint> out;
	for (const auto& chunk : m_chunks) {
		if (chunk.kind == WavMarker::ChunkKind::Cue) {
			out.insert(out.end(), chunk.cue_points.begin(), chunk.cue_points.end());
		}
	}
	return out;
}

std::vector<WavMarker::Label> WavFile::labels() const {
	std::vector<WavMarker::Label> out;
	for (const auto& chunk : m_chunks) {
		if (chunk.kind != WavMarker::ChunkKind::List || chunk.list.type != "adtl") {
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

std::vector<WavMarker::SampleLoop> WavFile::sample_loops() const {
	std::vector<WavMarker::SampleLoop> out;
	for (const auto& chunk : m_chunks) {
		if (chunk.kind == WavMarker::ChunkKind::Sampler) {
			out.insert(out.end(), chunk.sampler.loops.begin(), chunk.sampler.loops.end());
		}
	}
	return out;
}

const std::vector<uint8_t>* WavFile::audio_data() const {
	for (const auto& chunk : m_chunks) {
		if (chunk.kind == WavMarker::ChunkKind::Data) {
			return &chunk.audio_data;
		}
	}
	return nullptr;
}
