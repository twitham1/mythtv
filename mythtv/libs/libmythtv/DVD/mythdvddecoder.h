#ifndef AVFORMATDECODERDVD_H
#define AVFORMATDECODERDVD_H

// Qt
#include <QList>

// MythTV
#include "avformatdecoder.h"

#define INVALID_LBA 0xbfffffff

class MythDVDContext;

class MythDVDDecoder : public AvFormatDecoder
{
  public:
    MythDVDDecoder(MythPlayer *Parent, const ProgramInfo &PGInfo, PlayerFlags Flags);
    ~MythDVDDecoder() override;

    void Reset             (bool ResetVideoData, bool SeekReset, bool ResetFile) override;
    void UpdateFramesPlayed(void) override;
    bool GetFrame          (DecodeType Type, bool &Retry) override;

  protected:
    int  ReadPacket        (AVFormatContext *Ctx, AVPacket *Pkt, bool &StorePacket) override;
    bool ProcessVideoPacket(AVStream *Stream, AVPacket *Pkt, bool &Retry) override;
    bool ProcessVideoFrame (AVStream *Stream, AVFrame *Frame) override;
    bool ProcessDataPacket (AVStream *Curstream, AVPacket *Pkt, DecodeType Decodetype) override;

  private:
    bool DoRewindSeek      (long long DesiredFrame) override;
    void DoFastForwardSeek (long long DesiredFrame, bool &NeedFlush) override;
    void StreamChangeCheck (void) override;
    void PostProcessTracks (void) override;
    int  GetAudioLanguage  (uint AudioIndex, uint StreamIndex) override;
    AudioTrackType GetAudioTrackType(uint Index) override;

    void CheckContext          (int64_t Pts);
    void ReleaseLastVideoPkt   (void);
    static void ReleaseContext (MythDVDContext *&Context);
    long long   DVDFindPosition(long long DesiredFrame);

    MythDVDContext*        m_curContext      { nullptr };
    QList<MythDVDContext*> m_contextList;
    AVPacket*              m_lastVideoPkt    { nullptr };
    uint32_t               m_lbaLastVideoPkt { INVALID_LBA};
    int                    m_framesReq       { 0       };
    MythDVDContext*        m_returnContext   { nullptr };
};

#endif
