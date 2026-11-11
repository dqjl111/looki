#ifndef LTCPSERVER_H
#define LTCPSERVER_H

#include "lknetwork/ltcpsocket.h"

LK_NAMESPACE_BEGIN

class LKNETWORKDEF LTcpServer : public LSocket
{
public:
    auto operator=(LTcpServer&) -> LTcpServer& = delete;
    auto operator=(LTcpServer&&) -> LTcpServer& = delete;
    LTcpServer(LTcpServer&) = delete;

    LTcpServer();
    LTcpServer(const uint16_t port, const std::string &ipAddr = "0.0.0.0");

    //backlog最大挂起等待的连接数，=0时取系统默认值
    int listen(int backlog = 0, bool useAsync = true);

    bool isListening();
    bool hasPendingConnections();

    virtual std::shared_ptr<LTcpSocket> accept(LSockaddr& address, bool useAsync = true);
    virtual bool isSocketAcceptable(const LSockaddr& address) const;

    LSignal<> sigNewConnection;
protected:
    virtual void runObjectPoll() override;
};


LK_NAMESPACE_END

#endif // LTCPSERVER_H
