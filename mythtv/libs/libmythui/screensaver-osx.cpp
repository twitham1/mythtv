#include "screensaver-osx.h"

#include <CoreServices/CoreServices.h>

class ScreenSaverOSXPrivate
{
public:
    static void timerCallback(CFRunLoopTimerRef /* timer */, void * /* info */)
    {
        UpdateSystemActivity(OverallAct);
    };

    CFRunLoopTimerRef m_timer {nullptr};

    friend class ScreenSaverOSX;
};

ScreenSaverOSX::ScreenSaverOSX()
{
    d = new ScreenSaverOSXPrivate();
}

ScreenSaverOSX::~ScreenSaverOSX()
{
    Restore();
    delete d;
}

void ScreenSaverOSX::Disable(void)
{
    CFRunLoopTimerContext context = { 0, nullptr, nullptr, nullptr, nullptr };
    if (!d->m_timer)
    {
        d->m_timer = CFRunLoopTimerCreate(nullptr, CFAbsoluteTimeGetCurrent(),
                                          30, 0, 0,
                                          ScreenSaverOSXPrivate::timerCallback,
                                          &context);
        if (d->m_timer)
            CFRunLoopAddTimer(CFRunLoopGetCurrent(),
                              d->m_timer,
                              kCFRunLoopCommonModes);
    }
}

void ScreenSaverOSX::Restore(void)
{
    if (d->m_timer)
    {
        CFRunLoopTimerInvalidate(d->m_timer);
        CFRelease(d->m_timer);
        d->m_timer = nullptr;
    }
}

void ScreenSaverOSX::Reset(void)
{
    // Wake up the screen saver now.
    ScreenSaverOSXPrivate::timerCallback(nullptr, nullptr);
}

bool ScreenSaverOSX::Asleep(void)
{
    return false;
}
