#ifndef LSSLUDPSOCKET_H
#define LSSLUDPSOCKET_H

#include "lknetwork/lsocket.h"
#include "lknetwork/ssl/lssl.h"

LK_NAMESPACE_BEGIN
//使用DTLS 点对点进行UDP通信（一对一加密通信）
class LKNETWORKDEF LSslUdpSocket : public LSocket, public LSsl
{
public:
    auto operator=(LSslUdpSocket&) -> LSslUdpSocket& = delete;
    auto operator=(LSslUdpSocket&&) -> LSslUdpSocket& = delete;
    LSslUdpSocket(LSslUdpSocket&) = delete;

    LSslUdpSocket(const std::string& keyFile, const std::string& certFile, bool isServerSide = false);
    LSslUdpSocket(const std::string& keyFile,
                  const std::string& certFile,
                  const uint16_t serverPort,
                  const std::string &serverIp = "0.0.0.0",
                  bool isServerSide = false,
                  bool useAsync = true);

    virtual ~LSslUdpSocket();

    virtual int recv(char* buffer, int length) const override;
    virtual int send(const char* buffer, int length) const override;

    bool initBio(const uint16_t serverPort, const std::string &serverIp = "0.0.0.0", bool useAsync = true) noexcept;
    bool initBio(LSockaddr& addr, bool useAsync = true);

    LSignal<> sigReadyRead;
protected:
    BIO* m_bio = nullptr;
    virtual void runObjectPoll() override;
};


LK_NAMESPACE_END
#endif // LSSLUDPSOCKET_H
