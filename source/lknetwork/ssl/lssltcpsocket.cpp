#include "lssltcpsocket.h"

LK_NAMESPACE_BEGIN

LSslTcpSocket::LSslTcpSocket(const FD fd, std::shared_ptr<LSslCtx> ctx)
    : LTcpSocket(fd), LSsl(ctx) {

    if (m_ssl != nullptr)
    {
        SSL_set_fd(m_ssl, m_fd);

        if (const int ret = SSL_accept(m_ssl); ret != 1)
        {
            ::ERR_print_errors_fp(stderr);
            lWarn() << "Failed to accept TLS connection.";
        }
    }
}

LSslTcpSocket::LSslTcpSocket(const std::string &serverIp, const uint16_t serverPort, bool useAsync)
    : LTcpSocket(serverIp, serverPort, useAsync)
{
    if(isValidFD()){
        m_ctx = std::make_shared<LSslCtx>(false, false);
        initSsl();
        if (m_ssl != nullptr)
        {
            SSL_set_fd(m_ssl, m_fd);
            if (SSL_connect(m_ssl) <= 0)
            {
                ERR_print_errors_fp(stderr);
                lWarn() << "Failed to connect TLS server";
            }
        }
    }
}

int LSslTcpSocket::recv(char *buffer, int length) const
{
    if (m_ssl != nullptr){
        return SSL_read(m_ssl, buffer, length);
    }else{
        return LTcpSocket::recv(buffer, length);
    }
}

int LSslTcpSocket::send(const char *buffer, int length) const
{
    if (m_ssl != nullptr){
        return SSL_write(m_ssl, buffer, length);
    }else{
        return LTcpSocket::send(buffer, length);
    }
}
LK_NAMESPACE_END
