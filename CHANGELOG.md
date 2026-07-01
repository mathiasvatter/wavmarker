# Changelog

>[!NOTE]
>This is the initial version of `wavmarker` and experimental. The main use case right now is to copy markers and cue points from one `.wav` file to another. For more usage information, have a look at the `README.md`.

## [0.0.1] - 2026-07-01

### Added

- Initial C++20 implementation for parsing and writing RIFF/WAVE files.
- Typed support for `fmt `, `data`, `cue `, `LIST`/`adtl`/`labl`, `smpl`, and
  `bext` chunks.
- Preservation of unknown chunks and their raw payloads during round trips.
- Sample-loop transfer between WAV files, including cue-ID remapping and optional label transfer through `--copy-sample-loops`.
- Reflection-based field access and a patching API for WAV metadata.
- CLI commands for reading and modifying BEXT fields through `--get` and `--set`.
- WAV metadata inspection through `--info` and JSON export support.
