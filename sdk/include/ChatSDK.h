#pragma once
#include "common.h"
#include "SessionManager.h"
#include "LLMManager.h"



namespace ai_chat_sdk{
    class ChatSDK{
    public:
        bool initModels(const std::vector<std::shared_ptr<Config>> &configs);
        std::string createSession(const std::string &modelName);
        std::shared_ptr<Session> getSession(const std::string &sessionId) ;
        std::vector<std::string> getAllSessionsLists() const;
        bool deleteSession(const std::string &sessionId);
        std::vector<ModelInfo> getAvailableModels() const;
        void setModelNeedsProxy(const std::string& modelName, bool needsProxy);
        const ModelInfo* getModelInfo(const std::string& modelName) const;

        std::string sendMessage(const std::string &sessionId,const std::string &message);
        std::string sendMessageStream(const std::string &sessionId,const std::string &message,std::function<void(const std::string &,bool)> callback);

    private:
        void registerAllProvider(const std::vector<std::shared_ptr<Config>> &configs);
        void initProviders(const std::vector<std::shared_ptr<Config>> &configs);
        bool initAPIModelProviders(const std::string &modelName,const std::shared_ptr<ApiConfig> &apiConfig);
        bool initOllamaModelProviders(const std::string &modelName,const std::shared_ptr<OllamaConfig> &ollamaConfig);
    private:
        bool _initialized = false;
        std::unordered_map<std::string,std::shared_ptr<Config>> _modelConfigs;
        LLMManager _llmManager;
    public:
        SessionManager _sessionManager;

    };
};
