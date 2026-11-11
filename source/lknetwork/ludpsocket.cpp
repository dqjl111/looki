#include "ludpsocket.h"
#include "lkcore/ldebug.h"
#include "lkcore/lthread.h"

LK_NAMESPACE_BEGIN

LUdpSocket::LUdpSocket() : LSocket(AF_INET, SOCK_DGRAM, 0){
}

LUdpSocket::LUdpSocket(const uint16_t port, const std::string &ipAddr, bool useAsync)
 : LSocket(AF_INET, SOCK_DGRAM, 0)
{
    int ret = useAsync ? asyncBind(ipAddr, port) :bind(ipAddr, port);
    if(ret<0){
        lWarn() << "Failed to bind UDP server:" << ret;
    }
}

void LUdpSocket::runObjectPoll()
{
    if(bytesAvaliable() >0 ){
        sigReadyRead.emt();
    }
}

int LUdpSocket::sendto(const char *buffer, int length, const std::string &ipAddr, const uint16_t port) const
{
    LSockaddr t;
    if(buildSockAddr(ipAddr, port, t)){
        return sendto(buffer, length, t);
    }
    return 0;
}

int LUdpSocket::recvfrom(char *buffer, int length, std::string &ipAddr, uint16_t &port) const
{
    LSockaddr t;
    int s = recvfrom(buffer, length, t);
    if(s >0){
        ipAddr = sockAddrHost(t);
        port = t.port();
    }
    return s;
}


LK_NAMESPACE_END
