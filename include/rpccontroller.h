#pragma once
#include <string>
#include "google/protobuf/service.h"

namespace mprpc {
//RpcController是抽象类，我们重写它的常用四个方法
class MprpcController: public google::protobuf::RpcController {
public:
    MprpcController();
    ~MprpcController() = default;
    void Reset();
    bool Failed() const;
    std::string ErrorText() const;
    void SetFailed(const std::string& reason);
    //下面3个方法暂未实现
    void StartCancel();
    bool IsCanceled() const;
    void NotifyOnCancel(google::protobuf::Closure* callback);
private:    
    bool failed_; //rpc执行过程中的状态
    std::string errText_; //执行过程中的错误信息
};
} //namespace mprpc