#ifndef DECODERBASE_H_
#define DECODERBASE_H_

#include <cstdint>
#include <vector>
using namespace std;

#include "io/mythmediabuffer.h"
#include "remoteencoder.h"
#include "mythcontext.h"
#include "mythdbcon.h"
#include "programinfo.h"
#include "mythcodecid.h"
#include "mythavutil.h"
#include "videodisplayprofile.h"

class TeletextViewer;
class MythPlayer;
class AudioPlayer;
class MythCodecContext;

const int kDecoderProbeBufferSize = 256 * 1024;

/// Track types
enum TrackType
{
    kTrackTypeUnknown = 0,
    kTrackTypeAudio,            // 1
    kTrackTypeVideo,            // 2
    kTrackTypeSubtitle,         // 3
    kTrackTypeCC608,            // 4
    kTrackTypeCC708,            // 5
    kTrackTypeTeletextCaptions, // 6
    kTrackTypeTeletextMenu,     // 7
    kTrackTypeRawText,          // 8
    kTrackTypeAttachment,       // 9
    kTrackTypeCount,            // 10
    // The following are intentionally excluded from kTrackTypeCount which
    // is used when auto-selecting the correct tracks to decode according to
    // language, bitrate etc
    kTrackTypeTextSubtitle,     // 11
};
QString toString(TrackType type);
int to_track_type(const QString &str);

enum DecodeType
{
    kDecodeNothing = 0x00, // Demux and preprocess only.
    kDecodeVideo   = 0x01,
    kDecodeAudio   = 0x02,
    kDecodeAV      = 0x03,
};

enum AudioTrackType
{
    kAudioTypeNormal = 0,
    kAudioTypeAudioDescription, // Audio Description for the visually impaired
    kAudioTypeCleanEffects, // No dialog, soundtrack or effects only e.g. Karaoke
    kAudioTypeHearingImpaired, // Greater contrast between dialog and background audio
    kAudioTypeSpokenSubs, // Spoken subtitles for the visually impaired
    kAudioTypeCommentary // Director/actor/etc Commentary
};
QString toString(AudioTrackType type);

// Eof States
enum EofState
{
    kEofStateNone,     // no eof
    kEofStateDelayed,  // decoder eof, but let player drain buffered frames
    kEofStateImmediate // true eof
};

class StreamInfo
{
  public:
    StreamInfo() = default;
    StreamInfo(int a, int b, uint c, int d, int e, bool f = false,
               bool g = false, bool h = false,
               AudioTrackType i = kAudioTypeNormal) :
        m_av_stream_index(a),
        m_language(b), m_language_index(c), m_stream_id(d),
        m_easy_reader(f), m_wide_aspect_ratio(g), m_orig_num_channels(e), m_forced(h),
        m_audio_type(i) {}
    StreamInfo(int a, int b, uint c, int d, int e, int f,
               bool g = false, bool h = false, bool i = false,
               AudioTrackType j = kAudioTypeNormal) :
        m_av_stream_index(a), m_av_substream_index(e),
        m_language(b), m_language_index(c), m_stream_id(d),
        m_easy_reader(g), m_wide_aspect_ratio(h), m_orig_num_channels(f), m_forced(i),
        m_audio_type(j) {}

  public:
    int            m_av_stream_index    {-1};
    /// -1 for no substream, 0 for first dual audio stream, 1 for second dual
    int            m_av_substream_index {-1};
    int            m_language           {-2}; ///< ISO639 canonical language key
    uint           m_language_index     {0};
    int            m_stream_id          {-1};
    bool           m_easy_reader        {false};
    bool           m_wide_aspect_ratio  {false};
    int            m_orig_num_channels  {2};
    bool           m_forced             {false};
    AudioTrackType m_audio_type {kAudioTypeNormal};

    bool operator<(const StreamInfo& b) const
    {
        return (this->m_stream_id < b.m_stream_id);
    }
};
using sinfo_vec_t = vector<StreamInfo>;

inline AVRational AVRationalInit(int num, int den = 1) {
    AVRational result;
    result.num = num;
    result.den = den;
    return result;
}

class DecoderBase
{
  public:
    DecoderBase(MythPlayer *parent, const ProgramInfo &pginfo);
    DecoderBase(const DecoderBase& rhs);
    virtual ~DecoderBase();

    virtual void Reset(bool reset_video_data, bool seek_reset, bool reset_file);

    virtual int OpenFile(MythMediaBuffer *Buffer, bool novideo,
                         char testbuf[kDecoderProbeBufferSize],
                         int testbufsize = kDecoderProbeBufferSize) = 0;

    virtual void SetEofState(EofState eof)  { m_atEof = eof;  }
    virtual void SetEof(bool eof)  {
        m_atEof = eof ? kEofStateDelayed : kEofStateNone;
    }
    EofState     GetEof(void)      { return m_atEof; }

    void SetSeekSnap(uint64_t snap)  { m_seekSnap = snap; }
    uint64_t GetSeekSnap(void) const { return m_seekSnap;  }
    void SetLiveTVMode(bool live)  { m_livetv = live;      }

    // Must be done while player is paused.
    void SetProgramInfo(const ProgramInfo &pginfo);

    /// Disables AC3/DTS pass through
    virtual void SetDisablePassThrough(bool disable) { (void)disable; }
    // Reconfigure audio as necessary, following configuration change
    virtual void ForceSetupAudioStream(void) { }

    virtual void SetWatchingRecording(bool mode);
    /// Demux, preprocess and possibly decode a frame of video/audio.
    virtual bool GetFrame(DecodeType Type, bool &Retry) = 0;
    MythPlayer *GetPlayer() { return m_parent; }

    virtual int GetNumChapters(void)                      { return 0; }
    virtual int GetCurrentChapter(long long /*framesPlayed*/) { return 0; }
    virtual void GetChapterTimes(QList<long long> &/*times*/) { }
    virtual long long GetChapter(int /*chapter*/)             { return m_framesPlayed; }
    virtual bool DoRewind(long long desiredFrame, bool discardFrames = true);
    virtual bool DoFastForward(long long desiredFrame, bool discardFrames = true);
    virtual void SetIdrOnlyKeyframes(bool /*value*/) { }

    static uint64_t
        TranslatePositionAbsToRel(const frm_dir_map_t &deleteMap,
                                  uint64_t absPosition,
                                  const frm_pos_map_t &map = frm_pos_map_t(),
                                  float fallback_ratio = 1.0);
    static uint64_t
        TranslatePositionRelToAbs(const frm_dir_map_t &deleteMap,
                                  uint64_t relPosition,
                                  const frm_pos_map_t &map = frm_pos_map_t(),
                                  float fallback_ratio = 1.0);
    static uint64_t TranslatePosition(const frm_pos_map_t &map,
                                      long long key,
                                      float fallback_ratio);
    uint64_t TranslatePositionFrameToMs(long long position,
                                        float fallback_framerate,
                                        const frm_dir_map_t &cutlist);
    uint64_t TranslatePositionMsToFrame(uint64_t dur_ms,
                                        float fallback_framerate,
                                        const frm_dir_map_t &cutlist);

    float GetVideoAspect(void) const { return m_currentAspect; }

    virtual int64_t NormalizeVideoTimecode(int64_t timecode) { return timecode; }

    virtual bool IsLastFrameKey(void) const = 0;
    virtual void WriteStoredData(MythMediaBuffer *Buffer, bool storevid,
                                 long timecodeOffset) = 0;
    virtual void ClearStoredData(void) { }
    virtual void SetRawAudioState(bool state) { m_getRawFrames = state; }
    virtual bool GetRawAudioState(void) const { return m_getRawFrames; }
    virtual void SetRawVideoState(bool state) { m_getRawVideo = state; }
    virtual bool GetRawVideoState(void) const { return m_getRawVideo; }

    virtual long UpdateStoredFrameNum(long frame) = 0;

    virtual double  GetFPS(void) const { return m_fps; }
    /// Returns the estimated bitrate if the video were played at normal speed.
    uint GetRawBitrate(void) const { return m_bitrate; }

    virtual void UpdateFramesPlayed(void);
    long long GetFramesRead(void) const { return m_framesRead; }
    long long GetFramesPlayed(void) const { return m_framesPlayed; }
    void SetFramesPlayed(long long newValue) {m_framesPlayed = newValue;}

    virtual QString GetCodecDecoderName(void) const = 0;
    virtual QString GetRawEncodingType(void) { return QString(); }
    virtual MythCodecID GetVideoCodecID(void) const = 0;

    virtual void ResetPosMap(void);
    virtual bool SyncPositionMap(void);
    virtual bool PosMapFromDb(void);
    virtual bool PosMapFromEnc(void);

    virtual bool FindPosition(long long desired_value, bool search_adjusted,
                              int &lower_bound, int &upper_bound);

    uint64_t SavePositionMapDelta(long long first_frame, long long last_frame);
    virtual void SeekReset(long long newkey, uint skipFrames,
                           bool doFlush, bool discardFrames);

    void SetTranscoding(bool value) { m_transcoding = value; }

    bool IsErrored() const { return m_errored; }

    bool HasPositionMap(void) const { return GetPositionMapSize() != 0U; }

    void SetWaitForChange(void);
    bool GetWaitForChange(void) const;
    void SetReadAdjust(long long adjust);

    // Audio/Subtitle/EIA-608/EIA-708 stream selection
    void         SetDecodeAllSubtitles(bool DecodeAll);
    virtual QStringList GetTracks(uint Type);
    virtual uint GetTrackCount(uint Type);

    virtual int  GetTrackLanguageIndex(uint Type, uint TrackNo);
    virtual QString GetTrackDesc(uint Type, uint TrackNo);
    virtual int  SetTrack(uint Type, int TrackNo);
    int          GetTrack(uint Type);
    StreamInfo   GetTrackInfo(uint Type, uint TrackNo);
    int          ChangeTrack(uint Type, int Dir);
    virtual bool InsertTrack(uint Type, const StreamInfo &Info);
    int          NextTrack(uint Type);

    virtual int  GetTeletextDecoderType(void) const { return -1; }

    virtual QString GetXDS(const QString &/*key*/) const { return QString(); }
    virtual QByteArray GetSubHeader(uint /*trackNo*/) { return QByteArray(); }
    virtual void GetAttachmentData(uint /*trackNo*/, QByteArray &/*filename*/,
                                   QByteArray &/*data*/) {}

    // MHEG/MHI stuff
    virtual bool SetAudioByComponentTag(int /*tag*/) { return false; }
    virtual bool SetVideoByComponentTag(int /*tag*/) { return false; }

    void SaveTotalDuration(void);
    void ResetTotalDuration(void) { m_totalDuration = AVRationalInit(0); }
    void SaveTotalFrames(void);
    void TrackTotalDuration(bool track) { m_trackTotalDuration = track; }
    int GetfpsMultiplier(void) const { return m_fpsMultiplier; }
    MythCodecContext *GetMythCodecContext(void) { return m_mythCodecCtx; }
    VideoDisplayProfile * GetVideoDisplayProfile(void) { return &m_videoDisplayProfile; }
    AVPixelFormat GetBestVideoFormat(AVPixelFormat* Formats);

  protected:
    virtual int  AutoSelectTrack(uint Type);
    void         AutoSelectTracks(void);
    void         ResetTracks(void);
    void         FileChanged(void);
    virtual bool DoRewindSeek(long long desiredFrame);
    virtual void DoFastForwardSeek(long long desiredFrame, bool &needflush);

    long long ConditionallyUpdatePosMap(long long desiredFrame);
    long long GetLastFrameInPosMap(void) const;
    unsigned long GetPositionMapSize(void) const;

    struct PosMapEntry
    {
        long long index;    // frame or keyframe number
        long long adjFrame; // keyFrameAdjustTable adjusted frame number
        long long pos;      // position in stream
    };
    long long GetKey(const PosMapEntry &entry) const;

    MythPlayer          *m_parent                  {nullptr};
    ProgramInfo         *m_playbackInfo            {nullptr};
    AudioPlayer         *m_audio                   {nullptr};
    MythMediaBuffer     *m_ringBuffer              {nullptr};

    double               m_fps                     {29.97};
    int                  m_fpsMultiplier           {1};
    int                  m_fpsSkip                 {0};
    uint                 m_bitrate                 {4000};
    int                  m_currentWidth            {640};
    int                  m_currentHeight           {480};
    float                m_currentAspect           {1.33333F};

    long long            m_framesPlayed            {0};
    long long            m_framesRead              {0};
    unsigned long long   m_frameCounter            {0};
    AVRational           m_totalDuration;
    int                  m_keyframeDist            {-1};
    long long            m_lastKey                 {0};
    long long            m_indexOffset             {0};
    MythAVCopy           m_copyFrame;
    bool                 m_nextDecodedFrameIsKeyFrame { false };

    EofState             m_atEof                   {kEofStateNone};

    // The totalDuration field is only valid when the video is played
    // from start to finish without any jumping.  trackTotalDuration
    // indicates whether this is the case.
    bool                 m_trackTotalDuration      {false};

    bool                 m_exitAfterDecoded        {false};
    bool                 m_transcoding             {false};

    bool                 m_hasFullPositionMap      {false};
    bool                 m_recordingHasPositionMap {false};
    bool                 m_posmapStarted           {false};
    MarkTypes            m_positionMapType         {MARK_UNSET};

    mutable QMutex       m_positionMapLock         {QMutex::Recursive};
    vector<PosMapEntry>  m_positionMap;
    frm_pos_map_t        m_frameToDurMap; // guarded by m_positionMapLock
    frm_pos_map_t        m_durToFrameMap; // guarded by m_positionMapLock
    mutable QDateTime    m_lastPositionMapUpdate; // guarded by m_positionMapLock

    uint64_t             m_seekSnap                {UINT64_MAX};
    bool                 m_dontSyncPositionMap     {false};
    bool                 m_livetv                  {false};
    bool                 m_watchingRecording       {false};

    bool                 m_hasKeyFrameAdjustTable  {false};

    bool                 m_getRawFrames            {false};
    bool                 m_getRawVideo             {false};

    bool                 m_errored                 {false};

    bool                 m_waitingForChange        {false};
    bool                 m_justAfterChange         {false};
    long long            m_readAdjust              {0};
    int                  m_videoRotation           {0};

    // Audio/Subtitle/EIA-608/EIA-708 stream selection
    QMutex               m_trackLock                     { QMutex::Recursive };
    bool                 m_decodeAllSubtitles            { false };
    int                  m_currentTrack[kTrackTypeCount] { -1    };
    vector<StreamInfo>   m_tracks[kTrackTypeCount];
    StreamInfo           m_wantedTrack[kTrackTypeCount];
    StreamInfo           m_selectedTrack[kTrackTypeCount];

    /// language preferences for auto-selection of streams
    vector<int>          m_languagePreference;
    MythCodecContext    *m_mythCodecCtx         { nullptr };
    VideoDisplayProfile  m_videoDisplayProfile;
};
#endif
