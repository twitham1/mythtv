#include <sys/time.h>
#include <unistd.h>

#include "HLSReader.h"
#include "HLS/m3u.h"

#define LOC QString("%1: ").arg(m_curstream ? m_curstream->Url() : "HLSReader")

/**
 * Handles relative URLs without breaking URI encoded parameters by avoiding
 * storing the decoded URL in a QString.
 * It replaces M3U::RelativeURI("", M3U::DecodedURI(your segment URL here));
 */
static QUrl RelativeURI (const QString& baseString, const QString& uriString)
{
    QUrl base(baseString);
    QUrl uri(QUrl::fromEncoded(uriString.toLatin1()));

    return base.resolved(uri);
}

static uint64_t MDate(void)
{
    timeval  t;
    gettimeofday(&t, nullptr);
    return t.tv_sec * 1000000ULL + t.tv_usec;
}

HLSReader::~HLSReader(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "dtor -- start");
    Close();
    LOG(VB_RECORD, LOG_INFO, LOC + "dtor -- end");
}

bool HLSReader::Open(const QString & m3u, int bitrate_index)
{
    LOG(VB_RECORD, LOG_INFO, LOC + QString("Opening '%1'").arg(m3u));

    m_bitrate_index = bitrate_index;

    if (IsOpen(m3u))
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "Already open");
        return true;
    }
    Close(true);
    m_cancel = false;

    QByteArray buffer;

#ifdef HLS_USE_MYTHDOWNLOADMANAGER // MythDownloadManager leaks memory
    if (!DownloadURL(m3u, &buffer))
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "Open failed.");
        return false;   // can't download file
    }
#else
    MythSingleDownload downloader;
    if (!downloader.DownloadURL(m3u, &buffer))
    {
        LOG(VB_GENERAL, LOG_ERR,
            LOC + "Open failed: " + downloader.ErrorString());
        return false;   // can't download file
    }
#endif

    QTextStream text(&buffer);
    text.setCodec("UTF-8");

    if (!IsValidPlaylist(text))
    {
        LOG(VB_RECORD, LOG_ERR, LOC +
            QString("Open '%1': not a valid playlist").arg(m3u));
        return false;
    }

    m_m3u8 = m3u;

    QMutexLocker lock(&m_stream_lock);
    m_streams.clear();
    m_curstream = nullptr;

    if (!ParseM3U8(buffer))
        return false;

    if (m_bitrate_index == 0)
    {
        // Select the highest bitrate stream
        StreamContainer::iterator Istream;
        for (Istream = m_streams.begin(); Istream != m_streams.end(); ++Istream)
        {
            if (m_curstream == nullptr ||
                (*Istream)->Bitrate() > m_curstream->Bitrate())
                m_curstream = *Istream;
        }
    }
    else
    {
        if (m_streams.size() < m_bitrate_index)
        {
            LOG(VB_RECORD, LOG_ERR, LOC +
                QString("Open '%1': Only %2 bitrates, %3 is not a valid index")
                .arg(m3u)
                .arg(m_streams.size())
                .arg(m_bitrate_index));
            m_fatal = true;
            return false;
        }

        m_curstream = *(m_streams.begin() + (m_bitrate_index - 1));
    }

    if (!m_curstream)
    {
        LOG(VB_RECORD, LOG_ERR, LOC + QString("No stream selected"));
        m_fatal = true;
        return false;
    }
    LOG(VB_RECORD, LOG_INFO, LOC +
        QString("Selected stream with %3 bitrate")
        .arg(m_curstream->Bitrate()));

    m_playlistworker = new HLSPlaylistWorker(this);
    m_playlistworker->start();

    m_streamworker = new HLSStreamWorker(this);
    m_streamworker->start();

    LOG(VB_RECORD, LOG_INFO, LOC + "Open -- end");
    return true;
}

void HLSReader::Close(bool quiet)
{
    LOG(VB_RECORD, (quiet ? LOG_DEBUG : LOG_INFO), LOC + "Close -- start");

    Cancel(quiet);

    QMutexLocker stream_lock(&m_stream_lock);
    m_curstream = nullptr;

    StreamContainer::iterator Istream;
    for (Istream = m_streams.begin(); Istream != m_streams.end(); ++Istream)
        delete *Istream;
    m_streams.clear();

    delete m_streamworker;
    m_streamworker = nullptr;
    delete m_playlistworker;
    m_playlistworker = nullptr;

    LOG(VB_RECORD, (quiet ? LOG_DEBUG : LOG_INFO), LOC + "Close -- end");
}

void HLSReader::Cancel(bool quiet)
{
    LOG(VB_RECORD, (quiet ? LOG_DEBUG : LOG_INFO), LOC + "Cancel -- start");

    m_cancel = true;

    m_throttle_lock.lock();
    m_throttle_cond.wakeAll();
    m_throttle_lock.unlock();

    QMutexLocker lock(&m_stream_lock);

    if (m_curstream)
        LOG(VB_RECORD, LOG_INFO, LOC + "Cancel");

    if (m_playlistworker)
        m_playlistworker->Cancel();
    if (m_streamworker)
        m_streamworker->Cancel();
#ifdef HLS_USE_MYTHDOWNLOADMANAGER // MythDownloadManager leaks memory
    if (!m_sements.empty())
        CancelURL(m_segments.front().Url());
#endif

    LOG(VB_RECORD, (quiet ? LOG_DEBUG : LOG_INFO), LOC + "Cancel -- done");
}

void HLSReader::Throttle(bool val)
{
    LOG(VB_RECORD, LOG_INFO, LOC + QString("Throttle(%1)")
        .arg(val ? "true" : "false"));

    m_throttle_lock.lock();
    m_throttle = val;
    if (val)
        m_prebuffer_cnt += 4;
    else
        m_throttle_cond.wakeAll();
    m_throttle_lock.unlock();
}

int HLSReader::Read(uint8_t* buffer, int maxlen)
{
    if (!m_curstream)
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "Read: no stream selected");
        return 0;
    }
    if (m_cancel)
    {
        LOG(VB_RECORD, LOG_DEBUG, LOC + QString("Read: canceled"));
        return 0;
    }

    QMutexLocker lock(&m_buflock);

    int len = m_buffer.size() < maxlen ? m_buffer.size() : maxlen;
    LOG(VB_RECORD, LOG_DEBUG, LOC + QString("Reading %1 of %2 bytes")
        .arg(len).arg(m_buffer.size()));

    memcpy(buffer, m_buffer.constData(), len);
    if (len < m_buffer.size())
    {
        m_buffer.remove(0, len);
    }
    else
        m_buffer.clear();

    return len;
}

#ifdef HLS_USE_MYTHDOWNLOADMANAGER // MythDownloadManager leaks memory
bool HLSReader::DownloadURL(const QString &url, QByteArray *buffer)
{
    MythDownloadManager *mdm = GetMythDownloadManager();
    return mdm->download(url, buffer);
}

void HLSReader::CancelURL(const QString &url)
{
    MythDownloadManager *mdm = GetMythDownloadManager();
    mdm->cancelDownload(url);
    delete mdm;
}

void HLSReader::CancelURL(const QStringList &urls)
{
    MythDownloadManager *mdm = GetMythDownloadManager();
    mdm->cancelDownload(urls);
}
#endif

bool HLSReader::IsValidPlaylist(QTextStream & text)
{
    /* Parse stream and search for
     * EXT-X-TARGETDURATION or EXT-X-STREAM-INF tag, see
     * http://tools.ietf.org/html/draft-pantos-http-live-streaming-04#page-8 */
    QString line = text.readLine();
    if (!line.startsWith((const char*)"#EXTM3U"))
        return false;

    for (;;)
    {
        line = text.readLine();
        if (line.isNull())
            break;
        LOG(VB_RECORD, LOG_DEBUG,
            QString("IsValidPlaylist: |'%1'").arg(line));
        if (line.startsWith(QLatin1String("#EXT-X-TARGETDURATION"))  ||
            line.startsWith(QLatin1String("#EXT-X-MEDIA-SEQUENCE"))  ||
            line.startsWith(QLatin1String("#EXT-X-KEY"))             ||
            line.startsWith(QLatin1String("#EXT-X-ALLOW-CACHE"))     ||
            line.startsWith(QLatin1String("#EXT-X-ENDLIST"))         ||
            line.startsWith(QLatin1String("#EXT-X-STREAM-INF"))      ||
            line.startsWith(QLatin1String("#EXT-X-DISCONTINUITY"))   ||
            line.startsWith(QLatin1String("#EXT-X-VERSION")))
        {
            return true;
        }
    }

    LOG(VB_RECORD, LOG_ERR, QString("IsValidPlaylist: false"));
    return false;
}

bool HLSReader::ParseM3U8(const QByteArray& buffer, HLSRecStream* stream)
{
    /**
     * The http://tools.ietf.org/html/draft-pantos-http-live-streaming-04#page-8
     * document defines the following new tags: EXT-X-TARGETDURATION,
     * EXT-X-MEDIA-SEQUENCE, EXT-X-KEY, EXT-X-PROGRAM-DATE-TIME, EXT-X-
     * ALLOW-CACHE, EXT-X-STREAM-INF, EXT-X-ENDLIST, EXT-X-DISCONTINUITY,
     * and EXT-X-VERSION.
     */

    LOG(VB_RECORD, LOG_DEBUG, LOC + "ParseM3U8 -- begin");

    QTextStream text(buffer);

    QString line = text.readLine();
    if (line.isNull())
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "ParseM3U8: empty line");
        return false;
    }

    if (!line.startsWith(QLatin1String("#EXTM3U")))
    {
        LOG(VB_RECORD, LOG_ERR, LOC +
            "ParseM3U8: missing #EXTM3U tag .. aborting");
        return false;
    }

    /* What is the version ? */
    int version = 1;
    int p = buffer.indexOf("#EXT-X-VERSION:");
    if (p >= 0)
    {
        text.seek(p);
        if (!M3U::ParseVersion(text.readLine(), StreamURL(), version))
            return false;
    }

    if (buffer.indexOf("#EXT-X-STREAM-INF") >= 0)
    {
        // Meta index file
        LOG(VB_RECORD, LOG_INFO, LOC + "Meta index file");

        /* M3U8 Meta Index file */
        text.seek(0); // rewind
        while (!m_cancel)
        {
            line = text.readLine();
            if (line.isNull())
                break;

            LOG(VB_RECORD, LOG_INFO, LOC + QString("|%1").arg(line));
            if (line.startsWith(QLatin1String("#EXT-X-STREAM-INF")))
            {
                QString uri = text.readLine();
                if (uri.isNull() || uri.startsWith(QLatin1String("#")))
                {
                    LOG(VB_RECORD, LOG_INFO, LOC +
                        QString("ParseM3U8: Invalid EXT-X-STREAM-INF data '%1'")
                        .arg(uri));
                }
                else
                {
                    StreamContainer::iterator Istream;
                    QString url = RelativeURI(m_m3u8, uri).toString();

                    if ((Istream = m_streams.find(url)) == m_streams.end())
                    {
                        int id;
                        uint64_t bandwidth;
                        if (!M3U::ParseStreamInformation(line, url, StreamURL(),
                                                         id, bandwidth))
                            break;
                        HLSRecStream *hls =
                            new HLSRecStream(id, bandwidth, url);

                        if (hls)
                        {
                            LOG(VB_RECORD, LOG_INFO, LOC +
                                QString("Adding stream %1")
                                .arg(hls->toString()));
                            Istream = m_streams.insert(url, hls);
                        }
                    }
                    else
                        LOG(VB_RECORD, LOG_INFO, LOC +
                            QString("Already have stream '%1'").arg(url));
                }
            }
        }
    }
    else
    {
        LOG(VB_RECORD, LOG_DEBUG, LOC + "Meta playlist");

        HLSRecStream *hls;
        if (stream)
            hls = stream;
        else
        {
            /* No Meta playlist used */
            StreamContainer::iterator Istream;

            if ((Istream = m_streams.find(M3U::DecodedURI(m_m3u8))) ==
                m_streams.end())
            {
                hls = new HLSRecStream(0, 0, m_m3u8);
                if (hls)
                {
                    LOG(VB_RECORD, LOG_INFO, LOC +
                        QString("Adding new stream '%1'").arg(m_m3u8));
                    Istream = m_streams.insert(m_m3u8, hls);
                }
            }
            else
            {
                hls = *Istream;
                LOG(VB_RECORD, LOG_INFO, LOC +
                    QString("Updating stream '%1'").arg(hls->toString()));
            }

            /* Get TARGET-DURATION first */
            p = buffer.indexOf("#EXT-X-TARGETDURATION:");
            if (p >= 0)
            {
                int duration;

                text.seek(p);
                if (!M3U::ParseTargetDuration(text.readLine(), StreamURL(),
                                              duration))
                    return false;
                hls->SetTargetDuration(duration); /* seconds */
            }
            /* Store version */
            hls->SetVersion(version);
        }
        LOG(VB_RECORD, LOG_DEBUG, LOC +
            QString("%1 Playlist HLS protocol version: %2")
            .arg(hls->Live() ? "Live": "VOD").arg(version));

        // rewind
        text.seek(0);

        QString title;
        uint    segment_duration = -1;
        int64_t first_sequence   = -1;
        int64_t sequence_num     = 0;
        int     skipped = 0;

        SegmentContainer new_segments;

        QMutexLocker lock(&m_seq_lock);
        while (!m_cancel)
        {
            /* Next line */
            line = text.readLine();
            if (line.isNull())
                break;
            LOG(VB_RECORD, (m_debug ? LOG_INFO : LOG_DEBUG),
                LOC + QString("|%1").arg(line));

            if (line.startsWith(QLatin1String("#EXTINF")))
            {
                if (!M3U::ParseSegmentInformation(hls->Version(), line,
                                                  segment_duration,
                                                  title, StreamURL()))
                    return false;
            }
            else if (line.startsWith(QLatin1String("#EXT-X-TARGETDURATION")))
            {
                int duration;
                if (!M3U::ParseTargetDuration(line, StreamURL(), duration))
                    return false;
                hls->SetTargetDuration(duration); /* seconds */
            }
            else if (line.startsWith(QLatin1String("#EXT-X-MEDIA-SEQUENCE")))
            {
                if (!M3U::ParseMediaSequence(sequence_num, line, StreamURL()))
                    return false;
                if (first_sequence < 0)
                    first_sequence = sequence_num;
            }
            else if (line.startsWith(QLatin1String("#EXT-X-KEY")))
            {
#ifdef USING_LIBCRYPTO
                QString path, iv;
                if (!M3U::ParseKey(hls->Version(), line, m_aesmsg,  StreamURL(),
                                   path, iv))
                    return false;
                if (!path.isEmpty())
                    hls->SetKeyPath(path);

                if (!iv.isNull() && !hls->SetAESIV(iv))
                {
                    LOG(VB_RECORD, LOG_ERR, LOC + "invalid IV");
                    return false;
                }
#else
                LOG(VB_RECORD, LOG_ERR, LOC + "#EXT-X-KEY needs libcrypto");
                return false;
#endif
            }
            else if (line.startsWith(QLatin1String("#EXT-X-PROGRAM-DATE-TIME")))
            {
                QDateTime date;
                if (!M3U::ParseProgramDateTime(line, StreamURL(), date))
                    return false;
            }
            else if (line.startsWith(QLatin1String("#EXT-X-ALLOW-CACHE")))
            {
                bool do_cache;
                if (!M3U::ParseAllowCache(line, StreamURL(), do_cache))
                    return false;
                hls->SetCache(do_cache);
            }
            else if (line.startsWith(QLatin1String("#EXT-X-DISCONTINUITY")))
            {
                if (!M3U::ParseDiscontinuity(line, StreamURL()))
                    return false;
                ResetSequence();
            }
            else if (line.startsWith(QLatin1String("#EXT-X-VERSION")))
            {
                int version2;
                if (!M3U::ParseVersion(line, StreamURL(), version2))
                    return false;
                hls->SetVersion(version2);
            }
            else if (line.startsWith(QLatin1String("#EXT-X-ENDLIST")))
            {
                bool is_vod;
                if (!M3U::ParseEndList(StreamURL(), is_vod))
                    return false;
                hls->SetLive(!is_vod);
            }
            else if (!line.startsWith(QLatin1String("#")) && !line.isEmpty())
            {
                if (m_cur_seq < 0 || sequence_num > m_cur_seq)
                {
                    new_segments.push_back
                        (HLSRecSegment(sequence_num, segment_duration, title,
                                       RelativeURI(hls->Url(), line)));
                }
                else
                    ++skipped;

                ++sequence_num;
                segment_duration = -1; /* reset duration */
                title.clear();
            }
        }

        if (sequence_num < m_cur_seq)
        {
            // Sequence has been reset
            LOG(VB_RECORD, LOG_WARNING, LOC +
                QString("Sequence number has been reset from %1 to %2")
                .arg(m_cur_seq).arg(first_sequence));
            ResetSequence();
            return false;
        }

        SegmentContainer::iterator Inew = new_segments.begin();
        SegmentContainer::iterator Iseg = m_segments.end() - 1;

        // Does this playlist overlap?
        if (!m_segments.empty() && !new_segments.empty())
        {
            if ((*Iseg).Sequence() >= first_sequence &&
                (*Iseg).Sequence() < sequence_num)
            {
                // Find the beginning of the overlap
                while (Iseg != m_segments.begin() &&
                       (*Iseg).Sequence() > first_sequence &&
                       (*Iseg).Sequence() < sequence_num)
                    --Iseg;

                int64_t diff = (*Iseg).Sequence() - (*Inew).Sequence();
                if (new_segments.size() > diff)
                {
                    Inew += diff;

                    // Update overlapping segment info
                    for ( ; Iseg != m_segments.end(); ++Iseg, ++Inew)
                    {
                        if (Inew == new_segments.end())
                        {
                            LOG(VB_RECORD, LOG_ERR, LOC +
                                QString("Went off the end with %1 left")
                                .arg(m_segments.end() - Iseg));
                            break;
                        }
                        if ((*Iseg).Sequence() != (*Inew).Sequence())
                        {
                            LOG(VB_RECORD, LOG_ERR, LOC +
                                QString("Sequence non-sequential?  %1 != %2")
                                .arg((*Iseg).Sequence())
                                .arg((*Inew).Sequence()));
                            break;
                        }

                        (*Iseg).m_duration = (*Inew).Duration();
                        (*Iseg).m_title    = (*Inew).Title();
                        (*Iseg).m_url      = (*Inew).Url();
                    }
                }
            }
        }

        for ( ; Inew != new_segments.end(); ++Inew)
            m_segments.push_back(*Inew);

        m_playlist_size = new_segments.size() + skipped;
        int behind     = m_segments.size() - m_playlist_size;
        int max_behind = m_playlist_size / 2;
        if (behind > max_behind)
        {
            LOG(VB_RECORD, LOG_WARNING, LOC +
                QString("Not downloading fast enough! "
                        "%1 segments behind, skipping %2 segments. "
                        "playlist size: %3, queued: %4")
                .arg(behind).arg(behind - max_behind)
                .arg(m_playlist_size).arg(m_segments.size()));
            m_streamworker->CancelCurrentDownload();
            EnableDebugging();
            Iseg = m_segments.begin() + (behind - max_behind);
            m_segments.erase(m_segments.begin(), Iseg);
            m_bandwidthcheck = (m_bitrate_index == 0);
        }
        else if (m_debug_cnt > 0)
            --m_debug_cnt;
        else
            m_debug = false;
    }

    LOG(VB_RECORD, LOG_DEBUG, LOC + "ParseM3U8 -- end");
    return true;
}

bool HLSReader::LoadMetaPlaylists(MythSingleDownload& downloader)
{
    if (!m_curstream || m_cancel)
        return false;

    LOG(VB_RECORD, LOG_DEBUG, LOC +
        QString("LoadMetaPlaylists stream %1")
        .arg(m_curstream->toString()));

    StreamContainer::iterator   Istream;

    m_stream_lock.lock();
    if (m_bandwidthcheck /* && !m_segments.empty() */)
    {
        int buffered = PercentBuffered();

        if (buffered < 15)
        {
            // It is taking too long to download the segments
            LOG(VB_RECORD, LOG_WARNING, LOC +
                QString("Falling behind: only %1% buffered").arg(buffered));
            LOG(VB_RECORD, LOG_DEBUG, LOC +
                QString("playlist size %1, queued %2")
                .arg(m_playlist_size).arg(m_segments.size()));
            EnableDebugging();
            DecreaseBitrate(m_curstream->Id());
            m_bandwidthcheck = false;
        }
        else if (buffered > 85)
        {
            // Keeping up easily, raise the bitrate.
            LOG(VB_RECORD, LOG_DEBUG, LOC +
                QString("Plenty of bandwidth, downloading %1 of %2")
                .arg(m_playlist_size - m_segments.size())
                .arg(m_playlist_size));
            LOG(VB_RECORD, LOG_DEBUG, LOC +
                QString("playlist size %1, queued %2")
                .arg(m_playlist_size).arg(m_segments.size()));
            IncreaseBitrate(m_curstream->Id());
            m_bandwidthcheck = false;
        }
    }
    m_stream_lock.unlock();

    QByteArray buffer;

#ifdef HLS_USE_MYTHDOWNLOADMANAGER // MythDownloadManager leaks memory
    if (!DownloadURL(m_curstream->Url(), &buffer))
        return false;
#else
    if (!downloader.DownloadURL(m_curstream->Url(), &buffer))
    {
        LOG(VB_GENERAL, LOG_WARNING,
            LOC + "Download failed: " + downloader.ErrorString());
        return false;
    }
#endif

    if (m_cancel || !ParseM3U8(buffer, m_curstream))
        return false;

    // Signal SegmentWorker that there is work to do...
    m_streamworker->Wakeup();

    return true;
}

void HLSReader::DecreaseBitrate(int progid)
{
    HLSRecStream *hls = nullptr;
    uint64_t bitrate = m_curstream->Bitrate();
    uint64_t candidate = 0;
    StreamContainer::const_iterator Istream;

    for (Istream = m_streams.begin(); Istream != m_streams.end(); ++Istream)
    {
        if ((*Istream)->Id() != progid)
            continue;
        if (bitrate > (*Istream)->Bitrate() &&
            candidate < (*Istream)->Bitrate())
        {
            LOG(VB_RECORD, LOG_DEBUG, LOC +
                QString("candidate stream '%1' bitrate %2 >= %3")
                .arg(Istream.key()).arg(bitrate).arg((*Istream)->Bitrate()));
            hls = *Istream;
            candidate = hls->Bitrate();
        }
    }

    if (hls)
    {
        LOG(VB_RECORD, LOG_INFO, LOC +
            QString("Switching to a lower bitrate stream %1 -> %2")
            .arg(bitrate).arg(candidate));
        m_curstream = hls;
    }
    else
    {
        LOG(VB_RECORD, LOG_DEBUG, LOC +
            QString("Already at lowest bitrate %1").arg(bitrate));
    }
}

void HLSReader::IncreaseBitrate(int progid)
{
    HLSRecStream *hls = nullptr;
    uint64_t bitrate = m_curstream->Bitrate();
    uint64_t candidate = INT_MAX;
    StreamContainer::const_iterator Istream;

    for (Istream = m_streams.begin(); Istream != m_streams.end(); ++Istream)
    {
        if ((*Istream)->Id() != progid)
            continue;
        if (bitrate < (*Istream)->Bitrate() &&
            candidate > (*Istream)->Bitrate())
        {
            LOG(VB_RECORD, LOG_DEBUG, LOC +
                QString("candidate stream '%1' bitrate %2 >= %3")
                .arg(Istream.key()).arg(bitrate).arg((*Istream)->Bitrate()));
            hls = *Istream;
            candidate = hls->Bitrate();
        }
    }

    if (hls)
    {
        LOG(VB_RECORD, LOG_INFO, LOC +
            QString("Switching to a higher bitrate stream %1 -> %2")
            .arg(bitrate).arg(candidate));
        m_curstream = hls;
    }
    else
    {
        LOG(VB_RECORD, LOG_DEBUG, LOC +
            QString("Already at highest bitrate %1").arg(bitrate));
    }
}

bool HLSReader::LoadSegments(MythSingleDownload& downloader)
{
    LOG(VB_RECORD, LOG_DEBUG, LOC + "LoadSegment -- start");

    if (!m_curstream)
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "LoadSegment: current stream not set.");
        m_bandwidthcheck = (m_bitrate_index == 0);
        return false;
    }

    HLSRecSegment seg;
    for (;;)
    {
        m_seq_lock.lock();
        if (m_cancel || m_segments.empty())
        {
            m_seq_lock.unlock();
            break;
        }

        seg = m_segments.front();
        if (m_segments.size() > m_playlist_size)
        {
            LOG(VB_RECORD, (m_debug ? LOG_INFO : LOG_DEBUG), LOC +
                QString("Downloading segment %1 (1 of %2) with %3 behind")
                .arg(seg.Sequence())
                .arg(m_segments.size() + m_playlist_size)
                .arg(m_segments.size() - m_playlist_size));
        }
        else
        {
            LOG(VB_RECORD, (m_debug ? LOG_INFO : LOG_DEBUG), LOC +
                QString("Downloading segment %1 (%2 of %3)")
                .arg(seg.Sequence())
                .arg(m_playlist_size - m_segments.size() + 1)
                .arg(m_playlist_size));
        }
        m_seq_lock.unlock();

        m_stream_lock.lock();
        HLSRecStream *hls = m_curstream;
        m_stream_lock.unlock();
        if (!hls)
        {
            LOG(VB_RECORD, LOG_DEBUG, LOC + "LoadSegment -- no current stream");
            return false;
        }

        long throttle = DownloadSegmentData(downloader, hls,
                                       seg, m_playlist_size);

        m_seq_lock.lock();
        if (throttle < 0)
        {
            if (m_segments.size() > m_playlist_size)
            {
                SegmentContainer::iterator Iseg = m_segments.begin() +
                                          (m_segments.size() - m_playlist_size);
                m_segments.erase(m_segments.begin(), Iseg);
            }
            m_seq_lock.unlock();
            return false;
        }

        m_cur_seq = m_segments.front().Sequence();
        m_segments.pop_front();

        m_seq_lock.unlock();

        if (m_throttle && throttle == 0)
            throttle = 2;
        else if (throttle > 8)
            throttle = 8;
        if (throttle > 0)
        {
            LOG(VB_RECORD, LOG_INFO, LOC +
                QString("Throttling -- sleeping %1 secs.")
                .arg(throttle));
            throttle *= 1000;
            m_throttle_lock.lock();
            if (m_throttle_cond.wait(&m_throttle_lock, throttle))
                LOG(VB_RECORD, LOG_INFO, LOC + "Throttle aborted");
            m_throttle_lock.unlock();
            LOG(VB_RECORD, LOG_INFO, LOC + "Throttle done");
        }
        else
            usleep(5000);

        if (m_prebuffer_cnt == 0)
        {
            m_bandwidthcheck = (m_bitrate_index == 0);
            m_prebuffer_cnt = 2;
        }
        else
            --m_prebuffer_cnt;
    }

    LOG(VB_RECORD, LOG_DEBUG, LOC + "LoadSegment -- end");
    return true;
}

uint HLSReader::PercentBuffered(void) const
{
    if (m_playlist_size == 0 || m_segments.size() > m_playlist_size)
        return 0;
    return (static_cast<float>(m_playlist_size - m_segments.size()) /
            static_cast<float>(m_playlist_size)) * 100.0F;
}

int HLSReader::DownloadSegmentData(MythSingleDownload& downloader,
                                   HLSRecStream* hls,
                                   HLSRecSegment& segment, int playlist_size)
{
    uint64_t bandwidth = hls->AverageBandwidth();

    LOG(VB_RECORD, LOG_DEBUG, LOC +
        QString("Downloading %1 bandwidth %2 bitrate %3")
        .arg(segment.Sequence()).arg(bandwidth).arg(hls->Bitrate()));

    /* sanity check - can we download this segment on time? */
    if ((bandwidth > 0) && (hls->Bitrate() > 0))
    {
        uint64_t size = (segment.Duration() * hls->Bitrate()); /* bits */
        int estimated_time = (int)(size / bandwidth);
        if (estimated_time > segment.Duration())
        {
            LOG(VB_RECORD, LOG_WARNING, LOC +
                QString("downloading of %1 will take %2s, "
                        "which is longer than its playback (%3s) at %4kiB/s")
                .arg(segment.Sequence())
                .arg(estimated_time)
                .arg(segment.Duration())
                .arg(bandwidth / 8192));
        }
    }

    QByteArray buffer;
    uint64_t   start = MDate();

#ifdef HLS_USE_MYTHDOWNLOADMANAGER // MythDownloadManager leaks memory
                                   // and can only handle six download at a time
    if (!HLSReader::DownloadURL(segment.Url(), &buffer))
    {
        LOG(VB_RECORD, LOG_ERR, LOC +
            QString("%1 failed").arg(segment.Sequence()));
        if (estimated_time * 2 < segment.Duration())
        {
            if (!HLSReader::DownloadURL(segment.Url(), &buffer))
            {
                LOG(VB_RECORD, LOG_ERR, LOC + QString("%1 failed")
                    .arg(segment.Sequence()));
                return -1;
            }
        }
        else
            return 0;
    }
#else
    if (!downloader.DownloadURL(segment.Url(), &buffer))
    {
        LOG(VB_RECORD, LOG_ERR, LOC + QString("%1 failed: %2")
            .arg(segment.Sequence()).arg(downloader.ErrorString()));
        return -1;
    }
#endif

    uint64_t downloadduration = (MDate() - start) / 1000;

#ifdef USING_LIBCRYPTO
    /* If the segment is encrypted, decode it */
    if (segment.HasKeyPath())
    {
        if (!hls->DecodeData(downloader, hls->IVLoaded() ? hls->AESIV() : nullptr,
                             segment.KeyPath(),
                             buffer, segment.Sequence()))
            return 0;
    }
#endif

    int segment_len = buffer.size();

    m_buflock.lock();
    if (m_buffer.size() > segment_len * playlist_size)
    {
        LOG(VB_RECORD, LOG_WARNING, LOC +
            QString("streambuffer is not reading fast enough. "
                    "buffer size %1").arg(m_buffer.size()));
        EnableDebugging();
        if (++m_slow_cnt > 15)
        {
            m_slow_cnt = 15;
            m_fatal = true;
            return -1;
        }
    }
    else if (m_slow_cnt > 0)
        --m_slow_cnt;

    if (m_buffer.size() >= segment_len * playlist_size * 2)
    {
        LOG(VB_RECORD, LOG_WARNING, LOC +
            QString("streambuffer is not reading fast enough. "
                    "buffer size %1.  Dropping %2 bytes")
            .arg(m_buffer.size()).arg(segment_len));
        m_buffer.remove(0, segment_len);
    }

    m_buffer += buffer;
    m_buflock.unlock();

    if (hls->Bitrate() == 0 && segment.Duration() > 0)
    {
        /* Try to estimate the bandwidth for this stream */
        hls->SetBitrate((uint64_t)(((double)segment_len * 8) /
                                   ((double)segment.Duration())));
    }

    if (downloadduration < 1)
        downloadduration = 1;

    /* bits/sec */
    bandwidth = segment_len * 8 * 1000ULL / downloadduration;
    hls->AverageBandwidth(bandwidth);
    hls->SetCurrentByteRate(static_cast<uint64_t>
                            ((static_cast<double>(segment_len) /
                              static_cast<double>(segment.Duration()))));

    LOG(VB_RECORD, (m_debug ? LOG_INFO : LOG_DEBUG), LOC +
        QString("%1 took %3ms for %4 bytes: "
                "bandwidth:%5kiB/s")
        .arg(segment.Sequence())
        .arg(downloadduration)
        .arg(segment_len)
        .arg(bandwidth / 8192.0));

    return m_slow_cnt;
}

void HLSReader::PlaylistGood(void)
{
    QMutexLocker lock(&m_stream_lock);
    if (m_curstream)
        m_curstream->Good();
}

void HLSReader::PlaylistRetrying(void)
{
    QMutexLocker lock(&m_stream_lock);
    if (m_curstream)
        m_curstream->Retrying();
}

int  HLSReader::PlaylistRetryCount(void) const
{
    QMutexLocker lock(&m_stream_lock);
    if (m_curstream)
        return m_curstream->RetryCount();
    return 0;
}

void HLSReader::EnableDebugging(void)
{
    m_debug = true;
    m_debug_cnt = 5;
    LOG(VB_RECORD, LOG_INFO, LOC + "Debugging enabled");
}
