# wavmarker

Small C++20/CMake utility for parsing WAV files into structs and writing them back.

## Build

```sh
cmake -S . -B build
cmake --build build
```

## Usage

```sh
./build/wavmarker --info input.wav
./build/wavmarker input.wav output.wav
./build/wavmarker --set 'bext.originator="Sonuscore"' input.wav
./build/wavmarker --get bext.originator input.wav
./build/wavmarker --copy-sample-loops source.wav target.wav
./build/wavmarker --copy-sample-loops source.wav target.wav --no-labels
```

`--copy-sample-loops` replaces the sample loops in `target.wav` with those from
`source.wav` and overwrites `target.wav` in place. Labels belonging to the loops
are copied by default; `--no-labels` disables label copying. Other target audio and
metadata remain unchanged.

`--set` modifies a BEXT field and overwrites the input WAV in place. `--get`
prints the field as JSON. The `bext.` prefix is optional, so this shell-friendly
form is supported as well:

```sh
./build/wavmarker --set "originator"="Sonuscore" input.wav
```

Currently modeled chunks:

- `fmt `
- `data`
- `cue `
- `LIST` / `adtl` / `labl`
- `smpl`

Unknown chunks are preserved as raw payloads and written back unchanged.
