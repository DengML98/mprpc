#pragma once
#include <string>
#include <tuple>
#include <unordered_map>
#include "google/protobuf/service.h"
#include "google/protobuf/descriptor.h"
#include "SimpleIni.h"

//存放配置的单例类
namespace mprpc {
class MpRpcConfig {
public:
    static MpRpcConfig& GetInstance();
    //查询configMap_
    const std::string QuerryConfig(const std::string& key);
    //初始化，把配置文件写入configMap_
    void Init(int argc, char** argv); 
private:
    MpRpcConfig() = default;
    MpRpcConfig(const MpRpcConfig&) = delete;
    MpRpcConfig(MpRpcConfig&&) = delete;
    MpRpcConfig& operator=(const MpRpcConfig&) = delete;
    MpRpcConfig& operator=(MpRpcConfig&&) = delete;
    
    std::unordered_map<std::string, std::string> configMap_;   
};
} //namespace mprpc