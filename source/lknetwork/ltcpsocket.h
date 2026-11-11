#ifndef LTCPSOCKET_H
#define LTCPSOCKET_H

#include "lknetwork/lsocket.h"
#include "lkcore/lsignal.h"

LK_NAMESPACE_BEGIN

class LKNETWORKDEF LTcpSocket  : public LSocket
{
public:
    auto operator=(LTcpSocket&) -> LTcpSocket& = delete;
    auto operator=(LTcpSocket&&) -> LTcpSocket& = delete;
    LTcpSocket(LTcpSocket&) = delete;

    LTcpSocket();
    LTcpSocket(const FD filedescriptor);
    LTcpSocket(const std::string& serverIp, const uint16_t serverPort, bool useAsync = true);

    LSignal<> sigReadyRead;
protected:
    virtual void runObjectPoll() override;

};

LK_NAMESPACE_END

#endif // LTCPSOCKET_H
