#include <iostream> // 包含输入输出流
#include <fstream>  // 包含文件流
#include <cstring>  // 包含字符串处理函数
#include <thread>   // 包含线程支持
#include <mutex>    // 包含互斥锁
#include <chrono>   // 包含高精度计时功能
#include <vector>   // 包含向量支持
#include <random>   // 包含随机数生成
#include <arpa/inet.h> // 包含网络地址转换函数
#include <unistd.h> // 包含 Unix 标准函数
#include <sys/socket.h> // 包含套接字相关函数
#include <getopt.h> // 包含命令行参数解析函数
#include <csignal> // 包含信号处理

class Server {
public:
    Server(const std::string& file = "") : chunk_size(128 * 1024), file_size(10 * 1024 * 1024), exit_sig(false) {
        server_socket = socket(AF_INET, SOCK_STREAM, 0); // 创建套接字
        if (server_socket < 0) {
            std::cerr << "Failed to create socket." << std::endl;
            exit(EXIT_FAILURE);
        }
        int opt = 1;
        setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)); // 设置重用地址
        if (!file.empty()) {
            this->file.open(file, std::ios::binary); // 打开文件
        } else {
            generate_random_file(); // 生成随机文件
        }
    }

    void bind(int port, const std::string& interface = "") {
        sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr)); // 清零结构体
        server_addr.sin_family = AF_INET; // IPv4
        server_addr.sin_addr.s_addr = inet_addr(interface.c_str());
        server_addr.sin_port = htons(port); // 设置端口

        if (::bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            std::cerr << "Bind failed." << std::endl;
            exit(EXIT_FAILURE);
        }
        std::cout << "Server is bound to port " << port << std::endl;
    }

    void generate_random_file() {
        // 生成随机文件内容
        std::vector<char> buffer(1024); // 1KB
        std::default_random_engine generator;
        std::uniform_int_distribution<char> distribution(0, 255);
        for (int i = 0; i < file_size / 1024; ++i) {
            for (auto& byte : buffer) {
                byte = distribution(generator); // 生成随机字节
            }
            file.write(buffer.data(), buffer.size()); // 写入文件
        }
    }

    void start() {
        listen(server_socket, 128); // 监听连接
        try {
            while (!exit_sig) {
                sockaddr_in client_addr;
                socklen_t addr_len = sizeof(client_addr);
                int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &addr_len); // 接受连接
                if (client_socket < 0) {
                    std::cerr << "Accept failed." << std::endl;
                    continue;
                }
                std::cout << "Accepted connection from " << inet_ntoa(client_addr.sin_addr) << std::endl;
                std::thread(&Server::handle_client, this, client_socket, client_addr).detach(); // 启动新线程处理客户端
            }
        } catch (std::exception& e) {
            std::cerr << "Exception: " << e.what() << std::endl;
        }
    }

    void handle_client(int client_socket, sockaddr_in client_addr) {
        // 处理客户端请求
        size_t sent_data = 0;
        std::cout << inet_ntoa(client_addr.sin_addr) << " began downloading file." << std::endl;
        auto start_time = std::chrono::high_resolution_clock::now();

        try {
            while (true) {
                std::lock_guard<std::mutex> lock(mutex); // 加锁
                file.seekg(sent_data); // 定位文件指针
                std::vector<char> buffer(chunk_size); // 创建缓冲区
                file.read(buffer.data(), chunk_size); // 读取文件内容
                size_t bytes_read = file.gcount(); // 获取实际读取的字节数

                if (bytes_read <= 0) {
                    close(client_socket); // 关闭客户端连接
                    break; // 结束循环
                }
                send(client_socket, buffer.data(), bytes_read, 0); // 发送数据
                sent_data += bytes_read; // 更新已发送数据量
            }
        } catch (const std::exception&) {
            std::cerr << inet_ntoa(client_addr.sin_addr) << " reset connection while downloading." << std::endl;
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end_time - start_time; // 计算耗时
        std::cout << inet_ntoa(client_addr.sin_addr) << " finished downloading." << std::endl;
        std::cout << "Time used: " << elapsed.count() << " sec." << std::endl;
        close(client_socket); // 关闭客户端套接字
    }

private:
    int server_socket; // 服务器套接字
    std::ofstream file; // 文件流
    std::mutex mutex; // 互斥锁
    const size_t chunk_size; // 每次发送的块大小
    const size_t file_size; // 文件总大小
    bool exit_sig; // 退出标志
};

int main(int argc, char* argv[]) {
    int port = 52000; // 默认端口
    std::string file_path; // 文件路径

    // 解析命令行参数
    int option;
    while ((option = getopt(argc, argv, "p:f:")) != -1) {
        switch (option) {
            case 'p':
                port = std::stoi(optarg); // 设置端口
                break;
            case 'f':
                file_path = optarg; // 设置文件路径
                break;
            default:
                std::cerr << "Invalid option." << std::endl;
                return 1;
        }
    }

    Server server(file_path); // 创建服务器对象
    server.bind(port); // 绑定端口
    server.start(); // 启动服务器
    return 0; // 返回成功
}
