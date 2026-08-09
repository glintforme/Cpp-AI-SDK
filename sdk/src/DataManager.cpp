#include "../include/DataManager.h"
#include "../include/util/MyLog.h"
#include <memory>
#include <mutex>
#include <vector>
#include <cstring>

namespace ai_chat_sdk{

    DataManager::DataManager(const std::string &dbName) 
        : _dbName(dbName)
        , _db(nullptr)
    {
        int rc = sqlite3_open(_dbName.c_str(),&_db);
        if(rc != SQLITE_OK){
            ERR("打开数据库失败：{}", sqlite3_errmsg(_db));
        }
        INFO("打开数据库成功：{}", _dbName);

        if(!initDataBase()){
            ERR("初始化数据库表失败");
        }
    }

    DataManager::~DataManager(){
        if(_db){
            sqlite3_close(_db);
        }
    }

    bool DataManager::initDataBase(){
        std::string createSessionTable = R"(
            CREATE TABLE IF NOT EXISTS Session (
                session_id TEXT PRIMARY KEY,
                model_name TEXT NOT NULL,
                create_time INTEGER NOT NULL,
                update_time INTEGER NOT NULL 
            );
        )";

        if(!executeSQL(createSessionTable)){
            return false;
        }
        
        std::string createMessageTable = R"(
            CREATE TABLE IF NOT EXISTS Message (
                message_id TEXT PRIMARY KEY,
                session_id TEXT NOT NULL,
                role TEXT NOT NULL,
                content TEXT NOT NULL,
                timestamp INTEGER NOT NULL,
                FOREIGN KEY (session_id) REFERENCES Session (session_id) ON DELETE CASCADE
            );
        )";

        if(!executeSQL(createMessageTable)){
            return false;
        }
        return true;
    }

    bool DataManager::executeSQL(const std::string &sql){
        if(!_db){
            ERR("数据库连接为空");
            return false;
        }

        char* errMsg = nullptr;
        int rc = sqlite3_exec(_db,sql.c_str(),nullptr,nullptr,&errMsg);
        if(rc != SQLITE_OK){
            ERR("执行SQL语句失败：{}", errMsg ? errMsg : "unknown");
            sqlite3_free(errMsg);
            return false;
        }
        return true;
    }

    bool DataManager::insertSession(const Session &session){
        std::lock_guard<std::mutex> lock(_mutex);
        std::string insertSQL = R"(
            INSERT INTO Session (session_id,model_name,create_time,update_time)
            VALUES (?,?,?,?)
        )";
        
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(_db,insertSQL.c_str(),-1,&stmt,nullptr);
        if(rc != SQLITE_OK){
            ERR("insertSession 准备SQL语句失败：{}", sqlite3_errmsg(_db));
            return false;
        }

        sqlite3_bind_text(stmt,1,session._sessionid.c_str(),-1,SQLITE_STATIC);
        sqlite3_bind_text(stmt,2,session._modelName.c_str(),-1,SQLITE_STATIC);
        sqlite3_bind_int64(stmt,3,static_cast<int64_t>(session._createdAt));
        sqlite3_bind_int64(stmt,4,static_cast<int64_t>(session._updatedAt));

        rc = sqlite3_step(stmt);
        if(rc != SQLITE_DONE){
            ERR("insertSession 执行SQL语句失败：{}", sqlite3_errmsg(_db));
            sqlite3_finalize(stmt);
            return false;
        }
        sqlite3_finalize(stmt);
        INFO("insertSession 插入会话成功：{}", session._sessionid);
        return true;
    }

    std::shared_ptr<Session> DataManager::getSession(const std::string &sessionId) const{
        std::lock_guard<std::mutex> lock(_mutex);
        std::string selectSQL = R"(
            SELECT model_name,create_time,update_time FROM Session WHERE session_id = ?
        )";

        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(_db,selectSQL.c_str(),-1,&stmt,nullptr);
        if(rc != SQLITE_OK){
            ERR("getSession 准备SQL语句失败：{}", sqlite3_errmsg(_db));
            return nullptr;
        }
        sqlite3_bind_text(stmt,1,sessionId.c_str(),-1,SQLITE_STATIC);

        rc = sqlite3_step(stmt);
        if(rc != SQLITE_ROW){
            sqlite3_finalize(stmt);
            return nullptr;
        }

        const char* modelNamePtr = reinterpret_cast<const char*>(sqlite3_column_text(stmt,0));
        std::string modelName = modelNamePtr ? modelNamePtr : "";
        int64_t createTime = sqlite3_column_int64(stmt,1);
        int64_t updateTime = sqlite3_column_int64(stmt,2);
        auto session = std::make_shared<Session>(modelName);
        session->_sessionid = sessionId;
        session->_createdAt = static_cast<std::time_t>(createTime);
        session->_updatedAt = static_cast<std::time_t>(updateTime);
        sqlite3_finalize(stmt);
        INFO("getSession 获取会话成功：{}", sessionId);
        session->_messages = getSessionMessages(sessionId);
        return session;
    }

    bool DataManager::updateSessionTimestamp(const std::string &sessionId,std::time_t timestamp){
        std::lock_guard<std::mutex> lock(_mutex);
        std::string updateSQL = R"(
            UPDATE Session SET update_time = ? WHERE session_id = ?
        )";

        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(_db,updateSQL.c_str(),-1,&stmt,nullptr);
        if(rc != SQLITE_OK){
            ERR("updateSessionTimestamp 准备SQL语句失败：{}", sqlite3_errmsg(_db));
            return false;
        }
        sqlite3_bind_int64(stmt,1,static_cast<int64_t>(timestamp));
        sqlite3_bind_text(stmt,2,sessionId.c_str(),-1,SQLITE_STATIC);

        rc = sqlite3_step(stmt);
        if(rc != SQLITE_DONE){
            ERR("updateSessionTimestamp 执行SQL语句失败：{}", sqlite3_errmsg(_db));
            sqlite3_finalize(stmt);
            return false;
        }
        sqlite3_finalize(stmt);
        INFO("updateSessionTimestamp 更新会话时间戳成功：{}", sessionId);
        return true;
    }

    bool DataManager::deleteSession(const std::string &sessionId){
        std::lock_guard<std::mutex> lock(_mutex);
        std::string deleteSQL = R"(
            DELETE FROM Session WHERE session_id = ?
        )";

        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(_db,deleteSQL.c_str(),-1,&stmt,nullptr);
        if(rc != SQLITE_OK){
            ERR("deleteSession 准备SQL语句失败：{}", sqlite3_errmsg(_db));
            return false;
        }
        sqlite3_bind_text(stmt,1,sessionId.c_str(),-1,SQLITE_STATIC);

        rc = sqlite3_step(stmt);
        if(rc != SQLITE_DONE){
            ERR("deleteSession 执行SQL语句失败：{}", sqlite3_errmsg(_db));
            sqlite3_finalize(stmt);
            return false;
        }
        sqlite3_finalize(stmt);
        INFO("deleteSession 删除会话成功：{}", sessionId);
        return true;
    }

    std::vector<std::string> DataManager::getAllSessionIds() const{
        std::lock_guard<std::mutex> lock(_mutex);
        std::string selectSQL = R"(
            SELECT session_id FROM Session ORDER BY update_time DESC
        )";

        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(_db,selectSQL.c_str(),-1,&stmt,nullptr);
        if(rc != SQLITE_OK){
            ERR("getAllSessionIds 准备SQL语句失败：{}", sqlite3_errmsg(_db));
            return {};
        }

        std::vector<std::string> sessionIds;
        while(sqlite3_step(stmt) == SQLITE_ROW){
            const char* ptr = reinterpret_cast<const char*>(sqlite3_column_text(stmt,0));
            if(ptr) sessionIds.push_back(std::string(ptr));
        }
        sqlite3_finalize(stmt);
        INFO("getAllSessionIds 获取所有会话ID成功：{}", sessionIds.size());
        return sessionIds;
    }

    std::vector<std::shared_ptr<Session>> DataManager::getAllSessions() const{
        std::lock_guard<std::mutex> lock(_mutex);
        std::string selectSQL = R"(
            SELECT session_id, model_name,create_time,update_time FROM Session ORDER BY update_time DESC
        )";

        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(_db,selectSQL.c_str(),-1,&stmt,nullptr);
        if(rc != SQLITE_OK){
            ERR("getAllSessions 准备SQL语句失败：{}", sqlite3_errmsg(_db));
            return {};
        }

        std::vector<std::shared_ptr<Session>> sessions;
        while(sqlite3_step(stmt) == SQLITE_ROW){
            const char* sidPtr = reinterpret_cast<const char*>(sqlite3_column_text(stmt,0));
            const char* mnamePtr = reinterpret_cast<const char*>(sqlite3_column_text(stmt,1));
            std::string sessionId = sidPtr ? sidPtr : "";
            std::string modelName = mnamePtr ? mnamePtr : "";
            int64_t createTime = sqlite3_column_int64(stmt,2);
            int64_t updateTime = sqlite3_column_int64(stmt,3);

            auto session = std::make_shared<Session>(modelName);
            session->_sessionid = sessionId;
            session->_createdAt = static_cast<std::time_t>(createTime);
            session->_updatedAt = static_cast<std::time_t>(updateTime);
            sessions.push_back(session);
        }
        sqlite3_finalize(stmt);
        INFO("getAllSessions 获取所有会话成功：{}", sessions.size());
        return sessions;
    }

    size_t DataManager::getSessionCount() const{
        std::lock_guard<std::mutex> lock(_mutex);
        std::string selectSQL = R"(
            SELECT COUNT(*) FROM Session
        )";

        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(_db,selectSQL.c_str(),-1,&stmt,nullptr);
        if(rc != SQLITE_OK){
            ERR("getSessionCount 准备SQL语句失败：{}", sqlite3_errmsg(_db));
            return 0;
        }

        rc = sqlite3_step(stmt);
        size_t count = 0;
        if(rc == SQLITE_ROW){
            count = static_cast<size_t>(sqlite3_column_int64(stmt,0));
        }
        sqlite3_finalize(stmt);
        INFO("getSessionCount 获取会话总数成功：{}", count);
        return count;
    }

    bool DataManager::deleteAllSessions(){
        std::lock_guard<std::mutex> lock(_mutex);
        std::string deleteSQL = R"(
            DELETE FROM Session
        )";
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(_db,deleteSQL.c_str(),-1,&stmt,nullptr);
        if(rc != SQLITE_OK){
            ERR("deleteAllSessions 准备SQL语句失败：{}", sqlite3_errmsg(_db));
            return false;
        }

        rc = sqlite3_step(stmt);
        if(rc != SQLITE_DONE){
            ERR("deleteAllSessions 执行SQL语句失败：{}", sqlite3_errmsg(_db));
            sqlite3_finalize(stmt);
            return false;
        }
        sqlite3_finalize(stmt);
        INFO("deleteAllSessions 删除所有会话成功");
        return true;
    }

    bool DataManager::insertMessage(const std::string &sessionId,const Message &message){
        std::lock_guard<std::mutex> lock(_mutex);
        std::string insertSQL = R"(
            INSERT INTO Message (message_id, session_id,role, content, timestamp)
            VALUES (?, ?, ?, ?, ?)
        )";
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(_db,insertSQL.c_str(),-1,&stmt,nullptr);
        if(rc != SQLITE_OK){
            ERR("insertMessage 准备SQL语句失败：{}", sqlite3_errmsg(_db));
            return false;
        }

        sqlite3_bind_text(stmt,1,message._id.c_str(),-1,SQLITE_STATIC);
        sqlite3_bind_text(stmt,2,sessionId.c_str(),-1,SQLITE_STATIC);
        sqlite3_bind_text(stmt,3,message._role.c_str(),-1,SQLITE_STATIC);
        sqlite3_bind_text(stmt,4,message._content.c_str(),-1,SQLITE_STATIC);
        sqlite3_bind_int64(stmt,5,static_cast<int64_t>(message._timestamp));

        rc = sqlite3_step(stmt);
        if(rc != SQLITE_DONE){
            ERR("insertMessage 执行SQL语句失败：{}", sqlite3_errmsg(_db));
            sqlite3_finalize(stmt);
            return false;
        }

        std::string updateSQL = R"(
            UPDATE Session SET update_time = ? WHERE session_id = ?
        )";
        
        sqlite3_stmt* updateStmt;
        rc = sqlite3_prepare_v2(_db,updateSQL.c_str(),-1,&updateStmt,nullptr);
        if(rc != SQLITE_OK){
            ERR("insertMessage 准备更新SQL语句失败：{}", sqlite3_errmsg(_db));
            sqlite3_finalize(stmt);
            return false;
        }
        sqlite3_bind_int64(updateStmt,1,static_cast<int64_t>(message._timestamp));
        sqlite3_bind_text(updateStmt,2,sessionId.c_str(),-1,SQLITE_STATIC);
        
        rc = sqlite3_step(updateStmt);
        if(rc != SQLITE_DONE){
            ERR("insertMessage 执行更新SQL语句失败：{}", sqlite3_errmsg(_db));
            sqlite3_finalize(updateStmt);
            sqlite3_finalize(stmt);
            return false;
        }
        sqlite3_finalize(updateStmt);
        sqlite3_finalize(stmt);
        INFO("insertMessage 插入新消息成功");
        return true;
    }

    std::vector<Message> DataManager::getSessionMessages(const std::string &sessionId) const{
        std::lock_guard<std::mutex> lock(_mutex);
        std::string selectSQL = R"(
            SELECT message_id, role, content, timestamp FROM Message WHERE session_id = ? ORDER BY timestamp ASC
        )";
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(_db,selectSQL.c_str(),-1,&stmt,nullptr);
        if(rc != SQLITE_OK){
            ERR("getSessionMessages 准备SQL语句失败：{}", sqlite3_errmsg(_db));
            return {};
        }
        sqlite3_bind_text(stmt,1,sessionId.c_str(),-1,SQLITE_STATIC);

        std::vector<Message> messages;
        while((rc = sqlite3_step(stmt)) == SQLITE_ROW){
            const char* midPtr = reinterpret_cast<const char*>(sqlite3_column_text(stmt,0));
            const char* rolePtr = reinterpret_cast<const char*>(sqlite3_column_text(stmt,1));
            const char* contentPtr = reinterpret_cast<const char*>(sqlite3_column_text(stmt,2));
            std::string role = rolePtr ? rolePtr : "";
            std::string content = contentPtr ? contentPtr : "";
            Message message(role, content);
            if(midPtr) message._id = std::string(midPtr);
            message._timestamp = static_cast<std::time_t>(sqlite3_column_int64(stmt,3));
            messages.push_back(message);
        }
        if(rc != SQLITE_DONE){
            ERR("getSessionMessages 执行SQL语句失败：{}", sqlite3_errmsg(_db));
            sqlite3_finalize(stmt);
            return {};
        }
        sqlite3_finalize(stmt);
        INFO("getSessionMessages 获取会话中的所有消息成功: {}", messages.size());
        return messages;
    }

    bool DataManager::deleteAllMessages(const std::string &sessionId){
        std::lock_guard<std::mutex> lock(_mutex);
        std::string deleteSQL = R"(
            DELETE FROM Message WHERE session_id = ?
        )";
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(_db,deleteSQL.c_str(),-1,&stmt,nullptr);
        if(rc != SQLITE_OK){
            ERR("deleteAllMessages 准备SQL语句失败：{}", sqlite3_errmsg(_db));
            return false;
        }
        sqlite3_bind_text(stmt,1,sessionId.c_str(),-1,SQLITE_STATIC);
        
        rc = sqlite3_step(stmt);
        if(rc != SQLITE_DONE){
            ERR("deleteAllMessages 执行SQL语句失败：{}", sqlite3_errmsg(_db));
            sqlite3_finalize(stmt);
            return false;
        }
        sqlite3_finalize(stmt);
        INFO("deleteAllMessages 删除会话历史记录成功");
        return true;
    }
}
