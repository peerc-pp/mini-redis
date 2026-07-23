#include "base/unique_fd.h"
#include <unistd.h>

namespace mini_redis{
     UniqueFd::UniqueFd(int fd) noexcept:fd_(fd){}
     int UniqueFd::get() const noexcept{
        return fd_;
     }
     bool UniqueFd::is_valid() const noexcept{
        return fd_ >= 0;
     }
     UniqueFd:: UniqueFd(UniqueFd&& other) noexcept{
        fd_=other.fd_;
        other.fd_=kInvalidFd;
     }
     UniqueFd& UniqueFd:: operator=(UniqueFd&& other) noexcept{
        if(this!=&other){
                if(fd_>=0){
                    // close(fd): 释放当前 fd 对应的内核资源。
                    // 参数 fd 要求是一个有效 file descriptor；重复 close 同一个 fd 很危险。
                    close(fd_);
                }
                fd_=other.fd_;
                other.fd_=kInvalidFd;
      
        }
         return *this;
     }
     UniqueFd::~UniqueFd(){
        if(fd_!=kInvalidFd){
            close(fd_);
        }
     }
}