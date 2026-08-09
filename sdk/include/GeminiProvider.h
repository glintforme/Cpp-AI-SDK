#pragma once
#include "LLMProvider.h"

namespace ai_chat_sdk{

    class GeminiProvider : public LLMProvider{
    public:
        //初始化模型
        virtual bool initModel(const std::map<std::string,std::string> &model_config) override;
        //检查模型是否有效
        virtual bool isAvailable() const override;
        //获取模型名称
        virtual std::string getModelName() const override;
        //获取模型描述信息
        virtual std::string getModelDesc() const override;
        //发送消息给模型--全量返回
        virtual std::string sendMessage(const std::vector<Message> &messages,
                                        const std::map<std::string,std::string> &request_param) override;
        //发送消息给模型--增量返回
        virtual std::string sendMessageIncrement(const std::vector<Message> &messages,
                                                const std::map<std::string,std::string> &request_param,
                                                std::function<void(const std::string &,bool)> callback) override;
    };
}   //end namespace ai_chat_sdk