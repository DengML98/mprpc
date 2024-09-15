#pragma once 
#include <semaphore> //信号量， c++20
#include <string>
#include <iostream>
#include "zookeeper/zookeeper.h"

#include "rpcconfig.h"

namespace mprpc {
//封装zk客户端
class ZkClient {
public:
    ZkClient();
    ~ZkClient();
    //启动客户端，连接server
    int Start(const std::string& zk_ip, const uint16_t zk_port); 
    //在path位置create节点，state表示节点类型,默认是永久节点
    int Create(const char* path, const char* data, int datalen, int state = 0); 
    //对应get命令
    std::string GetData(const char* path);
private:
    //zk客户端句柄
    zhandle_t *zhandle_;

};
} //namespace mprpc