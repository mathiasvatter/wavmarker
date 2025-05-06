from dataclasses import dataclass
from typing import BinaryIO, List

import numpy
import struct


def align_chunk_size(size: int) -> int:
    if bool(size & 1):  # if odd number of bytes, move 1 byte further (data chunk is word-aligned)
        size += 1
    return size


def get_chunk(data: BinaryIO) -> (bytes, int, bytes):
    chunk_id = data.read(4)
    chunk_size = struct.unpack("<I", data.read(4))[0]  # +8
    chunk_size = align_chunk_size(chunk_size)
    return chunk_id, chunk_size, data.read(chunk_size)


def read_riff_chunk(data: BinaryIO) -> int:
    chunk_id = data.read(4)
    if chunk_id != b'RIFF':
        raise ValueError("Not a WAV file.")
    file_size = struct.unpack('<I', data.read(4))[0]  # + 8
    file_type = data.read(4)
    if file_type != b'WAVE':
        raise ValueError("Not a WAV file.")
    return file_size


def read_fmt_chunk(chunk_id: bytes, chunk_size: int, data: bytes, ) -> (int, int, int, int, int, int):
    codec, channels, sample_rate, byte_rate, block_align, bits = struct.unpack('<HHIIHH', data[:16])
    print(f'codec: {codec}, sample_rate: {sample_rate}, bits: {bits}')
    # if codec != 1 or chunk_size > 16:
    # warnings.warn("Unfamiliar format bytes", WavFileWarning)
    return codec, channels, sample_rate, byte_rate, block_align, bits


def read_cue_chunk(chunk_id: bytes, chunk_size: int, data: bytes) -> List[dict]:
    num_cues = struct.unpack('<I', data[:4])[0]
    cue_data = data[4:]
    cues = []
    for idx in range(num_cues):
        cue = cue_data[(idx * 24):(idx * 24 + 24)]
        cue_id, cue_position, cue_datachunkid, cue_chunkstart, cue_blockstart, cue_samplestart = struct.unpack(
            '<ii4siii', cue)
        # Füge die Cue-Informationen als dict in cues liste ein
        cues.append({
            'id': cue_id,
            'position': cue_position,
            'chunk_id': cue_datachunkid,
            'chunk_start': cue_chunkstart,
            'bock_start': cue_blockstart,
            'sample_start': cue_samplestart
        })
    print("CUES:", cues)
    return cues


def read_list_chunk(chunk_id: bytes, chunk_size: int, data: bytes) -> List[dict]:
    datatype = struct.unpack('<4s', data[:4])[0]
    labels = []
    if datatype == b'adtl':
        # The adtl list contains one or more b'labl' chunks
        offset = 4
        while offset < chunk_size:
            subchunk_id = data[offset:offset + 4]
            size = struct.unpack('<I', data[offset + 4:offset + 8])[0]
            if bool(size & 1):  # if odd number of bytes, move 1 byte further (data chunk is word-aligned)
                size += 1
            if subchunk_id == b'labl':
                cue_id = struct.unpack('<I', data[offset + 8: offset + 12])[0]
                cue_label = data[offset + 12:offset + 12 + size - 4].rstrip(bytes('\x00', 'UTF-8'))
                cue_label = cue_label.decode('ascii')
                labels.append({'id': cue_id, 'name': cue_label})
            offset += 8 + size
        print("LABELS:", labels)
    return labels


def read_bwf_chunk(chunk_id: bytes, chunk_size: bytes, data: bytes):
    pass


def read_smpl_chunk(chunk_id, chunk_size, data) -> List[dict]:
    loops = []
    str1 = data[:40 - 4]
    manuf, prod, sampleperiod, midiunitynote, midipitchfraction, smptefmt, smpteoffs, numsampleloops, samplerdata = struct.unpack(
        '<iiiiIiiii', str1)
    cents = midipitchfraction * 1. / (2 ** 32 - 1)
    pitch = 440. * 2 ** ((midiunitynote + cents - 69.) / 12)
    loop_data = data[40 - 4:]
    offset = 24
    for i in range(numsampleloops):
        str1 = loop_data[offset * i:offset * (i + 1)]
        cuepointid, datatype, start, end, fraction, playcount = struct.unpack('<iiiiii', str1)
        loops.append({'id': cuepointid, 'loop-type': datatype, 'position': start, 'end': end, 'fraction': fraction,
                      'play-count': playcount})
    print("LOOPS:", loops)
    return loops


def read_wave(file: str):
    # with open(file, 'rb') as wav_file:
    # data = wav_file.read()
    data = open(file, 'rb')
    file_size = read_riff_chunk(data)
    print(file_size)
    while data.tell() < file_size:
        chunk_id, chunk_size, chunk_data = get_chunk(data)
        print(chunk_id, chunk_size)
        if chunk_id == b'fmt ':
            codec, channels, sample_rate, byte_rate, block_align, bits = read_fmt_chunk(chunk_id, chunk_size,
                                                                                        chunk_data)
        elif chunk_id == b'cue ':
            cues = read_cue_chunk(chunk_id, chunk_size, chunk_data)
        elif chunk_id == b'bext':
            read_bwf_chunk(chunk_id, chunk_size, chunk_data)
        elif chunk_id == b'LIST':
            read_list_chunk(chunk_id, chunk_size, chunk_data)
        elif chunk_id == b'smpl':
            read_smpl_chunk(chunk_id, chunk_size, chunk_data)
    data.close()


def remove_next_chunk(data: BinaryIO, file_size: int, chunk_id: bytes) -> int:
    while data.tell() < file_size:
        c_id, chunk_size = struct.unpack('<4sI', data.read(8))
        if c_id == chunk_id:
            data.seek(-8, 1)  # gehe zurück zum Anfang des chunks
            current_position = data.tell()
            rest_of_file = data.read()[chunk_size + 8:]  # lese den Rest der Date
            data.seek(current_position, 0)  # gehe zurück zum Anfang des chunks
            data.truncate()
            data.write(rest_of_file)
            return chunk_size + 8
        else:
            data.seek(chunk_size, 1)
    return 0


def write_chunk(data: BinaryIO, chunk: bytes) -> int:
    current_position = data.tell()
    # Read the rest of the file
    rest_of_file = data.read()
    data.seek(current_position, 0)  # gehe zurück zum Anfang des chunks
    data.truncate()
    # Write the new chunk header and data
    data.write(chunk)
    # Write the rest of the file
    data.write(rest_of_file)
    return len(chunk)


def update_riff_size(data: BinaryIO, riff_size: int):
    data.seek(0)
    chunk_id = data.read(4)
    if chunk_id != b'RIFF':
        raise ValueError("Not a WAV file.")
    data.write(struct.pack('<I', riff_size))


def write_cue_chunk(data: BinaryIO, cues: List[dict]) -> int:
    num_cues = len(cues)
    cue_data = struct.pack('<4sII', b'cue ', 4 + num_cues * 24, num_cues)
    cue_points = b''
    for cue in cues:
        cue_id, position = cue['id'], cue['position']
        cue_points += struct.pack('<II4sIII', cue_id, position, b'data', 0, 0, position)
    cue_data += cue_points
    return write_chunk(data, cue_data)


def write_list_chunk(data: BinaryIO, cues: List[dict]) -> int:
    adtl_data = struct.pack('<4s', b'adtl')
    for cue in cues:
        cue_id, name = cue['id'], cue.get('name')
        if name is not None:  # cue hat einen namen
            # write label data
            print(name)
            label_data = name.encode('ascii') + b'\x00'
            if len(label_data) % 2 == 1:
                label_data += b'\x00'
            adtl_data += struct.pack('<4sII', b'labl', len(label_data) + 4, cue_id) + label_data
    list_data = struct.pack('<4sI', b'LIST', len(adtl_data)) + adtl_data
    return write_chunk(data, list_data)


def write_smpl_chunk(data: BinaryIO, cues: List[dict]) -> int:
    smpl_data = struct.pack('<4s', b'smpl')
    additional_data = b''
    loop_data = b''
    num_loops = 0
    for cue in cues:
        cue_id, loop_type, start, end, fraction, play_count = cue['id'], cue.get('loop-type'), cue['position'], cue.get(
            'end'), cue.get('fraction'), cue.get('play-count')
        if end is not None:
            num_loops += 1
            loop_type = 0 if loop_type is None else loop_type
            end = 0 if end is None else end
            fraction = 0 if fraction is None else fraction
            play_count = 0 if play_count is None else play_count
            loop = struct.pack('<iiiiii', cue_id, loop_type, start, end, fraction, play_count)
            loop_data += loop

    manuf = 0
    prod = 0
    midiunitynote = 0
    midipitchfraction = 0
    additional_data = struct.pack('<iiiiIiiii', manuf, prod, 0, midiunitynote, midipitchfraction, 0, 0, num_loops,
                                  0) + loop_data
    smpl_data += struct.pack('<I', len(additional_data)) + additional_data
    return write_chunk(data, smpl_data)


def write_markers(file: str, cues: List[dict]):
    """
    For each cue, there has to be a position key value.
    Possible values for the dictionaries: "position", "end", "name", "loop-type", "play-count", "fraction"
    """
    # open file and write data
    f = open(file, 'rb+')
    riff_size = read_riff_chunk(f)
    f.seek(0)
    file_data = f.read()

    # find existing chunks
    cue_index = file_data.find(b'cue ')
    list_index = file_data.find(b'LIST')
    smpl_index = file_data.find(b'smpl')
    # there is a cue chunk
    if cue_index != -1:
        f.seek(cue_index, 0)
        chunk_id, chunk_size, chunk_data = get_chunk(f)
        # print(chunk_id, chunk_size)
        old_cues = read_cue_chunk(chunk_id, chunk_size, chunk_data)

        # there is a list chunk
        if list_index != -1:
            f.seek(list_index, 0)
            chunk_id, chunk_size, chunk_data = get_chunk(f)
            labels = read_list_chunk(chunk_id, chunk_size, chunk_data)

            for i, cue in enumerate(old_cues):
                for j, label in enumerate(labels):
                    if label['id'] == cue['id']:
                        old_cues[i].update(labels[j])

            f.seek(list_index, 0)
            list_size = remove_next_chunk(f, riff_size, b'LIST')
            riff_size -= list_size

        if smpl_index != -1:
            f.seek(smpl_index, 0)
            chunk_id, chunk_size, chunk_data = get_chunk(f)
            loops = read_smpl_chunk(chunk_id, chunk_size, chunk_data)

            for i, cue in enumerate(old_cues):
                for j, loop in enumerate(loops):
                    if loop['id'] == cue['id']:
                        old_cues[i].update(loops[j])

            f.seek(smpl_index, 0)
            smpl_size = remove_next_chunk(f, riff_size, b'smpl')
            riff_size -= smpl_size

        for i, cue in enumerate(cues):
            cue['id'] = len(old_cues) + 1 + i
        cues = old_cues + cues

        f.seek(cue_index, 0)
        cue_size = remove_next_chunk(f, riff_size, b'cue ')
        riff_size -= cue_size

    else:
        for i, cue in enumerate(cues):
            cue['id'] = 1 + i

    cue_size = write_cue_chunk(f, cues)
    riff_size += cue_size
    list_size = write_list_chunk(f, cues)
    riff_size += list_size
    smpl_size = write_smpl_chunk(f, cues)
    riff_size += smpl_size

    update_riff_size(f, riff_size)
    f.close()


def delete_markers(file: str, regions: bool = True, marker: bool = True, labels: bool = True):
    """
    Removes all list chunk, smpl chunk and cue chunk
    """
    # open file and write data
    f = open(file, 'rb+')
    riff_size = read_riff_chunk(f)

    if marker:
        f.seek(0)
        file_data = f.read()
        cue_index = file_data.find(b'cue ')
        # there is a cue chunk
        if cue_index != -1:
            f.seek(cue_index, 0)
            cue_size = remove_next_chunk(f, riff_size, b'cue ')
            riff_size -= cue_size

    if labels:
        f.seek(0)
        file_data = f.read()
        list_index = file_data.find(b'LIST')
        # there is a list chunk
        if list_index != -1:
            f.seek(list_index, 0)
            list_size = remove_next_chunk(f, riff_size, b'LIST')
            riff_size -= list_size

    if regions:
        f.seek(0)
        file_data = f.read()
        smpl_index = file_data.find(b'smpl')
        if smpl_index != -1:
            f.seek(smpl_index, 0)
            smpl_size = remove_next_chunk(f, riff_size, b'smpl')
            riff_size -= smpl_size

    update_riff_size(f, riff_size)
    f.close()
