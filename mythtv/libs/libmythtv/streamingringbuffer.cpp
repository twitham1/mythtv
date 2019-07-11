#include <QUrl>
#include "mythcorecontext.h"
#include "mythlogging.h"

#include "streamingringbuffer.h"

#define LOC QString("StreamRingBuf(%1): ").arg(m_filename)

StreamingRingBuffer::StreamingRingBuffer(const QString &lfilename)
  : RingBuffer(kRingBuffer_HTTP)
{
    m_startReadAhead = false;
    StreamingRingBuffer::OpenFile(lfilename);
}

StreamingRingBuffer::~StreamingRingBuffer()
{
    KillReadAheadThread();

    m_rwLock.lockForWrite();
    if (m_context)
        ffurl_close(m_context);
    m_rwLock.unlock();
}

bool StreamingRingBuffer::IsOpen(void) const
{
    bool result;
    m_rwLock.lockForRead();
    result = (bool)m_context;
    m_rwLock.unlock();
    return result;
}

long long StreamingRingBuffer::GetReadPosition(void) const
{
    return 0;
}

/** \fn StreamingRingBuffer::OpenFile(const QString &, uint)
 *  \brief Opens a network stream for reading.
 *
 *  \param lfilename   Url of the stream to read.
 *  \param retry_ms    Ignored. This value is part of the API
 *                     inherited from the parent class.
 *  \return Returns true if the stream was opened.
 */
bool StreamingRingBuffer::OpenFile(const QString &lfilename, uint /*retry_ms*/)
{
    RingBuffer::AVFormatInitNetwork();

    m_rwLock.lockForWrite();

    m_safeFilename = lfilename;
    m_filename = lfilename;

    // TODO check whether local area file

    QUrl url = m_filename;
    if (url.path().endsWith(QLatin1String("m3u8"), Qt::CaseInsensitive))
    {
        url.setScheme("hls+http");
    }

    int res = ffurl_open(&m_context, url.toString().toLatin1(), AVIO_FLAG_READ,
                         nullptr, nullptr);
    if (res >=0 && m_context &&
        !m_context->is_streamed && ffurl_seek(m_context, 0, SEEK_SET) >= 0)
    {
        m_streamed   = false;
        m_allowSeeks = true;
    }

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Trying %1 (allow seeks: %2")
        .arg(m_filename).arg(m_allowSeeks));

    if (res < 0 || !m_context)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Failed to open stream (error %1)") .arg(res));
        m_lastError = QObject::tr("Failed to open stream (%1)").arg(res);
        m_rwLock.unlock();
        return false;
    }

    m_rwLock.unlock();

    return true;
}

long long StreamingRingBuffer::SeekInternal(long long pos, int whence)
{
    if (!m_context)
        return 0;

    m_posLock.lockForWrite();
    int seek = ffurl_seek(m_context, pos, whence);
    m_posLock.unlock();

    if (seek < 0)
    {
        m_ateof = true;
        return 0;
    }
    return pos;
}

int StreamingRingBuffer::safe_read(void *data, uint sz)
{
    uint len = 0;

    if (m_context)
    {
        while (len < sz)
        {
            int ret = ffurl_read(m_context, (unsigned char*)data + len, sz - len);
            if (ret < 0)
            {
                if (ret == AVERROR_EOF)
                {
                    m_ateof = true;
                }
                errno = ret;
                break;
            }
            if (ret == 0) // nothing read, exit early
                break;    // should EOF be set ??
            len += ret;
        }
    }
    return len;
}

long long StreamingRingBuffer::GetRealFileSizeInternal(void) const
{
    long long result = -1;
    m_rwLock.lockForRead();
    if (m_context)
        result = ffurl_size(m_context);
    m_rwLock.unlock();
    return result;
}
