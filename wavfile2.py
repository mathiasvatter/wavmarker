from dataclasses import dataclass
from typing import BinaryIO, List, Dict

import numpy
import struct


def align_chunk_size(size: int) -> int:
	if bool(size & 1):  # if odd number of bytes, move 1 byte further (data chunk is word-aligned)
		size += 1
	return size


def get_chunk(data: BinaryIO) -> tuple[bytes, int, bytes]:
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


def read_fmt_chunk(chunk_id: bytes, chunk_size: int, data: bytes, ) -> tuple[int, int, int, int, int, int]:
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
	# print("CUES:", cues)
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
		# print("LABELS:", labels)
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
		loops.append({
			'id': cuepointid, 
			'loop-type': datatype, 
			'position': start, 
			'end': end, 
			'fraction': fraction,
			'play-count': playcount
		})
	# print("LOOPS:", loops)
	return loops


def read_wave(file: str, readmarkers: bool = True) -> tuple[dict, dict]:
	"""
	Liest eine WAV-Datei und extrahiert Audioeigenschaften sowie diverse Marker-Daten.

	Args:
		file_path: Der Pfad zur WAV-Datei.

	Returns:
		Ein Tupel (wave_properties, all_markers_data):
		- wave_properties: Dictionary mit Audioeigenschaften (sample_rate, bits, channels etc.).
		- all_markers_data: Dictionary mit Listen für 'cues', 'labels', 'loops', 'bwf'.
	"""
	data = open(file, 'rb')
	file_size = read_riff_chunk(data)
	# print(file_size)
	all_markers_data = {
		"cues": [],
		"labels": [],
		"loops": [],
		"bwf": [] 
	}
	wave_properties = {
		"codec": None, "channels": None, "sample_rate": None, 
		"byte_rate": None, "block_align": None, "bits": None
	}
	while data.tell() < file_size:
		chunk_id, chunk_size, chunk_data = get_chunk(data)
		# print(chunk_id, chunk_size)
		if chunk_id == b'fmt ':
			codec, channels, sr, br, ba, bits = read_fmt_chunk(chunk_id, chunk_size, chunk_data)
			wave_properties.update({
						"codec": codec, "channels": channels, "sample_rate": sr,
						"byte_rate": br, "block_align": ba, "bits": bits
					})
		elif chunk_id == b'cue ':
			all_markers_data["cues"] = read_cue_chunk(chunk_id, chunk_size, chunk_data)
		elif chunk_id == b'bext':
			read_bwf_chunk(chunk_id, chunk_size, chunk_data)
		elif chunk_id == b'LIST':
			labels = read_list_chunk(chunk_id, chunk_size, chunk_data)
			if labels: # Annahme: gibt Liste von Label-Dicts zurück
						all_markers_data["labels"].extend(labels)
		elif chunk_id == b'smpl':
			all_markers_data["loops"] = read_smpl_chunk(chunk_id, chunk_size, chunk_data)
	data.close()
	return wave_properties, all_markers_data


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

def remove_chunk_at_current_pos(data_file_obj: BinaryIO, expected_chunk_id: bytes) -> int:
    """
    Entfernt den Chunk mit der expected_chunk_id an der aktuellen Dateizeigerposition.
    Gibt die Gesamtgröße des entfernten Chunks (Header + ausgerichtete Daten) zurück oder 0 bei Fehlern.
    """
    start_of_chunk_to_remove = data_file_obj.tell()

    # Lese ID und Größe des Chunks an der aktuellen Position
    id_bytes = data_file_obj.read(4)
    if not id_bytes:
        print(f"DEBUG remove_chunk_at_current_pos: Konnte ID nicht lesen bei {start_of_chunk_to_remove} (EOF?).")
        data_file_obj.seek(start_of_chunk_to_remove)
        return 0
        
    if id_bytes != expected_chunk_id:
        print(f"FEHLER in remove_chunk_at_current_pos: Erwartete Chunk-ID {expected_chunk_id}, aber {id_bytes} gefunden bei Position {start_of_chunk_to_remove}.")
        data_file_obj.seek(start_of_chunk_to_remove) # Zurückspulen
        return 0

    size_unaligned_bytes = data_file_obj.read(4)
    if len(size_unaligned_bytes) < 4:
        print(f"FEHLER in remove_chunk_at_current_pos: Konnte Größe für Chunk {expected_chunk_id} nicht lesen bei {start_of_chunk_to_remove}.")
        data_file_obj.seek(start_of_chunk_to_remove) # Zurückspulen
        return 0
    
    size_unaligned_content = struct.unpack("<I", size_unaligned_bytes)[0]
    size_aligned_content = align_chunk_size(size_unaligned_content) # Ausgerichtete Größe des DATEN-Teils
    
    total_removed_chunk_disk_size = 8 + size_aligned_content # ID (4) + Größenfeld (4) + Ausgerichtete Daten

    # Positioniere den Zeiger an das Ende des zu löschenden Chunks (d.h. Anfang des nächsten Chunks oder EOF)
    pos_after_removed_chunk_data = start_of_chunk_to_remove + total_removed_chunk_disk_size
    data_file_obj.seek(pos_after_removed_chunk_data)
    
    # Lese alle nachfolgenden Daten
    rest_of_file_data = data_file_obj.read()
    
    # Gehe zurück zum Anfang des Chunks, der entfernt wird
    data_file_obj.seek(start_of_chunk_to_remove)
    # Schreibe die nachfolgenden Daten über die Position des entfernten Chunks
    data_file_obj.write(rest_of_file_data)
    # Kürze die Datei auf ihre neue, kürzere Länge
    new_file_length = start_of_chunk_to_remove + len(rest_of_file_data)
    data_file_obj.truncate(new_file_length)
    
    # print(f"DEBUG remove_chunk_at_current_pos: {total_removed_chunk_disk_size} Bytes für Chunk '{expected_chunk_id}' entfernt. Neue Dateilänge: {new_file_length}.")
    return total_removed_chunk_disk_size



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


def write_smpl_chunk(data: BinaryIO, cues: List[dict], sample_rate: int) -> int:
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

	sampleperiod = int(1000000000.0 / sample_rate)  # Nanosekunden pro Sample
	manuf = 0
	prod = 0
	midiunitynote = 0
	midipitchfraction = 0
	smptefmt = 0
	smpteoffs = 0
	samplerdata = 0 # Reserviert, typischerweise 0
	additional_data = struct.pack('<iiiiIiiii', manuf, prod, sampleperiod, midiunitynote, midipitchfraction, smptefmt, smpteoffs, num_loops,
									samplerdata) + loop_data
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
	fmt_index = file_data.find(b'fmt ')
	cue_index = file_data.find(b'cue ')
	list_index = file_data.find(b'LIST')
	smpl_index = file_data.find(b'smpl')

	sr = 0
	if fmt_index != -1:
		f.seek(fmt_index, 0)
		chunk_id, chunk_size, chunk_data = get_chunk(f)
		codec, channels, sr, br, ba, bits = read_fmt_chunk(chunk_id, chunk_size, chunk_data)
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
			list_size = remove_chunk_at_current_pos(f, b'LIST')
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
			smpl_size = remove_chunk_at_current_pos(f, b'smpl')
			riff_size -= smpl_size

		for i, cue in enumerate(cues):
			cue['id'] = len(old_cues) + 1 + i
		cues = old_cues + cues

		f.seek(cue_index, 0)
		cue_size = remove_chunk_at_current_pos(f, b'cue ')
		riff_size -= cue_size

	else:
		for i, cue in enumerate(cues):
			cue['id'] = 1 + i

	cue_size = write_cue_chunk(f, cues)
	riff_size += cue_size
	list_size = write_list_chunk(f, cues)
	riff_size += list_size
	smpl_size = write_smpl_chunk(f, cues, sr)
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
			cue_size = remove_chunk_at_current_pos(f, b'cue ')
			riff_size -= cue_size

	if labels:
		f.seek(0)
		file_data = f.read()
		list_index = file_data.find(b'LIST')
		# there is a list chunk
		if list_index != -1:
			f.seek(list_index, 0)
			list_size = remove_chunk_at_current_pos(f, b'LIST')
			riff_size -= list_size

	if regions:
		f.seek(0)
		file_data = f.read()
		smpl_index = file_data.find(b'smpl')
		if smpl_index != -1:
			f.seek(smpl_index, 0)
			smpl_size = remove_chunk_at_current_pos(f, b'smpl')
			riff_size -= smpl_size

	update_riff_size(f, riff_size)
	f.close()


def transfer_wav_markers(source_wav_path: str, target_wav_path: str):
	"""
	Überträgt Marker-Daten (Cue-Punkte, Labels, Loops) von einer Quell-WAV-Datei
	zu einer Ziel-WAV-Datei.

	Args:
		source_wav_path: Pfad zur Quell-WAV-Datei.
		target_wav_path: Pfad zur Ziel-WAV-Datei.
	"""
	print(f"Starte Übertragung der Marker von '{source_wav_path}' nach '{target_wav_path}'...")

	source_props, source_marker_components = read_wave(source_wav_path)
	if source_props is None and source_marker_components is None: # Fehler beim Lesen
		print(f"Fehler beim Lesen der Quelldatei '{source_wav_path}'. Abbruch.")
		return

	source_cues = source_marker_components.get("cues", [])
	source_labels = source_marker_components.get("labels", [])
	source_loops = source_marker_components.get("loops", [])
	# source_bwf = source_marker_components.get("bwf", []) # Falls BWF auch übertragen werden soll


	# 2. Marker-Daten konsolidieren
	#    Ziel ist eine Liste von Dictionaries, die von write_markers verarbeitet werden kann.
	#    Jedes Dictionary repräsentiert einen Cue-Punkt und kann Label- und Loop-Infos enthalten.

	temp_markers_by_id: Dict[int, Dict] = {}

	# Beginne mit Cue-Punkten (diese haben meist die primäre 'id' und 'position')
	for cue in source_cues:
		cue_id = cue.get('id')
		if cue_id is not None:
			temp_markers_by_id[cue_id] = {
				'id': cue_id,
				'position': cue.get('position'),
				'name': cue.get('name'), # Falls read_cue_chunk bereits Namen liefert (unwahrscheinlich)
				'end': None, # Initialisieren für spätere Loop-Daten
				'loop-type': None,
				'fraction': None,
				'play-count': None
			}

	# Füge Label-Informationen hinzu oder aktualisiere sie
	for label in source_labels:
		label_id = label.get('id') # 'id' des Cue-Punkts, zu dem das Label gehört
		if label_id in temp_markers_by_id:
			temp_markers_by_id[label_id]['name'] = label.get('name')
		else:
			# Label ohne passenden Cue-Punkt. Könnte als reiner Regionsname ohne exakten Cue-Marker existieren.
			# Für write_markers ist es am besten, wenn ein Label mit einem Cue-Punkt (und dessen Position) verbunden ist.
			# Sie könnten hier einen neuen Eintrag erstellen, wenn Ihre `write_markers` das unterstützt.
			print(f"Warnung: Label mit ID {label_id} ('{label.get('name')}') gefunden, aber kein passender Cue-Punkt im 'cue '-Chunk. Es wird versucht, es als neuen Marker hinzuzufügen, falls es eine Position hat, oder es könnte verloren gehen.")

	# Füge Loop-Informationen hinzu oder aktualisiere sie
	for loop in source_loops:
		# 'id' in loop ist die 'cuepointid', die sich auf die ID eines Cue-Punkts bezieht
		loop_cue_id = loop.get('id') 
		if loop_cue_id in temp_markers_by_id:
			# Die 'position' ist bereits vom Cue-Punkt gesetzt.
			temp_markers_by_id[loop_cue_id]['loop-type'] = loop.get('loop-type')
			temp_markers_by_id[loop_cue_id]['end'] = loop.get('end')
			temp_markers_by_id[loop_cue_id]['fraction'] = loop.get('fraction')
			temp_markers_by_id[loop_cue_id]['play-count'] = loop.get('play-count')
		else:
			print(f"Warnung: Loop-Daten mit cuepointid {loop_cue_id} gefunden, aber kein passender Cue-Punkt. "
					"Dieser Loop wird übersprungen, da er nicht korrekt verankert werden kann.")

	# Konvertiere das temporäre Dictionary in die finale Liste
	consolidated_markers = list(temp_markers_by_id.values())

	# Optional: Sortiere Marker nach Position (write_markers könnte dies auch intern tun)
	consolidated_markers.sort(key=lambda m: m.get('position', 0))

	if not consolidated_markers:
		print("Keine Marker-Daten zum Schreiben nach der Konsolidierung vorhanden.")
		return

	for m_idx, m_entry in enumerate(consolidated_markers):
		print(f"  Marker {m_idx+1}: ID={m_entry.get('id')}, Pos={m_entry.get('position')}, Name={m_entry.get('name')}, End={m_entry.get('end')}, LoopType={m_entry.get('loop-type')}")


	# 4. Konsolidierte Marker-Daten in die Zieldatei schreiben
	print(f"Schreibe {len(consolidated_markers)} konsolidierte Marker in die Zieldatei '{target_wav_path}'...")
	# Annahme: write_markers ist in Ihrer wave.py Datei definiert und modifiziert,
	# um die sample_rate intern für write_smpl_chunk zu lesen.
	write_markers(target_wav_path, consolidated_markers)
	print("Marker erfolgreich in die Zieldatei übertragen.")