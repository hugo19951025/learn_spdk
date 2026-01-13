#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

int main(int argc, char* argv[]) {
    // 默认连接参数
    std::string host = "127.0.0.1";
    int port = 8888;
    std::string message = "Hello SPDK Server!";
    
    // 解析命令行参数
    if (argc > 1) {
        host = argv[1];
    }
    if (argc > 2) {
        port = std::stoi(argv[2]);
    }
    if (argc > 3) {
        message = argv[3];
    }
    
    std::cout << "Connecting to " << host << ":" << port << std::endl;
    
    // 创建socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        std::cerr << "Error creating socket: " << strerror(errno) << std::endl;
        return 1;
    }
    
    // 设置服务器地址
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    // 解析主机地址
    if (inet_pton(AF_INET, host.c_str(), &server_addr.sin_addr) <= 0) {
        if (host == "localhost") {
            server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        } else {
            std::cerr << "Invalid address: " << host << std::endl;
            close(sockfd);
            return 1;
        }
    }
    
    // 连接到服务器
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Connection failed: " << strerror(errno) << std::endl;
        close(sockfd);
        return 1;
    }
    
    std::cout << "Connected successfully!" << std::endl;
    
    // 发送消息
    std::cout << "Sending message: " << message << std::endl;
    ssize_t bytes_sent = send(sockfd, message.c_str(), message.length(), 0);
    if (bytes_sent < 0) {
        std::cerr << "Send failed: " << strerror(errno) << std::endl;
        close(sockfd);
        return 1;
    }
    
    std::cout << "Sent " << bytes_sent << " bytes" << std::endl;
    
    // 接收回显
    char buffer[1024];
    ssize_t bytes_received = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received < 0) {
        std::cerr << "Receive failed: " << strerror(errno) << std::endl;
    } else if (bytes_received == 0) {
        std::cout << "Connection closed by server" << std::endl;
    } else {
        buffer[bytes_received] = '\0';
        std::cout << "Received echo: " << buffer << std::endl;
    }
    
    // 关闭连接
    close(sockfd);
    std::cout << "Connection closed" << std::endl;
    
    return 0;
}