#ifndef PLAYBACKBOXHELPER_H
#define PLAYBACKBOXHELPER_H

#include <cstdint>

#include <QStringList>
#include <QDateTime>
#include <QMutex>

#include "mythcorecontext.h"
#include "metadatacommon.h"
#include "mthread.h"
#include "mythtypes.h"

class PreviewGenerator;
class PBHEventHandler;
class ProgramInfo;
class QStringList;
class QObject;
class QTimer;

enum CheckAvailabilityType {
    kCheckForCache,
    kCheckForMenuAction,
    kCheckForPlayAction,
    kCheckForPlaylistAction,
};

class PlaybackBoxHelper : public MThread
{
    friend class PBHEventHandler;

  public:
    explicit PlaybackBoxHelper(QObject *listener);
    ~PlaybackBoxHelper(void) override;

    void ForceFreeSpaceUpdate(void);
    void StopRecording(const ProgramInfo &pginfo);
    void DeleteRecording( uint recordingID, bool forceDelete,
                          bool forgetHistory);
    void DeleteRecordings(const QStringList &list);
    void UndeleteRecording(uint recordingID);
    void CheckAvailability(const ProgramInfo &pginfo,
                           CheckAvailabilityType cat = kCheckForCache);
    QString GetPreviewImage(const ProgramInfo &pginfo, bool check_availability = true);

    QString LocateArtwork(const QString &inetref, uint season,
                          VideoArtworkType type, const ProgramInfo *pginfo,
                          const QString &groupname = nullptr);

    uint64_t GetFreeSpaceTotalMB(void) const;
    uint64_t GetFreeSpaceUsedMB(void) const;

  private:
    void UpdateFreeSpace(void);

  private:
    QObject            *m_listener         {nullptr};
    PBHEventHandler    *m_eventHandler     {nullptr};
    mutable QMutex      m_lock;

    // Free disk space tracking
    uint64_t            m_freeSpaceTotalMB {0LL};
    uint64_t            m_freeSpaceUsedMB  {0LL};

    // Artwork Variables //////////////////////////////////////////////////////
    InfoMap             m_artworkCache;
};

#endif // PLAYBACKBOXHELPER_H
