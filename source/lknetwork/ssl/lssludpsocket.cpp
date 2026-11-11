#include "lssludpsocket.h"

LK_NAMESPACE_BEGIN

LSslUdpSocket::LSslUdpSocket(const std::string &keyFile,
                             const std::string &certFile,
                             bool isServerSide): LSocket(AF_INET, SOCK_DGRAM, 0)
{
    if(isServerSide){
        setSockOptReuse(true);
    }
    m_ctx = std::make_shared<LSslCtx>(isServerSide, true);
    m_ctx->initSecureFiles(keyFile, certFile);

    initSsl();
}

LSslUdpSocket::LSslUdpSocket(const std::string &keyFile,
                             const std::string &certFile,
                             const uint16_t serverPort,
                             const std::string &serverIp,
                             bool isServerSide,
                             bool useAsync):LSslUdpSocket(keyFile, certFile, isServerSide)
{
    initBio(serverPort, serverIp, useAsync);
}

LSslUdpSocket::~LSslUdpSocket()
{
    if(m_bio != nullptr){
        BIO_free_all(m_bio);
        m_bio = nullptr;
    }
}

int LSslUdpSocket::recv(char *buffer, int length) const
{
    if (m_ssl != nullptr){
        return SSL_read(m_ssl, buffer, length);
    }else{
        return LSocket::recv(buffer, length);
    }
}

int LSslUdpSocket::send(const char *buffer, int length) const
{
    if (m_ssl != nullptr){
        return SSL_write(m_ssl, buffer, length);
    }else{
        return LSocket::send(buffer, length);
    }
}

bool LSslUdpSocket::initBio(const uint16_t serverPort, const std::string &serverIp, bool useAsync) noexcept
{
    LSockaddr addr;
    if(buildSockAddr(serverIp, serverPort, addr)){
        return initBio(addr, useAsync);
    }else{
        lWarn() << "Invalid server ip or port!";
        return false;
    }
}

bool LSslUdpSocket::initBio(LSockaddr& addr, bool useAsync)
{
    if(m_bio != nullptr){
        return true;
    }
    int ret;
    if(m_ctx->isServer()){
        ret = useAsync ? asyncBind(addr) :bind(addr);
    }else{
        ret = useAsync ? asyncConnect(addr) :connect(addr);
    }

    if(ret<0){
        lWarn() << "Failed to connect Udp Secure server:" << ret;
        return false;
    }else{
        if(isValidFD()){
            if(m_ctx->isServer()){
                m_bio = BIO_new_dgram(m_fd, BIO_NOCLOSE);
                if(m_bio != nullptr){
                    timeval tval{.tv_sec = 5, .tv_usec = 0};
                    BIO_ctrl(m_bio, BIO_CTRL_DGRAM_SET_RECV_TIMEOUT, 0, &tval);
                    SSL_set_bio(m_ssl, m_bio, m_bio);
                    SSL_set_options(m_ssl, SSL_OP_COOKIE_EXCHANGE);
                }
            }else{
                m_bio = BIO_new_dgram(m_fd, BIO_CLOSE);
                if(m_bio != nullptr){
                     BIO_ctrl(m_bio, BIO_CTRL_DGRAM_SET_CONNECTED, 0, &addr.a);
                     if(m_ssl != nullptr){
                         SSL_set_bio(m_ssl, m_bio, m_bio);
                         if (SSL_connect(m_ssl) <= 0){
                             ERR_print_errors_fp(stderr);
                         }else{
                             return true;
                         }
                     }
                }
            }
        }
    }
    return false;
}

void LSslUdpSocket::runObjectPoll()
{
    if(m_ctx->isServer()){
        LSockaddr addr;
        if(DTLSv1_listen(m_ssl, reinterpret_cast<BIO_ADDR*>(&addr.a)) > 0){
            SSL_accept(m_ssl);
        }
    }

    if(bytesAvaliable() >0 ){
        sigReadyRead.emt();
    }
}


LK_NAMESPACE_END
