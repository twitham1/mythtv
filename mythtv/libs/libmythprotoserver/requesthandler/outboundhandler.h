#ifndef _OUTBOUNDREQUESTHANDLER_H_
#define _OUTBOUNDREQUESTHANDLER_H_

#include <QTimer>
#include <QString>
#include <QStringList>

#include "mythsocket.h"
#include "mythsocketmanager.h"
#include "socketrequesthandler.h"
#include "sockethandler.h"
#include "mythprotoserverexp.h"

class MainServer;

class PROTOSERVER_PUBLIC OutboundRequestHandler : public SocketRequestHandler
{
    Q_OBJECT
  public:
    OutboundRequestHandler(void);
    QString GetHandlerName(void) override // SocketRequestHandler
        { return "OUTBOUND"; }
    void connectionClosed(MythSocket *socket) override; // SocketRequestHandler

  public slots:
    void ConnectToMaster(void);

  protected:
    virtual bool AnnounceSocket(void)               { return false; }
    MythSocket *m_socket;

  private:
    bool DoConnectToMaster(void);
    QTimer      m_timer;
};

#endif
