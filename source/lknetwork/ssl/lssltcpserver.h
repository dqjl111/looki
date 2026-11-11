#ifndef LSSLTCPSERVER_H
#define LSSLTCPSERVER_H

#include "lknetwork/ssl/lssltcpsocket.h"
#include "lknetwork/ltcpserver.h"

LK_NAMESPACE_BEGIN

class LKNETWORKDEF LSslTcpServer  : public LTcpServer
{
public:
    auto operator=(LSslTcpServer&) -> LSslTcpServer& = delete;
    auto operator=(LSslTcpServer&&) -> LSslTcpServer& = delete;
    LSslTcpServer(LSslTcpServer&) = delete;

    LSslTcpServer(const std::string& keyFile, const std::string& certFile);
    LSslTcpServer(const std::string& keyFile,
                  const std::string& certFile,
                  const uint16_t port,
                  const std::string &ipAddr = "0.0.0.0");

    virtual std::shared_ptr<LTcpSocket> accept(LSockaddr& address, bool useAsync = true) override;
protected:
    std::shared_ptr<LSslCtx> m_ctx;
};


LK_NAMESPACE_END

#endif // LSSLTCPSERVER_H
