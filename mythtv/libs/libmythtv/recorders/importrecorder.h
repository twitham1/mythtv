// -*- Mode: c++ -*-

#ifndef _IMPORT_RECORDER_H_
#define _IMPORT_RECORDER_H_

#include <QMutex>

#include "dtvrecorder.h"
#include "tspacket.h"
#include "mpegstreamdata.h"
#include "DeviceReadBuffer.h"

struct AVFormatContext;
struct AVPacket;
class MythCommFlagPlayer;

/** \brief ImportRecorder imports files, creating a seek map and
 *         other stuff that MythTV likes to have for recording.
 *
 *  \note This currently only supports MPEG-TS files, but the
 *        plan is to support all files that ffmpeg does.
 */
class ImportRecorder : public DTVRecorder
{
  public:
    explicit ImportRecorder(TVRec*rec) : DTVRecorder(rec) {}
    ~ImportRecorder() = default;

    // RecorderBase
    void SetOptionsFromProfile(RecordingProfile *profile,
                               const QString &videodev,
                               const QString &audiodev,
                               const QString &vbidev) override; // DTVRecorder

    void run(void) override; // RecorderBase

    bool Open(void);
    void Close(void);

    void InitStreamData(void) override {} // DTVRecorder

    long long GetFramesWritten(void) override; // DTVRecorder
    RecordingQuality *GetRecordingQuality(const RecordingInfo*) const override // DTVRecorder
        {return nullptr;}
    void UpdateRecSize();

  private:
    int                 m_import_fd {-1};
    MythCommFlagPlayer *m_cfp       {nullptr};
    long long           m_nfc       {0};
};

#endif // _IMPORT_RECORDER_H_
