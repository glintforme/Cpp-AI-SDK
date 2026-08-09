#pragma once
#include "common.h"
#include <string>
#include <map>
#include <vector>
#include <functional>

namespace ai_chat_sdk{

    class LLMProvider{
    public:
        //初始化模型
        virtual bool initModel(const std::map<std::string,std::string> &model_config) = 0;
        //检查模型是否有效
        virtual bool isAvailable() const = 0;
        //获取模型名称
        virtual std::string getModelName()const = 0;
        //获取模型描述信息
        virtual std::string getModelDesc()const = 0;
        //发送消息给模型--全量返回
        virtual std::string sendMessage(const std::vector<Message> &messages,
                                        const std::map<std::string,std::string> &request_param) = 0;
        //发送消息给模型--增量返回
        virtual std::string sendMessageIncrement(const std::vector<Message> &messages,
                                                const std::map<std::string,std::string> &request_param,
                                                std::function<void(const std::string &,bool)> callback) = 0;
    protected:
        bool _isAvailable = false;  //标识模型是否有效
        std::string _api_key;       //保存API密钥
        std::string _endpoint;      //保存模型API地址
    };
}   //end namespace ai_chat_sdk