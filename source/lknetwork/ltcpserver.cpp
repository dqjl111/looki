#include "ltcpserver.h"
#include "lkcore/ldebug.h"

LK_NAMESPACE_BEGIN

LTcpServer::LTcpServer() : LSocket(AF_INET, SOCK_STREAM, 0){
    setSockOptReuse(true);
}

LTcpServer::LTcpServer(const uint16_t port, const std::string &ipAddr)
    : LSocket(AF_INET, SOCK_STREAM, 0)
{
    setSockOptReuse(true);
    if (bind(ipAddr, port) <0)
    {
        lWarn() << "Failed to bind TCP server.";
    }
}

int LTcpServer::listen(int backlog, bool useAsync)
{
    int error = ::listen(m_fd, backlog);
    if(error == 0){
        if(useAsync){
            connectObjectPoll();
        }
    }
    return error;
}

bool LTcpServer::isListening()
{
    int opt;
    socklen_t optLength = sizeof(opt);
#ifdef __GNUC__
    if (getsockopt(SOL_SOCKET, SO_ACCEPTCONN, &opt, &optLength) == 0) {
#else
    if (getSockOpt(SOL_SOCKET, SO_ACCEPTCONN, reinterpret_cast<char*>(&opt), &optLength) == 0) {
#endif
        return opt;
    }
    return false;
}

bool LTcpServer::hasPendingConnections()
{
    pollfd fd={m_fd, POLLIN, 0};
#ifdef __GNUC__
    int res = poll(&fd, 1, 0);
#else
    int res = WSAPoll(&fd, 1, 0);
#endif
    if(res == SOCKET_ERROR){
        lWarn() << "LTcpServer poll error:" << res;
        return false;
    }

    if(res>0)
    {
        return fd.revents & POLLIN;
    }
    return false;
}

std::shared_ptr<LTcpSocket> LTcpServer::accept(LSockaddr& address, bool useAsync)
{
    int addrLen = address.length();
    FD clientFD = ::accept(m_fd, &address.a, &addrLen);
    if(isValidFD(clientFD)  && isSocketAcceptable(address)){
        std::shared_ptr<LTcpSocket> s= std::make_shared<LTcpSocket>(clientFD);
        if(useAsync){
            s->connectObjectPoll();
        }
        return s;
    }
    return nullptr;

}

bool LTcpServer::isSocketAcceptable(const LSockaddr &address) const
{
    return address.a.sa_family == AF_INET6 || address.a.sa_family == AF_INET;
}

void LTcpServer::runObjectPoll()
{
    if(hasPendingConnections()){
        lWarn() << "hasPendingConnections";
        sigNewConnection.emt();
    }
}
LK_NAMESPACE_END
