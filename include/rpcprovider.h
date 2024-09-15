#pragma once
#include <unordered_map>
#include <functional>
#include <iostream>
#include <shared_mutex>
#include <string>
#include <tuple>

#include "google/protobuf/service.h"
#include "google/protobuf/descriptor.h"
#include "muduo/net/TcpServer.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/InetAddress.h"
#include "muduo/net/TcpConnection.h"

#include "loger.h"
#include "rpcheader.pb.h"
#include "rpcchannel.h"
#include "rpccontroller.h"
#include "rpcconfig.h"
#include "zkclient.h"

namespace mprpc {
class RpcProvider {
public:    
    //发布服务,即把服务写入serviceMap_
    void NotifyService(google::protobuf::Service* service); //形参用抽象基类接受任意的service子类
    //启动网络服务
    void Run();
     //通过service_name和method_name查询serviceMap_是否存在对应的方法,
    const std::tuple<google::protobuf::Service*, const google::protobuf::MethodDescriptor*> 
        QuerryService(const std::string& service_name, const std::string& method_name);
private:
    std::unordered_map<std::string, google::protobuf::Service*> serviceMap_; 
    
    //muduo库经典三函数
    void MuduoStart(const std::string& ip, const uint16_t port, const std::string& server_name, uint8_t thread_num);
    void OnConnection(const muduo::net::TcpConnectionPtr& conn);
    void OnMessage(const muduo::net::TcpConnectionPtr& conn, muduo::net::Buffer* buf, muduo::Timestamp time);

    //CallMethod需要传入一个google::protobuf::Closure *done，我们需要自己写一个函数，为了得到done，做什么呢：序列化响应和网络发送
    void SendRpcResponse(const muduo::net::TcpConnectionPtr& conn, google::protobuf::Message* response);
};
}