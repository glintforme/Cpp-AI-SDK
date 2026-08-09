#pragma once
#include "LLMProvider.h"

namespace ai_chat_sdk{

    class DeepSeekProvider : public LLMProvider{
    public:
        virtual bool initModel(const std::map<std::string,std::string> &model_config);
        virtual bool isAvailable() const;
        virtual std::string getModelName() const;
        virtual std::string getModelDesc() const;
        virtual std::string sendMessage(const std::vector<Message> &messages,const std::map<std::string,std::string> &request_param);
        virtual std::string sendMessageIncrement(const std::vector<Message> &messages,const std::map<std::string,std::string> &request_param,std::function<void(const std::string &,bool)> callback);
        

    };
}   //end ai_chat_sdk
