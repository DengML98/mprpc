#include "rpcprovider.h"
#include "user.pb.h"
#include "connect_pool.hpp"


class Rpc: public fixbug::RpcService {
public:
    std::pair<int32_t, std::vector<std::vector<std::string>>> Query(const std::string& sql) {
        auto conn_ptr = ConnectPool::GetInstance().GetConnection();
        if (!conn_ptr) {
            std::cout << "connect mysql failed" << std::endl;
            return {-1, {}};
        }
        auto res = conn_ptr->query(sql);
        if (!res) {
            std::cout << "query failed" << std::endl;
            return {-1, {}};
        }

        std::vector<std::vector<std::string>> msg;
        uint32_t num_fields = mysql_num_fields(res); //列数
        auto fields = mysql_fetch_fields(res); //字段信息
        //先把列名存入数组
        std::vector<std::string> col;
        for (int i = 0; i < num_fields; i++) { //遍历该行的每一列
            col.push_back(fields[i].name);
        }
        msg.push_back(col);
        //再把内容存入数组
        uint32_t count = 0;
        while (auto row = mysql_fetch_row(res)) { //取一行
            std::vector<std::string> tmp;
            for (int i = 0; i < num_fields; i++) { //遍历该行的每一列
                tmp.push_back(row[i] ? row[i] : ""); //字段为NULL则填""
            }
            msg.push_back(tmp);
            ++count;
        }
        mysql_free_result(res);

        return {count, msg};
    }    

    void Query(::google::protobuf::RpcController* controller,
                       const ::fixbug::QueryRequest* request,
                       ::fixbug::QueryResponse* response,
                       ::google::protobuf::Closure* done)
    {
        const auto [sucess, msg] = Query(request->sql());
        response->set_success(sucess);
        auto list = response->mutable_msg();
        for (auto v: msg) {
            auto vectorString = list->add_lists(); //你遍历一个数组，我就创建一个数组
            for (auto str: v) {
                vectorString->add_items(str); //一遍历一个元素，我就创建一个元素
            }
        }
        done->Run();
    } 

    std::pair<bool, std::string> Update(const std::string& sql) {
        auto conn_ptr = ConnectPool::GetInstance().GetConnection();
        if (!conn_ptr) {
            return {false, "connect mysql failed"};
        }
        return conn_ptr->update(sql) ? std::make_pair(true, "update sucess") : std::make_pair(false, "update failed");
    }

    void Update(::google::protobuf::RpcController* controller,
                       const ::fixbug::UpdateRequest* request,
                       ::fixbug::UpdateResponse* response,
                       ::google::protobuf::Closure* done)
    {
        const auto [sucess, msg] = Update(request->sql());
        response->set_success(sucess);
        response->set_msg(msg);
        done->Run();
    }
};



int main(int argc, char** argv) {
    //1. 初始化框架：把配置文件写入configMap_
    auto& config = mprpc::MpRpcConfig::GetInstance(); //获取单例的引用
    config.Init(argc, argv); 
    std::cout << "zookeeper_ip: " << config.QuerryConfig("zookeeper_ip") << std::endl;
    std::cout << "zookeeper_port: " << config.QuerryConfig("zookeeper_port") << std::endl;
    std::cout << "rpc_ip: " << config.QuerryConfig("rpc_ip") << std::endl;
    std::cout << "rpc_port: " << config.QuerryConfig("rpc_port") << std::endl;
    std::cout << "nginx_ip: " << config.QuerryConfig("nginx_ip") << std::endl;
    std::cout << "nginx_port: " << config.QuerryConfig("nginx_port") << std::endl;
    //2. 发布服务: 把本地服务写入serviceMap_
    mprpc::RpcProvider provider;
    provider.NotifyService(new Rpc); 
    // provider.NotifyService(new ProductService); //可以把多个方法发布成rpc服务
    
    //3. 启动服务器，开始提供服务
    provider.Run();
}