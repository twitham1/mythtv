#ifndef MINIPLAYER_H_
#define MINIPLAYER_H_

#include <mythscreentype.h>

#include "musiccommon.h"

class QTimer;

class MPUBLIC MiniPlayer : public MusicCommon
{
  Q_OBJECT

  public:
    explicit MiniPlayer(MythScreenStack *parent);
    ~MiniPlayer();

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *) override; // MusicCommon

  public slots:
    void timerTimeout(void);

  private:
    QTimer *m_displayTimer {nullptr};
};

#endif
