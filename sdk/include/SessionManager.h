#pragma once
#include <atomic>
#include <unordered_map>
#include <mutex>
#include <memory>
#include "DataManager.h"
#include "common.h"

namespace ai_chat_sdk{

    class SessionManager{
    public:
        SessionManager(const std::string &dbName = "chatDB.db");
        std::string createSession(const std::string &modelName);
        std::shared_ptr<Session> getSession(const std::string &sessionId);
        /**
         * @brief 给会话追加一条消息
         * @param sessionId 会话id
         * @param role      消息角色: "user" 或 "assistant"
         * @param message   消息正文
         */
        bool addMessage(const std::string &sessionId,
                        const std::string &role,
                        const std::string &message);
        std::vector<Message> getHistoryMessages(const std::string &sessionId) const;
        void updateSessionTimestamp(const std::string &sessionId);
        std::vector<std::string> getAllSessionLists() const;
        bool deleteSession(const std::string &sessionId);
        void clearAllSession();
        size_t getSessionCount() const;
    
    private:
        std::string generateSessionId();
        std::string generateMessageId(size_t messageCount);

    private:
        std::unordered_map<std::string,std::shared_ptr<Session>> _sessions;
        mutable std::mutex _mutex;
        std::atomic<int64_t> _sessionCount = {0};
        DataManager _dataManager;
        
    };
};
