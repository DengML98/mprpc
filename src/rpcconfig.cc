#include "rpcconfig.h"

namespace mprpc {



MpRpcConfig& MpRpcConfig::GetInstance() {
    static MpRpcConfig config; //c++11已经保证了静态局部变量的初始化是线程安全的
    return config;
}

const std::string MpRpcConfig::QuerryConfig(const std::string& key) {
    auto it = configMap_.find(key);
    if (it == configMap_.end()) {
        return "";
    }
    return it->second;
}

void MpRpcConfig::Init(int argc, char** argv) {
    int opt = 0;
	std::string conf, rpc;

	while ((opt = getopt(argc, argv, "i:r:")) != -1) {
		switch (opt) {
			case 'i':
                conf = optarg;
				break;
            case 'r':
                rpc = optarg;
                break;
		}
	}
    if (conf.empty()) {
        std::cerr << "error! please input format: command -i <config_path> [-r rpc1]" << std::endl;	
        exit(EXIT_FAILURE);
    }

    CSimpleIniA ini;
    SI_Error rc = ini.LoadFile(conf.c_str());
    if (rc < 0) {
        std::cerr << conf << "加载失败，请检查输入的文件路径！" << std::endl;
        exit(EXIT_FAILURE);
    }
    std::string zookeeper_ip = ini.GetValue("zookeeper", "ip", "");
    std::string zookeeper_port = ini.GetValue("zookeeper", "port", "");
    if (zookeeper_ip.empty() || zookeeper_port.empty()) {
        std::cerr << conf << "内容无效，请检查！" << std::endl;
        exit(EXIT_FAILURE);
    }
    configMap_.insert({"zookeeper_ip", zookeeper_ip});
    configMap_.insert({"zookeeper_port", zookeeper_port});


    std::string rpc_ip = ini.GetValue(rpc.c_str(), "ip", "");
    std::string rpc_port = ini.GetValue(rpc.c_str(), "port", "");
    std::string nginx_ip = ini.GetValue(rpc.c_str(), "nginx_ip", "");
    std::string nginx_port = ini.GetValue(rpc.c_str(), "nginx_port", "");
    //下面是服务端才会配置的,若输入了-r，则表示是服务端,再判断内容是否有效
    if (!rpc.empty()) {
        if (rpc_ip.empty() || rpc_port.empty() || nginx_ip.empty() || nginx_port.empty()) {
            std::cerr << conf << "服务端配置无效，请检查！" << std::endl;
            exit(EXIT_FAILURE);
        }
        configMap_.insert({"rpc_ip", rpc_ip});
        configMap_.insert({"rpc_port", rpc_port});
        configMap_.insert({"nginx_ip", nginx_ip});
        configMap_.insert({"nginx_port", nginx_port});

    }
}
} //namespace mprpc