//公共配置结构
#pragma once
#include <string>
#include <ctime>
#include <vector>

namespace ai_chat_sdk{
    //消息结构
    struct Message{
        std::string _id; //消息唯一标识符
        std::string _role; //用户"user"或助手"assistant"
        std::string _content; //消息内容
        std::time_t _timestamp; //消息生成的时间

        //构造函数
        Message(const std::string &role,const std::string &content)
                :_role(role),
                _content(content),
                _timestamp(std::time(nullptr))
        {}

    };
    //会话结构
    struct Session{
        std::string _sessionid; //会话内容
        std::string _modelName;    //模型名称
        std::vector<Message> _messages; //消息列表
        std::time_t _createdAt; //消息创建时间
        std::time_t _updatedAt; //消息更新时间

        Session(const std::string &modelName = "")
                :_modelName(modelName),
                _createdAt(std::time(nullptr)),
                _updatedAt(std::time(nullptr))
        {}
    };
    //调整模型时配置信息
    struct Config{
        virtual ~Config() = default;    //启用运行时类型识别
        std::string _modelName;    //模型名称
        double _temperature = 0.7; //采样温度，官方API文档有表格参考，默认设置为0.7
        int _maxTokens = 2048; //最大生成token数，默认官方文档建议2048
    };
    //API配置结构
    //Ollama本地接入大模型不需要api-key，所以需要将api-key单独设计
    struct ApiConfig : public Config{
        std::string _apiKey;   //接入云端模型时需要的认证信息
    };
    //Ollama模型配置结构
    struct OllamaConfig : public Config{
        std::string _modelDesc; //模型的描述
        std::string _endpoint; //模型的访问地址-base url
    };
    //LLM模型信息
    struct ModelInfo{
        std::string _name;  //模型名称
        std::string _desc;  //模型的描述
        std::string _provider; //模型的提供方
        std::string _endpoint; //模型的访问地址-base url
        bool _isAvailable = false; //模型是否初始化，默认为否
        bool _needsProxy = false; //是否需要代理才能访问（如 Google Gemini 在国内需翻墙）

        ModelInfo(const std::string &name = "",const std::string &desc = "",const std::string provider = "",const std::string endpoint = "")
                :_name(name),
                _desc(desc),
                _provider(provider),
                _endpoint(endpoint),
                _isAvailable(false),
                _needsProxy(false)
        {}
    };
}   //ai_chat_sdk 结束
