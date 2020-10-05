#ifndef MYTH_SCREENSAVER_OSX_H
#define MYTH_SCREENSAVER_OSX_H

// MythTV
#include "mythscreensaver.h"

// macOS
#include <IOKit/pwr_mgt/IOPMLib.h>

class MythScreenSaverOSX : public MythScreenSaver
{
  public:
    MythScreenSaverOSX() = default;
   ~MythScreenSaverOSX() override = default;

    void Disable() override;
    void Restore() override;
    void Reset() override;
    bool Asleep() override;

  private:
    Q_DISABLE_COPY(MythScreenSaverOSX)
    IOPMAssertionID iopm_id {kIOPMNullAssertionID};
};

#endif
