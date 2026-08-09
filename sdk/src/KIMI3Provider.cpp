#include "../include/KIMI3Provider.h"
#include "../include/util/MyLog.h"
#include <httplib.h>
#include <jsoncpp/json/json.h>
#include <sstream>

namespace ai_chat_sdk{

    bool KIMI3Provider::initModel(const std::map<std::string,std::string> &model_config){
        auto it = model_config.find("api_key");
        if(it == model_config.end()){
            ERR("KIMI3Provider: api_key is empty");
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
            _endpoint = "https://api.moonshot.cn";
        }

        _isAvailable = true;
        INFO("KIMI3Provider init success with endpoint : {}", _endpoint);
        return true;
    }

    bool KIMI3Provider::isAvailable() const{
        return _isAvailable;
    }

    std::string KIMI3Provider::getModelName() const{
        return "kimi-k3";
    }

    std::string KIMI3Provider::getModelDesc() const{
        return "国内顶级大模型之KIMI3为你服务!";
    }

    std::string KIMI3Provider::sendMessage(const std::vector<Message> &messages,const std::map<std::string,std::string> &request_param){
        if(!isAvailable()){
            ERR("KIMI3Provider: model is not available");
            return "";
        }

        double temperature = 1.0;
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
        INFO("DeepSeekProvider: request body : {}", requestBodyStr);

        httplib::Client client(_endpoint.c_str());
        client.set_connection_timeout(30,0);
        client.set_read_timeout(60,0);

        httplib::Headers headers={
            {"Authorization", "Bearer " + _api_key},
            {"Content-Type", "application/json"}
        };

        auto response = client.Post("/v1/chat/completions",headers,requestBodyStr,"application/json");
        if(!response){
            ERR("DeepSeekProvider sendMessage POST request faild");
            return "";
        }
        INFO("DeepSeekProvider sendMessage POST request success with status code : {}", response->status);
        INFO("DeepSeekProvider sendMessage POST request body : {}", response->body);

        if(response->status != 200){
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
                    INFO("DeepSeekProvider response text: {}",replyContent);
                    return replyContent;
                }
            }
        }

        ERR("KIMI3Provider sendMessage POST response body parse failed, error: {}", parseError);
        return "kimi-k3 response json parse failed";
    }

    std::string KIMI3Provider::sendMessageIncrement(const std::vector<Message> &messages,const std::map<std::string,std::string> &request_param,std::function<void(const std::string &,bool)> callback){
        if(!isAvailable()){
            ERR("KIMI3Provider: model is not available");
            return "";
        }

        // kimi-k3 等模型要求 temperature 必须为 1,传其它值 API 返回 400
        double temperature = 1.0;
        int maxTokens = 2048;
        if(request_param.find("temperature") != request_param.end()){
            double t = std::stod(request_param.at("temperature"));
            if(std::abs(t - 1.0) > 1e-9){
                WARN("KIMI3Provider: kimi-k3 requires temperature=1, overriding {} -> 1", t);
            }
            temperature = 1.0;
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
        INFO("KIMI3Provider: request body : {}", requestBodyStr);

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
                errorMsg = "HTTP status code: " + std::to_string(res.status) + ", body: " + res.body;
                return false;
            }
            return true;
        };

        req.content_receiver = [&](const char* data, size_t len, size_t offset, size_t totalLength){
            if(gotError){
                return false;
            }

            buffer.append(data,len);
            INFO("KIMI3Provider: stream data: {}",buffer);

            size_t pos;
            while((pos = buffer.find("\n\n")) != std::string::npos){
                std::string chunk = buffer.substr(0,pos);
                buffer.erase(0, pos + 2);

                if(chunk.empty() || chunk[0] == ':'){
                    continue;
                }

                if(chunk.compare(0,5,"data:") == 0){
                    std::string modelData = chunk.substr(5);
                    // SSE 格式为 "data: "，冒号后可能带一个空格，需要跳过
                    if(!modelData.empty() && modelData[0] == ' '){
                        modelData.erase(0,1);
                    }

                    if(modelData == "[DONE]"){
                        callback("",true);
                        streamFinish = true;
                        return true;
                    }

                    Json::Value modelDataJson;
                    Json::CharReaderBuilder reader;
                    std::string errors;
                    std::istringstream modelDataStream(modelData);
                    if(Json::parseFromStream(reader, modelDataStream, &modelDataJson, &errors)){
                        if(modelDataJson.isMember("choices") && modelDataJson["choices"].isArray() && !modelDataJson["choices"].empty() && modelDataJson["choices"][0].isMember("delta")){
                            auto delta = modelDataJson["choices"][0]["delta"];
                            std::string content;
                            if(delta.isMember("content") && delta["content"].isString()){
                                content = delta["content"].asString();
                            }
                            // kimi-k3 思考过程 reasoning_content 仅内部累计，不透传前端
                            std::string thinking;
                            if(delta.isMember("reasoning_content") && delta["reasoning_content"].isString()){
                                thinking = delta["reasoning_content"].asString();
                            }
                            if(!thinking.empty()){
                                fullResponse += thinking;
                            }
                            if(!content.empty()){
                                fullResponse += content;
                                callback(content,false);
                            }
                        }
                    }
                    else{
                        WARN("KIMI3Provider: parse model data error, errors: {}", errors);
                    }
                }
            }
            return true;
        };

        auto result = client.send(req);
        if(!result){
            ERR("KIMI3Provider Network error: {}, httpStatus: {}, msg: {}",
                httplib::to_string(result.error()), statusCode, errorMsg);
            return "";
        }

        if(!streamFinish){
            WARN("KIMI3Provider stream ended without [DONE] marker");
            callback("",true);
        }

        return fullResponse;
    }

}   //end ai_chat_sdk
