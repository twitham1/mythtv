#ifndef ZMMINIPLAYER_H_
#define ZMMINIPLAYER_H_

#include <mythexp.h>
#include "zmliveplayer.h"

class QTimer;
class MythUIImage;

class MPUBLIC ZMMiniPlayer : public ZMLivePlayer
{
  Q_OBJECT

  public:
    explicit ZMMiniPlayer(MythScreenStack *parent);
    ~ZMMiniPlayer();

    bool Create(void) override; // ZMLivePlayer
    bool keyPressEvent(QKeyEvent *) override; // ZMLivePlayer
    void customEvent (QEvent* event) override; // MythUIType

    void setAlarmMonitor(int monID) { m_alarmMonitor = monID; }

  public slots:
    void timerTimeout(void);

  private:
    QTimer      *m_displayTimer {nullptr};

    MythUIText  *m_monitorText  {nullptr};
    MythUIText  *m_statusText   {nullptr};
    MythUIImage *m_image        {nullptr};
};

#endif
