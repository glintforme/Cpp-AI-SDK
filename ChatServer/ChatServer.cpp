#include "ChatServer.h"
#include <ai_chat_sdk/util/MyLog.h>
#include <httplib.h>
#include <jsoncpp/json/forwards.h>
#include <jsoncpp/json/value.h>
#include <jsoncpp/json/reader.h>
#include <jsoncpp/json/writer.h>
#include <fstream>
#include <vector>
#include <thread>



namespace ai_chat_server{

ChatServer::ChatServer(const ServerConfig& config){
    _chatSDK = std::make_shared<ai_chat_sdk::ChatSDK>();

    std::vector<std::shared_ptr<ai_chat_sdk::Config>> modelConfigs;

    // DeepSeek
    auto deepseekConfig = std::make_shared<ai_chat_sdk::ApiConfig>();
    deepseekConfig->_modelName = "deepseek-v4-pro";
    deepseekConfig->_apiKey = config.deepseekAPIKey;
    deepseekConfig->_temperature = config.temperature;
    deepseekConfig->_maxTokens = config.maxTokens;
    if (!deepseekConfig->_apiKey.empty()) {
        modelConfigs.push_back(deepseekConfig);
    } else {
        INFO("DeepSeek API Key 未设置，跳过注册 deepseek-v4-pro");
    }

    // KIMI3 (云端模型 kimi-k3)
    auto kimi3Config = std::make_shared<ai_chat_sdk::ApiConfig>();
    kimi3Config->_modelName = "kimi-k3";
    kimi3Config->_apiKey = config.KIMI3APIKey;
    kimi3Config->_temperature = config.temperature;
    kimi3Config->_maxTokens = config.maxTokens;
    if (!kimi3Config->_apiKey.empty()) {
        modelConfigs.push_back(kimi3Config);
    } else {
        INFO("KIMI3 API Key 未设置，跳过注册 kimi-k3");
    }

    // Gemini
    auto geminiConfig = std::make_shared<ai_chat_sdk::ApiConfig>();
    geminiConfig->_modelName = "gemini-3.5-flash";
    geminiConfig->_apiKey = config.geminiAPIKey;
    geminiConfig->_temperature = config.temperature;
    geminiConfig->_maxTokens = config.maxTokens;
    if (!geminiConfig->_apiKey.empty()) {
        modelConfigs.push_back(geminiConfig);
    } else {
        INFO("Gemini API Key 未设置，跳过注册 gemini-3.5-flash");
    }

    // Ollama 本地模型：只有三个参数都完整时才注册，避免空配置导致下游报错
    if (!config.ollamaModelName.empty()
        && !config.ollamaModelDesc.empty()
        && !config.ollamaEndpoint.empty()) {
        auto ollamaConfig = std::make_shared<ai_chat_sdk::OllamaConfig>();
        ollamaConfig->_modelName = config.ollamaModelName;
        ollamaConfig->_modelDesc = config.ollamaModelDesc;
        ollamaConfig->_endpoint = config.ollamaEndpoint;
        ollamaConfig->_temperature = config.temperature;
        ollamaConfig->_maxTokens = config.maxTokens;
        modelConfigs.push_back(ollamaConfig);
        INFO("注册 Ollama 模型: {} -> {}", ollamaConfig->_modelName, ollamaConfig->_endpoint);
    } else {
        if (!config.ollamaModelName.empty() || !config.ollamaModelDesc.empty() || !config.ollamaEndpoint.empty()) {
            WARN("Ollama 配置不完整，已跳过注册 (需要同时配置 name/desc/endpoint)");
        } else {
            INFO("Ollama 未配置，跳过注册");
        }
    }

    INFO("start init ChatSDK models (共 {} 个待注册模型)...", modelConfigs.size());
    if(modelConfigs.empty()){
        ERR("没有任何可用的模型配置！ChatSDK 无法初始化");
        return;
    }
    if(!_chatSDK->initModels(modelConfigs)){
        ERR("ChatSDK init Failed!!!");
        return;
    }
    INFO("ChatSDK models init success!!!");

    // 标记需要代理的模型（目前 Google Gemini 在国内需要翻墙访问）
    _chatSDK->setModelNeedsProxy("gemini-3.5-flash", true);

    // 创建http服务器
    _chatServer = std::make_unique<httplib::Server>();
    if(!_chatServer){
        ERR("ChatServer init Failed!!!");
        return;
    }
}

bool ChatServer::start(){
    if(_isRunning.load()){
        ERR("ChatServer is running!!!");
        return false;
    }

    // 设置CORS预检请求处理（OPTIONS）
    _chatServer->Options(R"(/api/(.*))", [](const httplib::Request& request, httplib::Response& response){
        response.set_header("Access-Control-Allow-Origin", "*");
        response.set_header("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
        response.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
        response.set_header("Access-Control-Max-Age", "3600");
        response.status = 204;
    });

    // 设置路由规则
    setHttpRoutes();

    // 设置静态资源的路径
    // 按顺序尝试多个候选目录，兼容不同运行场景：
    //   1. ${cwd}/www            - 在 build 目录运行，且 cmake 已复制 www 至此
    //   2. ${cwd}/../ChatServer/www - 在 build 目录运行，直接引用源码目录
    //   3. ${cwd}/ChatServer/www   - 在工程根目录运行
    {
        std::vector<std::string> candidates = {
            "./www",
            "../ChatServer/www",
            "./ChatServer/www"
        };
        std::string chosen;
        for (const auto& p : candidates) {
            if (std::ifstream(p + "/index.html").good()) {
                chosen = p;
                break;
            }
        }
        if (chosen.empty()) {
            WARN("未找到前端资源 www 目录！将尝试使用默认路径 ./www，访问前端页面可能失败");
            chosen = "./www";
        } else {
            INFO("挂载前端静态资源目录: {}", chosen);
        }
        _chatServer->set_mount_point("/", chosen.c_str());
    }

    // 为了不卡服务器云不卡主线程，服务器在单独的线程中运行
    std::thread serverThread([this](){
        _chatServer->listen(_config.host, _config.port);
        INFO("ChatServer start on {} :{}", _config.host, _config.port);
    });

    serverThread.detach();
    _isRunning.store(true);
    INFO("ChatServer start success!!!");
    return true;
}

void ChatServer::stop(){
    if(!_isRunning.load()){
        ERR("ChatServer is not running!!!");
        return;
    }

    if(_chatServer){
        _chatServer->stop();
    }

    _isRunning.store(false);
    INFO("ChatServer stop success!!!");
}

bool ChatServer::isRunning() const{
    return _isRunning.load();
}

// 构造响应
std::string ChatServer::buildResponse(const std::string& message, bool success){
    Json::Value responseJson;
    responseJson["success"] = success;
    responseJson["message"] = message;

    // 序列化
    Json::StreamWriterBuilder writerBuilder;
    return Json::writeString(writerBuilder, responseJson);
}

// 处理创建会话请求
void ChatServer::handleCreateSessionRequest(const httplib::Request& request, httplib::Response& response)
{
    response.set_header("Access-Control-Allow-Origin", "*");
    // 获取请求参数，请求参数在请求体
    // 通过反序列化拿到请求体的json格式
    Json::Value requestJson;
    Json::Reader reader;
    if(!reader.parse(request.body, requestJson)){
        std::string errorJsonStr = buildResponse("parse request body failed, json format error");
        response.status = 400; // 客户端发送的请求有语法错误，服务器无法理解或处理该请求
        response.set_content(errorJsonStr, "application/json");
        return;
    }

    // 获取请求参数
    std::string modelName = requestJson.get("model", "deepseek-chat").asString();
    
    // 创建会话
    std::string sessionID = _chatSDK->createSession(modelName);
    if(sessionID.empty()){
        std::string errorJsonStr = buildResponse("create session failed");
        response.status = 500; // 服务器内部错误，无法完成请求
        response.set_content(errorJsonStr, "application/json");
        return;
    }

    // 构建响应体
    Json::Value dataJson;
    dataJson["session_id"] = sessionID;
    dataJson["model"] = modelName;

    Json::Value responseJson;
    responseJson["success"] = true;
    responseJson["message"] = "create session success";
    responseJson["data"] = dataJson;

    // 序列化
    Json::StreamWriterBuilder writerBuilder;
    std::string responseJsonStr = Json::writeString(writerBuilder, responseJson);

    response.status = 200; // 成功
    response.set_content(responseJsonStr, "application/json");
}


// 处理获取会话列表请求
void ChatServer::handleGetSessionListsRequest(const httplib::Request& request, httplib::Response& response)
{
    response.set_header("Access-Control-Allow-Origin", "*");
    // 获取会话列表
    std::vector<std::string> sessionIDs = _chatSDK->getAllSessionsLists();

    // 构建session信息
    Json::Value dataArray(Json::arrayValue);
    for(const auto& sessionID : sessionIDs){
        auto session = _chatSDK->getSession(sessionID);
        if(session){
            Json::Value sessionJson;
            sessionJson["id"] = session->_sessionid;
            sessionJson["model"] = session->_modelName;
            sessionJson["created_at"] = static_cast<int64_t>(session->_createdAt);
            sessionJson["updated_at"] = static_cast<int64_t>(session->_updatedAt);
            sessionJson["message_count"] = session->_messages.size();
            if(!session->_messages.empty()){
                sessionJson["first_user_message"] = session->_messages.front()._content;
            }

            dataArray.append(sessionJson);
        }
    }

    // 构建响应体
    Json::Value responseJson;
    responseJson["success"] = true;
    responseJson["message"] = "get session lists success";
    responseJson["data"] = dataArray;

    // 序列化
    Json::StreamWriterBuilder writerBuilder;
    std::string responseJsonStr = Json::writeString(writerBuilder, responseJson);

    response.status = 200; // 成功
    response.set_content(responseJsonStr, "application/json");
}

// 处理获取模型列表请求
void ChatServer::handleGetModelListsRequest(const httplib::Request& request, httplib::Response& response)
{
    response.set_header("Access-Control-Allow-Origin", "*");
    // 获取支持的模型列表
    auto modelLists = _chatSDK->getAvailableModels();

    // 构建响应体
    Json::Value dataArray(Json::arrayValue);
    for(const auto& modelInfo : modelLists){
        Json::Value modelJson;
        modelJson["name"] = modelInfo._name;
        modelJson["desc"] = modelInfo._desc;
        modelJson["needs_proxy"] = modelInfo._needsProxy;
        dataArray.append(modelJson);
    }

    // 构建响应体
    Json::Value responseJson;
    responseJson["success"] = true;
    responseJson["message"] = "get model lists success";
    responseJson["data"] = dataArray;

    // 序列化
    Json::StreamWriterBuilder writerBuilder;
    std::string responseJsonStr = Json::writeString(writerBuilder, responseJson);

    response.status = 200; // 成功
    response.set_content(responseJsonStr, "application/json");
}

// 代理检测：尝试访问 Google API 端点，判断代理是否可用
void ChatServer::handleProxyCheckRequest(const httplib::Request& request, httplib::Response& response)
{
    response.set_header("Access-Control-Allow-Origin", "*");

    bool proxyAvailable = false;
    try {
        httplib::Client client("https://generativelanguage.googleapis.com");
        client.set_connection_timeout(3, 0);
        client.set_read_timeout(3, 0);
        auto result = client.Get("/");
        // 只要有响应（即使是403/404）也说明网络可达
        proxyAvailable = (result != nullptr);
    } catch (...) {
        proxyAvailable = false;
    }

    Json::Value responseJson;
    responseJson["success"] = true;
    responseJson["proxy_available"] = proxyAvailable;
    responseJson["message"] = proxyAvailable ? "代理可用" : "代理不可用，Gemini 模型需要代理访问";

    Json::StreamWriterBuilder writerBuilder;
    std::string responseJsonStr = Json::writeString(writerBuilder, responseJson);

    response.status = 200;
    response.set_content(responseJsonStr, "application/json");
}

// 处理删除会话请求
void ChatServer::handleDeleteSessionRequest(const httplib::Request& request, httplib::Response& response)
{
    response.set_header("Access-Control-Allow-Origin", "*");
    // 获取会话id，注意：会话id是一个路径参数
    std::string sessionId = request.matches[1];
    
    // 删除会话
    bool ret = _chatSDK->deleteSession(sessionId);
    if(ret){
        std::string errorJsonStr = buildResponse("delete session success", true);
        response.status = 200; 
        response.set_content(errorJsonStr, "application/json");
    }else{
        std::string errorJsonStr = buildResponse("delete session failed, session not found");
        response.status = 404;  // 会话不存在
        response.set_content(errorJsonStr, "application/json");
    }

}

// 处理获取历史消息请求
void ChatServer::handleGetHistoryMessagesRequest(const httplib::Request& request, httplib::Response& response)
{
    response.set_header("Access-Control-Allow-Origin", "*");
    // 获取会话id
    std::string sessionId = request.matches[1];
    // 获取会话
    auto session = _chatSDK->getSession(sessionId);
    if(!session){
        std::string errorJsonStr = buildResponse("session not found");
        response.status = 404;  // 会话不存在
        response.set_content(errorJsonStr, "application/json");
        return;
    }

    // 构建历史消息列表
    Json::Value dataArray(Json::arrayValue);
    for(const auto& message : session->_messages){
        Json::Value messageJson;
        messageJson["id"] = message._id;
        messageJson["role"] = message._role;
        messageJson["content"] = message._content;
        messageJson["timestamp"] = static_cast<int64_t>(message._timestamp);
        dataArray.append(messageJson);
    }

    // 构建响应体
    Json::Value responseJson;
    responseJson["success"] = true;
    responseJson["message"] = "get history messages success";
    responseJson["data"] = dataArray;

    // 序列化
    Json::StreamWriterBuilder writerBuilder;
    std::string responseJsonStr = Json::writeString(writerBuilder, responseJson);

    response.status = 200; // 成功
    response.set_content(responseJsonStr, "application/json");
}


// 处理发送消息请求-全量返回
void ChatServer::handleSendMessageRequest(const httplib::Request& request, httplib::Response& response)
{
    response.set_header("Access-Control-Allow-Origin", "*");
    // 获取请求参数
    Json::Value requestJson;
    Json::Reader reader;
    if(!reader.parse(request.body, requestJson)){
        std::string errorJsonStr = buildResponse("parse request body failed, json format error");
        response.status = 400;  // 解析请求参数失败
        response.set_content(errorJsonStr, "application/json");
        return;
    }

    // 解析请求参数
    std::string sessionId = requestJson["session_id"].asString();
    std::string message = requestJson["message"].asString();
    if(sessionId.empty() || message.empty()){
        std::string errorJsonStr = buildResponse("session_id or message is empty");
        response.status = 400;  // 解析请求参数失败
        response.set_content(errorJsonStr, "application/json");
        return;
    }

    // 代理检测：如果当前模型需要代理，先检查代理是否可用
    auto session = _chatSDK->getSession(sessionId);
    if(session){
        const auto* modelInfo = _chatSDK->getModelInfo(session->_modelName);
        if(modelInfo && modelInfo->_needsProxy){
            bool proxyAvailable = false;
            try {
                httplib::Client client("https://generativelanguage.googleapis.com");
                client.set_connection_timeout(3, 0);
                client.set_read_timeout(3, 0);
                auto result = client.Get("/");
                proxyAvailable = (result != nullptr);
            } catch (...) {
                proxyAvailable = false;
            }
            if(!proxyAvailable){
                std::string errorJsonStr = buildResponse("当前模型需要代理访问，请开启代理后重试", false);
                response.status = 503;
                response.set_content(errorJsonStr, "application/json");
                return;
            }
        }
    }

    // 发送消息
    std::string assistantMessage = _chatSDK->sendMessage(sessionId, message);
    if(assistantMessage.empty()){
        std::string errorJsonStr = buildResponse("Failed to send AI response message");
        response.status = 500;  // 发送消息失败
        response.set_content(errorJsonStr, "application/json");
        return;
    }

    // 构造响应参数
    Json::Value dataJson;
    dataJson["session_id"] = sessionId;
    dataJson["response"] = assistantMessage;
    dataJson["data"]["assistant_message"] = assistantMessage;

    // 构建响应体
    Json::Value responseJson;
    responseJson["success"] = true;
    responseJson["message"] = "send message success";
    responseJson["data"] = dataJson;

    // 序列化
    Json::StreamWriterBuilder writerBuilder;
    std::string responseJsonStr = Json::writeString(writerBuilder, responseJson);

    response.status = 200; // 成功
    response.set_content(responseJsonStr, "application/json");
}



// 处理发送消息请求-增量返回
void ChatServer::handleSendMessageStreamRequest(const httplib::Request& request, httplib::Response& response)
{
    // 获取请求参数
    Json::Value requestJson;
    Json::Reader reader;
    if(!reader.parse(request.body, requestJson)){
        std::string errorJsonStr = buildResponse("parse request body failed, json format error");
        response.status = 400;  // 解析请求参数失败
        response.set_content(errorJsonStr, "application/json");
        return;
    }

    // 解析请求参数
    std::string sessionId = requestJson["session_id"].asString();
    std::string message = requestJson["message"].asString();
    if(sessionId.empty() || message.empty()){
        std::string errorJsonStr = buildResponse("session_id or message is empty");
        response.status = 400;  // 解析请求参数失败
        response.set_content(errorJsonStr, "application/json");
        return;
    }

    // 准备流式响应（立即建立 SSE 通道，避免代理检测等操作阻塞响应头）
    response.status = 200; // 成功
    response.set_header("Cache-Control", "no-cache");              // 不使用缓存，服务器立即将数据发送到网络
    response.set_header("Connection", "keep-alive");               // 保持连接，服务器不会关闭连接
    response.set_header("Access-Control-Allow-Origin", "*");        // 允许跨域请求
    response.set_header("Access-Control-Allow-Headers", "*");      // 允许所有请求头
    
    // set_chunked_content_provider：告诉服务器，响应内从不是一次性发送的，而是分多次逐步发送给客户端，一般用在实时生成响应内容 或者 流式数据传输场景
    // 
    response.set_chunked_content_provider("text/event-stream", [this, sessionId, message](size_t offset, httplib::DataSink& dataSink)->bool{

        auto writeChunk = [&](const std::string& chunk, bool last){ 
            // 将chunk转换为SSE数据格式
            // Json::valueToQuotedString: 对chunk进行Json转换，目的防止chunk中包含一些特殊字符来破坏数据格式，比如：在chunk中包含了两个连续的换行，就会影响SSE数据格式
            std::string sseData = "data: " + Json::valueToQuotedString(chunk.c_str()) + "\n\n";

            // 需要将模型返回的结果 chunk 发送给客户单
            dataSink.write(sseData.c_str(), sseData.size());  // 将数据写入响应流，即立即发送给客户单，该方法不会等待缓冲区满之后发送

            // 处理结束标记
            if(last){
                // 流向响应结束
                std::string doneData = "data: [DONE]\n\n";
                dataSink.write(doneData.c_str(), doneData.size());
                dataSink.done();    // 表示流式响应结束
                return false;       // 不再有后续数据
            }
            return true;
        };
        
        // 先给客户端发送一个空的数据块，避免客户端长时间的等待
        if (!writeChunk("", false)) {
            return false;
        }

        // 代理检测：在 SSE 通道建立之后进行，检测失败则通过 SSE 返回友好错误并结束
        {
            auto session = _chatSDK->getSession(sessionId);
            if(session){
                const auto* modelInfo = _chatSDK->getModelInfo(session->_modelName);
                if(modelInfo && modelInfo->_needsProxy){
                    bool proxyAvailable = false;
                    try {
                        httplib::Client client("https://generativelanguage.googleapis.com");
                        client.set_connection_timeout(3, 0);
                        client.set_read_timeout(3, 0);
                        auto result = client.Get("/");
                        proxyAvailable = (result != nullptr);
                    } catch (...) {
                        proxyAvailable = false;
                    }
                    if(!proxyAvailable){
                        writeChunk("[系统] 当前模型需要代理访问，请开启代理后重试。", false);
                        writeChunk("", true);
                        return false;
                    }
                }
            }
        }
        
        // 发送消息流
        std::string finalResp = _chatSDK->sendMessageStream(sessionId, message, writeChunk);

        // sendMessageStream 返回空字符串意味着上游有错误
        if (finalResp.empty()) {
            ERR("ChatServer: sendMessageStream returned empty, sessionId={}", sessionId);
            // 给前端补一个明确的错误提示数据块 & DONE，让前端不要无限"正在思考"
            std::string errTip = "[系统] 模型未返回响应，可能是历史消息格式错误或网络问题。请刷新页面重试或新建会话。";
            std::string errSse = "data: " + Json::valueToQuotedString(errTip.c_str()) + "\n\n";
            dataSink.write(errSse.c_str(), errSse.size());
            std::string doneData = "data: [DONE]\n\n";
            dataSink.write(doneData.c_str(), doneData.size());
            dataSink.done();
        }

        return false;   // 不再有后续数据
    });
}

// 设置HTTP路由规则
// 注意: httplib 会把带捕获组的路由当作正则表达式，必须使用正确的正则语法
//       更具体的路由必须先注册（例如 /history 路由要比 单纯 session_id 路由先注册）
void ChatServer::setHttpRoutes(){
    // ----- 静态资源 -----
    // 注意：httplib set_mount_point 的路径是相对于 进程当前工作目录 (cwd)
    // CMakeLists 会把 ChatServer/www 复制到 build/www
    // 若希望直接在源码目录运行，也支持尝试读取 ../ChatServer/www

    // ----- 非参数路由 -----
    // 创建会话
    _chatServer->Post("/api/session", [this](const httplib::Request& request, httplib::Response& response){
        handleCreateSessionRequest(request, response);
    });
    // 会话列表
    _chatServer->Get("/api/sessions", [this](const httplib::Request& request, httplib::Response& response){
        handleGetSessionListsRequest(request, response);
    });
    // 模型列表
    _chatServer->Get("/api/models", [this](const httplib::Request& request, httplib::Response& response){
        handleGetModelListsRequest(request, response);
    });
    // 代理检测
    _chatServer->Get("/api/proxy/check", [this](const httplib::Request& request, httplib::Response& response){
        handleProxyCheckRequest(request, response);
    });
    // 发送消息-全量
    _chatServer->Post("/api/message", [this](const httplib::Request& request, httplib::Response& response){
        handleSendMessageRequest(request, response);
    });
    // 发送消息-流式
    _chatServer->Post("/api/message/async", [this](const httplib::Request& request, httplib::Response& response){
        handleSendMessageStreamRequest(request, response);
    });

    // ----- 带参数的正则路由（更具体的在前）-----
    // 获取历史消息：/api/session/{session_id}/history
    _chatServer->Get(R"(/api/session/([^/]+)/history)", [this](const httplib::Request& request, httplib::Response& response){
        handleGetHistoryMessagesRequest(request, response);
    });

    // 删除会话：/api/session/{session_id}
    _chatServer->Delete(R"(/api/session/([^/]+))", [this](const httplib::Request& request, httplib::Response& response){
        handleDeleteSessionRequest(request, response);
    });
}


} // end ai_chat_server