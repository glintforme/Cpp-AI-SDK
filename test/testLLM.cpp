#include <gtest/gtest.h>
#include <istream>
#include <memory>
#include <spdlog/common.h>
#include "../sdk/include/DeepSeekProvider.h"
#include "../sdk/include/GeminiProvider.h"
#include "../sdk/include/KIMI3Provider.h"
#include "../sdk/include/OllamaLLMProvider.h"
#include "../sdk/include/ChatSDK.h"
#include "../sdk/include/util/MyLog.h"
#include <iostream>
#include <string>
#include <vector>

#if 0
TEST(DeepSeekProviderTest, sendMessage){
    auto provider = std::make_shared<ai_chat_sdk::DeepSeekProvider>();
    ASSERT_TRUE(provider != nullptr);

    std::map<std::string, std::string> modelParam;
    modelParam["api_key"] = std::getenv("deepseek_apikey");
    modelParam["endpoint"] = "https://api.deepseek.com";

    provider->initModel(modelParam);
    ASSERT_TRUE(provider->isAvailable());

    std::map<std::string, std::string> requestParam = {
        {"temperature", "0.7"},
        {"max_tokens", "2048"}
    };
    std::vector<ai_chat_sdk::Message> messages;
    messages.push_back({"user", "你是谁？"});

    // 实例化DeepSeekProvider的对象
    // 调用sendMessage方法
    //std::string response = provider->sendMessage(messages, requestParam);

    auto writeChunk = [&](const std::string& chunk, bool last){
        INFO("chunk : {}", chunk);
        if(last){
            INFO("[DONE]");
        } 
    };
    std::string fullData = provider->sendMessageIncrement(messages, requestParam, writeChunk);
    ASSERT_FALSE(fullData.empty());
    INFO("response : {}", fullData);
}



TEST(GeminiProviderTest, sendMessage){
    auto provider = std::make_shared<ai_chat_sdk::GeminiProvider>();
    ASSERT_TRUE(provider != nullptr);

    std::map<std::string, std::string> modelParam;
    modelParam["api_key"] = std::getenv("gemini_apikey");
    modelParam["endpoint"] = "https://generativelanguage.googleapis.com";

    provider->initModel(modelParam);
    ASSERT_TRUE(provider->isAvailable());

    std::map<std::string, std::string> requestParam = {
        {"temperature", "0.7"},
        {"max_tokens", "2048"}
    };
    std::vector<ai_chat_sdk::Message> messages;
    messages.push_back({"user", "你是谁？"});

    // 实例化DeepSeekProvider的对象
    // 调用sendMessage方法
    // std::string fullData = provider->sendMessage(messages, requestParam);
    // ASSERT_FALSE(fullData.empty());

    auto writeChunk = [&](const std::string& chunk, bool last){ 
        INFO("chunk : {}", chunk);
        if(last){
            INFO("[DONE]"); 
        } 
    };
    std::string fullData = provider->sendMessageIncrement(messages, requestParam, writeChunk);
    ASSERT_FALSE(fullData.empty());
    INFO("response : {}", fullData);
}

TEST(KIMI3ProviderTest, sendMessage){
    auto provider = std::make_shared<ai_chat_sdk::KIMI3Provider>();
    ASSERT_TRUE(provider != nullptr);

    std::map<std::string, std::string> modelParam;
    modelParam["api_key"] = std::getenv("KIMI3_apikey");
    modelParam["endpoint"] = "https://api.moonshot.cn";

    provider->initModel(modelParam);
    ASSERT_TRUE(provider->isAvailable());

    std::map<std::string, std::string> requestParam = {
        {"temperature", "1.0"},
        {"max_tokens", "2048"}
    };
    std::vector<ai_chat_sdk::Message> messages;
    messages.push_back({"user", "你是谁？"});

    // 实例化DeepSeekProvider的对象
    // 调用sendMessage方法
    // std::string fullData = provider->sendMessage(messages, requestParam);
    // ASSERT_FALSE(fullData.empty());

    auto writeChunk = [&](const std::string& chunk, bool last){ 
        INFO("chunk : {}", chunk);
        if(last){
            INFO("[DONE]"); 
        } 
    };
    std::string fullData = provider->sendMessageIncrement(messages, requestParam, writeChunk);
    ASSERT_FALSE(fullData.empty());
    INFO("response : {}", fullData);
}

TEST(OllamaLLMProviderTest, sendMessage){
    auto provider = std::make_shared<ai_chat_sdk::OllamaLLMProvider>();
    ASSERT_TRUE(provider != nullptr);

    std::map<std::string, std::string> modelParam;
    modelParam["model_name"] = "deepseek-r1:1.5b";
    modelParam["model_desc"] = "本地部署deepseek-r1:1.5b模型，采用专家混合架构，专注于深度理解与推理";
    modelParam["endpoint"] = "http://localhost:11434";

    provider->initModel(modelParam);
    ASSERT_TRUE(provider->isAvailable());

    std::map<std::string, std::string> requestParam = {
        {"temperature", "0.7"},
        {"max_tokens", "2048"}
    };
    std::vector<ai_chat_sdk::Message> messages;
    messages.push_back({"user", "你是谁？"});

    // 实例化DeepSeekProvider的对象
    // 调用sendMessage方法
    // std::string fullData = provider->sendMessage(messages, requestParam);
    // ASSERT_FALSE(fullData.empty());

    auto writeChunk = [&](const std::string& chunk, bool last){ 
        INFO("chunk : {}", chunk);
        if(last){
            INFO("[DONE]"); 
        } 
    };
    std::string fullData = provider->sendMessageIncrement(messages, requestParam, writeChunk);
    ASSERT_FALSE(fullData.empty());
    INFO("response : {}", fullData);
}
#endif

// 测试ChatSDK
TEST(ChatSDKTest, sendMessage){
    auto sdk = std::make_shared<ai_chat_sdk::ChatSDK>();
    ASSERT_TRUE(sdk != nullptr);

    // 配置支持的模型参数：云模型-deepseek-v4-pro kimi3 gemini-3.5-flash   Ollama本地接入deepseek-r1:1.5b
    // deepseek-v4-pro
    auto deepseekConfig = std::make_shared<ai_chat_sdk::ApiConfig>();
    ASSERT_TRUE(deepseekConfig != nullptr);
    deepseekConfig->_modelName = "deepseek-v4-pro";
    deepseekConfig->_apiKey = std::getenv("deepseek_apikey");
    ASSERT_FALSE(deepseekConfig->_apiKey.empty());
    deepseekConfig->_temperature = 0.7;
    deepseekConfig->_maxTokens = 2048;

    // KIMI3
    auto KIMI3Config = std::make_shared<ai_chat_sdk::ApiConfig>();
    ASSERT_TRUE(KIMI3Config != nullptr);
    KIMI3Config->_modelName = "kimi-k3";
    KIMI3Config->_apiKey = std::getenv("KIMI3_apikey");
    ASSERT_FALSE(KIMI3Config->_apiKey.empty());
    KIMI3Config->_temperature = 1.0;
    KIMI3Config->_maxTokens = 2048;

    // gemini-3.5-flash
    auto geminiConfig = std::make_shared<ai_chat_sdk::ApiConfig>();
    ASSERT_TRUE(geminiConfig != nullptr);
    geminiConfig->_modelName = "gemini-3.5-flash";
    geminiConfig->_apiKey = std::getenv("gemini_apikey");
    ASSERT_FALSE(geminiConfig->_apiKey.empty());
    geminiConfig->_temperature = 0.7;
    geminiConfig->_maxTokens = 2048;

    // Ollama本地接入deepseek-r1:1.5b
    auto ollamaConfig = std::make_shared<ai_chat_sdk::OllamaConfig>();
    ASSERT_TRUE(ollamaConfig != nullptr);
    ollamaConfig->_modelName = "deepseek-r1:1.5b";
    ollamaConfig->_modelDesc = "本地部署deepseek-r1:1.5b模型，采用专家混合架构，专注于深度理解与推理";
    ollamaConfig->_endpoint = "http://localhost:11434";
    ollamaConfig->_temperature = 0.7;
    ollamaConfig->_maxTokens = 2048;

    std::vector<std::shared_ptr<ai_chat_sdk::Config>> modelConfigs = {
        deepseekConfig, KIMI3Config, geminiConfig, ollamaConfig
    };

    sdk->initModels(modelConfigs);

    // 创建会话
    auto sessionId = sdk->createSession(geminiConfig->_modelName);   //在这里切换模型apiconfig
    ASSERT_FALSE(sessionId.empty());

    std::string message;
    std::cout<<">>> ";
    std::getline(std::cin, message);
    auto response = sdk->sendMessage(sessionId, message);
    ASSERT_FALSE(response.empty());

    std::cout<<">>> ";
    std::getline(std::cin, message);
    auto response2 = sdk->sendMessage(sessionId, message);
    ASSERT_FALSE(response2.empty());

    // 获取会话历史消息
    auto messages = sdk->_sessionManager.getHistoryMessages(sessionId);
    for(const auto& msg : messages){
        std::cout<<msg._role<<": "<<msg._content<<std::endl;
    }
    ASSERT_FALSE(messages.empty());
}

int main(int argc, char **argv) {
    // 初始化spdlog日志库
    ai_chat_sdk::Logger::init_logger("testLLM", "stdout", spdlog::level::debug);

    // 初始化gtest库
    testing::InitGoogleTest(&argc, argv);

    // 执行所有的测试用例
    return RUN_ALL_TESTS();
}
