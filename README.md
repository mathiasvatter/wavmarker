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
```

Currently modeled chunks:

- `fmt `
- `data`
- `cue `
- `LIST` / `adtl` / `labl`
- `smpl`

Unknown chunks are preserved as raw payloads and written back unchanged.
