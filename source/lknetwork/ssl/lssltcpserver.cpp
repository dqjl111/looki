#include "lssltcpserver.h"

LK_NAMESPACE_BEGIN
LSslTcpServer::LSslTcpServer(const std::string &keyFile, const std::string &certFile) : LTcpServer()
{
    m_ctx = std::make_shared<LSslCtx>(true, false);
    m_ctx->initSecureFiles(keyFile, certFile);
}

LSslTcpServer::LSslTcpServer(const std::string &keyFile, const std::string &certFile, const uint16_t port, const std::string &ipAddr)
    :LTcpServer(port, ipAddr)
{
    m_ctx = std::make_shared<LSslCtx>(true, false);
    m_ctx->initSecureFiles(keyFile, certFile);
}

std::shared_ptr<LTcpSocket> LSslTcpServer::accept(LSockaddr &address, bool useAsync)
{
    int addrLen = address.length();
    FD clientFD = ::accept(m_fd, &address.a, &addrLen);
    if(isValidFD(clientFD)  && isSocketAcceptable(address)){
        std::shared_ptr<LSslTcpSocket> s= std::make_shared<LSslTcpSocket>(clientFD, m_ctx);
        if(useAsync){
            s->connectObjectPoll();
        }
        return s;
    }
    return nullptr;
}


LK_NAMESPACE_END
