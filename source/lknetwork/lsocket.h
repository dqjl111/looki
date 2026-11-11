#ifndef LSOCKET_H
#define LSOCKET_H

#include "lkcore/io/lurl.h"
#include "lkcore/lobject.h"

#ifdef __GNUC__
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#include <BaseTsd.h>
#endif

#include <string>

LK_NAMESPACE_BEGIN


union LSockaddr {
    sockaddr a;
    sockaddr_in a4;
    sockaddr_in6 a6;

    inline int length() const{
        return a.sa_family == AF_INET6 ? sizeof(a6) : sizeof(a4);
    }

    inline const uint16_t& port() const{
        return a.sa_family == AF_INET6 ? a6.sin6_port : a4.sin_port;
    }

    LSockaddr& operator= (const sockaddr& a){
        if(a.sa_family == AF_INET6){
            a6 = reinterpret_cast<const sockaddr_in6&>(a);
        }else{
            a4 = reinterpret_cast<const sockaddr_in&>(a);
        }
        return *this;
    }
};

class LKNETWORKDEF LSocket : public LObject
{
public:
    auto operator=(LSocket&) -> LSocket& = delete;
    auto operator=(LSocket&&) -> LSocket& = delete;
    LSocket(LSocket&) = delete;

#ifdef __GNUC__
    using FD = int;
    const static FD INVALID_FD = -1;
#else
    using FD = SOCKET;
    const static FD INVALID_FD = INVALID_SOCKET;
#endif

    LSocket(FD filedescriptor = INVALID_FD);
    LSocket(const int domain, const int type, const int protocol);
    virtual ~LSocket();

    static bool buildSockAddr(const std::string& host, const uint16_t& port, LSockaddr& address);
    static std::string sockAddrHost(const LSockaddr& address);

    void initSocket(const int domain, const int type, const int protocol);
    inline int bind(const LSockaddr& address) const{
        return ::bind(m_fd, &address.a, address.length());
    }

    int bind(const std::string& ipAddr, const uint16_t port) const;
    int connect(const LSockaddr& address) const;
    int connect(const std::string& ipAddr, const uint16_t port) noexcept;

    int asyncBind(const LSockaddr& address);
    int asyncBind(const std::string& ipAddr, const uint16_t port);
    int asyncConnect(const LSockaddr& address);
    int asyncConnect(const std::string& ipAddr, const uint16_t port) noexcept;

    virtual int recv(char* buffer, int length) const;
    virtual int send(const char* buffer, int length) const;

    inline int getSockOpt(const int level, const int optname, char *optval, socklen_t* optlen) const{
        return ::getsockopt(m_fd, level, optname, optval, optlen);
    }

    inline int setSockOpt(const int level, const int optname, const char *optval, const int optlen) const{
        return ::setsockopt(m_fd, level, optname, optval, optlen);
    }

    bool setSockOptReuse(bool isReuse = true);
    bool setSockOptNoDelay(bool isNoDelay = true);
    //-1-获取状态失败 0-连接正常 其他-连接可能存在问题或已断开
    int socketError();

    inline const FD& getFd() const{
        return m_fd;
    }

    inline static bool isValidFD(const FD& fd){
#ifdef __GNUC__
        return fd >= 0;
#else
        return fd != INVALID_SOCKET;
#endif
    }

    inline bool isValidFD() const{
        return isValidFD(m_fd);
    }

    void close();
    size_t bytesAvaliable() const;
    inline auto lastError() const{
#ifdef __GNUC__
        return stderror(errno);
#else
        return WSAGetLastError();
#endif
    }
protected:
    FD m_fd = INVALID_FD;

};


LK_NAMESPACE_END

#endif //LSOCKET_H
