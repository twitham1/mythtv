#ifndef AUDIOPLAYER_H
#define AUDIOPLAYER_H

#include "audiooutputsettings.h"
#include "mythtvexp.h"
#include "volumebase.h" // MuteState

#include <QCoreApplication>
#include <QMutex>

#include <cstdint>
#include <vector>

using std::vector;

class  MythPlayer;
class  AudioOutput;
struct AVCodecContext;
struct AVPacket;

namespace MythTV
{
    class Visual;
}

class MTV_PUBLIC AudioPlayer
{
    Q_DECLARE_TR_FUNCTIONS(AudioPlayer);

  public:
    AudioPlayer(MythPlayer *parent, bool muted);
   ~AudioPlayer();

    void addVisual(MythTV::Visual *vis);
    void removeVisual(MythTV::Visual *vis);

    void  Reset(void);
    void  DeleteOutput(void);
    QString ReinitAudio(void);
    void  SetAudioOutput(AudioOutput *ao);
    void  SetAudioInfo(const QString &main_device,
                       const QString &passthru_device,
                       uint           samplerate,
                       int            codec_profile = -1);
    void  SetAudioParams(AudioFormat format, int orig_channels, int channels,
                         AVCodecID codec, int samplerate, bool passthru,
                         int codec_profile = -1);
    void  SetEffDsp(int dsprate);

    void  CheckFormat(void);
    void  SetNoAudio(void)           { m_no_audio_out = true;    }
    bool  HasAudioIn(void) const     { return !m_no_audio_in;    }
    bool  HasAudioOut(void) const    { return !m_no_audio_out;   }
    bool  ControlsVolume(void) const { return m_controls_volume; }

    bool  Pause(bool pause);
    bool  IsPaused(void);
    void  PauseAudioUntilBuffered(void);
    AVCodecID GetCodec(void)    const { return m_codec;         }
    int   GetNumChannels(void)  const { return m_channels;      }
    int   GetOrigChannels(void) const { return m_orig_channels; }
    int   GetSampleRate(void)   const { return m_samplerate;    }
    uint  GetVolume(void);
    uint  AdjustVolume(int change);
    uint  SetVolume(int newvolume);
    float GetStretchFactor(void) const { return m_stretchfactor; }
    void  SetStretchFactor(float factor);
    bool  IsUpmixing(void);
    bool  EnableUpmix(bool enable, bool toggle = false);
    bool  CanUpmix(void);
    bool  CanPassthrough(int samplerate, int channels, AVCodecID codec, int profile);
    bool  CanDownmix(void);
    bool  CanAC3(void);
    bool  CanDTS(void);
    bool  CanEAC3(void);
    bool  CanTrueHD(void);
    bool  CanDTSHD(void);
    uint  GetMaxChannels(void);
    int   GetMaxHDRate(void);
    int64_t GetAudioTime(void);
    AudioFormat GetFormat(void) const { return m_format; }
    bool CanProcess(AudioFormat fmt);
    uint32_t CanProcess(void);
    int   DecodeAudio(AVCodecContext *ctx,
                      uint8_t *buffer, int &data_size,
                      const AVPacket *pkt);

    bool      IsMuted(void) { return GetMuteState() == kMuteAll; }
    bool      SetMuted(bool mute);
    MuteState GetMuteState(void);
    MuteState SetMuteState(MuteState);
    MuteState IncrMuteState(void);

    void AddAudioData(char *buffer, int len, int64_t timecode, int frames);
    bool NeedDecodingBeforePassthrough(void);
    int64_t LengthLastData(void);
    bool GetBufferStatus(uint &fill, uint &total);
    bool IsBufferAlmostFull(void);
    int64_t GetAudioBufferedTime(void);
    
    /**
     * Return internal AudioOutput object
     */
    AudioOutput *GetAudioOutput(void) const { return m_audioOutput; }

  private:
    void AddVisuals(void);
    void RemoveVisuals(void);
    void ResetVisuals(void);

  private:
    MythPlayer  *m_parent            {nullptr};
    AudioOutput *m_audioOutput       {nullptr};
    int          m_channels          {-1};
    int          m_orig_channels     {-1};
    AVCodecID    m_codec             {AV_CODEC_ID_NONE};
    AudioFormat  m_format            {FORMAT_NONE};
    int          m_samplerate        {44100};
    int          m_codec_profile     {0};
    float        m_stretchfactor     {1.0F};
    bool         m_passthru          {false};
    QMutex       m_lock              {QMutex::Recursive};
    bool         m_muted_on_creation;
    QString      m_main_device;
    QString      m_passthru_device;
    bool         m_no_audio_in       {false};
    bool         m_no_audio_out      {true};
    bool         m_controls_volume   {true};
    vector<MythTV::Visual*> m_visuals;
};

#endif // AUDIOPLAYER_H
