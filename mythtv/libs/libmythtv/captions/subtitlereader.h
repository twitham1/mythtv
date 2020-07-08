#ifndef SUBTITLEREADER_H
#define SUBTITLEREADER_H

#include <QMutex>

extern "C" {
#include "libavcodec/avcodec.h"
}

#include "mythdeque.h"
#include "textsubtitleparser.h"

class AVSubtitles
{
  public:
    AVSubtitles() = default;
    MythDeque<AVSubtitle> m_buffers;
    QMutex                m_lock;
    bool                  m_fixPosition {false};
};

class RawTextSubs
{
  public:
    RawTextSubs(void) = default;

    QStringList m_buffers;
    uint64_t    m_duration {0};
    QMutex      m_lock;
};

class SubtitleReader
{
  public:
    SubtitleReader() = default;
    ~SubtitleReader();

    void EnableAVSubtitles(bool enable);
    void EnableTextSubtitles(bool enable);
    void EnableRawTextSubtitles(bool enable);

    AVSubtitles* GetAVSubtitles(void) { return &m_avSubtitles; }
    bool AddAVSubtitle(AVSubtitle& subtitle, bool fix_position,
                       bool allow_forced);
    void ClearAVSubtitles(void);
    static void FreeAVSubtitle(AVSubtitle &sub);

    TextSubtitles* GetTextSubtitles(void) { return &m_textSubtitles; }
    bool HasTextSubtitles(void);
    void LoadExternalSubtitles(const QString &subtitleFileName, bool isInProgress);

    QStringList GetRawTextSubtitles(uint64_t &duration);
    void AddRawTextSubtitle(const QStringList& list, uint64_t duration);
    void ClearRawTextSubtitles(void);

  private:
    AVSubtitles   m_avSubtitles;
    bool          m_avSubtitlesEnabled      {false};
    TextSubtitles m_textSubtitles;
    bool          m_textSubtitlesEnabled    {false};
    RawTextSubs   m_rawTextSubtitles;
    bool          m_rawTextSubtitlesEnabled {false};
};

#endif // SUBTITLEREADER_H
