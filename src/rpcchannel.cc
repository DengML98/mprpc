#include "rpcchannel.h"

namespace mprpc {
    //客户端的所有rpc请求都走这个，需要自己定义：我们在这里做数据序列化(填装proto)和网络发送
void MpRpcChannel::CallMethod(const google::protobuf::MethodDescriptor* method, google::protobuf::RpcController* controller, 
                    const google::protobuf::Message* request, google::protobuf::Message* response, google::protobuf::Closure* done)
{   
    //1. 序列化数据，组织要发送的字符流：header_size + rpc_header(service+method+args_size) + args_str
    auto [service_name, method_name, args_str] = std::make_tuple(method->service()->name(), method->name(), std::string(""));
    //把形参序列化,存入args_str
    if (!request->SerializeToString(&args_str)) {
        controller->SetFailed("形参serialize error!");
        return;
    }
    uint32_t args_size = args_str.size();
    //装填rpc_header的proto,序列化存入rpc_header_str
    mprpc::RpcHeader rpc_header;
    rpc_header.set_service_name(service_name);
    rpc_header.set_method_name(method_name);
    rpc_header.set_args_size(args_size);
    std::string rpc_header_str;
    if (!rpc_header.SerializeToString(&rpc_header_str)) {
        controller->SetFailed("rpc_header serialize error!");
        return;
    }
    uint32_t header_size = rpc_header_str.size();

    std::string send_str;
    send_str.insert(0, std::string((char*)&header_size, 4)); //前4个字节放header_size的2进制比表示
    send_str += rpc_header_str + args_str;

    //2.把字符流发送出去，客户端使用socket编程即可，不需要高并发
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        controller->SetFailed("socketfd create error!");
        return;
    } 
    //向zk查询提供rpc方法的节点ip和port
    ZkClient zk_cli;
    auto& config = MpRpcConfig::GetInstance();
    std::string zk_ip = config.QuerryConfig("zookeeper_ip");
    uint16_t zk_port = stoi(config.QuerryConfig("zookeeper_port"));
    if (zk_cli.Start(zk_ip, zk_port) == -1) {
        controller->SetFailed("zookeeper connect error!");
        return;
    }
    std::string method_path = std::string("/") + method->service()->name() + "/" + method->name();
    std::string ip_port = zk_cli.GetData(method_path.c_str());
    if (ip_port == "") {
        controller->SetFailed("method path not exist!");
        return;
    }
    int idx = ip_port.find(":");
    std::string ip = std::string(ip_port.begin(), ip_port.begin() + idx);
    uint16_t port = stoi(std::string(ip_port.begin() + idx + 1, ip_port.end()));
    sockaddr_in server_addr = {AF_INET, htons(port), inet_addr(ip.c_str())};
    if (connect(fd, (sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        controller->SetFailed("socket connect error!");
        close(fd);
        return;
    }
    if (send(fd, send_str.c_str(), send_str.size(), 0) == -1) {
        controller->SetFailed("socket send error!");
        close(fd);
        return;
    }

    //3. 接受响应，反序列化存入response,返回给客户端
    char recv_buf[1024] = {0};
    //设置recv超时时间: 3s
    timeval timeout{3, 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    int recv_size = recv(fd, recv_buf, sizeof(recv_buf), 0);
    close(fd); 
    if (recv_size < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) controller->SetFailed("socket recv timeout");
        else controller->SetFailed("socket recv error, recv_size = -1");
        return;
    } else if (recv_size == 0) {
        controller->SetFailed("socket connect close! recv_size = 0");
        return;
    } 

    //不能用ParseFromString：recv_buf有\0后面就截断了，如recv_buf = {'a', '\0', 'b'}
    if (!response->ParseFromArray(recv_buf, recv_size)) {
        controller->SetFailed("response parse error!");
        return;
    } 
}
}