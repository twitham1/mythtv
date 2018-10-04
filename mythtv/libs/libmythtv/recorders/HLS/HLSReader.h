#ifndef _HLS_Reader_h_
#define _HLS_Reader_h_

#include <QObject>
#include <QString>
#include <QUrl>
#include <QTextStream>

/*
  Use MythSingleDownload.

  MythDownloadManager leaks memory and QNetworkAccessManager can only
  handle six simultaneous downloads.  Each HLS stream can be
  downloading the playlist and the segments at the same time, so the
  limit of six could impact performance if recording more than three
  HLS channels.
*/

#ifdef HLS_USE_MYTHDOWNLOADMANAGER
#include "mythdownloadmanager.h"
#else
#include "mythsingledownload.h"
#endif

#include "mythlogging.h"
#include "mythtvexp.h"

#include "HLSSegment.h"
#include "HLSStream.h"
#include "HLSStreamWorker.h"
#include "HLSPlaylistWorker.h"


class MTV_PUBLIC  HLSReader
{
    friend class HLSStreamWorker;
    friend class HLSPlaylistWorker;

  public:
    typedef QMap<QString, HLSRecStream* > StreamContainer;
    typedef QList<HLSRecSegment> SegmentContainer;

    HLSReader(void);
    ~HLSReader(void);

    bool Open(const QString & uri, int bitrate_index = 0);
    void Close(bool quiet = false);
    int Read(uint8_t* buffer, int len);
    void Throttle(bool val);
    bool IsThrottled(void) const { return m_throttle; }
    bool IsOpen(const QString& url) const
    { return m_curstream && m_m3u8 == url; }
    bool FatalError(void) const { return m_fatal; }

    bool LoadMetaPlaylists(MythSingleDownload& downloader);
    void ResetStream(void)
      { QMutexLocker lock(&m_stream_lock); m_curstream = nullptr; }
    void ResetSequence(void) { m_cur_seq = -1; }

    QString StreamURL(void) const
    { return QString("%1").arg(m_curstream ? m_curstream->Url() : ""); }

#ifdef HLS_USE_MYTHDOWNLOADMANAGER // MythDownloadManager leaks memory
    static bool DownloadURL(const QString &url, QByteArray *buffer);
#endif
    static void CancelURL(const QString &url);
    static void CancelURL(const QStringList &urls);

    static bool IsValidPlaylist(QTextStream & text);

  protected:
    void Cancel(bool quiet = false);
    bool LoadSegments(MythSingleDownload& downloader);
    uint PercentBuffered(void) const;
    int  TargetDuration(void) const
    { return (m_curstream ? m_curstream->TargetDuration() : 0); }

    void AllowPlaylistSwitch(void) { m_bandwidthcheck = true; }

    void PlaylistGood(void);
    void PlaylistRetrying(void);
    int  PlaylistRetryCount(void) const;

  private:
    bool ParseM3U8(const QByteArray & buffer, HLSRecStream* stream = nullptr);
    void DecreaseBitrate(int progid);
    void IncreaseBitrate(int progid);

    // Downloading
    bool LoadSegments(HLSRecStream & hlsstream);
    int DownloadSegmentData(MythSingleDownload& downloader, HLSRecStream* hls,
			    HLSRecSegment& segment, int playlist_size);

    // Debug
    void EnableDebugging(void);

  private:
    QString          m_m3u8;
    StreamContainer  m_streams;
    SegmentContainer m_segments;
    HLSRecStream    *m_curstream;
    int64_t          m_cur_seq;
    int              m_bitrate_index;

    bool m_fatal;
    bool m_cancel;
    bool m_throttle;
    bool m_aesmsg;   // only print one time that the media is encrypted

    HLSPlaylistWorker *m_playlistworker;
    HLSStreamWorker   *m_streamworker;

#if 0
    int64_t    m_seq_begin;
    int64_t    m_seq_first;
    int64_t    m_seq_next;
    int64_t    m_seq_end;
#endif
    int        m_playlist_size;
    bool       m_bandwidthcheck;
    uint       m_prebuffer_cnt;
    QMutex     m_seq_lock;
    mutable QMutex m_stream_lock;
    QMutex     m_throttle_lock;
    QWaitCondition  m_throttle_cond;
    bool       m_debug;
    int        m_debug_cnt;

    // Downloading
    int         m_slow_cnt;
    QByteArray  m_buffer;
    QMutex      m_buflock;
};

#endif
