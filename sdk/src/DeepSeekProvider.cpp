#include "../include/DeepSeekProvider.h"
#include "../include/util/MyLog.h"
#include <httplib.h>
#include <jsoncpp/json/json.h>
#include <sstream>

namespace ai_chat_sdk{

    bool DeepSeekProvider::initModel(const std::map<std::string,std::string> &model_config){
        auto it = model_config.find("api_key");
        if(it == model_config.end()){
            ERR("DeepSeekProvider: api_key is empty");
            return false;
        }
        else{
            _api_key = it->second;
        }

        it = model_config.find("base_url");
        if(it != model_config.end()){
            _endpoint = it->second;
        }
        else{
            _endpoint = "https://api.deepseek.com";
        }

        _isAvailable = true;
        INFO("DeepSeekProvider init success with endpoint : {}", _endpoint);
        return true;
    }

    bool DeepSeekProvider::isAvailable() const{
        return _isAvailable;
    }

    std::string DeepSeekProvider::getModelName() const{
        return "deepseek-v4-pro";
    }

    std::string DeepSeekProvider::getModelDesc() const{
        return "国内顶级大模型之DeepSeek-v4-pro为你服务!";
    }

    std::string DeepSeekProvider::sendMessage(const std::vector<Message> &messages,const std::map<std::string,std::string> &request_param){
        if(!isAvailable()){
            ERR("DeepSeekProvider: model is not available");
            return "";
        }

        double temperature = 0.7;
        int maxTokens = 2048;
        if(request_param.find("temperature") != request_param.end()){
            temperature = std::stod(request_param.at("temperature"));
        }
        if(request_param.find("max_tokens") != request_param.end()){
            maxTokens = std::stoi(request_param.at("max_tokens"));
        }

        Json::Value messageArray(Json::arrayValue);
        for(const auto &message : messages){
            Json::Value messageObject;
            messageObject["role"] = message._role;
            messageObject["content"] = message._content;
            messageArray.append(messageObject);
        }

        Json::Value requestBody;
        requestBody["model"] = getModelName();
        requestBody["messages"] = messageArray;
        requestBody["temperature"] = temperature;
        requestBody["max_tokens"] = maxTokens;

        Json::StreamWriterBuilder writerBuilder;
        writerBuilder["indentation"] = "";
        std::string requestBodyStr = Json::writeString(writerBuilder,requestBody);
        INFO("DeepSeekProvider: request messages count: {}, request body: {}", messageArray.size(), requestBodyStr);

        httplib::Client client(_endpoint.c_str());
        client.set_connection_timeout(30,0);
        client.set_read_timeout(120,0);

        httplib::Headers headers={
            {"Authorization", "Bearer " + _api_key},
            {"Content-Type", "application/json"}
        };

        auto response = client.Post("/v1/chat/completions",headers,requestBodyStr,"application/json");
        if(!response){
            ERR("DeepSeekProvider sendMessage POST request failed: {}", httplib::to_string(response.error()));
            return "";
        }
        INFO("DeepSeekProvider sendMessage POST status code: {}", response->status);

        if(response->status != 200){
            ERR("DeepSeekProvider sendMessage HTTP {} body (前400字): {}",
                response->status,
                response->body.size() > 400 ? response->body.substr(0,400)+"..." : response->body);
            return "";
        }

        Json::Value responseBody;
        Json::CharReaderBuilder readerBuilder;
        std::string parseError;
        std::istringstream responseStream(response->body);
        if(Json::parseFromStream(readerBuilder,responseStream, &responseBody, &parseError)){
            if(responseBody.isMember("choices") && responseBody["choices"].isArray() && !responseBody["choices"].empty()){
                auto choice = responseBody["choices"][0];
                if(choice.isMember("message") && choice["message"].isMember("content")){
                    std::string replyContent =  choice["message"]["content"].asString();
                    INFO("DeepSeekProvider response text (前200字): {}",
                         replyContent.size() > 200 ? replyContent.substr(0,200)+"..." : replyContent);
                    return replyContent;
                }
            }
        }

        ERR("DeepSeekProvider sendMessage parse failed, parseError: {}", parseError);
        return "";
    }

    std::string DeepSeekProvider::sendMessageIncrement(const std::vector<Message> &messages,const std::map<std::string,std::string> &request_param,std::function<void(const std::string &,bool)> callback){
        if(!isAvailable()){
            ERR("DeepSeekProvider: model is not available");
            return "";
        }

        double temperature = 0.7;
        int maxTokens = 2048;
        if(request_param.find("temperature") != request_param.end()){
            temperature = std::stod(request_param.at("temperature"));
        }
        if(request_param.find("max_tokens") != request_param.end()){
            maxTokens = std::stoi(request_param.at("max_tokens"));
        }

        Json::Value messageArray(Json::arrayValue);
        for(const auto &message : messages){
            Json::Value messageObject;
            messageObject["role"] = message._role;
            messageObject["content"] = message._content;
            messageArray.append(messageObject);
        }

        Json::Value requestBody;
        requestBody["model"] = getModelName();
        requestBody["messages"] = messageArray;
        requestBody["temperature"] = temperature;
        requestBody["max_tokens"] = maxTokens;
        requestBody["stream"] = true;

        Json::StreamWriterBuilder writerBuilder;
        writerBuilder["indentation"] = "";
        std::string requestBodyStr = Json::writeString(writerBuilder,requestBody);
        INFO("DeepSeekProvider: stream request messages count: {}, model={}, temperature={}, maxTokens={}",
             messageArray.size(), getModelName(), temperature, maxTokens);

        httplib::Client client(_endpoint.c_str());
        client.set_connection_timeout(30,0);
        client.set_read_timeout(300,0);

        httplib::Headers headers={
            {"Authorization", "Bearer " + _api_key},
            {"Content-Type", "application/json"},
            {"Accept", "text/event-stream"}
        };

        std::string buffer;
        bool gotError = false;
        std::string errorMsg;
        int statusCode = 0;
        bool streamFinish = false;
        std::string fullResponse;

        httplib::Request req;
        req.method = "POST";
        req.path = "/v1/chat/completions";
        req.body = requestBodyStr;
        req.headers = headers;
        req.response_handler = [&](const httplib::Response &res){
            statusCode = res.status;
            if(res.status != 200){
                gotError = true;
                errorMsg = "HTTP status code: " + std::to_string(res.status)
                         + ", body(前400): " + res.body.substr(0, 400);
                return false;
            }
            return true;
        };

        req.content_receiver = [&](const char* data, size_t len, size_t offset, size_t totalLength){
            (void)offset;
            (void)totalLength;
            if(gotError){
                return false;
            }

            buffer.append(data, len);

            size_t pos;
            while((pos = buffer.find("\n\n")) != std::string::npos){
                std::string chunk = buffer.substr(0, pos);
                buffer.erase(0, pos + 2);

                if(chunk.empty() || chunk[0] == ':'){
                    continue;
                }

                // 按工程约定：比较前 5 个字符与 "data:"
                if(chunk.size() < 5 || chunk.compare(0, 5, "data:") != 0){
                    continue;
                }
                std::string modelData = chunk.substr(5);
                // "data:" 后面可能带一个可选空格
                if(!modelData.empty() && modelData[0] == ' '){
                    modelData.erase(0, 1);
                }

                if(modelData == "[DONE]"){
                    callback("", true);
                    streamFinish = true;
                    return true;
                }

                Json::Value modelDataJson;
                Json::CharReaderBuilder reader;
                std::string errors;
                std::istringstream modelDataStream(modelData);
                if(Json::parseFromStream(reader, modelDataStream, &modelDataJson, &errors)){
                    if(modelDataJson.isMember("choices") && modelDataJson["choices"].isArray()
                       && !modelDataJson["choices"].empty()
                       && modelDataJson["choices"][0].isMember("delta")){
                        const auto& delta = modelDataJson["choices"][0]["delta"];
                        std::string content;
                        if(delta.isMember("content") && delta["content"].isString()){
                            content = delta["content"].asString();
                        }
                        // reasoning_content (思考过程) 仅在内部累计用于存储完整响应，
                        // 不透传给前端，避免把模型的思维链显示给用户
                        std::string thinking;
                        if(delta.isMember("reasoning_content")
                           && delta["reasoning_content"].isString()){
                            thinking = delta["reasoning_content"].asString();
                        }
                        if(!thinking.empty()){
                            fullResponse += thinking;
                        }
                        if(!content.empty()){
                            fullResponse += content;
                            callback(content, false);
                        }
                    }
                }
                else{
                    WARN("DeepSeekProvider: parse stream chunk failed, errors={}, raw(前150字): {}",
                         errors, modelData.substr(0, 150));
                }
            }
            return true;
        };

        auto result = client.send(req);
        if(!result){
            ERR("DeepSeekProvider stream request failed: error={}, httpStatus={}, msg={}",
                httplib::to_string(result.error()), statusCode, errorMsg);
            return "";
        }

        if(!streamFinish){
            WARN("DeepSeekProvider stream ended without [DONE] marker");
            callback("", true);
        }

        INFO("DeepSeekProvider stream done, received response total: {} bytes", fullResponse.size());
        return fullResponse;
    }

}   //end ai_chat_sdk
