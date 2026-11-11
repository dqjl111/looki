#ifndef LSSLTCPSOCKET_H
#define LSSLTCPSOCKET_H

#include "lknetwork/ltcpsocket.h"
#include "lknetwork/ssl/lssl.h"

LK_NAMESPACE_BEGIN

class LKNETWORKDEF LSslTcpSocket : public LTcpSocket, public LSsl
{
public:
    auto operator=(LSslTcpSocket&) -> LSslTcpSocket& = delete;
    auto operator=(LSslTcpSocket&&) -> LSslTcpSocket& = delete;
    LSslTcpSocket(LSslTcpSocket&) = delete;

    LSslTcpSocket(const FD fd, std::shared_ptr<LSslCtx> ctxHolder);
    LSslTcpSocket(const std::string &serverIp, const uint16_t serverPort, bool useAsync);

    virtual int recv(char* buffer, int length) const override;
    virtual int send(const char* buffer, int length) const override;
};


LK_NAMESPACE_END
#endif // LSSLTCPSOCKET_H
