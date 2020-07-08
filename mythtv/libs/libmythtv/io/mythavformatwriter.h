#ifndef AVFORMATWRITER_H_
#define AVFORMATWRITER_H_

// Qt
#include <QList>

// MythTV
#include "mythconfig.h"
#include "mythavutil.h"
#include "io/mythmediawriter.h"
#include "io/mythavformatbuffer.h"

#undef HAVE_AV_CONFIG_H
extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
}

class MTV_PUBLIC MythAVFormatWriter : public MythMediaWriter
{
  public:
    MythAVFormatWriter() = default;
   ~MythAVFormatWriter() override;

    bool Init                (void) override;
    bool OpenFile            (void) override;
    bool CloseFile           (void) override;
    int  WriteVideoFrame     (VideoFrame *Frame) override;
    int  WriteAudioFrame     (unsigned char *Buffer, int FrameNumber, long long &Timecode) override;
    int  WriteTextFrame      (int VBIMode, unsigned char *Buffer, int Length,
                              long long Timecode, int PageNumber) override;
    int  WriteSeekTable      (void) override;
    bool SwitchToNextFile    (void) override;

    bool NextFrameIsKeyFrame (void);
    bool ReOpen              (const QString& Filename);

  private:
    AVStream* AddVideoStream (void);
    bool      OpenVideo      (void);
    AVStream* AddAudioStream (void);
    bool      OpenAudio      (void);
    AVFrame*  AllocPicture   (enum AVPixelFormat PixFmt);
    void      Cleanup        (void);
    AVRational  GetCodecTimeBase (void);
    static bool FindAudioFormat  (AVCodecContext *Ctx, AVCodec *Codec, AVSampleFormat Format);

    MythAVFormatBuffer    *m_avfBuffer     { nullptr };
    MythMediaBuffer       *m_buffer        { nullptr };
    AVOutputFormat         m_fmt           { };
    AVFormatContext       *m_ctx           { nullptr };
    MythCodecMap           m_codecMap;
    AVStream              *m_videoStream   { nullptr };
    AVCodec               *m_avVideoCodec  { nullptr };
    AVStream              *m_audioStream   { nullptr };
    AVCodec               *m_avAudioCodec  { nullptr };
    AVFrame               *m_picture       { nullptr };
    AVFrame               *m_audPicture    { nullptr };
    unsigned char         *m_audioInBuf    { nullptr };
    unsigned char         *m_audioInPBuf   { nullptr };
    QList<long long>       m_bufferedVideoFrameTimes;
    QList<int>             m_bufferedVideoFrameTypes;
    QList<long long>       m_bufferedAudioFrameTimes;
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */

