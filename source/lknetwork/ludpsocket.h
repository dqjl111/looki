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
        const int ret = (int)::recvfrom(m_fd, buffer, length, 0, &address.a, &addrLen);
#ifdef _WIN32
        // Windows 特有问题：若之前向“无人监听”的端口发送过数据，会收到 ICMP 端口不可达，
        // 下一次 recvfrom 返回 WSAECONNRESET(10054)。这不是真正的接收错误，
        // 按“无数据可读”处理，并清空错误码，避免上层误判为故障。
        if (ret < 0 && WSAGetLastError() == WSAECONNRESET) {
            WSASetLastError(0);
            return 0;
        }
#endif
        return ret;
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
