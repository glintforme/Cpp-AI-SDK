#include "../include/LLMManager.h"
#include "../include/util/MyLog.h"
#include "../include/common.h"

namespace ai_chat_sdk{

    bool LLMManager::registerProvider(const std::string &modelName,std::unique_ptr<LLMProvider> provider){
        if(!provider){
            ERR("cannot register nullptr provider, modelName = {}", modelName);
            return false;
        }
        _providers[modelName] = std::move(provider);
        _modelInfos[modelName] = ModelInfo(modelName);
        INFO("provider {} registered successfully", modelName);
        return true;
    }

    bool LLMManager::initModel(const std::string& modelName, const std::map<std::string, std::string>& modelParam){
        auto it = _providers.find(modelName);
        if(it == _providers.end()){
            ERR("model provider not found, modelName = {}", modelName);
            return false;
        }
        bool isSuccess = it->second->initModel(modelParam);
        if(!isSuccess){
            ERR("init model failed, modelName = {}", modelName);
            return false;
        }
        else{
            INFO("init model success, modelName = {}", modelName);
            _modelInfos[modelName]._desc = it->second->getModelDesc();
            _modelInfos[modelName]._isAvailable = true;
            _modelInfos[modelName]._provider = it->second->getModelName();
        }
        return isSuccess;
    }

    std::vector<ModelInfo> LLMManager::getAvailableModels() const{
        std::vector<ModelInfo> models;
        for(const auto &pair : _modelInfos){
            if(pair.second._isAvailable){
                models.push_back(pair.second);
            }
        }
        return models;
    }

    bool LLMManager::isModelAvailable(const std::string& modelName) const{
        auto it = _modelInfos.find(modelName);
        if(it == _modelInfos.end()){
            return false;
        }
        return it->second._isAvailable;
    }

    void LLMManager::setModelNeedsProxy(const std::string& modelName, bool needsProxy){
        auto it = _modelInfos.find(modelName);
        if(it != _modelInfos.end()){
            it->second._needsProxy = needsProxy;
        }
    }

    const ModelInfo* LLMManager::getModelInfo(const std::string& modelName) const{
        auto it = _modelInfos.find(modelName);
        if(it == _modelInfos.end()){
            return nullptr;
        }
        return &it->second;
    }

    std::string LLMManager::sendMessage(const std::string& modelName, const std::vector<Message>& messages, const std::map<std::string, std::string>& requestParam){
        if(!isModelAvailable(modelName)){
            ERR("model {} is not available", modelName);
            return "";
        }
        auto it = _providers.find(modelName);
        if(it == _providers.end()){
            ERR("model provider not found, modelName = {}", modelName);
            return "";
        }
        return it->second->sendMessage(messages,requestParam);
    }

    std::string LLMManager::sendMessageStream(const std::string& modelName, const std::vector<Message>& messages, const std::map<std::string, std::string>& requestParam, std::function<void(const std::string&, bool)>& callback){
        auto it = _providers.find(modelName);
        if(it == _providers.end()){
            ERR("model provider not found, modelName = {}", modelName);
            return "";
        }
        if(!it->second->isAvailable()){
            ERR("model {} is not active", modelName);
            return "";
        }
        return it->second->sendMessageIncrement(messages,requestParam,callback);
    }

}
