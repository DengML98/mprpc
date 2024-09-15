#pragma once
#include <iostream>
#include <cstring>
#include <stdio.h>
#include <string>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <utility>
#include <queue>
#include <mutex>
#include <thread>
#include <condition_variable>

//加锁队列，充当日志缓冲区；只需要push，pop前加锁
template<typename T>
class LockQueue {
public:
    //生产者调用
    void push(const T& data) {
        std::unique_lock<std::mutex> lock(mtx_); //出作用域自动解锁
        que_.push(data);
        cond_.notify_one(); //通知它
    }
    //消费者调用
    const T pop() {
        std::unique_lock lock(mtx_);
        while (que_.empty()) {
            cond_.wait(lock); //释放锁，等待被唤醒
        }
        //被唤醒后重新获得锁，出循环
        const T data = que_.front();
        que_.pop();
        return data; 
    }
private:
    std::queue<T> que_;
    std::mutex mtx_;
    std::condition_variable cond_;
};


typedef enum LogLevel {
    INFO,
    ERROR
} LogLevel;

class Log {
public:
    Log(const Log&) = delete;
    Log(Log&&) = delete;
    Log& operator=(const Log&) = delete;
    Log& operator=(Log&&) = delete;
    ~Log() = default;

    static Log& GetInstance();
    void SetLevel(const LogLevel& level);
    void Loger(const std::string& msg);
private:
    LogLevel level_;
    LockQueue<std::string> lockQue_;
    
    Log(); //单例
};

//初始化的时候开一个消费者线程不断的消费队列
Log::Log() {
    std::thread write_thread([&](){//[&]抓到了this指针,所以可以使用成员变量
        while (1) {
           //获取时间
            std::time_t current_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            std::tm* localTime = std::localtime(&current_time);
            std::stringstream s1, s2;
            s1 << std::put_time(localTime, "%Y-%m-%d"); // 年-月-日 时 格式
            s2 << std::put_time(localTime, "[%H:%M:%S]"); //时:分:秒 
            //写入文件
            std::string file_name = s1.str() + ".txt";
            FILE* fp = fopen(file_name.c_str(), "a+");//追加写入文件，不存在就创建
            if (!fp) {
                std::cerr << "open file error" << std::endl;//打开失败，进入下一次循环继续打开
            } 
            else {
                std::string str = s2.str() + lockQue_.pop() + "\n";//获取一行日志，队列无数据在此阻塞
                fputs(std::move(str.c_str()), fp); //写入一行
                fclose(fp);
            }
        } 
    });
    write_thread.detach();
}
Log& Log::GetInstance() {
    static Log loger;  //局部静态变量的初始化是线程安全的
    return loger;
}

void Log::SetLevel(const LogLevel& level) {
    level_ = level;
}
void Log::Loger(const std::string& msg) {
    lockQue_.push(msg);
}


#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#define LOG(level, msg, ...)\
    do {\
        auto& log = Log::GetInstance();\
        log.SetLevel(level);\
        char buf[1024] = {0};\
        snprintf(buf, sizeof(buf), (std::string("[") + #level + "][" + __FILENAME__ + ":" + std::to_string(__LINE__) + "] " +\
                                     msg).c_str(), ##__VA_ARGS__);\
        log.Loger(buf);\
    } while (0)
#define LOG_INFO(msg, ...) LOG(INFO, msg, ##__VA_ARGS__)
#define LOG_ERROR(msg, ...) LOG(ERROR, msg, ##__VA_ARGS__)
