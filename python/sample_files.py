from typing import List, Dict, Set
from collections import defaultdict
import os


def find_wav_files(start_directory: str) -> List[str]:
	"""
	Sucht rekursiv nach .wav-Dateien in einem angegebenen Verzeichnis und gibt eine Liste der gefundenen Dateien zurück.
	"""
	wav_files_list: List[str] = []
	# Stelle sicher, dass wir mit einem absoluten Pfad arbeiten
	abs_start_directory = os.path.abspath(start_directory)

	# Prüfen, ob das Startverzeichnis gültig ist
	if not os.path.isdir(abs_start_directory):
		raise ValueError(f"Ungültiges Startverzeichnis angegeben: {abs_start_directory}")

	for dirpath, dirnames, filenames in os.walk(abs_start_directory):
		# Gehe alle Dateinamen im aktuellen Verzeichnis durch
		for filename in filenames:
			# Prüfe, ob der Dateiname (unabhängig von Groß-/Kleinschreibung) auf .wav endet
			if filename.lower().endswith(".wav"):
				full_path = os.path.join(dirpath, filename)
				wav_files_list.append(full_path)

	return wav_files_list


def group_files_by_mic_position(
    filepaths: List[str],
    mic_positions: List[str] = ["Full", "Close", "Tree", "Far", "BleedStrings", "BleedOrch"],
    separator: str = "_",
    extension: str = ".wav"
) -> List[List[str]]:
	"""
	Ordnet Dateien basierend auf Mikrofonpositionen und Basisnamen.
	"""

	# Dictionary zum Gruppieren: Dict[basisname, Dict[mikrofonposition, dateipfad]]
	# Verwende defaultdict für einfacheres Hinzufügen.
	grouped_files: Dict[str, Dict[str, str]] = defaultdict(dict)

	# Set der erwarteten Mikrofonpositionen für effiziente Vergleiche
	expected_mic_set: Set[str] = set(mic_positions)
	num_expected_mics: int = len(mic_positions)

    # --- Verarbeite jeden Dateipfad aus der Eingabeliste ---
	for full_path in filepaths:
		# Extrahiere den Dateinamen aus dem Pfad
		filename = os.path.basename(full_path)

		# Prüfe, ob die Datei die korrekte Endung hat (Groß-/Kleinschreibung ignorieren)
		if not filename.lower().endswith(extension.lower()):
			continue # Überspringe Dateien mit falscher Endung

		# Prüfe für jede erwartete Mikrofonposition, ob sie im Dateinamen enthalten ist
		for mic_pos in mic_positions:
			fix = f"{separator}{mic_pos}{separator}"

			if fix in filename:
				# Extrahiere den Basisnamen
				base_name = filename.replace(fix, separator)

                # Speichere den Dateipfad, zugeordnet zum Basisnamen und der Mikrofonposition
                # Falls für diese Kombination schon ein Pfad existiert, wird er überschrieben
                # (und eine Warnung ausgegeben).
				if mic_pos in grouped_files[base_name]:
					raise ValueError(f"Doppelte Datei für Basisnamen '{base_name}' und "
									f"Position '{mic_pos}' gefunden: Ersetze "
									f"'{grouped_files[base_name][mic_pos]}' mit '{full_path}'")
				grouped_files[base_name][mic_pos] = full_path

				# Sobald eine passende Position für diesen Pfad gefunden wurde,
				# gehe zum nächsten Pfad in der Eingabeliste über.
				break

	# --- Filtere nach vollständigen Gruppen und formatiere die Ausgabe ---
	complete_groups: List[List[str]] = []

	# Gehe durch alle gefundenen Basisnamen und die dazugehörigen Dateien
	for base_name, found_files_dict in grouped_files.items():
		# Erzeuge ein Set der für diesen Basisnamen gefundenen Mikrofonpositionen
		found_mic_set: Set[str] = set(found_files_dict.keys())

		# Prüfe, ob *genau* die erwarteten Mikrofonpositionen gefunden wurden
		# Der Check `len(found_mic_set) == num_expected_mics` ist durch Set-Gleichheit implizit
		if found_mic_set == expected_mic_set:
			# Erstelle die Liste der Dateipfade in der gewünschten Reihenfolge
			# Kein try-except nötig, da der Set-Vergleich sicherstellt, dass alle Keys existieren
			ordered_paths: List[str] = [found_files_dict[mic_pos] for mic_pos in mic_positions]
			complete_groups.append(ordered_paths)
			print(f"Vollständige Gruppe für Basisnamen gefunden: '{base_name}'")
		# Optional: Logge unvollständige Gruppen (nur wenn mind. eine Datei gefunden wurde)
		elif len(found_mic_set) > 0:
			missing_mics: Set[str] = expected_mic_set - found_mic_set
			raise ValueError(f"Unvollständige Gruppe für Basisnamen '{base_name}': "
							f"Gefunden {len(found_mic_set)}/{num_expected_mics} Positionen. "
							f"Fehlend: {missing_mics}")

	print(f"Insgesamt {len(complete_groups)} vollständige Gruppen aus der Liste zusammengestellt.")
	return complete_groups