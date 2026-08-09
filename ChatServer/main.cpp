#include "ChatServer.h"
#include <gflags/gflags.h>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <string>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <spdlog/common.h>
// 正确的日志头文件路径
#include <ai_chat_sdk/util/MyLog.h>

// 定义gflags参数
DEFINE_string(host, "0.0.0.0", "服务器绑定的地址");
DEFINE_int32(port, 8080, "服务器绑定的端口号");
DEFINE_string(log_level, "INFO", "日志级别 (TRACE, DEBUG, INFO, WARN, ERROR, CRITICAL)");
DEFINE_double(temperature, 0.7, "温度值，影响生成文本的随机性 (0.0~2.0)");
DEFINE_int32(max_tokens, 2048, "最大token数 (正数)");
DEFINE_string(config_file, "./ChatServer.conf", "配置文件路径");
// Ollama配置参数 (默认使用本地部署的 deepseek-r1:1.5b)
DEFINE_string(ollama_model_name, "deepseek-r1:1.5b", "Ollama模型名称 (默认: deepseek-r1:1.5b)");
DEFINE_string(ollama_model_desc, "本地 DeepSeek R1 1.5B 推理模型", "Ollama模型描述");
DEFINE_string(ollama_endpoint, "http://127.0.0.1:11434", "Ollama API地址 (默认: http://127.0.0.1:11434)");

// 版本号
const std::string VERSION = "1.0.0";
const std::string PROGRAM_NAME = "AIChatServer";

// 从环境变量获取API密钥
std::string getEnvVar(const std::string& key) {
    char* value = std::getenv(key.c_str());
    return value ? std::string(value) : "";
}

// 生成默认配置文件
void generateDefaultConfig(const std::string& configPath) {
    std::ofstream configFile(configPath);
    if (!configFile.is_open()) {
        std::cerr << "警告: 无法生成默认配置文件: " << configPath << std::endl;
        return;
    }

    configFile << "# =========================================" << std::endl;
    configFile << "# AIChatServer 默认配置文件" << std::endl;
    configFile << "# 版本: " << VERSION << std::endl;
    configFile << "# =========================================" << std::endl;
    configFile << std::endl;
    configFile << "# 服务器绑定地址" << std::endl;
    configFile << "--host=0.0.0.0" << std::endl;
    configFile << std::endl;
    configFile << "# 服务器绑定端口号" << std::endl;
    configFile << "--port=8080" << std::endl;
    configFile << std::endl;
    configFile << "# 日志级别: TRACE, DEBUG, INFO, WARN, ERROR, CRITICAL" << std::endl;
    configFile << "--log_level=INFO" << std::endl;
    configFile << std::endl;
    configFile << "# 温度值，影响生成文本的随机性，范围 0.0~2.0" << std::endl;
    configFile << "# 值越高回答越随机，值越低回答越确定" << std::endl;
    configFile << "--temperature=0.7" << std::endl;
    configFile << std::endl;
    configFile << "# 最大token数，限制模型生成的最大长度" << std::endl;
    configFile << "--max_tokens=2048" << std::endl;
    configFile << std::endl;
    configFile << "# =========================================" << std::endl;
    configFile << "# Ollama 本地模型配置" << std::endl;
    configFile << "# 默认使用本地部署的 deepseek-r1:1.5b" << std::endl;
    configFile << "# =========================================" << std::endl;
    configFile << "# Ollama模型名称，需要与 Ollama 服务端已拉取的模型名一致" << std::endl;
    configFile << "--ollama_model_name=deepseek-r1:1.5b" << std::endl;
    configFile << std::endl;
    configFile << "# Ollama模型描述，用于在前端展示" << std::endl;
    configFile << "--ollama_model_desc=本地 DeepSeek R1 1.5B 推理模型" << std::endl;
    configFile << std::endl;
    configFile << "# Ollama API服务地址" << std::endl;
    configFile << "--ollama_endpoint=http://127.0.0.1:11434" << std::endl;
    configFile << std::endl;
    configFile << "# =========================================" << std::endl;
    configFile << "# 云端 API 密钥配置 (通过环境变量设置)" << std::endl;
    configFile << "# =========================================" << std::endl;
    configFile << "# DeepSeek API Key: 设置环境变量 deepseek_apikey" << std::endl;
    configFile << "#   示例: export deepseek_apikey=sk-xxxxxx" << std::endl;
    configFile << "#" << std::endl;
    configFile << "# KIMI3 API Key: 设置环境变量 KIMI3_apikey" << std::endl;
    configFile << "#   示例: export KIMI3_apikey=sk-xxxxxx" << std::endl;
    configFile << "#" << std::endl;
    configFile << "# Gemini API Key: 设置环境变量 gemini_apikey" << std::endl;
    configFile << "#   示例: export gemini_apikey=xxxxxx" << std::endl;

    configFile.close();
    std::cout << "已生成默认配置文件: " << configPath << std::endl;
}

// 显示接口说明和使用案例
void showAPIInfo() {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║              AIChatServer - API 接口说明                    ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════════════════════════╝" << std::endl;
    std::cout << std::endl;

    std::cout << "【REST API 接口列表】" << std::endl;
    std::cout << "──────────────────────────────────────────────────────────────" << std::endl;
    std::cout << std::endl;

    std::cout << "1. 获取所有会话列表" << std::endl;
    std::cout << "   方法: GET    路径: /api/sessions" << std::endl;
    std::cout << "   响应: { success, message, data: [{id, model, created_at," << std::endl;
    std::cout << "            updated_at, message_count, first_user_message}] }" << std::endl;
    std::cout << std::endl;

    std::cout << "2. 获取可用模型列表" << std::endl;
    std::cout << "   方法: GET    路径: /api/models" << std::endl;
    std::cout << "   响应: { success, message, data: [{name, desc}] }" << std::endl;
    std::cout << std::endl;

    std::cout << "3. 创建新会话" << std::endl;
    std::cout << "   方法: POST   路径: /api/session" << std::endl;
    std::cout << "   请求: { model: \"模型名称\" }" << std::endl;
    std::cout << "   响应: { success, message, data: {session_id, model} }" << std::endl;
    std::cout << std::endl;

    std::cout << "4. 获取会话历史消息" << std::endl;
    std::cout << "   方法: GET    路径: /api/session/{session_id}/history" << std::endl;
    std::cout << "   响应: { success, message, data: [{id, role, content," << std::endl;
    std::cout << "            timestamp}] }" << std::endl;
    std::cout << std::endl;

    std::cout << "5. 发送消息 (流式响应 SSE)" << std::endl;
    std::cout << "   方法: POST   路径: /api/message/async" << std::endl;
    std::cout << "   请求: { session_id, message }" << std::endl;
    std::cout << "   响应: text/event-stream 格式" << std::endl;
    std::cout << "         data: 正文内容\\n\\n" << std::endl;
    std::cout << "         data: [DONE]\\n\\n" << std::endl;
    std::cout << std::endl;

    std::cout << "6. 删除会话" << std::endl;
    std::cout << "   方法: DELETE 路径: /api/session/{session_id}" << std::endl;
    std::cout << "   响应: { success, message }" << std::endl;
    std::cout << std::endl;

    std::cout << "7. 发送消息 (全量返回)" << std::endl;
    std::cout << "   方法: POST   路径: /api/message" << std::endl;
    std::cout << "   请求: { session_id, message }" << std::endl;
    std::cout << "   响应: { success, message, data: {session_id, response} }" << std::endl;
    std::cout << std::endl;

    std::cout << "【使用案例】" << std::endl;
    std::cout << "──────────────────────────────────────────────────────────────" << std::endl;
    std::cout << std::endl;
    std::cout << "  案例1: 基本启动（使用默认参数）" << std::endl;
    std::cout << "    ./AIChatServer" << std::endl;
    std::cout << std::endl;
    std::cout << "  案例2: 指定端口和日志级别" << std::endl;
    std::cout << "    ./AIChatServer --port=9000 --log_level=DEBUG" << std::endl;
    std::cout << std::endl;
    std::cout << "  案例3: 使用指定配置文件" << std::endl;
    std::cout << "    ./AIChatServer --config_file=/path/to/my_config.conf" << std::endl;
    std::cout << std::endl;
    std::cout << "  案例4: 配置环境变量后启动（以DeepSeek为例）" << std::endl;
    std::cout << "    export deepseek_apikey=sk-xxxxxxxxxxxxxxxx" << std::endl;
    std::cout << "    ./AIChatServer" << std::endl;
    std::cout << std::endl;
    std::cout << "  案例5: 使用Ollama本地模型 (默认已配置 deepseek-r1:1.5b)" << std::endl;
    std::cout << "    # 默认即启用本地 deepseek-r1:1.5b，无需额外参数" << std::endl;
    std::cout << "    ./AIChatServer" << std::endl;
    std::cout << std::endl;
    std::cout << "    # 如需切换为其他 Ollama 模型" << std::endl;
    std::cout << "    ./AIChatServer \\" << std::endl;
    std::cout << "      --ollama_model_name=llama3:8b \\" << std::endl;
    std::cout << "      --ollama_model_desc=\"Llama3 8B 本地模型\" \\" << std::endl;
    std::cout << "      --ollama_endpoint=http://127.0.0.1:11434" << std::endl;
    std::cout << std::endl;
    std::cout << "  案例6: 使用curl测试API" << std::endl;
    std::cout << "    # 获取模型列表" << std::endl;
    std::cout << "    curl http://localhost:8080/api/models" << std::endl;
    std::cout << "    # 创建会话" << std::endl;
    std::cout << "    curl -X POST http://localhost:8080/api/session \\" << std::endl;
    std::cout << "      -H 'Content-Type: application/json' \\" << std::endl;
    std::cout << "      -d '{\"model\":\"deepseek-v4-pro\"}'" << std::endl;
    std::cout << std::endl;
    std::cout << "【支持的模型清单】" << std::endl;
    std::cout << "──────────────────────────────────────────────────────────────" << std::endl;
    std::cout << "  云端模型 (需配置对应环境变量):" << std::endl;
    std::cout << "    - deepseek-v4-pro    (需 export deepseek_apikey=sk-xxx)" << std::endl;
    std::cout << "    - gemini-3.5-flash   (需 export gemini_apikey=xxx)" << std::endl;
    std::cout << "    - kimi-k3            (需 export KIMI3_apikey=sk-xxx)" << std::endl;
    std::cout << "  本地模型 (默认启用，需本地运行 Ollama 服务):" << std::endl;
    std::cout << "    - deepseek-r1:1.5b   (默认 http://127.0.0.1:11434)" << std::endl;
    std::cout << std::endl;
}

// 验证配置参数
bool validateConfig(ai_chat_server::ServerConfig& config) {
    bool hasError = false;

    // 验证温度值
    if (config.temperature < 0.0 || config.temperature > 2.0) {
        ERR("错误: 温度值必须在 0.0 到 2.0 之间，当前值: {}", config.temperature);
        hasError = true;
    }

    // 验证最大token数
    if (config.maxTokens <= 0) {
        ERR("错误: 最大token数必须为正数，当前值: {}", config.maxTokens);
        hasError = true;
    }

    // 检查至少有一个API密钥不为空 或 Ollama配置完整
    bool hasCloudAPI = !config.deepseekAPIKey.empty()
                    || !config.KIMI3APIKey.empty()
                    || !config.geminiAPIKey.empty();

    bool hasOllamaConfig = !config.ollamaModelName.empty()
                        && !config.ollamaModelDesc.empty()
                        && !config.ollamaEndpoint.empty();

    if (!hasCloudAPI && !hasOllamaConfig) {
        ERR("错误: 至少需要提供一个有效的云端API密钥，或配置完整的Ollama本地模型参数");
        ERR("  - 云端API密钥通过环境变量设置: deepseek_apikey / KIMI3_apikey / gemini_apikey");
        ERR("  - Ollama本地模型需同时提供: ollama_model_name, ollama_model_desc, ollama_endpoint");
        hasError = true;
    }

    // 如果Ollama配置有任意一项非空，则必须全部完整
    bool hasPartialOllama = !config.ollamaModelName.empty()
                         || !config.ollamaModelDesc.empty()
                         || !config.ollamaEndpoint.empty();

    if (hasPartialOllama && !hasOllamaConfig) {
        ERR("错误: Ollama配置不完整，必须同时提供 ollama_model_name、ollama_model_desc、ollama_endpoint 三个参数");
        ERR("  当前 ollama_model_name: {}", config.ollamaModelName.empty() ? "(空)" : config.ollamaModelName);
        ERR("  当前 ollama_model_desc: {}", config.ollamaModelDesc.empty() ? "(空)" : config.ollamaModelDesc);
        ERR("  当前 ollama_endpoint:   {}", config.ollamaEndpoint.empty() ? "(空)" : config.ollamaEndpoint);
        hasError = true;
    }

    // 验证端口号范围
    if (config.port <= 0 || config.port > 65535) {
        ERR("错误: 端口号必须在 1~65535 之间，当前值: {}", config.port);
        hasError = true;
    }

    return !hasError;
}

int main(int argc, char** argv) {
    try {
        // =========================================
        // 第一步: 预解析，处理 -h/--help 和 -v/--version
        // =========================================
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "-v" || arg == "--version") {
                std::cout << PROGRAM_NAME << " version " << VERSION << std::endl;
                return 0;
            }
            if (arg == "-h" || arg == "--help") {
                // 让gflags先输出标准帮助
                gflags::SetUsageMessage(PROGRAM_NAME + " - AI聊天服务器\n\n使用方法: ./" + PROGRAM_NAME + " [options]");
                gflags::ShowUsageWithFlagsRestrict(argv[0], "main");
                // 然后输出API说明
                showAPIInfo();
                return 0;
            }
        }

        // =========================================
        // 第二步: 第一次解析命令行参数（拿到 config_file 路径）
        // =========================================
        gflags::SetUsageMessage(PROGRAM_NAME + " - AI聊天服务器\n\n使用方法: ./" + PROGRAM_NAME + " [options]");
        gflags::SetVersionString(VERSION);
        gflags::ParseCommandLineFlags(&argc, &argv, false); // false: 不移除已识别参数，保留给第二次解析

        // =========================================
        // 第三步: 如果配置文件不存在，生成默认配置文件
        // =========================================
        {
            std::ifstream testFile(FLAGS_config_file);
            if (!testFile.good()) {
                generateDefaultConfig(FLAGS_config_file);
            }
        }

        // =========================================
        // 第四步: 设置 flagfile 并重新解析（配置文件 -> 命令行覆盖）
        // =========================================
        {
            std::ifstream file(FLAGS_config_file);
            if (file) {
                gflags::SetCommandLineOption("flagfile", FLAGS_config_file.c_str());
                file.close();
            }
        }
        // 再次解析命令行参数，确保命令行参数优先级高于配置文件
        int argc_copy = argc;
        char** argv_copy = argv;
        gflags::ParseCommandLineFlags(&argc_copy, &argv_copy, true);

        // =========================================
        // 第五步: 构建 ServerConfig
        // =========================================
        ai_chat_server::ServerConfig config;
        config.host = FLAGS_host;
        config.port = FLAGS_port;
        config.logLevel = FLAGS_log_level;
        config.temperature = FLAGS_temperature;
        config.maxTokens = FLAGS_max_tokens;

        // 从环境变量获取API密钥
        config.deepseekAPIKey = getEnvVar("deepseek_apikey");
        config.KIMI3APIKey = getEnvVar("KIMI3_apikey");
        config.geminiAPIKey = getEnvVar("gemini_apikey");
        // 从命令行参数/配置文件获取Ollama配置
        config.ollamaModelName = FLAGS_ollama_model_name;
        config.ollamaModelDesc = FLAGS_ollama_model_desc;
        config.ollamaEndpoint = FLAGS_ollama_endpoint;

        // =========================================
        // 第六步: 验证配置参数
        // =========================================
        if (!validateConfig(config)) {
            std::cerr << "\n配置验证失败，请检查参数设置后重试。" << std::endl;
            std::cerr << "使用 -h 或 --help 查看详细帮助信息。" << std::endl;
            return 1;
        }

        // =========================================
        // 第七步: 设置日志级别并初始化
        // =========================================
        spdlog::level::level_enum logLevel = spdlog::level::info;
        std::string levelUpper = config.logLevel;
        // 转换为大写比较
        for (auto& c : levelUpper) c = std::toupper(c);
        if (levelUpper == "TRACE") logLevel = spdlog::level::trace;
        else if (levelUpper == "DEBUG") logLevel = spdlog::level::debug;
        else if (levelUpper == "INFO") logLevel = spdlog::level::info;
        else if (levelUpper == "WARN" || levelUpper == "WARNING") logLevel = spdlog::level::warn;
        else if (levelUpper == "ERROR") logLevel = spdlog::level::err;
        else if (levelUpper == "CRITICAL") logLevel = spdlog::level::critical;
        else {
            std::cerr << "警告: 未知的日志级别 '" << config.logLevel
                      << "'，将使用默认 INFO 级别" << std::endl;
            logLevel = spdlog::level::info;
        }

        ai_chat_sdk::Logger::init_logger("ChatServer", "stdout", logLevel);

        // =========================================
        // 第八步: 显示当前配置
        // =========================================
        INFO("══════════════════════════════════════════");
        INFO("{} 启动配置 (版本 {})", PROGRAM_NAME, VERSION);
        INFO("══════════════════════════════════════════");
        INFO("  监听地址     : {}", config.host);
        INFO("  监听端口     : {}", config.port);
        INFO("  日志级别     : {}", config.logLevel);
        INFO("  温度值       : {}", config.temperature);
        INFO("  最大Token    : {}", config.maxTokens);
        INFO("  配置文件     : {}", FLAGS_config_file);
        INFO("──────────────────────────────────────────");
        INFO("  DeepSeek Key : {}", (config.deepseekAPIKey.empty() ? "未设置" : "已设置 (" + std::to_string(config.deepseekAPIKey.size()) + " 字符)"));
        INFO("  KIMI3 Key    : {}", (config.KIMI3APIKey.empty() ? "未设置" : "已设置 (" + std::to_string(config.KIMI3APIKey.size()) + " 字符)"));
        INFO("  Gemini Key   : {}", (config.geminiAPIKey.empty() ? "未设置" : "已设置 (" + std::to_string(config.geminiAPIKey.size()) + " 字符)"));
        INFO("──────────────────────────────────────────");
        INFO("  Ollama模型   : {}", (config.ollamaModelName.empty() ? "未设置" : config.ollamaModelName));
        INFO("  Ollama描述   : {}", (config.ollamaModelDesc.empty() ? "未设置" : config.ollamaModelDesc));
        INFO("  Ollama端点   : {}", (config.ollamaEndpoint.empty() ? "未设置" : config.ollamaEndpoint));
        INFO("══════════════════════════════════════════");

        // =========================================
        // 第九步: 创建并启动 ChatServer
        // =========================================
        ai_chat_server::ChatServer server(config);
        if (server.start()) {
            INFO("{} 启动成功!", PROGRAM_NAME);
            INFO("服务访问地址: http://{}:{}", config.host, config.port);
            INFO("前端页面地址: http://{}:{}/index.html", config.host, config.port);

            // 主线程等待，让服务器在单独线程中运行
            while (server.isRunning()) {
                std::this_thread::sleep_for(std::chrono::seconds(100));
            }
        } else {
            ERR("{} 启动失败!", PROGRAM_NAME);
            return 1;
        }

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "发生异常: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "发生未知异常" << std::endl;
        return 1;
    }
}
