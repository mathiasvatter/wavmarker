# wavmarker

[![Release](https://img.shields.io/github/v/release/mathiasvatter/wavmarker)](https://github.com/mathiasvatter/wavmarker/releases)
[![Build](https://img.shields.io/github/actions/workflow/status/mathiasvatter/wavmarker/build-and-release.yml)](https://github.com/mathiasvatter/wavmarker/actions)
[![License](https://img.shields.io/badge/license-MIT-blue)](LICENSE.md)
[![Downloads](https://img.shields.io/github/downloads/mathiasvatter/wavmarker/total)](https://github.com/mathiasvatter/wavmarker/releases)

Small C++20/CMake utility for parsing WAV files into structs and writing them back.

## Build

```sh
./build.sh release
```

## Usage

```sh
./wavmarker --info input.wav
./wavmarker input.wav output.wav
./wavmarker --set 'bext.originator="WAVMARKER"' input.wav
./wavmarker --get bext.originator input.wav
./wavmarker --copy-markers source.wav target.wav
./wavmarker --copy-markers source.wav target.wav --no-labels
./wavmarker --remove-markers input.wav
```

`--copy-markers` replaces the cue markers in `target.wav` with those from
`source.wav` and overwrites `target.wav` in place. Labels are copied by default;
`--no-labels` disables label copying. `--remove-markers` removes all cue markers,
their labels, and all sampler loops. If the source contains a `smpl` chunk, its sampler loop data is
copied too. Without a source `smpl` chunk, existing target loops remain unchanged.
Audio and other metadata remain unchanged.

`--set` modifies a BEXT field and overwrites the input WAV in place. `--get`
prints the field as JSON. The `bext.` prefix is optional, so this shell-friendly
form is supported as well:

```sh
./wavmarker --set "originator"="WAVMARKER" input.wav
```

Currently modeled chunks:

- `fmt `
- `data`
- `cue `
- `LIST` / `adtl` / `labl`
- `smpl`

Unknown chunks are preserved as raw payloads and written back unchanged.
