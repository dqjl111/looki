#include "lssl.h"

LK_NAMESPACE_BEGIN

void initSslSystem()
{
    static bool initialized = false;
    if (!initialized)
    {
        SSL_library_init();
        SSL_load_error_strings();
        OpenSSL_add_ssl_algorithms();

        initialized = true;
    }
}

std::array<unsigned char, LSslCtx::COOKIE_LEN> LSslCtx::m_cookie; // NOLINT

/**
 * @brief Generate a new cookie to use for the DTLS connection
 *
 * @param ssl SSL context
 * @param cookie Buffer to create the cookie
 * @param len Cookie buffer length
 *
 * @return int 1 for success
 */
// NOLINTNEXTLINE
int LSslCtx::genCookie(SSL *ssl, unsigned char* cookie, unsigned int* len) noexcept
{
    std::srand(std::time(nullptr)); // NOLINT
    for (unsigned int i = 0; i < COOKIE_LEN; ++i)
    {
        m_cookie[i] = static_cast<unsigned char>(::rand()); // NOLINT
    }

    LSockaddr addr;
    static_cast<void>(BIO_dgram_get_peer(SSL_get_rbio(ssl), &addr.a));
    std::array<unsigned char, EVP_MAX_MD_SIZE> result = {};
    unsigned int resLen = 0;
    if(addr.a.sa_family == AF_INET6){
        const auto length = sizeof(in6_addr) + sizeof(uint16_t);
        std::array<unsigned char, length> buffer = {};
        ::memcpy(buffer.data(), &addr.a6.sin6_port, sizeof(uint16_t));
        ::memcpy(&buffer[sizeof(uint16_t)], &addr.a6.sin6_addr, sizeof(in6_addr));
        HMAC(EVP_sha1(), m_cookie.data(), COOKIE_LEN, buffer.data(), length, result.data(), &resLen);
    }else{
        const auto length = sizeof(in_addr) + sizeof(uint16_t);
        std::array<unsigned char, length> buffer = {};
        ::memcpy(buffer.data(), &addr.a4.sin_port, sizeof(uint16_t));
        ::memcpy(&buffer[sizeof(uint16_t)], &addr.a4.sin_addr, sizeof(in_addr));
        HMAC(EVP_sha1(), m_cookie.data(), COOKIE_LEN, buffer.data(), length, result.data(), &resLen);
    }
    ::memcpy(cookie, result.data(), resLen);
    *len = resLen;
    return 1;
}

/**
 * @brief Verify the cookie passed in is valid
 *
 * @param ssl SSL context
 * @param cookie Cookie to verify
 * @param len Length of cookie
 *
 * @return int 1 for valid cookie, 0 for invalid
 */
// NOLINTNEXTLINE
int LSslCtx::verifyCookie(SSL* ssl, const unsigned char* cookie, unsigned int len) noexcept
{
    LSockaddr addr;
    std::array<unsigned char, EVP_MAX_MD_SIZE> result = {};
    unsigned int resLen = 0;
    static_cast<void>(BIO_dgram_get_peer(SSL_get_rbio(ssl), &addr.a));
    if(addr.a.sa_family == AF_INET6){
        const auto length = sizeof(in6_addr) + sizeof(uint16_t);
        std::array<unsigned char, length> buffer = {};
        ::memcpy(buffer.data(), &addr.a6.sin6_port, sizeof(uint16_t));
        ::memcpy(&buffer[sizeof(uint16_t)], &addr.a6.sin6_addr, sizeof(in6_addr));
        HMAC(EVP_sha1(), m_cookie.data(), COOKIE_LEN, buffer.data(), length, result.data(), &resLen);
    }else{
        const auto length = sizeof(in_addr) + sizeof(uint16_t);
        std::array<unsigned char, length> buffer = {};
        ::memcpy(buffer.data(), &addr.a4.sin_port, sizeof(uint16_t));
        ::memcpy(&buffer[sizeof(uint16_t)], &addr.a4.sin_addr, sizeof(in_addr));
        HMAC(EVP_sha1(), m_cookie.data(), COOKIE_LEN, buffer.data(), length, result.data(), &resLen);
    }

    if ((len == resLen) && (::memcmp(result.data(), cookie, resLen) == 0))
    {
        return 1;
    }
    return 0;
}

/**
 * @brief Callback verification method
 *
 * @return int 1 for good callback
 */
// NOLINTNEXTLINE
int LSslCtx::verifyCallback (int val, X509_STORE_CTX *ctx) noexcept
{
    static_cast<void>(val);
    static_cast<void>(ctx);

    return 1;
}


LSslCtx::LSslCtx(bool isServer, bool isUdp)
    :m_isServer(isServer), m_isUdp(isUdp)
{
    initSslSystem();
    if(m_isUdp){
        if(m_isServer){
            m_d = SSL_CTX_new(DTLS_server_method());
        }else{
            m_d = SSL_CTX_new(DTLS_client_method());
        }
    }else{
        if(m_isServer){
            m_d = SSL_CTX_new(TLS_server_method());
        }else{
            m_d = SSL_CTX_new(TLS_client_method());
        }
    }

    if(m_d){
        ERR_print_errors_fp(stderr);
        lWarn() << "Failed to new ssl ctx!";
    }
}

LSslCtx::~LSslCtx()
{
    if (m_d != nullptr)
    {
        SSL_CTX_free(m_d);
        m_d = nullptr;
    }
}

void LSslCtx::initSecureFiles(const std::string &keyFile, const std::string &certFile)
{
    if (certFile.empty() || keyFile.empty())
    {
        lWarn() << "Empty key file or cert file for TCP server.";
        return;
    }

    if(m_d != nullptr){
        if (SSL_CTX_use_certificate_file(m_d, certFile.c_str(), SSL_FILETYPE_PEM) != 1)
        {
            ERR_print_errors_fp(stderr);
            lWarn() << "Failed to use pem certificate file";
            return;
        }
        if (SSL_CTX_use_PrivateKey_file(m_d, keyFile.c_str(), SSL_FILETYPE_PEM) != 1)
        {
            ERR_print_errors_fp(stderr);
            lWarn() << "Failed to use pem private key file";
            return;
        }
        if (SSL_CTX_check_private_key(m_d) != 1)
        {
            ERR_print_errors_fp(stderr);
            lWarn() << "Keys do not match!";
            return;
        }

        if(m_isUdp){
            if(m_isServer){
                SSL_CTX_set_verify(m_d, SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE, LSslCtx::verifyCallback);

                SSL_CTX_set_read_ahead(m_d, 1);
                SSL_CTX_set_cookie_generate_cb(m_d, LSslCtx::genCookie);
                SSL_CTX_set_cookie_verify_cb(m_d, LSslCtx::verifyCookie);
            }else{
                SSL_CTX_set_verify_depth(m_d, 2);
                SSL_CTX_set_read_ahead(m_d, 1);
            }
        }
    }
}

LSsl::LSsl(){}

LSsl::LSsl(std::shared_ptr<LSslCtx> ctx): m_ctx(ctx)
{
    initSsl();
}

LSsl::~LSsl()
{
    if (m_ssl != nullptr)
    {
        SSL_shutdown(m_ssl);
        SSL_free(m_ssl);
        m_ssl = nullptr;
    }
}

void LSsl::initSsl()
{
    if(m_ctx != nullptr){
        if(m_ctx->m_d != nullptr){
            m_ssl = SSL_new(m_ctx->m_d);
        }
    }
}



LK_NAMESPACE_END
