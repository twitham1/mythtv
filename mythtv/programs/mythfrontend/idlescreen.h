#ifndef _IDLESCREEN_H_
#define _IDLESCREEN_H_

#include <mythscreentype.h>
// libmyth
#include "programinfo.h"

class MythUIStateType;
class MythUIButtonList;
class QTimer;

class IdleScreen : public MythScreenType
{
    Q_OBJECT

  public:
    explicit IdleScreen(MythScreenStack *parent);
    virtual ~IdleScreen();

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType
    void customEvent(QEvent *e) override; // MythUIType


  public slots:
    void UpdateStatus(void);
    void UpdateScreen(void);
    bool UpdateScheduledList();

  protected:
    void Load(void) override; // MythScreenType
    void Init(void) override; // MythScreenType

  private:
    bool CheckConnectionToServer(void);
    bool PendingSchedUpdate() const             { return m_pendingSchedUpdate; }
    void SetPendingSchedUpdate(bool newState)   { m_pendingSchedUpdate = newState; }

    QTimer           *m_updateScreenTimer     {nullptr};

    MythUIStateType  *m_statusState           {nullptr};
    MythUIButtonList *m_currentRecordings     {nullptr};
    MythUIButtonList *m_nextRecordings        {nullptr};
    MythUIButtonList *m_conflictingRecordings {nullptr};
    MythUIText       *m_conflictWarning       {nullptr};

    int             m_secondsToShutdown       {-1};

    QMutex          m_schedUpdateMutex;
    bool            m_pendingSchedUpdate      {false};
    ProgramList     m_scheduledList;
    bool            m_hasConflicts            {false};
};

#endif
