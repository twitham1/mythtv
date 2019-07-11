#ifndef PLAYBACKSOCK_H_
#define PLAYBACKSOCK_H_

#include <vector>
using namespace std;

#include <QStringList>
#include <QDateTime>
#include <QMutex>
#include <QSize>

#include "referencecounter.h"
#include "programinfo.h" // ProgramInfo
#include "programtypes.h" // RecStatus::Type
#include "inputinfo.h"

class MythSocket;
class MainServer;
class ProgramInfo;

typedef enum {
    kPBSEvents_None       = 0,
    kPBSEvents_Normal     = 1,
    kPBSEvents_NonSystem  = 2,
    kPBSEvents_SystemOnly = 3
} PlaybackSockEventsMode;

class PlaybackSock : public ReferenceCounter
{
  public:
    PlaybackSock(MainServer *parent, MythSocket *lsock,
                 QString lhostname, PlaybackSockEventsMode eventsMode);

    void SetDisconnected(void) { m_disconnected = true; }
    bool IsDisconnected(void) const { return m_disconnected; }

    MythSocket *getSocket(void) const { return m_sock; }
    QString getHostname(void) const { return m_hostname; }

    bool isLocal(void) const { return m_local; }
    bool wantsEvents(void) const;
    bool wantsNonSystemEvents(void) const;
    bool wantsSystemEvents(void) const;
    bool wantsOnlySystemEvents(void) const;
    PlaybackSockEventsMode eventsMode(void) const;

    bool getBlockShutdown(void) const { return m_blockshutdown; }
    void setBlockShutdown(bool value) { m_blockshutdown = value; }

    bool IsFrontend(void) const { return m_frontend; }
    void SetAsFrontend(void) { m_frontend = true; }

    // all backend<->backend stuff below here
    bool isSlaveBackend(void) const { return m_backend; }
    void setAsSlaveBackend(void) { m_backend = true; m_mediaserver = true; }

    bool isMediaServer(void) const { return m_mediaserver; }
    void setAsMediaServer(void) { m_mediaserver = true; }

    void setIP(QString &lip) { m_ip = lip; }
    QString getIP(void) const { return m_ip; }

    bool GoToSleep(void);
    void GetDiskSpace(QStringList &o_strlist);
    int DeleteFile(const QString &filename, const QString &sgroup);
    int StopRecording(const ProgramInfo *pginfo);
    int CheckRecordingActive(const ProgramInfo *pginfo);
    int DeleteRecording(const ProgramInfo *pginfo, bool forceMetadataDelete = false);
    bool FillProgramInfo(ProgramInfo &pginfo, const QString &playbackhost);
    QStringList GetSGFileList(QString &host, QString &groupname,
                              QString &directory, bool fileNamesOnly);
    QStringList GetSGFileQuery(QString &host, QString &groupname,
                               QString &filename);
    QString GetFileHash(const QString& filename, const QString& storageGroup);
    QStringList GetFindFile(const QString &host, const QString &filename,
                            const QString &storageGroup, bool useRegex);

    QStringList GenPreviewPixmap(const QString     &token,
                                 const ProgramInfo *pginfo);
    QStringList GenPreviewPixmap(const QString     &token,
                                 const ProgramInfo *pginfo,
                                 bool               time_fmt_sec,
                                 long long          time,
                                 const QString     &outputFile,
                                 const QSize       &outputSize);
    QDateTime PixmapLastModified(const ProgramInfo *pginfo);
    bool CheckFile(ProgramInfo *pginfo);

    bool IsBusy(int        capturecardnum,
                InputInfo *busy_input  = nullptr,
                int        time_buffer = 5);
    int GetEncoderState(int capturecardnum);
    long long GetMaxBitrate(int capturecardnum);
    ProgramInfo *GetRecording(uint cardid);
    bool EncoderIsRecording(int capturecardnum, const ProgramInfo *pginfo);
    RecStatus::Type StartRecording(int capturecardnum,
                                 ProgramInfo *pginfo);
    RecStatus::Type GetRecordingStatus(int capturecardnum);
    void RecordPending(int capturecardnum, const ProgramInfo *pginfo,
                       int secsleft, bool hasLater);
    int SetSignalMonitoringRate(int capturecardnum, int rate, int notifyFrontend);
    void SetNextLiveTVDir(int capturecardnum, const QString& dir);
    void CancelNextRecording(int capturecardnum, bool cancel);

    QStringList ForwardRequest(const QStringList&);

    bool ReadStringList(QStringList &list);

    bool AddChildInput(uint childid);

    // Enforce reference counting
  protected:
    virtual ~PlaybackSock();

  private:
    bool SendReceiveStringList(QStringList &strlist, uint min_reply_length = 0);

    MythSocket             *m_sock          {nullptr};
    QString                 m_hostname;
    QString                 m_ip;

    bool                    m_local         {true};
    PlaybackSockEventsMode  m_eventsMode;
    bool                    m_blockshutdown {true};
    bool                    m_backend       {false};
    bool                    m_mediaserver   {false};
    bool                    m_frontend      {false};

    QMutex                  m_sockLock;

    bool                    m_disconnected  {false};

    MainServer             *m_parent        {nullptr};
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
