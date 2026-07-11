#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstdio>
#include <cerrno>
#include <fcntl.h>
#include <sys/epoll.h>
#include <unordered_map>
using namespace std;

class UniqueFd{
    private:
        int fd_;
    public:
        explicit UniqueFd(int fd):fd_(fd){}
        int get() const{
            return fd_;
        }
        bool isValid() const{
            return fd_>=0;
        }
        UniqueFd(const UniqueFd&)=delete;
        UniqueFd& operator=(const UniqueFd&)=delete;
        UniqueFd(UniqueFd&& other) noexcept : fd_(other.fd_){
            other.fd_=-1;
        }
        UniqueFd& operator=(UniqueFd&& other) noexcept{
            if(this!=&other){
                if(fd_>=0){
                    // close(fd): 释放当前 fd 对应的内核资源。
                    // 参数 fd 要求是一个有效 file descriptor；重复 close 同一个 fd 很危险。
                    close(fd_);
                }
                fd_=other.fd_;
                other.fd_=-1;
            }
            return *this;
        }
        ~UniqueFd(){
            if(fd_>=0){
                // 析构时自动 close，体现 RAII：对象生命周期结束，fd 也随之释放。
                close(fd_);
            }
        }
};

// 用 fcntl(fd, F_GETFL, 0) 取出当前 flags。
// 把 O_NONBLOCK 加进去。
// 用 fcntl(fd, F_SETFL, flags | O_NONBLOCK) 设置回去。
// 成功返回 true，失败返回 false。

bool set_non_blocking(int fd){
    int flags=fcntl(fd, F_GETFL, 0);
    if(flags==-1){
        return false;
    }
    return fcntl(fd,F_SETFL,flags|O_NONBLOCK)==0? true:false;

}

int main(){
    // socket(domain, type, protocol): 向 kernel 创建一个 socket，成功返回 fd，失败返回 -1。
    // AF_INET: IPv4；SOCK_STREAM: TCP byte stream；0: 使用默认 TCP protocol。
    UniqueFd socket_fd(socket(AF_INET,SOCK_STREAM,0));
    // 如果失败，输出错误并退出
    if (!socket_fd.isValid()) {
    std::perror("socket");
    return 1;
    }

    

// 设置服务器地址为 127.0.0.1:6380
    sockaddr_in server_addr;
    // sin_family: 地址族，这里必须和 socket() 的 AF_INET 对应。
    server_addr.sin_family=AF_INET;
    // sin_port: 端口号要求使用 network byte order，所以用 htons() 转换。
    server_addr.sin_port=htons(6380);

    // inet_pton(af, src, dst): 把字符串 IP 转成二进制网络地址。
    // AF_INET: IPv4；"127.0.0.1": 本机地址；&server_addr.sin_addr: 写入目标。
    inet_pton(AF_INET,"127.0.0.1",&server_addr.sin_addr);

// 绑定地址
    // bind(sockfd, addr, addrlen): 把 socket fd 绑定到本地 IP 和 port。
    // 参数 1: socket fd；参数 2: sockaddr*；参数 3: 地址结构体大小。
    // 成功返回 0，失败返回 -1。
    int result=bind(
        socket_fd.get(),
        reinterpret_cast<sockaddr*>(&server_addr),
        sizeof(server_addr)
    );
    if(result==-1){
       
        std::perror("bind");
        
        return 1;
    }
// 开始监听
    // listen(sockfd, backlog): 把 socket 变成 listening socket。
    // 参数 1: server socket fd；参数 2: accept queue 的参考长度。
    // 成功返回 0，失败返回 -1。
    int result1=listen(socket_fd.get(), 10);
    if(result1==-1){
       std::perror("listen");
        
        return 1;
    }

     if (!set_non_blocking(socket_fd.get())) {
        std::perror("fcntl");
        return 1;
    }

    UniqueFd epoll_fd(epoll_create1(0));
    if(!epoll_fd.isValid()){
        std::perror("epoll_create1");
        return 1;
    }
    epoll_event event{};
    event.events=EPOLLIN;
    event.data.fd=socket_fd.get();

    if(epoll_ctl(epoll_fd.get(),EPOLL_CTL_ADD,socket_fd.get(),&event)==-1){
        std::perror("epoll_ctl");
        return 1;
    }
   

    std::unordered_map<int, UniqueFd> clients;

while(true){// 调用 accept 等待客户端
    // accept(sockfd, addr, addrlen): 从 listening socket 接收一个新连接。
    // 参数 1: listening fd；参数 2/3 可用于拿到客户端地址，这里不关心所以传 nullptr。
    // 成功返回新的 client fd；失败返回 -1。
    epoll_event events[16];
    int n=epoll_wait(epoll_fd.get(),events,16,-1);
    if(n==-1){
        if (errno == EINTR) {
            continue;
        }
        std::perror("epoll_wait");
        return 1;
    }

    for(int i=0;i<n;i++){  
        if(events[i].data.fd==socket_fd.get()){   
            UniqueFd client_fd(accept(socket_fd.get(),nullptr,nullptr));

            if(!client_fd.isValid()){
                if (errno == EINTR) {
                continue;
            }else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
                std::perror("accept");
                return 1;
            }
            if(!set_non_blocking(client_fd.get())){
                perror("fcntl");
                return 1;
            };
            int fd=client_fd.get();
            epoll_event client_event{};
            client_event.events=EPOLLIN;
            client_event.data.fd=client_fd.get();
            if(epoll_ctl(epoll_fd.get(),EPOLL_CTL_ADD,client_fd.get(),&client_event)==-1){
                std::perror("epoll_ctl");
                return 1;
            }
            clients.emplace(fd,std::move(client_fd));

            // 客户端连接后输出提示
            std::cout << "Client connected\n";


            // while(true){// 准备 buffer
            //     char buffer[4096];
            // // recv 收数据
            //     // recv(sockfd, buf, len, flags): 从 client fd 读取数据。
            //     // 参数 1: client fd；参数 2: 写入 buffer；参数 3: 最多读多少字节；参数 4: flags，这里用 0。
            //     // 返回值 >0 表示读到字节数；==0 表示对端关闭；==-1 表示出错。
            //     ssize_t byte_received=recv(client_fd.get(),buffer,sizeof(buffer),0);
            // // 如果收到数据
            //     if(byte_received>0){
            //         std::cout<<"Received "<<byte_received<<" bytes from client"<<std::endl;
            //         // 将数据原样发回去
            //         ssize_t total_sent=0;
            //         while(total_sent<byte_received){
            //             // send(sockfd, buf, len, flags): 向 client fd 发送数据。
            //             // 参数 1: client fd；参数 2: 待发送数据起点；参数 3: 本次最多发送字节数；参数 4: flags。
            //             // 返回值 >=0 表示本次实际发送字节数；==-1 表示出错。TCP 下可能 partial write。
            //             ssize_t byte_sent=send(client_fd.get(),buffer+total_sent,byte_received-total_sent,0);
                    
            //             if(byte_sent==-1){
            //                 if (errno == EINTR) {
            //                     continue;
            //                 }
            //                 std::perror("send");
            //                 break;
            //             }else{
            //                 total_sent += byte_sent;
            //                 std::cout<<"Sent "<<byte_sent<<" bytes back to client"<<std::endl;
            //             }
            //         }
            //     }else if(byte_received==0){
            //         std::cout<<"Client closed the connection"<<std::endl;
            //         break;
            //     }else{
            //         if (errno == EINTR) {
            //             continue;
            //         }else if(errno== EAGAIN||errno==EWOULDBLOCK){
            //                     std::cout<<"no data available now"<<std::endl;
            //                     break;
            //         }else{
            //             std::perror("recv");
            //                 break;     
            //         }
                
            //     }
            // }
        }
    }
}


return 0;
}
