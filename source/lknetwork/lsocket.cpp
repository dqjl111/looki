#include "lsocket.h"
#include "lkcore/ldebug.h"

#ifdef __GNUC__
#include <fcntl.h>
#endif
LK_NAMESPACE_BEGIN


class LSockContext
{
protected:
#ifdef _WIN32
    WSADATA m_wsaData; // NOLINT
#endif

public:
    LSockContext(){
#ifdef _WIN32
        if (::WSAStartup(MAKEWORD(2, 2), &m_wsaData) != 0)
        {
            lWarn() << "Could not start-up Windows sockets";
        }
#endif
    }
    ~LSockContext(){
#ifdef _WIN32
        ::WSACleanup();
#endif
    }
};


LSockContext* sockContext(){
    static LSockContext ctx;
    return &ctx;
}

LSocket::LSocket(FD filedescriptor) :m_fd(filedescriptor)
{
    sockContext();
}

LSocket::LSocket(const int domain, const int type, const int protocol)
{
    sockContext();
    initSocket(domain, type, protocol);
}

LSocket::~LSocket()
{
    close();
}

bool LSocket::buildSockAddr(const std::string &host, const uint16_t &port, LSockaddr &address)
{
    LUrl url;
    addrinfo hints{};
    hints.ai_socktype = AF_UNSPEC;

    url.setHost(host);
    url.setPort(port);
    if(url.ipVersion() == 6){
        hints.ai_family = AF_INET6;
    }else{
        hints.ai_family = AF_INET;
    }
    std::unique_ptr<addrinfo, decltype(&::freeaddrinfo)> resultlistOwner{nullptr, &::freeaddrinfo};
    addrinfo* tmpResultlist = resultlistOwner.get();
    int ret;
    ret = ::getaddrinfo(url.host().data(), url.port().data(), &hints, &tmpResultlist);
    resultlistOwner.reset(tmpResultlist);

    if (ret == 0){
        address = *resultlistOwner->ai_addr;
        return true;
    }
    return false;
}

std::string LSocket::sockAddrHost(const LSockaddr &address)
{
    char name[INET6_ADDRSTRLEN];
    ::getnameinfo(&address.a, address.length(),
                  name,
                  INET6_ADDRSTRLEN,
                  nullptr,
                  0,
                  NI_NUMERICHOST | NI_NUMERICSERV);
    return name;
}

void LSocket::initSocket(const int domain, const int type, const int protocol)
{
    m_fd = ::socket(domain, type, protocol);
}

int LSocket::bind(const std::string &ipAddr, const uint16_t port) const
{
    LSockaddr t;
    if(buildSockAddr(ipAddr, port, t)){
        return bind(t);
    }
    return -1;
}

int LSocket::connect(const LSockaddr &address) const
{
    return ::connect(m_fd, &address.a, address.length());
}

int LSocket::connect(const std::string &ipAddr, const uint16_t port) noexcept
{
    LSockaddr t;
    if(buildSockAddr(ipAddr, port, t)){
        return connect(t);
    }
    return -1;
}

int LSocket::asyncBind(const LSockaddr &address)
{
    int ret = bind(address);
    if(ret == 0){
        connectObjectPoll();
    }else{
        disconnectObjectPoll();
    }
    return ret;
}

int LSocket::asyncBind(const std::string &ipAddr, const uint16_t port)
{
    int ret = bind(ipAddr, port);
    if(ret == 0){
        connectObjectPoll();
    }else{
        disconnectObjectPoll();
    }
    return ret;
}

int LSocket::asyncConnect(const LSockaddr &address)
{
    int ret = connect(address);
    if(ret == 0){
        connectObjectPoll();
    }else{
        disconnectObjectPoll();
    }
    return ret;
}

int LSocket::asyncConnect(const std::string &ipAddr, const uint16_t port) noexcept
{
    int ret = connect(ipAddr, port);
    if(ret == 0){
        connectObjectPoll();
    }else{
        disconnectObjectPoll();
    }
    return ret;
}

int LSocket::recv(char *buffer, int length) const
{
    return ::recv(m_fd, buffer, length, 0);
}

int LSocket::send(const char *buffer, int length) const
{
    return ::send(m_fd, buffer, length, 0);
}

bool LSocket::setSockOptReuse(bool isReuse)
{
    int opt = isReuse? 1: 0;
#ifdef __GNUC__
    if (setsockopt(SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) != 0)
#else
    if (setSockOpt(SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt)) == SOCKET_ERROR)
#endif
    {
        lWarn() << "Failed to set SockOptReus:" << isReuse;
        return false;
    }
    return true;
}

bool LSocket::setSockOptNoDelay(bool isNoDelay)
{
    int opt = isNoDelay? 1: 0;
#ifdef __GNUC__
    if (setsockopt(IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)) != 0)
#else
    if (setSockOpt(IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&opt), sizeof(opt)) == SOCKET_ERROR)
#endif
    {
        lWarn() << "Failed to set SockOptNoDelay:" << isNoDelay;
        return false;
    }
    return true;
}

bool LSocket::setNonBlocking(bool nonBlocking)
{
    if(!isValidFD()){
        return false;
    }
#ifdef __GNUC__
    const int flags = ::fcntl(m_fd, F_GETFL, 0);
    if(flags < 0){
        lWarn() << "Failed to get socket flags:" << errno;
        return false;
    }
    const int newFlags = nonBlocking ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
    if(::fcntl(m_fd, F_SETFL, newFlags) < 0){
        lWarn() << "Failed to set socket non-blocking:" << errno;
        return false;
    }
#else
    u_long mode = nonBlocking ? 1 : 0;
    if(::ioctlsocket(m_fd, FIONBIO, &mode) == SOCKET_ERROR){
        lWarn() << "Failed to set socket non-blocking:" << WSAGetLastError();
        return false;
    }
#endif
    return true;
}

int LSocket::socketError()
{
    int opt = 0;
    socklen_t optlen = sizeof(opt);
#ifdef __GNUC__
    int result = getsockopt(SOL_SOCKET, SO_ERROR, &opt, &optlen);
#else
    int result = getSockOpt(SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&opt), &optlen);
#endif
    if (result == -1) { // 获取SO_ERROR失败
        return -1; // 错误情况
    }
    return opt;
}


void LSocket::close()
{
    disconnectObjectPoll();
    if(isValidFD()){
        FD fd = m_fd;
        m_fd = INVALID_FD;
#ifdef __GNUC__
        ::shutdown(fd, SHUT_RDWR);
        ::close(fd);
#else
        ::shutdown(fd, SD_BOTH);
        ::closesocket(fd);
#endif 
    }
}

size_t LSocket::bytesAvaliable() const
{
    u_long ret =0;
    if(isValidFD()){
#ifdef __GNUC__
        ::ioctl(m_fd, FIONREAD, &ret);
#else
        ::ioctlsocket(m_fd, FIONREAD, &ret);
#endif
    }
    return ret;
}
LK_NAMESPACE_END
