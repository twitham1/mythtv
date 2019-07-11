#ifndef ALARMNOTIFYTHREAD_H
#define ALARMNOTIFYTHREAD_H

// Qt headers

// MythTV headers
#include "mthread.h"

// zm
#include "zmdefines.h"

class AlarmNotifyThread : public MThread
{
  protected:
    AlarmNotifyThread(void);

    static AlarmNotifyThread *m_alarmNotifyThread;

  public:
    ~AlarmNotifyThread(void);

    static AlarmNotifyThread *get(void);
    void stop(void);

  protected:
    void run(void) override; // MThread

  private:
    volatile bool m_stop;
};

#endif // ALARMNOTIFYTHREAD_H
