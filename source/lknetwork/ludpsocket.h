#ifndef LUDPSOCKET_H
#define LUDPSOCKET_H

#include "lknetwork/lsocket.h"
#include "lkcore/lobject.h"
#include "lkcore/lsignal.h"

LK_NAMESPACE_BEGIN

class LKNETWORKDEF LUdpSocket : public LSocket
{
public:
    auto operator=(LUdpSocket&) -> LUdpSocket& = delete;
    auto operator=(LUdpSocket&&) -> LUdpSocket& = delete;
    LUdpSocket(LUdpSocket&) = delete;

    LUdpSocket();
    LUdpSocket(const uint16_t port, const std::string &ipAddr = "0.0.0.0", bool useAsync = true);

    inline int recvfrom(char* buffer, int length, LSockaddr& address) const
    {
        int addrLen = address.length();
        return ::recvfrom(m_fd, buffer, length, 0, &address.a, &addrLen);
    }

    int recvfrom(char* buffer, int length, std::string& ipAddr, uint16_t& port) const;

    inline int sendto(const char* buffer, int length, const LSockaddr& address) const
    {
        return ::sendto(m_fd, buffer, length, 0, &address.a, address.length());
    }

    int sendto(const char* buffer, int length, const std::string& ipAddr, const uint16_t port) const;


    LSignal<> sigReadyRead;
protected:
    void initOpt();
    virtual void runObjectPoll() override;
};


LK_NAMESPACE_END

#endif // LUDPSOCKET_H
