#ifndef DVBCAM_H
#define DVBCAM_H

#include <deque>
using namespace std;

#include <QWaitCondition>
#include <QRunnable>
#include <QString>
#include <QMutex>

#include "mpegtables.h"

#include "dvbtypes.h"

class ChannelBase;
class cCiHandler;
class MThread;
class DVBCam;

typedef QMap<const ChannelBase*, ProgramMapTable*> pmt_list_t;

class DVBCam : public QRunnable
{
  public:
    explicit DVBCam(const QString &device);
    ~DVBCam();

    bool Start(void);
    bool Stop(void);
    bool IsRunning(void) const
    {
        QMutexLocker locker(&m_ciHandlerLock);
        return m_ciHandlerRunning;
    }

    void SetPMT(const ChannelBase *chan, const ProgramMapTable *pmt);
    void SetTimeOffset(double offset_in_seconds);

  private:
    void run(void) override; // QRunnable
    void HandleUserIO(void);
    void HandlePMT(void);

    void SendPMT(const ProgramMapTable &pmt, uint cplm);

    QString         m_device;
    int             m_numslots         {0};

    mutable QMutex  m_ciHandlerLock;
    QWaitCondition  m_ciHandlerWait;
    bool            m_ciHandlerDoRun   {false};
    bool            m_ciHandlerRunning {false};
    cCiHandler     *m_ciHandler        {nullptr};
    MThread        *m_ciHandlerThread  {nullptr};

    QMutex          m_pmtLock;
    pmt_list_t      m_pmtList;
    pmt_list_t      m_pmtAddList;
    bool            m_havePmt          {false};
    bool            m_pmtSent          {false};
    bool            m_pmtUpdated       {false};
    bool            m_pmtAdded         {false};
};

#endif // DVBCAM_H

