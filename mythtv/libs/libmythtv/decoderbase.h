#ifndef DECODERBASE_H_
#define DECODERBASE_H_

#include <cstdint>
#include <vector>
using namespace std;

#include "ringbuffer.h"
#include "remoteencoder.h"
#include "mythcontext.h"
#include "mythdbcon.h"
#include "programinfo.h"
#include "mythcodecid.h"
#include "mythavutil.h"
#include "videodisplayprofile.h"

class RingBuffer;
class TeletextViewer;
class MythPlayer;
class AudioPlayer;
class MythCodecContext;

const int kDecoderProbeBufferSize = 256 * 1024;

/// Track types
typedef enum TrackTypes
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
} TrackType;
QString toString(TrackType type);
int to_track_type(const QString &str);

typedef enum DecodeTypes
{
    kDecodeNothing = 0x00, // Demux and preprocess only.
    kDecodeVideo   = 0x01,
    kDecodeAudio   = 0x02,
    kDecodeAV      = 0x03,
} DecodeType;

typedef enum AudioTrackType
{
    kAudioTypeNormal = 0,
    kAudioTypeAudioDescription, // Audio Description for the visually impaired
    kAudioTypeCleanEffects, // No dialog, soundtrack or effects only e.g. Karaoke
    kAudioTypeHearingImpaired, // Greater contrast between dialog and background audio
    kAudioTypeSpokenSubs, // Spoken subtitles for the visually impaired
    kAudioTypeCommentary // Director/actor/etc Commentary
} AudioTrackType;
QString toString(AudioTrackType type);

// Eof States
typedef enum
{
    kEofStateNone,     // no eof
    kEofStateDelayed,  // decoder eof, but let player drain buffered frames
    kEofStateImmediate // true eof
} EofState;

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
typedef vector<StreamInfo> sinfo_vec_t;

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

    virtual int OpenFile(RingBuffer *rbuffer, bool novideo,
                         char testbuf[kDecoderProbeBufferSize],
                         int testbufsize = kDecoderProbeBufferSize) = 0;

    virtual void SetEofState(EofState eof)  { m_ateof = eof;  }
    virtual void SetEof(bool eof)  {
        m_ateof = eof ? kEofStateDelayed : kEofStateNone;
    }
    EofState     GetEof(void)      { return m_ateof; }

    void SetSeekSnap(uint64_t snap)  { m_seeksnap = snap; }
    uint64_t GetSeekSnap(void) const { return m_seeksnap;  }
    void SetLiveTVMode(bool live)  { m_livetv = live;      }

    // Must be done while player is paused.
    void SetProgramInfo(const ProgramInfo &pginfo);

    /// Disables AC3/DTS pass through
    virtual void SetDisablePassThrough(bool disable) { (void)disable; }
    // Reconfigure audio as necessary, following configuration change
    virtual void ForceSetupAudioStream(void) { }

    virtual void SetWatchingRecording(bool mode);
    /// Demux, preprocess and possibly decode a frame of video/audio.
    virtual bool GetFrame(DecodeType) = 0;
    MythPlayer *GetPlayer() { return m_parent; }

    virtual int GetNumChapters(void)                      { return 0; }
    virtual int GetCurrentChapter(long long /*framesPlayed*/) { return 0; }
    virtual void GetChapterTimes(QList<long long> &/*times*/) { return;   }
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

    float GetVideoAspect(void) const { return m_current_aspect; }

    virtual int64_t NormalizeVideoTimecode(int64_t timecode) { return timecode; }

    virtual bool IsLastFrameKey(void) const = 0;
    virtual bool IsCodecMPEG(void) const { return false; }
    virtual void WriteStoredData(RingBuffer *rb, bool storevid,
                                 long timecodeOffset) = 0;
    virtual void ClearStoredData(void) { return; }
    virtual void SetRawAudioState(bool state) { m_getrawframes = state; }
    virtual bool GetRawAudioState(void) const { return m_getrawframes; }
    virtual void SetRawVideoState(bool state) { m_getrawvideo = state; }
    virtual bool GetRawVideoState(void) const { return m_getrawvideo; }

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
    virtual void *GetVideoCodecPrivate(void) { return nullptr; }

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

    bool HasPositionMap(void) const { return GetPositionMapSize(); }

    void SetWaitForChange(void);
    bool GetWaitForChange(void) const;
    void SetReadAdjust(long long adjust);

    // Audio/Subtitle/EIA-608/EIA-708 stream selection
    void SetDecodeAllSubtitles(bool val) { m_decodeAllSubtitles = val; }
    virtual QStringList GetTracks(uint type) const;
    virtual uint GetTrackCount(uint type) const
        { return m_tracks[type].size(); }

    virtual int  GetTrackLanguageIndex(uint type, uint trackNo) const;
    virtual QString GetTrackDesc(uint type, uint trackNo) const;
    virtual int  SetTrack(uint type, int trackNo);
    int          GetTrack(uint type) const { return m_currentTrack[type]; }
    StreamInfo   GetTrackInfo(uint type, uint trackNo) const;
    inline  int  IncrementTrack(uint type);
    inline  int  DecrementTrack(uint type);
    inline  int  ChangeTrack(uint type, int dir);
    virtual bool InsertTrack(uint type, const StreamInfo&);
    inline int   NextTrack(uint type);

    virtual int  GetTeletextDecoderType(void) const { return -1; }

    virtual QString GetXDS(const QString&) const { return QString(); }
    virtual QByteArray GetSubHeader(uint /*trackNo*/) const { return QByteArray(); }
    virtual void GetAttachmentData(uint /*trackNo*/, QByteArray &/*filename*/,
                                   QByteArray &/*data*/) {}

    // MHEG/MHI stuff
    virtual bool SetAudioByComponentTag(int) { return false; }
    virtual bool SetVideoByComponentTag(int) { return false; }

    void SaveTotalDuration(void);
    void ResetTotalDuration(void) { m_totalDuration = AVRationalInit(0); }
    void SaveTotalFrames(void);
    bool GetVideoInverted(void) const { return m_video_inverted; }
    void TrackTotalDuration(bool track) { m_trackTotalDuration = track; }
    int GetfpsMultiplier(void) { return m_fpsMultiplier; }
    MythCodecContext *GetMythCodecContext(void) { return m_mythcodecctx; }
    VideoDisplayProfile * GetVideoDisplayProfile(void) { return &m_videoDisplayProfile; }

  protected:
    virtual int  AutoSelectTrack(uint type);
    inline  void AutoSelectTracks(void);
    inline  void ResetTracks(void);

    void FileChanged(void);

    virtual bool DoRewindSeek(long long desiredFrame);
    virtual void DoFastForwardSeek(long long desiredFrame, bool &needflush);

    long long ConditionallyUpdatePosMap(long long desiredFrame);
    long long GetLastFrameInPosMap(void) const;
    unsigned long GetPositionMapSize(void) const;

    typedef struct posmapentry
    {
        long long index;    // frame or keyframe number
        long long adjFrame; // keyFrameAdjustTable adjusted frame number
        long long pos;      // position in stream
    } PosMapEntry;
    long long GetKey(const PosMapEntry &entry) const;

    MythPlayer          *m_parent                  {nullptr};
    ProgramInfo         *m_playbackinfo            {nullptr};
    AudioPlayer         *m_audio                   {nullptr};
    RingBuffer          *ringBuffer                {nullptr};

    int                  m_current_width           {640};
    int                  m_current_height          {480};
    float                m_current_aspect          {1.33333f};
    double               m_fps                     {29.97};
    int                  m_fpsMultiplier           {1};
    int                  m_fpsSkip                 {0};
    uint                 m_bitrate                 {4000};

    long long            m_framesPlayed            {0};
    long long            m_framesRead              {0};
    AVRational           m_totalDuration;
    long long            m_lastKey                 {0};
    int                  m_keyframedist            {-1};
    long long            m_indexOffset             {0};
    MythAVCopy           m_copyFrame;

    // The totalDuration field is only valid when the video is played
    // from start to finish without any jumping.  trackTotalDuration
    // indicates whether this is the case.
    bool                 m_trackTotalDuration      {false};

    EofState             m_ateof                   {kEofStateNone};
    bool                 m_exitafterdecoded        {false};
    bool                 m_transcoding             {false};

    bool                 m_hasFullPositionMap      {false};
    bool                 m_recordingHasPositionMap {false};
    bool                 m_posmapStarted           {false};
    MarkTypes            m_positionMapType         {MARK_UNSET};

    mutable QMutex       m_positionMapLock         {QMutex::Recursive};
    vector<PosMapEntry>  m_positionMap;
    frm_pos_map_t        m_frameToDurMap; // guarded by m_positionMapLock
    frm_pos_map_t        m_durToFrameMap; // guarded by m_positionMapLock
    bool                 m_dontSyncPositionMap     {false};
    mutable QDateTime    m_lastPositionMapUpdate; // guarded by m_positionMapLock

    uint64_t             m_seeksnap                {UINT64_MAX};
    bool                 m_livetv                  {false};
    bool                 m_watchingrecording       {false};

    bool                 m_hasKeyFrameAdjustTable  {false};

    bool                 m_getrawframes            {false};
    bool                 m_getrawvideo             {false};

    bool                 m_errored                 {false};

    bool                 m_waitingForChange        {false};
    long long            m_readAdjust              {0};
    bool                 m_justAfterChange         {false};
    bool                 m_video_inverted          {false};

    // Audio/Subtitle/EIA-608/EIA-708 stream selection
    bool                 m_decodeAllSubtitles      {false};
    int                  m_currentTrack[kTrackTypeCount];
    sinfo_vec_t          m_tracks[kTrackTypeCount];
    StreamInfo           m_wantedTrack[kTrackTypeCount];
    StreamInfo           m_selectedTrack[(uint)kTrackTypeCount];
    /// language preferences for auto-selection of streams
    vector<int>  m_languagePreference;
    MythCodecContext    *m_mythcodecctx            {nullptr};
    VideoDisplayProfile  m_videoDisplayProfile;
};

inline int DecoderBase::IncrementTrack(uint type)
{
    int next_track = -1;
    int size = m_tracks[type].size();
    if (size)
        next_track = (max(-1, m_currentTrack[type]) + 1) % size;
    return SetTrack(type, next_track);
}

inline int DecoderBase::DecrementTrack(uint type)
{
    int next_track = -1;
    int size = m_tracks[type].size();
    if (size)
        next_track = (max(+0, m_currentTrack[type]) + size - 1) % size;
    return SetTrack(type, next_track);
}

inline int DecoderBase::ChangeTrack(uint type, int dir)
{
    if (dir > 0)
        return IncrementTrack(type);
    else
        return DecrementTrack(type);
}

inline void DecoderBase::AutoSelectTracks(void)
{
    for (uint i = 0; i < kTrackTypeCount; i++)
        AutoSelectTrack(i);
}

inline void DecoderBase::ResetTracks(void)
{
    for (uint i = 0; i < kTrackTypeCount; i++)
        m_currentTrack[i] = -1;
}

inline int DecoderBase::NextTrack(uint type)
{
    int next_track = -1;
    int size = m_tracks[type].size();
    if (size)
        next_track = (max(0, m_currentTrack[type]) + 1) % size;
    return next_track;
}
#endif
