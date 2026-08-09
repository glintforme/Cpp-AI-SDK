#include "../include/GeminiProvider.h"
#include "../include/util/MyLog.h"

#include <httplib.h>
#include <jsoncpp/json/json.h>
#include <sstream>

namespace ai_chat_sdk{

    bool GeminiProvider::initModel(const std::map<std::string,std::string> &model_config){
        auto it = model_config.find("api_key");
        if(it == model_config.end()){
            ERR("GeminiProvider::initModel: api_Key not found in model_config");
            return false;
        }
        _api_key = it->second;

        it = model_config.find("endpoint");
        if(it == model_config.end()){
            // 注意: httplib::Client 只使用 scheme/host/port,会忽略 URL 中的路径部分,
            // 因此版本前缀 /v1beta/openai 必须写在请求 path 中,不能放在 endpoint 里
            _endpoint = "https://generativelanguage.googleapis.com";
        }
        else{
            _endpoint = it->second;
        }
        
        _isAvailable = true;
        INFO("GeminiProvider::initModel: init model success, endpoint: {}", _endpoint);
        return true;
    }

    bool GeminiProvider::isAvailable() const{
        return _isAvailable;
    }

    std::string GeminiProvider::getModelName() const{
        return "gemini-3.5-flash";
    }

    std::string GeminiProvider::getModelDesc() const{
        return "Gemini 3.5 Flash,国外云端大模型-需要注意梯子问题";
    }

    std::string GeminiProvider::sendMessage(const std::vector<Message> &messages,const std::map<std::string,std::string> &request_param){
        if(!_isAvailable){
            ERR("GeminiProvider::sendMessage: model is not available");
            return "";
        }

        float temperature = 0.7f;
        int max_tokens = 2048;
        if(request_param.find("temperature") != request_param.end()){
            temperature = std::stof(request_param.at("temperature"));
        }
        if(request_param.find("max_tokens") != request_param.end()){
            max_tokens = std::stoi(request_param.at("max_tokens"));
        }
        
        Json::Value messageArray(Json::arrayValue);
        for(const auto &message : messages){
            Json::Value messageObj(Json::objectValue);
            messageObj["role"] = message._role;
            messageObj["content"] = message._content;
            messageArray.append(messageObj);
        }

        Json::Value requestBody(Json::objectValue);
        requestBody["model"] = getModelName();
        requestBody["messages"] = messageArray;
        requestBody["temperature"] = temperature;
        requestBody["max_tokens"] = max_tokens;

        Json::StreamWriterBuilder writerBuilder;
        writerBuilder["indentation"] = "";
        std::string requestBodyStr = Json::writeString(writerBuilder, requestBody);

        httplib::Client client(_endpoint);
        client.set_connection_timeout(30,0);
        client.set_read_timeout(60,0);

        httplib::Headers headers = {
            {"Authorization", "Bearer " + _api_key},
            {"Content-Type", "application/json"},
            {"Accept", "application/json"},
        };
        
        auto response = client.Post("/v1beta/openai/chat/completions", headers, requestBodyStr, "application/json");
        if(!response){
            ERR("GeminiProvider::sendMessage: post request failed, error: {}", httplib::to_string(response.error()));
            return "";
        }

        INFO("GeminiProvider::sendMessage: response status: {}", response->status);
        INFO("GeminiProvider::sendMessage: response body: {}", response->body);

        if(response->status != 200){
            ERR("GeminiProvider::sendMessage: response status not 200, status: {}", response->status);
            return "";
        }

        Json::CharReaderBuilder readerBuilder;
        Json::Value responseBody;
        std::istringstream responseStream(response->body);
        std::string jsonError;
        if(!Json::parseFromStream(readerBuilder, responseStream, &responseBody, &jsonError)){
            ERR("GeminiProvider::sendMessage: parse response body failed, error: {}", jsonError);
            return "";
        }

        if(responseBody.isMember("choices") && responseBody["choices"].isArray() && !responseBody["choices"].empty()){
            Json::Value choice = responseBody["choices"][0];
            if(choice.isMember("message") &&  choice["message"].isMember("content")){
                std::string reply = choice["message"]["content"].asString();
                INFO("GeminiProvider::sendMessage: reply: {}", reply);
                return reply;
            }
        }

        ERR("GeminiProvider::sendMessage: Invalid response body from Gemini API");
        return "";
    }

    std::string GeminiProvider::sendMessageIncrement(const std::vector<Message> &messages,const std::map<std::string,std::string> &request_param,std::function<void(const std::string &,bool)> callback){
        if(!_isAvailable){
            ERR("GeminiProvider::sendMessageIncrement: model is not available");
            return "";
        }

        float temperature = 0.7f;
        int max_tokens = 2048;
        if(request_param.find("temperature") != request_param.end()){
            temperature = std::stof(request_param.at("temperature"));
        }
        if(request_param.find("max_tokens") != request_param.end()){
            max_tokens = std::stoi(request_param.at("max_tokens"));
        }

        Json::Value messageArray(Json::arrayValue);
        for(const auto& message : messages){
            Json::Value messageObj(Json::objectValue);
            messageObj["role"] = message._role;
            messageObj["content"] = message._content;
            messageArray.append(messageObj);
        }

        Json::Value requestBody(Json::objectValue);
        requestBody["model"] = getModelName();
        requestBody["messages"] = messageArray;
        requestBody["temperature"] = temperature;
        requestBody["max_tokens"] = max_tokens;
        requestBody["stream"] = true;

        Json::StreamWriterBuilder writerBuilder;
        writerBuilder["indentation"] = "";
        std::string requestBodyStr = Json::writeString(writerBuilder, requestBody);
        INFO("GeminiProvider::sendMessageIncrement: stream request messages count: {}, model={}",
             messageArray.size(), getModelName());

        httplib::Client client(_endpoint);
        client.set_connection_timeout(30,0);
        client.set_read_timeout(300,0);

        httplib::Headers headers = {
            {"Authorization", "Bearer " + _api_key},
            {"Content-Type", "application/json"},
            {"Accept", "text/event-stream"},
        };

        std::string buffer;
        bool gotError = false;
        std::string errorMsg;
        int statusCode = 0;
        bool streamFinish = false;
        std::string fullData;

        httplib::Request request;
        request.path = "/v1beta/openai/chat/completions";
        request.method = "POST";
        request.body = requestBodyStr;
        request.headers = headers;
        request.response_handler = [&](const httplib::Response &response){
            statusCode = response.status;
            if(statusCode != 200){
                gotError = true;
                errorMsg = "HTTP status code: " + std::to_string(statusCode)
                         + ", body(前400): " + response.body.substr(0, 400);
                return false;
            }
            return true;
        };

        request.content_receiver = [&](const char* data, size_t dataLength, size_t offset, size_t totalLength){
            (void)offset;
            (void)totalLength;
            if(gotError){
                return false;
            }
            buffer.append(data, dataLength);
            size_t pos = 0;
            while((pos = buffer.find("\n\n")) != std::string::npos){
                std::string line = buffer.substr(0, pos);
                buffer.erase(0, pos + 2);
                if(line.empty() || line[0] == ':'){
                    continue;
                }

                // 按工程约定：比较前 5 个字符与 "data:"（而不是 6 长度的 "data: "）
                if(line.size() < 5 || line.compare(0, 5, "data:") != 0){
                    continue;
                }
                std::string dataStr = line.substr(5);
                // 跳过冒号后可选的单个空格
                if(!dataStr.empty() && dataStr[0] == ' '){
                    dataStr.erase(0, 1);
                }

                if(dataStr == "[DONE]"){
                    callback("", true);
                    streamFinish = true;
                    return true;
                }
                
                Json::Value chunk;
                Json::CharReaderBuilder reader;
                std::string error;
                std::istringstream ss(dataStr);
                if(Json::parseFromStream(reader, ss, &chunk, &error)){
                    if(chunk.isMember("choices") && chunk["choices"].isArray() && !chunk["choices"].empty()){
                        Json::Value choice = chunk["choices"][0];
                        if(choice.isMember("delta") && choice["delta"].isObject()){
                            std::string deltaContent;
                            if(choice["delta"].isMember("content") && choice["delta"]["content"].isString()){
                                deltaContent = choice["delta"]["content"].asString();
                            }
                            if(!deltaContent.empty()){
                                fullData += deltaContent;
                                callback(deltaContent, false);
                            }
                        }
                    }
                }
                else{
                    WARN("GeminiProvider::sendMessageIncrement: failed to parse json chunk, error: {}, raw(前150字): {}",
                         error, dataStr.substr(0, 150));
                }
            }
            return true;
        };
        
        auto res = client.send(request);
        if(!res){
            ERR("GeminiProvider::sendMessageIncrement: request failed, error={}, httpStatus={}, msg={}",
                httplib::to_string(res.error()), statusCode, errorMsg);
            return "";
        }

        if(!streamFinish){
            WARN("GeminiProvider stream ended without [DONE] marker");
            callback("", true);
        }
        INFO("GeminiProvider::sendMessageIncrement done, response size: {}", fullData.size());
        return fullData;
    }

}   // end ai_chat_sdk
