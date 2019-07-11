#ifndef MYTHHDD_H
#define MYTHHDD_H

#include "mythmedia.h"

class MBASE_PUBLIC MythHDD : public MythMediaDevice
{
  public:
    MythHDD(QObject* par, const char* DevicePath,
            bool SuperMount, bool AllowEject);

    MythMediaStatus checkMedia(void) override; // MythMediaDevice
    MythMediaError eject(bool) override; // MythMediaDevice

    static MythHDD* Get(QObject* par, const char* devicePath,
                        bool SuperMount, bool AllowEject);
};

#endif
