#include <iostream> // 包含输入输出流
#include <fstream>  // 包含文件流
#include <cstring>  // 包含字符串处理函数
#include <arpa/inet.h> // 包含网络地址转换函数
#include <unistd.h> // 包含 Unix 标准函数
#include <getopt.h> // 包含命令行参数解析函数
#include <sys/socket.h> // 包含套接字相关函数
#include <chrono> // 包含高精度计时功能

// 下载函数，接收服务器 IP、端口和文件路径
void download(const std::string& ip, int port, const std::string& file_path) {
    // 创建套接字
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "Failed to create socket." << std::endl; // 创建失败提示
        return;
    }

    // 设置服务器地址
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr)); // 清零结构体
    server_addr.sin_family = AF_INET; // IPv4
    server_addr.sin_port = htons(port); // 设置端口
    inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr); // 转换 IP 地址格式

    // 连接到服务器
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Connection failed." << std::endl; // 连接失败提示
        close(sock); // 关闭套接字
        return;
    }

    // 打开输出文件
    std::ofstream f;
    if (!file_path.empty()) { // 如果指定了文件路径
        f.open(file_path, std::ios::binary); // 以二进制模式打开文件
        if (!f.is_open()) {
            std::cerr << "Failed to open file." << std::endl; // 文件打开失败提示
            close(sock); // 关闭套接字
            return;
        }
    }

    char buffer[8192]; // 创建接收缓冲区
    while (true) { // 循环接收数据
        ssize_t bytes_received = recv(sock, buffer, sizeof(buffer), 0); // 接收数据
        if (bytes_received <= 0) {
            break; // 连接关闭或出错，退出循环
        }
        if (f.is_open()) { // 如果文件打开成功
            f.write(buffer, bytes_received); // 写入数据到文件
        }
    }

    if (f.is_open()) {
        f.close(); // 关闭文件
    }
    close(sock); // 关闭套接字
}

int main(int argc, char* argv[]) {
    std::string ip = "10.0.0.1"; // 默认 IP
    int port = 52000; // 默认端口
    std::string path; // 文件路径

    // 解析命令行参数
    int option;
    while ((option = getopt(argc, argv, "s:p:o:")) != -1) {
        switch (option) {
            case 's':
                ip = optarg; // 设置服务器 IP
                break;
            case 'p':
                port = std::stoi(optarg); // 设置服务器端口
                break;
            case 'o':
                path = optarg; // 设置输出文件路径
                break;
            default:
                std::cerr << "Invalid option." << std::endl; // 无效选项提示
                return 1;
        }
    }

    // 记录开始时间
    auto start_time = std::chrono::high_resolution_clock::now();
    download(ip, port, path); // 调用下载函数
    // 记录结束时间
    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time; // 计算时间差
    
    std::cout << "Time used: " << elapsed.count() << " sec" << std::endl; // 输出耗时
    return 0; // 返回成功
}