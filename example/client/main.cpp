#include <signal.h>
#include "rpcprovider.h"

#include "user.pb.h"

//客户端(rpc调用方)编程示例：
int main(int argc, char** argv) {
    // //1.初始化框架
    auto& config = mprpc::MpRpcConfig::GetInstance();
    config.Init(argc, argv);
    std::string zk_ip = config.QuerryConfig("zookeeper_ip"), zk_port = config.QuerryConfig("zookeeper_port");
    std::cout << "zookeeper_ip: " << zk_ip << std::endl;
    std::cout << "zookeeper_port:" << zk_port << std::endl;



    mprpc::MprpcController controller; //记录rpc调用中的状态和错误信息
    mprpc::MpRpcChannel channel;
    fixbug::RpcService_Stub stub(&channel);

    while (1) {
        std::cout << "1: 查询, 2: 增删改" << std::endl;
        std::cout << "Input choice:" << std::endl;
        std::string str;
        std::getline(std::cin, str);
        try {
            uint8_t choice = std::stoi(str);
            if (choice == 1) {
                fixbug::QueryRequest request;
                fixbug::QueryResponse response;

                std::cout << "Input sql:" << std::endl;
                std::string sql;
                std::getline(std::cin, sql);
                if (sql[sql.size() - 1] == ';') sql.pop_back();
                request.set_sql(sql);

                stub.Query(&controller, &request, &response, nullptr);
                if (controller.Failed()) std::cerr << controller.ErrorText() << std::endl;
                else if (response.success() == -1) std::cout << "query failed" << std::endl;
                else {
                    std::cout << "查到" << response.success() << "条记录" << std::endl;
                    auto res = response.mutable_msg();
                    for (int i = 0; i < res->lists_size(); i++) {
                        auto vec = res->lists(i);
                        for (int j = 0; j < vec.items_size(); j++) {
                            std::cout << vec.items(j) << " ";
                        }
                        std::cout << std::endl;
                    }
                }
            } 
            else if (choice == 2) {
                fixbug::UpdateRequest request;
                fixbug::UpdateResponse response;

                std::cout << "Input sql:" << std::endl;
                std::string sql;
                std::getline(std::cin, sql);
                if (sql[sql.size() - 1] == ';') sql.pop_back();
                request.set_sql(sql);

                stub.Update(&controller, &request, &response, nullptr);
                if (controller.Failed()) std::cerr << controller.ErrorText() << std::endl;
                std::cout << response.msg() << std::endl;
                if (response.success()) {
                    //update sucess
                } else {
                    //update sucess   
                }            
            } 
            else std::cerr << "输入错误" << std::endl;

            controller.Reset();
        } 
        catch(...) {
            std::cerr << "输入错误" << std::endl;
        }
        std::cout << std::endl;
    }
}