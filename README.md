# VIDtorio

[VIDtorio](https://crypticstick.github.io/vidtorio/) is a web app for generating static and animated displays in Factorio.

More details coming soon (WIP).

## Development Setup

1. Install the required dependencies:
    - Install packages
        - Ubuntu:  `sudo apt install git cmake make ninja-build npm python3`
    - Install emsdk (https://emscripten.org/docs/getting_started/downloads.html)

2. Build the WebAssembly module:

```bash
emcmake cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

3. Run the Web App:

```bash
npm install
npm run dev
```
