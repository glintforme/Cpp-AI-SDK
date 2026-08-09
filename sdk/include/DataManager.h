#pragma once

#include <sqlite3.h>
#include <memory>
#include <mutex>
#include <string>
#include "common.h"

namespace ai_chat_sdk{
    class DataManager{
    public:
        DataManager(const std::string &dbName);
        ~DataManager();

        bool insertSession(const Session &session);
        std::shared_ptr<Session> getSession(const std::string &sessionId) const;
        bool updateSessionTimestamp(const std::string &sessionId,std::time_t timestamp);
        bool deleteSession(const std::string &sessionId);
        std::vector<std::string> getAllSessionIds() const;
        std::vector<std::shared_ptr<Session>> getAllSessions() const;
        bool deleteAllSessions();
        size_t getSessionCount() const;

        bool insertMessage(const std::string &sessionId,const Message &message);
        std::vector<Message> getSessionMessages(const std::string &sessionId) const;
        bool deleteAllMessages(const std::string &sessionId);
        
        
    private:
        bool initDataBase();
        bool executeSQL(const std::string &sql);
    private:
        sqlite3 *_db = nullptr;
        mutable std::mutex _mutex;
        std::string _dbName;
    };
};
