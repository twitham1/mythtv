/** -*- Mode: c++ -*-
 *  CetonRTSP
 *  Copyright (c) 2011 Ronald Frazier
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#include <QStringList>
#include <QTcpSocket>
#include <QUrl>
#include <QVector>

// MythTV includes
#include "cetonrtsp.h"
#include "mythlogging.h"
#include "mythsocket.h"


#define LOC QString("CetonRTSP(%1): ").arg(m_requestUrl.toString())

QMutex CetonRTSP::s_rtspMutex;

CetonRTSP::CetonRTSP(const QString &ip, uint tuner, ushort port)
{
    m_requestUrl.setHost(ip);
    m_requestUrl.setPort(port);
    m_requestUrl.setScheme("rtsp");
    m_requestUrl.setPath(QString("cetonmpeg%1").arg(tuner));
}

CetonRTSP::CetonRTSP(const QUrl &url) :
    m_requestUrl(url)
{
    if (url.port() < 0)
    {
        // default rtsp port
        m_requestUrl.setPort(554);
    }
}

CetonRTSP::~CetonRTSP()
{
    StopKeepAlive();
}

bool CetonRTSP::ProcessRequest(
    const QString &method, const QStringList* headers,
    bool use_control, bool waitforanswer, const QString &alternative)
{
    QMutexLocker locker(&s_rtspMutex);

    m_responseHeaders.clear();
    m_responseContent.clear();

    // Create socket if socket object has never been created or in non-connected state
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState)
    {
        if (!m_socket)
        {
            m_socket = new QTcpSocket();
        }
        else
        {
            m_socket->close();
        }
        m_socket->connectToHost(m_requestUrl.host(), m_requestUrl.port(),
                               QAbstractSocket::ReadWrite);
        bool ok = m_socket->waitForConnected();

        if (!ok)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Could not connect to server %1:%2")
                .arg(m_requestUrl.host()).arg(m_requestUrl.port()));
            delete m_socket;
            m_socket = nullptr;
            return false;
        }
    }
    else
    {
        // empty socket's waiting data just in case
        m_socket->waitForReadyRead(30);
        do
        {
            QVector<char> trash;
            uint avail = m_socket->bytesAvailable();
            trash.resize(std::max((uint)trash.size(), avail));
            m_socket->read(trash.data(), avail);
            m_socket->waitForReadyRead(30);
        }
        while (m_socket->bytesAvailable() > 0);
    }

    QStringList requestHeaders;
    requestHeaders.append(QString("%1 %2 RTSP/1.0")
        .arg(method)
        .arg(alternative.size() ? alternative :
             (use_control ? m_controlUrl.toString() : m_requestUrl.toString())));
    requestHeaders.append(QString("User-Agent: MythTV Ceton Recorder"));
    requestHeaders.append(QString("CSeq: %1").arg(++m_sequenceNumber));
    if (m_sessionId != "0")
        requestHeaders.append(QString("Session: %1").arg(m_sessionId));
    if (headers != nullptr)
    {
        for(int i = 0; i < headers->count(); i++)
        {
            const QString& header = headers->at(i);
            requestHeaders.append(header);
        }
    }
    requestHeaders.append(QString("\r\n"));
    QString request = requestHeaders.join("\r\n");


    LOG(VB_RECORD, LOG_DEBUG, LOC + QString("write: %1").arg(request));
    m_socket->write(request.toLatin1());

    m_responseHeaders.clear();
    m_responseContent.clear();

    if (!waitforanswer)
        return true;

    QRegExp firstLineRegex(
        "^RTSP/1.0 (\\d+) ([^\r\n]+)", Qt::CaseSensitive, QRegExp::RegExp2);
    QRegExp headerRegex(
        "^([^:]+):\\s*([^\\r\\n]+)", Qt::CaseSensitive, QRegExp::RegExp2);
    QRegExp blankLineRegex(
        "^[\\r\\n]*$", Qt::CaseSensitive, QRegExp::RegExp2);

    bool firstLine = true;
    while (true)
    {
        if (!m_socket->canReadLine())
        {
            bool ready = m_socket->waitForReadyRead(30 * 1000);
            if (!ready)
            {
                LOG(VB_RECORD, LOG_ERR, LOC + "RTSP server did not respond after 30s");
                return false;
            }
            continue;
        }

        QString line = m_socket->readLine();
        LOG(VB_RECORD, LOG_DEBUG, LOC + QString("read: %1").arg(line));

        if (firstLine)
        {
            if (firstLineRegex.indexIn(line) == -1)
            {
                m_responseCode = -1;
                m_responseMessage =
                    QString("Could not parse first line of response: '%1'")
                    .arg(line);
                return false;
            }

            QStringList parts = firstLineRegex.capturedTexts();
            m_responseCode = parts.at(1).toInt();
            m_responseMessage = parts.at(2);

            if (m_responseCode != 200)
            {
                m_responseMessage =
                    QString("Server couldn't process the request: '%1'")
                    .arg(m_responseMessage);
                return false;
            }
            firstLine = false;
            continue;
        }

        if (blankLineRegex.indexIn(line) != -1) break;

        if (headerRegex.indexIn(line) == -1)
        {
            m_responseCode = -1;
            m_responseMessage = QString("Could not parse response header: '%1'")
                .arg(line);
            return false;
        }
        QStringList parts = headerRegex.capturedTexts();
        m_responseHeaders.insert(parts.at(1), parts.at(2));
    }

    QString cSeq;

    if (m_responseHeaders.contains("CSeq"))
    {
        cSeq = m_responseHeaders["CSeq"];
    }
    else
    {
        // Handle broken implementation, such as VLC
        // doesn't respect the case of "CSeq", so find it regardless of the spelling
        foreach (QString key, m_responseHeaders.keys())
        {
            if (key.compare("CSeq", Qt::CaseInsensitive) == 0)
            {
                cSeq = m_responseHeaders.value(key);
                break;
            }
        }
    }
    if (cSeq != QString("%1").arg(m_sequenceNumber))
    {
        LOG(VB_RECORD, LOG_WARNING, LOC +
            QString("Expected CSeq of %1 but got %2")
            .arg(m_sequenceNumber).arg(cSeq));
    }

    m_responseContent.clear();
    int contentLength = m_responseHeaders.value("Content-Length").toInt();
    if (contentLength > 0)
    {
        m_responseContent.resize(contentLength);
        char* data = m_responseContent.data();
        int bytesRead = 0;
        while (bytesRead < contentLength)
        {
            if (m_socket->bytesAvailable() == 0)
                m_socket->waitForReadyRead();

            int count = m_socket->read(data+bytesRead, contentLength-bytesRead);
            if (count == -1)
            {
                m_responseCode = -1;
                m_responseMessage = "Could not read response content";
                return false;
            }
            bytesRead += count;
        }
        LOG(VB_RECORD, LOG_DEBUG, LOC +
            QString("received: %1").arg(m_responseContent.constData()));
    }
    return true;
}

bool CetonRTSP::GetOptions(QStringList &options)
{
    if (ProcessRequest("OPTIONS"))
    {
        options = m_responseHeaders.value("Public").split(QRegExp(",\\s*"));
        m_canGetParameter = options.contains("GET_PARAMETER");

        return true;
    }
    return false;
}

/**
 * splitLines. prepare SDP content for easy read
 */
QStringList CetonRTSP::splitLines(const QByteArray &lines)
{
    QStringList list;
    QTextStream stream(lines);
    QString line;

    do
    {
        line = stream.readLine();
        if (!line.isNull())
        {
            list.append(line);
        }
    }
    while (!line.isNull());

    return list;
}

/**
 * readParameters. Scan a line like: Session: 1234556;destination=xx;client_port
 * and return the first entry and fill the arguments in the provided Params
 */
QString CetonRTSP::readParameters(const QString &key, Params &parameters)
{
    QString val;

    if (!m_responseHeaders.contains(key))
    {
        return val;
    }

    QStringList header = m_responseHeaders.value(key).split(";");

    for (int i = 0; i < header.size(); i++)
    {
        QString entry = header[i].trimmed();

        if (i ==0)
        {
            val = entry;
            continue;
        }
        QStringList args = entry.split("=");

        parameters.insert(args[0].trimmed(),
                          args.size() > 1 ? args[1].trimmed() : QString());
    }
    return val;
}

/**
 * Return the base URL for the last DESCRIBE answer
 */
QUrl CetonRTSP::GetBaseUrl(void)
{
    if (m_responseHeaders.contains("Content-Base"))
    {
        return m_responseHeaders["Content-Base"];
    }
    if (m_responseHeaders.contains("Content-Location"))
    {
        return m_responseHeaders["Content-Location"];
    }
    return m_requestUrl;
}

bool CetonRTSP::Describe(void)
{
    QStringList headers;

    headers.append("Accept: application/sdp");

    if (!ProcessRequest("DESCRIBE", &headers))
        return false;

    // find control url
    QStringList lines = splitLines(m_responseContent);
    bool found = false;
    QUrl base = m_controlUrl = GetBaseUrl();

    foreach (QString line, lines)
    {
        if (line.startsWith("m="))
        {
            if (found)
            {
                // another new stream, no need to parse further
                break;
            }
            if (!line.startsWith("m=video"))
            {
                // not a video stream
                continue;
            }
            QStringList args = line.split(" ");
            if (args[2] == "RTP/AVP" && args[3] == "33")
            {
                found = true;
            }
            continue;
        }
        if (line.startsWith("c="))
        {
            // TODO, connection parameter
            // assume we will always get a control entry
            continue;
        }
        if (line.startsWith("a=control:"))
        {
            // Per RFC: a=control:rtsp://example.com/foo
            // This attribute may contain either relative and absolute URLs,
            // following the rules and conventions set out in RFC 1808 [25].
            QString url = line.mid(10).trimmed();
            m_controlUrl = url;
            if (url == "*")
            {
                m_controlUrl = base;
            }
            else if (m_controlUrl.isRelative())
            {
                m_controlUrl = base.resolved(m_controlUrl);
            }
            continue;
        }
    }

    if (!found)
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "expected content to be type "
            "\"m=video 0 RTP/AVP 33\" but it appears not to be");
        m_controlUrl = QUrl();
        return false;
    }

    return true;
}

bool CetonRTSP::Setup(ushort clientPort1, ushort clientPort2,
                      ushort &rtpPort, ushort &rtcpPort,
                      uint32_t &ssrc)
{
    LOG(VB_GENERAL, LOG_INFO, QString("CetonRTSP: ") +
        QString("Transport: RTP/AVP;unicast;client_port=%1-%2")
        .arg(clientPort1).arg(clientPort2));

    QStringList extraHeaders;
    extraHeaders.append(
        QString("Transport: RTP/AVP;unicast;client_port=%1-%2")
        .arg(clientPort1).arg(clientPort2));

    if (!ProcessRequest("SETUP", &extraHeaders, true))
        return false;

    Params params;
    QString session = readParameters("Session", params);

    if (session.isEmpty())
    {
        LOG(VB_RECORD, LOG_ERR, LOC +
            "session id not found in SETUP response");
        return false;
    }
    if (session.size() < 8)
    {
        LOG(VB_RECORD, LOG_WARNING, LOC +
            "invalid session id received");
    }
    m_sessionId = session;

    if (params.contains("timeout"))
    {
        m_timeout = params["timeout"].toInt();
    }

    QString transport = readParameters("Transport", params);
    Q_UNUSED(transport);
    if (params.contains("ssrc"))
    {
        bool ok;
        ssrc = params["ssrc"].toUInt(&ok, 16);
    }
    if (params.contains("server_port"))
    {
        QString line = params["server_port"];
        QStringList val = line.split("-");

        rtpPort = val[0].toInt();
        rtcpPort = val.size() > 1 ? val[1].toInt() : 0;
    }

    return true;
}

bool CetonRTSP::Play(void)
{
    bool result = ProcessRequest("PLAY");

    StartKeepAlive();
    return result;
}

bool CetonRTSP::Teardown(void)
{
    StopKeepAlive();

    bool result = ProcessRequest("TEARDOWN");

    QMutexLocker locker(&s_rtspMutex);

    delete m_socket;
    m_socket = nullptr;

    m_sessionId = "0";
    return result;
}

void CetonRTSP::StartKeepAlive()
{
    if (m_timer)
        return;
    int timeout = std::max(m_timeout - 5, 5);
    LOG(VB_RECORD, LOG_DEBUG, LOC +
        QString("Start KeepAlive, every %1s").arg(timeout));
    m_timer = startTimer(timeout * 1000);
}

void CetonRTSP::StopKeepAlive()
{
    if (m_timer)
    {
        killTimer(m_timer);
        LOG(VB_RECORD, LOG_DEBUG, LOC + "Stop KeepAlive");
    }
    m_timer = 0;
}

void CetonRTSP::timerEvent(QTimerEvent* /*event*/)
{
    QStringList dummy;

    LOG(VB_RECORD, LOG_DEBUG, LOC + "Sending KeepAlive");
    if (m_canGetParameter)
    {
        ProcessRequest("GET_PARAMETER", nullptr, false, false);
    }
    else
    {
        ProcessRequest("OPTIONS", nullptr, false, false, "*");
    }
}
