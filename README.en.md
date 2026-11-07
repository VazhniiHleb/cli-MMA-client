# cli-MMA-client

**English** | [Русский](README.ru.md) | [简体中文](README.md)

A command-line client for [MMA](https://github.com/Edestuss0/MMA), a binary request/response and channel protocol over TCP. The client builds an MMA frame from a local config file, sends it to a server and prints the payload of the response.

> **Status: early development.** The request (route, payload, server address) is hard-coded in `main()`. See [Current limitations](#current-limitations).

## How it works

MMA has no fixed wire format. The byte layout of every frame is described by a shared `FramerConfig`, so a client and a server understand each other only if both use **the same layout**. This client reads the layout from `framer.cfg` at start-up.

A frame consists of:

- **Header** (`header_length` bytes): opcode, version, route length and options count (one byte each, at configurable positions), plus three big-endian 32-bit fields: options length, payload length and request ID.
- **Body**: route, options and payload, in a configurable order.

The full specification (opcodes, options encoding, error handling) is in the [protocol repository](https://github.com/Edestuss0/MMA).

## Requirements

- A C++20 compiler (GCC or Clang; MinGW-w64 for Windows builds)
- CMake 3.15 or newer
- [Asio](https://think-async.com/Asio/) (standalone, header-only). `asio.hpp` must be on the compiler's include path; the CMake project does not download it.

```bash
sudo apt install libasio-dev   # Debian / Ubuntu
brew install asio              # macOS
```

## Build

```bash
cmake -S . -B build
cmake --build build
```

The binary is `build/MMA_client`.

Cross-compiling for Windows (MinGW-w64, statically linked; the Asio headers must be visible to the MinGW compiler too):

```bash
cmake -S . -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/windows.cmake
cmake --build build-win
```

## Configuration

The client reads `framer.cfg` from the **current working directory**. Start from the provided example:

```bash
cp framer.cfg.example framer.cfg
```

The syntax is deliberately minimal:

- one setting per line: `key value` (a single space between them), ended by a newline;
- values are unsigned integers;
- **no blank lines, no comments and no unknown keys.** The parser does not handle them and the client will crash.

`framer.cfg.example` contains the default MMA layout:

```
header_length 29
opcode_pos 1
version_pos 2
route_len_pos 3
options_count_pos 4
big_bytes_config.options 1
big_bytes_config.payload 2
big_bytes_config.req_id 3
body_orders.route 1
body_orders.options 2
body_orders.payload 3
max_message_length 10000000
```

| Key | Meaning |
| --- | --- |
| `header_length` | Total header size in bytes. |
| `opcode_pos`, `version_pos`, `route_len_pos`, `options_count_pos` | **1-based** position of the corresponding one-byte header field (`opcode_pos 1` is the first byte of the header). |
| `big_bytes_config.options` | Slot (1–3) of the 4-byte big-endian *options length* field. |
| `big_bytes_config.payload` | Slot (1–3) of the 4-byte big-endian *payload length* field. |
| `big_bytes_config.req_id` | Slot (1–3) of the 4-byte big-endian *request ID* field. |
| `body_orders.route`, `body_orders.options`, `body_orders.payload` | Position (1–3) of each section in the body. Every value 1–3 must be used exactly once. |

The three 4-byte fields occupy the last 12 bytes of the header. Slot *n* starts at byte offset `header_length - 12 + 4 * (n - 1)`, counting from 0. The three `big_bytes_config.*` values must likewise be 1, 2 and 3 in some order.

`max_message_length`, `options_key_first`, `options_len_order`, `payload_len_order` and `req_id_order` are accepted by the parser but not used yet.

Use the same values as the server's `FramerConfig`; otherwise the server cannot decode the frame.

## Usage

```bash
./build/MMA_client
```

The client reads `framer.cfg`, sends one `Once` request (opcode 2) to the server, prints the sent packet as hex and then prints the payload of the response.

## Current limitations

- The request is hard-coded in `main()`: route `POW`, payload `21`, request ID `1`, server `0.0.0.0:8080`. Edit it and rebuild to change it. For a local server use `127.0.0.1`, since connecting to `0.0.0.0` is not portable (it fails on Windows).
- Options are not sent (the options count is always 0), and options in responses are ignored.
- Only the response payload is printed; the response opcode is not interpreted.
- Only the `Once` request opcode is used. `ChannelConnect` and `ChannelMessage` are not implemented.
- There is no TLS. MMA itself provides no encryption or authentication, so use a trusted network or tunnel the connection through TLS.

## Repository layout

| Path | Description |
| --- | --- |
| `main.cpp` | The client (CMake target `MMA_client`). |
| `includes/conf.hpp` | `MmaFramerConfig` and protocol constants (opcodes, version). |
| `includes/tools.hpp` | Big-endian read/write helpers. |
| `cmake/windows.cmake` | MinGW-w64 toolchain file for Windows builds. |
| `framer.cfg.example` | Example client config (default MMA layout). |
| `main_alt.cpp` | Experimental POSIX-only client with command-line flags. Not part of the CMake build. |
| `server_kfg` | Sample configuration of an MMA server (different format; not read by this client). |
