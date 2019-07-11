#ifndef MYTH_MEDIA_MONITOR_WINDOWS_H
#define MYTH_MEDIA_MONITOR_WINDOWS_H

#include "mythmediamonitor.h"

#define DEFAULT_DVD "e:"
#define DEFAULT_CD  "e:"

class MediaMonitorWindows : public MediaMonitor
{
  public:
    MediaMonitorWindows(QObject *par, unsigned long interval, bool allowEject);

    bool AddDevice(MythMediaDevice* pDevice) override; // MediaMonitor

    QStringList GetCDROMBlockDevices(void);
};
#endif // MYTH_MEDIA_MONITOR_WINDOWS_H
