
#include "websocket_mythevent.h"
#include "mythcorecontext.h"
#include "mythevent.h"
#include "mythlogging.h"

WebSocketMythEvent::WebSocketMythEvent()
{
    setObjectName("WebSocketMythEvent");
    gCoreContext->addListener(this);
}

WebSocketMythEvent::~WebSocketMythEvent()
{
    gCoreContext->removeListener(this);
}

bool WebSocketMythEvent::HandleTextFrame(const WebSocketFrame &frame)
{
    QString message = QString(frame.m_payload);

    if (message.isEmpty())
        return false;

    QStringList tokens = message.split(" ", QString::SkipEmptyParts);

    if (tokens[0] == "WS_EVENT_ENABLE") // Only send events if asked
    {
        m_sendEvents = true;
        LOG(VB_HTTP, LOG_NOTICE, "WebSocketMythEvent: Enabled");
    }
    else if (tokens[0] == "WS_EVENT_DISABLE")
    {
        m_sendEvents = false;
        LOG(VB_HTTP, LOG_NOTICE, "WebSocketMythEvent: Disabled");
    }
    else if (tokens[0] == "WS_EVENT_SET_FILTER")
    {
        m_filters.clear();

        if (tokens.length() == 1)
            return true;

        m_filters = tokens.mid(1);

        QString filterString = m_filters.join(", ");
        LOG(VB_HTTP, LOG_NOTICE, QString("WebSocketMythEvent: Updated filters (%1)").arg(filterString));
    }

    return false;
}

void WebSocketMythEvent::customEvent(QEvent* event)
{
    if (event->type() == MythEvent::MythEventMessage)
    {
        if (!m_sendEvents)
            return;

        MythEvent *me = static_cast<MythEvent *>(event);
        QString message = me->Message();

        if (message.startsWith("SYSTEM_EVENT"))
            message.remove(0, 13); // Strip SYSTEM_EVENT from the frontend, it's not useful

        QStringList tokens = message.split(" ", QString::SkipEmptyParts);

        if (tokens.isEmpty())
            return;

        // If no-one is listening for this event, then ignore it
        if (!m_filters.contains("ALL") && !m_filters.contains(tokens[0]))
            return;

        SendTextMessage(message);
    }
}
