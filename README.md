# cli-MMA-client

[English](README.en.md) | [Русский](README.ru.md) | **简体中文**

[MMA](https://github.com/Edestuss0/MMA) 协议的命令行客户端。MMA 是基于 TCP 的二进制请求/响应与通道协议。客户端根据本地配置文件构造 MMA 帧，发送给服务器，并打印响应的 payload。

> **状态：早期开发阶段。** 请求内容（route、payload、服务器地址）目前写死在 `main()` 中。参见[当前限制](#当前限制)。

## 工作原理

MMA 没有固定的传输格式。每个帧的字节布局由共享的 `FramerConfig` 描述，因此只有当客户端和服务器使用**完全相同的布局**时，双方才能互相理解。本客户端在启动时从 `framer.cfg` 读取布局。

一个帧由以下部分组成：

- **头部**（`header_length` 字节）：操作码（opcode）、版本、route 长度和 options 数量（各占 1 字节，位置可配置），以及三个大端序 32 位字段：options 长度、payload 长度和请求 ID。
- **消息体**：route、options 和 payload，顺序可配置。

完整规范（操作码、options 编码、错误处理）见[协议仓库](https://github.com/Edestuss0/MMA)。

## 环境要求

- 支持 C++20 的编译器（GCC 或 Clang；构建 Windows 版本需要 MinGW-w64）
- CMake 3.15 或更高版本
- [Asio](https://think-async.com/Asio/)（独立版，仅头文件）。`asio.hpp` 必须位于编译器的头文件搜索路径中；CMake 项目不会自动下载它。

```bash
sudo apt install libasio-dev   # Debian / Ubuntu
brew install asio              # macOS
```

## 构建

```bash
cmake -S . -B build
cmake --build build
```

可执行文件为 `build/MMA_client`。

为 Windows 交叉编译（MinGW-w64，静态链接；MinGW 编译器同样需要能找到 Asio 头文件）：

```bash
cmake -S . -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/windows.cmake
cmake --build build-win
```

## 配置

客户端从**当前工作目录**读取 `framer.cfg`。请从提供的示例开始：

```bash
cp framer.cfg.example framer.cfg
```

语法刻意保持极简：

- 每行一个设置：`键 值`（中间一个空格），以换行结束；
- 值为非负整数；
- **不允许空行、注释和未知键。** 解析器不会处理这些内容，客户端会崩溃。

`framer.cfg.example` 包含 MMA 的默认布局：

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

| 键 | 含义 |
| --- | --- |
| `header_length` | 头部总字节数。 |
| `opcode_pos`、`version_pos`、`route_len_pos`、`options_count_pos` | 对应单字节头部字段的位置，**从 1 开始计数**（`opcode_pos 1` 表示头部第一个字节）。 |
| `big_bytes_config.options` | 4 字节大端序字段*options 长度*所在的槽位（1–3）。 |
| `big_bytes_config.payload` | 4 字节大端序字段*payload 长度*所在的槽位（1–3）。 |
| `big_bytes_config.req_id` | 4 字节大端序字段*请求 ID*所在的槽位（1–3）。 |
| `body_orders.route`、`body_orders.options`、`body_orders.payload` | 各部分在消息体中的位置（1–3）。1–3 每个值必须恰好使用一次。 |

三个 4 字节字段占据头部的最后 12 个字节。槽位 *n* 的起始偏移为 `header_length - 12 + 4 * (n - 1)`（从 0 开始计数）。三个 `big_bytes_config.*` 的值同样必须是 1、2、3 的某种排列。

`max_message_length`、`options_key_first`、`options_len_order`、`payload_len_order` 和 `req_id_order` 会被解析器接受，但目前尚未使用。

这些值必须与服务器的 `FramerConfig` 一致，否则服务器无法解码该帧。

## 使用方法

```bash
./build/MMA_client
```

客户端读取 `framer.cfg`，向服务器发送一个 `Once` 请求（操作码 2），以十六进制打印已发送的数据包，然后打印响应的 payload。

## 当前限制

- 请求写死在 `main()` 中：route 为 `POW`，payload 为 `21`，请求 ID 为 `1`，服务器为 `0.0.0.0:8080`。如需修改，请编辑代码并重新构建。连接本机服务器请使用 `127.0.0.1`，因为连接 `0.0.0.0` 不具备可移植性（在 Windows 上会失败）。
- 不发送 options（options 数量始终为 0），响应中的 options 会被忽略。
- 只打印响应的 payload，不解析响应的操作码。
- 仅使用 `Once` 请求操作码，`ChannelConnect` 和 `ChannelMessage` 尚未实现。
- 不支持 TLS。MMA 本身不提供加密或认证，请在可信网络中使用，或通过 TLS 隧道传输。

## 仓库结构

| 路径 | 说明 |
| --- | --- |
| `main.cpp` | 客户端（CMake 目标 `MMA_client`）。 |
| `includes/conf.hpp` | `MmaFramerConfig` 和协议常量（操作码、版本）。 |
| `includes/tools.hpp` | 大端序读写辅助函数。 |
| `cmake/windows.cmake` | 用于 Windows 构建的 MinGW-w64 工具链文件。 |
| `framer.cfg.example` | 客户端配置示例（MMA 默认布局）。 |
| `main_alt.cpp` | 仅支持 POSIX 的实验性客户端，带命令行参数。不属于 CMake 构建。 |
| `server_kfg` | MMA 服务器的配置示例（格式不同，本客户端不会读取）。 |
