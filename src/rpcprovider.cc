#include "rpcprovider.h"

namespace mprpc {

const std::tuple<google::protobuf::Service*, const google::protobuf::MethodDescriptor*> RpcProvider::QuerryService(const std::string& service_name, const std::string& method_name) {
    auto it = serviceMap_.find(service_name);
    if (it == serviceMap_.end()) {
        return {nullptr, nullptr};
    }
    return {it->second, it->second->GetDescriptor()->FindMethodByName(method_name)};
}

void RpcProvider::NotifyService(google::protobuf::Service *service) {
    serviceMap_.insert({service->GetDescriptor()->name(), service});
}
  
//启动网络服务，即muduo库编程
void RpcProvider::Run() {
    //1. 连接zk
    auto& config = mprpc::MpRpcConfig::GetInstance(); //获取单例的引用
    std::string zk_ip = config.QuerryConfig("zookeeper_ip");
    uint16_t zk_port = stoi(config.QuerryConfig("zookeeper_port"));
    std::string rpc_ip = config.QuerryConfig("rpc_ip");
    uint16_t rpc_port = stoi(config.QuerryConfig("rpc_port"));
    std::string nginx_ip = config.QuerryConfig("nginx_ip");
    uint16_t nginx_port = stoi(config.QuerryConfig("nginx_port"));

    ZkClient zk_cli;
    if (zk_cli.Start(zk_ip, zk_port) != 0) {//zkclinet也会发起连接，我们下面的muduo又是连接
        LOG_ERROR("zookeeper connect failed!");
        exit(EXIT_SUCCESS);
    } //session的timeout时间是30s，cli每1/3的timeout发送一个心跳，重置timeout 

    //2. 连接到zk后开始注册：先创建服务名父节点，再把每个方法注册为子节点
    for (auto& it: serviceMap_) {
        std::string service_path = "/" + it.first; /* /UserServiceRpc  */
        if (zk_cli.Create(service_path.c_str(), nullptr, 0) == -1) {
            LOG_ERROR("create zk node: %s failed!", service_path.c_str());
            exit(EXIT_FAILURE);
        }
        auto desc = it.second->GetDescriptor();
        for (int i = 0; i < desc->method_count(); ++i) {
            std::string method_path = service_path + "/" + desc->method(i)->name(); /* /UserServiceRpc/Login  */
            //方法都注册为临时性节点,status不是0就是临时结点；为啥要临时节点：防止节点挂了，节点本身不删，服务删掉，别人就调用不了了
            //每个method的内容写nginx的ip+port
            std::string data = nginx_ip + ":" + std::to_string(nginx_port);
            if(zk_cli.Create(method_path.c_str(), data.c_str(), data.size(), ZOO_EPHEMERAL)) {
                LOG_ERROR("create zk node: %s failed!", method_path.c_str());
                exit(EXIT_FAILURE);
            }
        }
    }
    //3. 启动muduo网络服务，监听rpc调用请求
    MuduoStart(rpc_ip, rpc_port, "RpcProvider", 4);
}

void RpcProvider::MuduoStart(const std::string& ip, const uint16_t port, const std::string& server_name, uint8_t thread_num) {
    muduo::net::EventLoop eventLoop_;
    muduo::net::TcpServer server(&eventLoop_, muduo::net::InetAddress(ip, port), server_name);//第3个参数就是个标识，服务器名
    // 绑定连接回调和消息读写回调，分离网络代码和业务代码
    server.setConnectionCallback([&](const muduo::net::TcpConnectionPtr& conn){this->OnConnection(conn);});
    server.setMessageCallback([&](const muduo::net::TcpConnectionPtr& conn, muduo::net::Buffer* buf, muduo::Timestamp time)
        {this->OnMessage(conn, buf, time);});
    // server.setConnectionCallback(bind(&RpcProvider::OnConnection, this, std::placeholders::_1));
    server.setMessageCallback(bind(&RpcProvider::OnMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    server.setThreadNum(thread_num);//设置线程数，一个io线程，三个工作线程
    server.start();
    eventLoop_.loop(); 
}

//连接事件回调
void RpcProvider::OnConnection(const muduo::net::TcpConnectionPtr& conn) {
    if (!conn->connected()) {//断开连接了
        conn->shutdown(); //关闭文件描述符，对应socket的close
    }
}

//读写事件回调：这里就是：反序列化客户端请求->执行对应函数->发回响应给recv
void RpcProvider::OnMessage(const muduo::net::TcpConnectionPtr& conn, muduo::net::Buffer* buf, muduo::Timestamp time) {
    //拿到客户端请求(字节流)：xxxxxxxxxx
    std::string recv_str = buf->retrieveAllAsString();

    //1. 反序列化
    //先从字符流中读前4个字节，转成10进制，得到rpc_header_size
    uint32_t header_size = 0;
    recv_str.copy((char*)&header_size, 4, 0); //从0位置拷贝4个字节放到整数里，就会显示成10进制
    //通过rpc_header_size截取对应长度，得到rpcheader_str的字符流
    std::string rpcheader_str = recv_str.substr(4, header_size);
    //反序列化得到service_name、method_name、args_size
    mprpc::RpcHeader rpc_header;
    if (!rpc_header.ParseFromString(rpcheader_str)) {
        LOG_ERROR("反序列化头部失败！");
        return;
    }
    std::string service_name = rpc_header.service_name();
    std::string method_name = rpc_header.method_name();
    int args_size = rpc_header.args_size(); 

    //2. 查表获取对应的服务和方法
    //服务方法一定存在，客户端从zk查过来的，但是我们这里依旧做了判断
    auto [service, method] = RpcProvider::QuerryService(service_name, method_name);
    if (!service) {
        LOG_ERROR("service: %s not exist!", service_name.c_str());
        return;
    }
    if (!method) {
        LOG_ERROR("method: %s not exist!", method_name.c_str());
        return;
    }

    //3. 执行CallMethod回调，但要先装填好它: request、response、done
    //反序列化args,存入request(只有服务和方法存在我们才反序列化这个，不然没意义)
    auto request = service->GetRequestPrototype(method).New();
    if (!request->ParseFromString(recv_str.substr(4 + header_size, args_size))) {
        LOG_ERROR("request 反序列化失败！");    
        return;
    }
    auto response = service->GetResponsePrototype(method).New(); //这个不是堆区的吗
    //这个done给下面的CallMethod用的，有很多重载，这里选择：
    /*  template <typename Class, typename Arg1, typename Arg2>
        inline Closure* NewCallback(Class* object, void (Class::*method)(Arg1, Arg2), Arg1 arg1, Arg2 arg2) {
            return new internal::MethodClosure2<Class, Arg1, Arg2>(object, method, true, arg1, arg2);
        } */
    //他是模板肯定要指定类型，然后再传参啊！
    auto done = google::protobuf::NewCallback<RpcProvider, const muduo::net::TcpConnectionPtr&, google::protobuf::Message*>
                (this, &RpcProvider::SendRpcResponse, conn, response);

    //服务端面对所有的请求也是走CallMethod回调(只不过不用重写)，他会根据request去执行对应的rpc方法，如Login
    //在Login里做3件事：执行本地的Login->结果写入response->执行done->Run(); 这个Run就是我们在此给done传入的SendRpcResponse
    service->CallMethod(method, nullptr, request, response, done);
}   

//此函数专门用于给done使用
void RpcProvider::SendRpcResponse(const muduo::net::TcpConnectionPtr& conn, google::protobuf::Message* response) {
    //把序列化后的响应发送出去
    std::string response_str;
    if (!response->SerializeToString(&response_str)) {
        LOG_ERROR("serialize response_str error!");
    } else {
        conn->send(response_str);
    }
    conn->shutdown();  //服务端主动关闭，模拟短连接
}
}// namespace mprpc 
