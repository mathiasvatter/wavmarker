import wavfile2 as wav
import sample_files as files
import os
from typing import Dict, List


def find_file_pairs(dir_with_markers: str, dir_wo_markers: str) -> Dict[str, Dict[str, str]]:
	"""
	Findet Paare von .wav-Dateien mit identischen Basisnamen in zwei Verzeichnissen.

	Args:
		dir_with_markers: Pfad zum Verzeichnis mit den Quelldateien (mit Markern).
		dir_wo_markers: Pfad zum Verzeichnis mit den Zieldateien (ohne Marker).

	Returns:
		Ein Dictionary, bei dem die Schlüssel die Basisdateinamen sind 
		(z.B. "audio.wav") und die Werte Dictionaries mit den Schlüsseln 
		'source' und 'target' sind, die die vollständigen Pfade enthalten.
		Gibt ein leeres Dictionary zurück, wenn Verzeichnisse ungültig sind 
		oder keine Paare gefunden wurden.
	"""
	found_pairs: Dict[str, Dict[str, str]] = {}

	# 1. Prüfen, ob Verzeichnisse existieren
	if not os.path.isdir(dir_with_markers):
		print(f"FEHLER: Quellverzeichnis nicht gefunden: {dir_with_markers}")
		return found_pairs
	if not os.path.isdir(dir_wo_markers):
		print(f"FEHLER: Zielverzeichnis nicht gefunden: {dir_wo_markers}")
		return found_pairs


	files_with_markers_paths: List[str] = files.find_wav_files(dir_with_markers)
	files_wo_markers_paths: List[str] = files.find_wav_files(dir_wo_markers)
	print(f"Dateien im Quellverzeichnis gefunden (initial): {len(files_with_markers_paths)}")
	print(f"Dateien im Zielverzeichnis gefunden (initial): {len(files_wo_markers_paths)}")


	# 3. Lookup-Dictionary für Zieldateien erstellen (nur .wav)
	lookup_files_wo_markers: Dict[str, str] = {}
	target_wav_count = 0
	for f_path in files_wo_markers_paths:
		base_name = os.path.basename(f_path)
		# Nur .wav-Dateien berücksichtigen
		if base_name.lower().endswith(".wav"):
			target_wav_count += 1
			# Umgang mit möglichen Duplikaten im Zielverzeichnis (letzte gewinnt)
			if base_name in lookup_files_wo_markers:
				print(f"WARNUNG: Doppelter Dateiname '{base_name}' im Zielverzeichnis '{dir_wo_markers}'. "
						f"Pfad '{f_path}' überschreibt '{lookup_files_wo_markers[base_name]}'.")
			lookup_files_wo_markers[base_name] = f_path
			
	print(f"{target_wav_count} .wav-Dateien im Zielverzeichnis gefunden.")

	# 4. Quelldateien durchgehen und Paare finden (nur .wav)
	unmatched_source_files_count = 0
	source_wav_count = 0
	for source_path in files_with_markers_paths:
		base_name = os.path.basename(source_path)
		
		# Nur .wav-Dateien aus der Quelle berücksichtigen
		if not base_name.lower().endswith(".wav"):
			continue
		source_wav_count += 1

		# Prüfen, ob eine passende Datei im Ziel-Lookup existiert
		if base_name in lookup_files_wo_markers:
			target_path = lookup_files_wo_markers[base_name]
			# Paar gefunden, zum Ergebnis hinzufügen
			found_pairs[base_name] = {
				'source': source_path,
				'target': target_path
			}
		else:
			unmatched_source_files_count += 1
			
	print(f"{source_wav_count} .wav-Dateien im Quellverzeichnis gefunden.")

	# 5. Zusammenfassung ausgeben und Paare zurückgeben
	print(f"\nPaarungs-Suche abgeschlossen:")
	print(f"  -> {len(found_pairs)} gleichnamige .wav-Dateipaare gefunden.")
	if unmatched_source_files_count > 0:
		print(f"  -> {unmatched_source_files_count} Quelldateien (.wav) hatten kein passendes Gegenstück im Zielverzeichnis.")

	unmatched_target_files_count = target_wav_count - len(found_pairs)
	if unmatched_target_files_count > 0:
		print(f"  -> {unmatched_target_files_count} Zieldateien (.wav) hatten kein passendes Gegenstück im Quellverzeichnis.")
		
	return found_pairs



if __name__ == "__main__":
    

	# file_with_markers = "/Volumes/LaCieSSD/DYNAMEDION/06_Sonuscore/69_LUX_Orchestra/Violins1_Sus_BleedOrch_65-95_A2.wav"
	# file_wo_markers = '/Volumes/LaCieSSD/DYNAMEDION/06_Sonuscore/69_LUX_Orchestra/Violins1_Sus_BleedOrch_65-95_A2 copy.wav'
	# wav.delete_markers(file_wo_markers)
	# print(wav.read_wave(file_wo_markers))
	# wav.transfer_wav_markers(file_with_markers, file_wo_markers)


	# collect all files from two directories
	dir_with_markers = "/Volumes/LaCieSSD/DYNAMEDION/06_Sonuscore/69_LUX_Orchestra/06_Latest_Samples/Violins 1"
	dir_wo_markers = "/Volumes/LaCieSSD/DYNAMEDION/06_Sonuscore/69_LUX_Orchestra/11_Denoised_Samples"

	file_pairs = find_file_pairs(dir_with_markers, dir_wo_markers)

	if file_pairs:
		print("\nGefundene Paare:")
		# Ausgabe der gefundenen Paare
		for basename, paths in file_pairs.items():
			print(f"  - {basename}:")
			print(f"    Quelle: {paths['source']}")
			print(f"    Ziel:   {paths['target']}")
			wav.transfer_wav_markers(paths['source'], paths['target'])

	# ({'codec': 1, 'channels': 2, 'sample_rate': 48000, 'byte_rate': 288000, 'block_align': 6, 'bits': 24}, {'cues': [{'id': 1, 'position': 192000, 'chunk_id': b'data', 'chunk_start': 0, 'bock_start': 0, 'sample_start': 192000}], 'labels': [{'id': 1, 'name': '#'}], 'loops': [{'id': 1, 'loop-type': 0, 'position': 192000, 'end': 320000, 'fraction': 0, 'play-count': 0}], 'bwf': []})
	# ({'codec': 1, 'channels': 2, 'sample_rate': 48000, 'byte_rate': 288000, 'block_align': 6, 'bits': 24}, {'cues': [{'id': 1, 'position': 192000, 'chunk_id': b'data', 'chunk_start': 0, 'bock_start': 0, 'sample_start': 192000}, {'id': 2, 'position': 192000, 'chunk_id': b'data', 'chunk_start': 0, 'bock_start': 0, 'sample_start': 192000}], 'labels': [{'id': 2, 'name': '#'}], 'loops': [{'id': 2, 'loop-type': 0, 'position': 192000, 'end': 320000, 'fraction': 0, 'play-count': 0}], 'bwf': []})