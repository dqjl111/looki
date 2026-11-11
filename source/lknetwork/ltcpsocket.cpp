#include "ltcpsocket.h"
#include "lkcore/ldebug.h"
LK_NAMESPACE_BEGIN

LTcpSocket::LTcpSocket() : LSocket(AF_INET, SOCK_STREAM, 0){
    setSockOptNoDelay(true);
}

LTcpSocket::LTcpSocket(const FD filedescriptor): LSocket(filedescriptor)
{
}

LTcpSocket::LTcpSocket(const std::string& serverIp, const uint16_t serverPort, bool useAsync)
    :LSocket(AF_INET, SOCK_STREAM, 0){
    setSockOptNoDelay(true);
    int ret = useAsync ? asyncConnect(serverIp, serverPort) :connect(serverIp, serverPort);
    if(ret<0){
        lWarn() << "Failed to connect TCP server:" << ret;
    }
}

void LTcpSocket::runObjectPoll()
{
    if(bytesAvaliable() >0 ){
        sigReadyRead.emt();
    }
}


LK_NAMESPACE_END
