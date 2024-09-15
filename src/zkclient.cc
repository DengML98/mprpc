#include "zkclient.h"

namespace mprpc {

ZkClient::ZkClient(): zhandle_(nullptr) {}

ZkClient::~ZkClient() {
    if (zhandle_ != nullptr) {
        zookeeper_close(zhandle_);//关闭句柄，释放资源
    }
}

int ZkClient::Start(const std::string& zk_ip, const uint16_t zk_port) {
    // std::string connect_str = zk_ip + ":" + std::to_string(zk_port); 
    std::string connect_str = "127.0.0.1:2181,127.0.0.1:2182,127.0.0.1:2183";

    /* zookeeper_mt: 多线程版本,为什么用这个：zk的客户都安api提供了3个线程：
    1. api调用线程；2.网络io线程(poll，客户端程序，不需要高并发)；3.watcher回调线程    
    zookeeper_init(api调用线程)调用后会创建2个线程：网络io、watcher回调   */
    //参数1：ip:host,参数2：watch回调，参数3：超时时间
    auto watch = [](zhandle_t* zh, int type, int state, const char* path, void* watcherCtx){
        if (type == ZOO_SESSION_EVENT && state == ZOO_CONNECTED_STATE) {//连接成功
            // sem_post((sem_t*)zoo_get_context(zh));//信号量加1，主线程就解除阻塞了
            ((std::counting_semaphore<>*)zoo_get_context(zh))->release();
        }
    };
    zhandle_ = zookeeper_init(connect_str.c_str(), watch, 30000, nullptr, nullptr, 0);
    if (!zhandle_) {
        return -1;
    }
    //到这里表示创建句柄成功，不代表连接成功，因为这个init函数是异步的，所以需要用一个信号量来获取ZOO_CONNECTED_STATE
    //sem_t sem;
    //sem_init(&sem, 0, 0);
    //zoo_set_context(zhandle_, &sem);
    //sem_wait(&sem); //这里刚开始肯定阻塞

    //创建一个可以允许两个线程同时访问的信号量，初始化计数量为0
    std::counting_semaphore<> sem(0);
    zoo_set_context(zhandle_, &sem);
    sem.acquire(); //请求信号量， 阻塞等待其他线程release信号量
    
    return 0;
}


int ZkClient::Create(const char* path, const char* data, int datalen, int state) {
    char path_buf[128] = {0};
    //先判断path位置是否存在节点,不存在再创建
    if (zoo_exists(zhandle_, path, 0, nullptr) == ZNONODE) {
        if (zoo_create(zhandle_, path, data, datalen, &ZOO_OPEN_ACL_UNSAFE, state, path_buf, sizeof(path_buf)) != ZOK) {
            return -1;
        }
    }
    return 0; // 创建成功/已经存在
}

//对应get命令
std::string ZkClient::GetData(const char* path) {
    char buf[64] = {0};
    int buflen = sizeof(buf);
    return zoo_get(zhandle_, path, 0, buf, &buflen, nullptr) != ZOK ? "" : buf;
}
} //namespace mprpc