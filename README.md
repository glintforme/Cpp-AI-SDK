# AI 智能聊天助手系统

> 基于 C++17 从零实现的 AI 智能聊天助手，采用「SDK 接入层 + HTTP 服务层 + Web 前端」三层架构，对接 DeepSeek、Gemini、Kimi 等云端大模型及 Ollama 本地模型，支持流式响应、会话持久化、Markdown / 代码高亮 / LaTeX 公式渲染。

---

## 目录

- [功能特性](#功能特性)
- [项目架构](#项目架构)
- [技术栈](#技术栈)
- [环境依赖](#环境依赖)
- [快速开始](#快速开始)
- [配置说明](#配置说明)
- [API 接口](#api-接口)
- [项目结构](#项目结构)
- [技术亮点](#技术亮点)
- [常见问题](#常见问题)

---

## 功能特性

### 后端

- **多模型接入**：支持 DeepSeek-V4-Pro、Gemini-3.5-Flash、Kimi-K3（云端）+ DeepSeek-R1:1.5b（Ollama 本地），基于多态架构可灵活扩展
- **流式响应**：基于 SSE（Server-Sent Events）协议实现流式输出，逐字显示 AI 回复
- **会话管理**：SQLite 持久化存储会话与消息，支持多轮上下文对话
- **参数配置**：gflags 支持命令行参数 + 配置文件双模式，含参数安全校验
- **代理检测**：自动检测 Gemini 模型的代理可用性，前端显示代理状态提示

### 前端

- **会话管理**：左侧会话列表，支持新建 / 切换 / 删除
- **模型选择**：弹窗式模型选择，显示模型描述与代理状态
- **Markdown 渲染**：集成 marked.js，支持表格、列表、引用等语法
- **代码高亮**：集成 highlight.js，支持多种语言语法高亮 + 一键复制
- **数学公式**：集成 KaTeX，支持 `$O(n)$` 内联公式和 `$$O(n^2)$$` 块级公式渲染
- **响应式布局**：适配桌面端与移动端

---

## 项目架构

```
┌─────────────────────────────────────────────────────────┐
│                     Web 前端 (浏览器)                      │
│  会话管理 │ SSE流式解析 │ Markdown渲染 │ 代码高亮 │ 公式渲染  │
└──────────────────────┬──────────────────────────────────┘
                       │ HTTP / SSE
┌──────────────────────▼──────────────────────────────────┐
│              ChatServer (C++ httplib 服务层)              │
│   路由分发 │ 参数校验(gflags) │ 代理检测 │ 静态资源服务      │
└──────────────────────┬──────────────────────────────────┘
                       │ 调用
┌──────────────────────▼──────────────────────────────────┐
│            libai_chat_sdk.a (C++ SDK 接入层)              │
│  ┌─────────────┐  ┌──────────────┐  ┌────────────────┐  │
│  │ ChatSDK     │  │ LLMManager   │  │ SessionManager │  │
│  │ (统一门面)   │  │ (多模型管理)  │  │ (会话/消息管理) │  │
│  └─────────────┘  └──────┬───────┘  └────────┬───────┘  │
│                          │ 多态                │ SQLite    │
│         ┌────────┬───────┼────────┬────────┐  │          │
│    DeepSeek  Gemini  KIMI3  Ollama           DataManager  │
└─────────────────────────────────────────────────────────┘
```

---

## 技术栈

| 层级 | 技术 |
|------|------|
| **SDK 接入层** | C++17、httplib、jsoncpp、SQLite3、OpenSSL、spdlog、fmt |
| **HTTP 服务层** | C++17、httplib、gflags、jsoncpp |
| **前端展示层** | 原生 HTML / CSS / JS、marked.js、highlight.js、KaTeX、Font Awesome |
| **构建工具** | CMake >= 3.10 |

---

## 环境依赖

### 系统要求

- Linux（推荐 Ubuntu 20.04+）
- C++17 兼容编译器（g++ 9+ / clang++ 10+）
- CMake >= 3.10

### 安装依赖库（Ubuntu / Debian）

```bash
sudo apt update
sudo apt install -y build-essential cmake libjsoncpp-dev libsqlite3-dev \
    libssl-dev libgflags-dev libspdlog-dev libfmt-dev
```

### 安装 cpp-httplib

cpp-httplib 是 header-only 库，需手动安装：

```bash
git clone https://github.com/yhirose/cpp-httplib.git
cd cpp-httplib
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
sudo make install
```

### （可选）安装 Ollama 用于本地模型

```bash
# 安装 Ollama
curl -fsSL https://ollama.com/install.sh | sh

# 拉取本地模型
ollama pull deepseek-r1:1.5b

# 启动 Ollama 服务（默认监听 127.0.0.1:11434）
ollama serve
```

---

## 快速开始

### 1. 配置 API 密钥

通过环境变量配置云端模型的 API Key：

```bash
export deepseek_apikey="sk-your-deepseek-key"
export KIMI3_apikey="sk-your-kimi-key"
export gemini_apikey="your-gemini-key"
```

### 2. 编译 SDK 静态库

```bash
cd sdk
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
//在以上几步之后，我建议使用代码安装-安装路径为/usr/local/lib
sudo make install
```

编译成功后会在 `sdk/build/` 下生成 `libai_chat_sdk.a`。

### 3. 编译 ChatServer

```bash
cd ChatServer
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
```

编译成功后会在 `ChatServer/build/` 下生成可执行文件 `AIChatServer`，同时自动拷贝前端资源到 `build/www/`。

### 4. 运行服务

```bash
cd ChatServer/build
./AIChatServer
```

首次运行会自动生成默认配置文件 `ChatServer.conf`。

### 5. 访问页面

浏览器打开 [http://localhost:8080](http://localhost:8080) 即可使用。

---

## 配置说明

支持两种配置方式：**命令行参数** 和 **配置文件**（两者可混用，命令行优先级更高）。

### 命令行参数

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `--host` | string | `0.0.0.0` | 服务器绑定地址 |
| `--port` | int | `8080` | 服务器绑定端口 |
| `--log_level` | string | `INFO` | 日志级别（TRACE/DEBUG/INFO/WARN/ERROR/CRITICAL） |
| `--temperature` | double | `0.7` | 温度值，控制生成随机性（0.0 ~ 2.0） |
| `--max_tokens` | int | `2048` | 最大生成 token 数 |
| `--config_file` | string | `./ChatServer.conf` | 配置文件路径 |
| `--ollama_model_name` | string | `deepseek-r1:1.5b` | Ollama 模型名称 |
| `--ollama_model_desc` | string | `本地 DeepSeek R1 1.5B 推理模型` | Ollama 模型描述（前端展示） |
| `--ollama_endpoint` | string | `http://127.0.0.1:11434` | Ollama API 地址 |

**示例**：

```bash
# 命令行指定端口和日志级别
./AIChatServer --port=9090 --log_level=DEBUG

# 使用配置文件
./AIChatServer --config_file=/path/to/my.conf
```

### 配置文件格式

首次运行自动生成 `ChatServer.conf`，格式如下：

```
--host=0.0.0.0
--port=8080
--log_level=INFO
--temperature=0.7
--max_tokens=2048
--ollama_model_name=deepseek-r1:1.5b
--ollama_endpoint=http://127.0.0.1:11434
```

### 帮助与版本

```bash
./AIChatServer -h      # 查看帮助
./AIChatServer -v      # 查看版本
```

---

## API 接口

| 方法 | 路径 | 说明 |
|------|------|------|
| GET | `/api/sessions` | 获取所有会话列表 |
| POST | `/api/session` | 创建新会话 |
| GET | `/api/session/{id}` | 获取会话详情 |
| DELETE | `/api/session/{id}` | 删除会话 |
| GET | `/api/session/{id}/history` | 获取会话历史消息 |
| GET | `/api/models` | 获取可用模型列表 |
| GET | `/api/proxy/check` | 检测代理可用性 |
| POST | `/api/session/{id}/message` | 发送消息（全量响应） |
| POST | `/api/session/{id}/message/stream` | 发送消息（SSE 流式响应） |

### 示例：创建会话

```bash
curl -X POST http://localhost:8080/api/session \
  -H "Content-Type: application/json" \
  -d '{"model_name": "deepseek-v4-pro"}'
```

### 示例：流式发送消息

```bash
curl -X POST http://localhost:8080/api/session/{session_id}/message/stream \
  -H "Content-Type: application/json" \
  -d '{"message": "你好，请介绍一下自己"}' \
  --no-buffer
```

响应格式（SSE）：

```
data: "你好"

data: "，我是"

data: "AI助手"

data: [DONE]
```

---

## 项目结构

```
AI_SDK/
├── sdk/                            # SDK 接入层（静态库 libai_chat_sdk.a）
│   ├── include/                    # 头文件
│   │   ├── ChatSDK.h               # 对外门面类
│   │   ├── LLMManager.h            # 模型管理器
│   │   ├── LLMProvider.h           # 模型抽象基类（多态接口）
│   │   ├── DeepSeekProvider.h      # DeepSeek 实现
│   │   ├── GeminiProvider.h        # Gemini 实现
│   │   ├── KIMI3Provider.h         # Kimi 实现
│   │   ├── OllamaLLMProvider.h     # Ollama 本地模型实现
│   │   ├── SessionManager.h        # 会话管理
│   │   ├── DataManager.h           # SQLite 数据访问层
│   │   ├── common.h                # 公共数据结构
│   │   └── util/MyLog.h            # 日志工具
│   ├── src/                        # 实现文件
│   └── CMakeLists.txt
│
├── ChatServer/                     # HTTP 服务层
│   ├── main.cpp                    # 程序入口（gflags 配置）
│   ├── ChatServer.cpp              # 路由处理
│   ├── ChatServer.h                # 服务类声明
│   ├── www/                        # 前端静态资源
│   │   ├── index.html              # 页面结构
│   │   ├── script.js               # 交互逻辑（SSE / Markdown / KaTeX）
│   │   └── styles.css              # 样式
│   └── CMakeLists.txt
│
└── test/                           # 测试代码
```

---

## 技术亮点

### 1. 多态架构的模型接入层

基于 `LLMProvider` 抽象基类，通过多态统一接入 4 种异构大模型 API。新增模型仅需继承实现 `sendMessage`（同步）和 `sendMessageIncrement`（流式）两个虚函数，符合开闭原则。

### 2. SSE 流式响应

使用 httplib 的 `set_chunked_content_provider` 实现 SSE 流式传输，前端通过 `fetch + ReadableStream` 实时解析增量数据块，实现逐字显示效果。处理了多模型 SSE 协议差异（前缀长度、`reasoning_content` 与 `content` 分离、`[DONE]` 标记）。

### 3. 长会话渲染性能优化

针对长会话切换卡顿问题，使用 `DocumentFragment` 批量插入 DOM + `requestAnimationFrame` 异步分批增强代码块，将主线程阻塞优化为渐进式渲染。

### 4. LaTeX 数学公式渲染

集成 KaTeX，自动识别 Markdown 文本中的 `$...$`（内联）和 `$$...$$`（块级）LaTeX 公式并渲染，同时排除代码块内的 `$` 符号避免误解析。

### 5. 代理检测与友好提示

Gemini 模型在国内需代理访问。系统自动检测代理可用性，在模型选择界面显示代理状态标签，发送消息时前后端双重校验，代理不可用时返回友好提示。

---

## 常见问题

### Q: 启动后提示找不到 `libai_chat_sdk.a`？

A: 需要先编译 SDK，再编译 ChatServer：

```bash
cd sdk && mkdir -p build && cd build && cmake .. && make -j$(nproc)
cd ../../ChatServer && mkdir -p build && cd build && cmake .. && make -j$(nproc)
```

### Q: Gemini 模型无响应？

A: Gemini API 在国内需要代理访问。请确保已开启代理，系统会自动检测并在界面提示代理状态。

### Q: Ollama 本地模型无法使用？

A: 请确认 Ollama 服务已启动（`ollama serve`）且已拉取对应模型（`ollama pull deepseek-r1:1.5b`）。可通过 `--ollama_endpoint` 参数指定 Ollama 地址。

### Q: API 密钥如何配置？

A: 通过环境变量设置，变量名区分大小写：

```bash
export deepseek_apikey="sk-xxx"    # DeepSeek
export KIMI3_apikey="sk-xxx"       # Kimi
export gemini_apikey="xxx"         # Gemini
```

### Q: 如何修改监听端口？

A: 启动时指定 `--port` 参数，或修改 `ChatServer.conf` 配置文件：

```bash
./AIChatServer --port=9090
```

---

## License

MIT License
