#pragma once
#include <mysql/mysql.h>
#include <string>
#include <ctime>

// mysql的客户端操作类
class MySql {
public:
    MySql();                            // 初始化数据库连接资源
    ~MySql();                           // 释放连接接资源
    bool connect(const std::string& ip, const uint16_t port, const std::string& user, const std::string& password, const std::string& dbname);                     // 连接数据库
    bool update(std::string sql);       // 更新(insert, delete, update都是这个接口)
    MYSQL_RES* query(std::string sql);  // 查询操作
    MYSQL* GetConnection();             // 获取连接

    void refreshAliveTime() { aliveTime_ = std::clock(); }            //每次进队列刷新时间
    std::clock_t GetAliveTime() { return std::clock() - aliveTime_; } //获取存活时间 ms
private:
    MYSQL* conn_;                       //一条连接
    std::clock_t aliveTime_;            //记录在队列后的存活时间(配合数据库连接池使用的)
};


MySql::MySql() {
    conn_ = mysql_init(nullptr);
}

MySql::~MySql() {
    if (conn_) {
        mysql_close(conn_);
    }
}

bool MySql::connect(const std::string& ip, const uint16_t port, const std::string& user, const std::string& password, const std::string& dbname) {
    MYSQL* p = mysql_real_connect(conn_, ip.c_str(), user.c_str(), password.c_str(), dbname.c_str(), port, nullptr, 0);
    if (p) {
        // C和C++代码默认的编码字符是ASCII，如果不设置，从MySQL上拉下来的中文会乱码
        mysql_query(conn_, "set names gbk");
    } 
    return p; //p若为空不就是false吗
}

bool MySql::update(std::string sql) {
    return !mysql_query(conn_, sql.c_str());
}

MYSQL_RES* MySql::query(std::string sql) {
    if (mysql_query(conn_, sql.c_str())) {
        return nullptr;
    }
    return mysql_use_result(conn_);
    /* 
    查询返回后一般这样操作   
    if (res) {
        MYSQL_ROW row;
        while (row = mysql_fetch_row(res)) {//赋值给res，且res不空
            //根据自己的表字段来获取数据
            std::cout << row[0] << std::endl;
            std::cout << row[1] << std::endl;
            std::cout << row[2] << std::endl;
            std::cout << row[3] << std::endl;
        }
    }
    mysql_free_result(res);
    */
}
MYSQL* MySql::GetConnection() {
    return conn_;
}