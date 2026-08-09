#include "../include/SessionManager.h"
#include "../include/util/MyLog.h"
#include <iomanip>
#include <sstream>
#include <vector>
#include <algorithm>

namespace ai_chat_sdk{

    SessionManager::SessionManager(const std::string &dbName)
        :_dataManager(dbName)
    {
        auto sessions = _dataManager.getAllSessions();
        for(auto &session : sessions){
            _sessions[session->_sessionid] = session;
        }
    }

    std::string SessionManager::generateSessionId(){
        _sessionCount.fetch_add(1);
        std::time_t now = std::time(nullptr);

        std::ostringstream oss;
        oss << "session_" << now << "_" << std::setw(8) << std::setfill('0') << _sessionCount.load();
        return oss.str();
    }

    std::string SessionManager::generateMessageId(size_t messageCount){
        std::time_t now = std::time(nullptr);

        std::ostringstream oss;
        oss << "msg_" << now << "_" << std::setw(10) << std::setfill('0') << messageCount;
        return oss.str();
    }

    std::string SessionManager::createSession(const std::string &modelName){
        std::lock_guard<std::mutex> lock(_mutex);
        std::string sessionId = generateSessionId();

        auto session = std::make_shared<Session>(modelName);
        session->_sessionid = sessionId;
        session->_createdAt = std::time(nullptr);
        session->_updatedAt = session->_createdAt;

        _sessions[sessionId] = session;
        _dataManager.insertSession(*session);
        return sessionId;
    }

    std::shared_ptr<Session> SessionManager::getSession(const std::string &sessionId){
        {
            std::lock_guard<std::mutex> lock(_mutex);
            auto it = _sessions.find(sessionId);
            if(it != _sessions.end()){
                it->second->_messages = _dataManager.getSessionMessages(sessionId);
                return it->second;
            }
        }
        auto session = _dataManager.getSession(sessionId);
        if(session){
            std::lock_guard<std::mutex> lock(_mutex);
            auto it = _sessions.find(sessionId);
            if(it == _sessions.end()){
                _sessions[sessionId] = session;
            }
            return session;
        }
        WARN("sessionId = {} not found", sessionId);
        return nullptr;
    }

    bool SessionManager::addMessage(const std::string &sessionId,
                                    const std::string &role,
                                    const std::string &message){
        std::shared_ptr<Session> session;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            auto it = _sessions.find(sessionId);
            if(it == _sessions.end()){
                ERR("SessionManager::addMessage: sessionId={} not found", sessionId);
                return false;
            }
            session = it->second;
        }

        // 角色合法性
        std::string realRole = role;
        if (realRole != "user" && realRole != "assistant" && realRole != "system") {
            WARN("SessionManager::addMessage: unknown role '{}', fallback to 'user'", realRole);
            realRole = "user";
        }
        
        Message msg(realRole, message);
        msg._id = generateMessageId(session->_messages.size() + 1);
        msg._timestamp = std::time(nullptr);
        INFO("SessionManager::addMessage: session={}, role={}, content(前50字)={}, ts={}",
             sessionId, realRole,
             message.size() > 50 ? message.substr(0, 50) + "..." : message,
             msg._timestamp);

        {
            std::lock_guard<std::mutex> lock(_mutex);
            session->_messages.push_back(msg);
            session->_updatedAt = msg._timestamp;
        }
        _dataManager.insertMessage(sessionId, msg);
        _dataManager.updateSessionTimestamp(sessionId, session->_updatedAt);
        INFO("SessionManager::addMessage success: session={}", sessionId);
        return true;
    }

    std::vector<Message> SessionManager::getHistoryMessages(const std::string &sessionId) const{
        {
            std::lock_guard<std::mutex> lock(_mutex);
            auto it = _sessions.find(sessionId);
            if(it != _sessions.end()){
                return it->second->_messages;
            }   
        }
        return _dataManager.getSessionMessages(sessionId); 
    }

    void SessionManager::updateSessionTimestamp(const std::string &sessionId){
        std::time_t now = 0;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            auto it = _sessions.find(sessionId);
            if(it != _sessions.end()){
                it->second->_updatedAt = std::time(nullptr);
                now = it->second->_updatedAt;
            }
        }
        if(now != 0){
            _dataManager.updateSessionTimestamp(sessionId, now);
        }
    }

    std::vector<std::string> SessionManager::getAllSessionLists() const{
        auto sessions = _dataManager.getAllSessions();

        std::lock_guard<std::mutex> lock(_mutex);
        std::vector<std::pair<std::time_t,std::shared_ptr<Session>>> temp;
        temp.reserve(_sessions.size() + sessions.size());
        for(const auto &pair : _sessions){
            temp.emplace_back(pair.second->_updatedAt, pair.second);
        }
        for(const auto &session : sessions){
            if(_sessions.find(session->_sessionid) == _sessions.end()){
                temp.emplace_back(session->_updatedAt, session);
            }
        }
        
        std::sort(temp.begin(),temp.end(),[](const auto &a,const auto &b){
            return a.first > b.first;
        });
        
        std::vector<std::string> sessionIds;
        for(const auto &pair : temp){
            sessionIds.push_back(pair.second->_sessionid);
        }
        
        return sessionIds;
    }

    bool SessionManager::deleteSession(const std::string &sessionId){
        {
            std::lock_guard<std::mutex> lock(_mutex);
            auto it = _sessions.find(sessionId);
            if(it == _sessions.end()){
                return false;
            }
            _sessions.erase(it);
        }
        _dataManager.deleteSession(sessionId);
        return true;
    }

    void SessionManager::clearAllSession(){
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _sessions.clear();
        }
        _dataManager.deleteAllSessions();
    }

    size_t SessionManager::getSessionCount() const{
        std::lock_guard<std::mutex> lock(_mutex);
        return _sessions.size();
    }
        
}
