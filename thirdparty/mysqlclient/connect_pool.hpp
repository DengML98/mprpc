#pragma once
#include <string>
#include <queue>
#include <mutex>
#include <atomic>
#include <thread>
#include <memory>
#include <condition_variable>
#include <functional>

#include "mysql.hpp"
#include "SimpleIni.h"

//连接池类
class ConnectPool {
public:
    ConnectPool(const ConnectPool&) = delete;
    ConnectPool(ConnectPool&&) = delete;
    ConnectPool& operator=(const ConnectPool&) = delete;
    ConnectPool& operator=(ConnectPool&&) = delete;
    ~ConnectPool() = default;

    static ConnectPool& GetInstance();
    //给外部提供的接口：从连接池获取一条连接，且用智能指针管理，自定义删除器，使其析构时归还连接而不是释放连接
    std::unique_ptr<MySql, std::function<void(MySql*)>> GetConnection(); 
private:
    ConnectPool();               // 构造函数私有化,单例
    bool LoadConfig();           // 加载配置文件

    std::string ip_;             // mysql的ip
    uint16_t port_;              // mysql的port
    std::string username_;       // 登录mysql的用户名 
    std::string password_;       // 登录mysql的密码
    std::string database_;       // 要访问的数据库名
    uint32_t initSize_;          // 初始连接数
    uint32_t maxSize_;           // 最大连接数
    uint16_t maxIdleTime_;       // 最大空闲时间 s
    int connectTimeout_;         // 获取连接的超时时间 ms

    std::queue<MySql*> connQue_;       // 存储空闲连接的队列
    std::mutex queueMtx_;              // 保护队列的互斥锁
    std::condition_variable cv_;       //条件变量
    std::atomic<uint16_t> connectCnt_; //记录连接的总数，且是线程安全的
};

ConnectPool& ConnectPool::GetInstance() {
    static ConnectPool pool; //静态局部变量的初始化是线程安全的
    return pool;
}

bool ConnectPool::LoadConfig() {
    CSimpleIniA ini;
    if (ini.LoadFile("/home/gyl/work/mprpc/thirdparty/mysqlclient/mysql.conf") < 0) {
        return false;
    }
    ip_ = ini.GetValue("mysql", "ip");
    port_ = std::stoi(ini.GetValue("mysql", "port"));
    username_ = ini.GetValue("mysql", "username");
    password_ = ini.GetValue("mysql", "password");
    database_ = ini.GetValue("mysql", "database");
    initSize_ = std::stoi(ini.GetValue("mysql", "initSize"));
    maxSize_ = std::stoi(ini.GetValue("mysql", "maxSize"));
    maxIdleTime_ = std::stoi(ini.GetValue("mysql", "maxIdleTime"));
    connectTimeout_ = std::stoi(ini.GetValue("mysql", "connectTimeout"));
    return true;
}

ConnectPool::ConnectPool() {
    if (!LoadConfig()) {
        return;
    }
    //连接池第一次时先创建初始数量的连接供外部使用
    for (int i = 0; i < initSize_; ++i) {
        MySql* p = new MySql;
        while (1) {
            if (p->connect(ip_, port_, username_, password_, database_)) {
                break;
            }
        }
        //出循环就一定连接上了
        connQue_.push(p);
        p->refreshAliveTime();
        ++connectCnt_;
    }

    //创建一个生产者线程，等待连接不够请求再创建的请求
    std::thread produce([&](){
        while (1) {
            std::unique_lock<std::mutex> lock(queueMtx_);
            while (!connQue_.empty()) {//说明初始连接数都没用完
                cv_.wait(lock);
            }
            //被唤醒说明初始连接数用完且不够了，拿到锁开始生产
            if (connectCnt_ < maxSize_) {
                MySql* p = new MySql;
                while (1) {
                    if (p->connect(ip_, port_, username_, password_, database_)) {
                        break;
                    }
                }
                //出循环就一定连接上了
                connQue_.push(p);
                p->refreshAliveTime();
                ++connectCnt_;
            }
            //通知等待的消费者们
            cv_.notify_all();
        }
    });//函数较短就原地写，长就拆走，也可用std::bind
    produce.detach();

    //开一个线程专门扫描超过最大空闲时间，进行连接回收
    std::thread scan([&](){
        while (1) {
            //用sleep模拟定时,每次睡一个最大空闲时间
            std::this_thread::sleep_for(std::chrono::seconds(maxIdleTime_));
            //可能会操作队列，要加锁
            std::unique_lock<std::mutex> lock(queueMtx_);
            while (connectCnt_ > initSize_) {
                //队首元素的存活时间最长，若它都没超过最大空闲时间，则可以不用判断了
                MySql* p = connQue_.front();
                if (p->GetAliveTime() < (maxIdleTime_ * 1000)) {
                    break;
                } 
                connQue_.pop();
                --connectCnt_;
                delete p;
            }
        }
    });
    scan.detach();
}

std::unique_ptr<MySql, std::function<void(MySql*)>> ConnectPool::GetConnection() {
    std::unique_lock<std::mutex> lock(queueMtx_);
    if (connQue_.empty()) {
        //等待，时间若未被唤醒，也自动醒
        cv_.wait_for(lock, std::chrono::milliseconds(connectTimeout_));
        if (connQue_.empty()) {
            return nullptr; //获取连接超时
        }
    }
    //能走到这：要么队列不为空，要么队列为空被唤醒后不为空
    //自定义删除器，当客户端调用此函数获取连接，用完后，智能指针析构->归还连接
    std::unique_ptr<MySql, std::function<void(MySql*)>> sp(connQue_.front(), [&](MySql* p){
        std::unique_lock<std::mutex> lock(queueMtx_);
        connQue_.push(p);
        p->refreshAliveTime();
    });
    connQue_.pop();
    //消费后若队列空，则通知生产者
    if (connQue_.empty()) {
        cv_.notify_all();
    }
    return sp;
}