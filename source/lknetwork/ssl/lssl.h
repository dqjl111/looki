#ifndef LSSL_H
#define LSSL_H

#include "lknetwork/lsocket.h"
#include "lkcore/lsignal.h"

#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

LK_NAMESPACE_BEGIN
class LSsl;
class LKNETWORKDEF LSslCtx{
public:
    auto operator=(LSslCtx&) -> LSslCtx& = delete;
    auto operator=(LSslCtx&&) -> LSslCtx& = delete;
    LSslCtx(LSslCtx&) = delete;

    LSslCtx(bool isServer = false, bool isUdp = false);
    virtual ~LSslCtx();
    void initSecureFiles(const std::string& keyFile, const std::string& certFile);

    inline bool isServer() const{
        return m_isServer;
    }
    inline bool isUdp() const{
        return m_isUdp;
    }
protected:
    static int genCookie(SSL *ssl, unsigned char* cookie, unsigned int* len) noexcept;
    static int verifyCookie(SSL* ssl, const unsigned char* cookie, unsigned int len) noexcept;
    static int verifyCallback (int val, X509_STORE_CTX *ctx) noexcept;

    /// @brief SSL/TLS context
    SSL_CTX* m_d = nullptr;
    uint8_t m_isServer:1;
    uint8_t m_isUdp:1;

    constexpr static unsigned int COOKIE_LEN = 16; // NOLINT
    static std::array<unsigned char, COOKIE_LEN> m_cookie; // NOLINT

    friend class LSsl;
};

class LKNETWORKDEF LSsl
{
public:
    auto operator=(LSsl&) -> LSsl& = delete;
    auto operator=(LSsl&&) -> LSsl& = delete;
    LSsl(LSsl&) = delete;

    LSsl();
    LSsl(std::shared_ptr<LSslCtx> ctx);
    virtual ~LSsl();
protected:
    void initSsl();
    /// @brief SSL/TLS instance
    SSL* m_ssl = nullptr;
    std::shared_ptr<LSslCtx> m_ctx = nullptr;

};


LK_NAMESPACE_END
#endif // LSSL_H
