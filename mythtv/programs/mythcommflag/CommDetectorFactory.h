#ifndef COMMDETECTOR_FACTORY_H
#define COMMDETECTOR_FACTORY_H

#include "programinfo.h"

class CommDetectorBase;
class MythPlayer;
class RemoteEncoder;
class QDateTime;

class CommDetectorFactory
{
  public:
    CommDetectorFactory() = default;
    ~CommDetectorFactory() = default;

    static CommDetectorBase* makeCommDetector(
        SkipType commDetectMethod,
        bool showProgress,
        bool fullSpeed, MythPlayer* player,
        int chanid,
        const QDateTime& startedAt,
        const QDateTime& stopsAt,
        const QDateTime& recordingStartedAt,
        const QDateTime& recordingStopsAt,
        bool useDB);
};

#endif // COMMDETECTOR_FACTORY_H

/* vim: set expandtab tabstop=4 shiftwidth=4: */
