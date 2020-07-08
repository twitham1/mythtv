// Own header
#include "screensaver-x11.h"

// QT headers
#include <QDateTime>
#include <QTimer>

// Mythdb headers
#include "mythlogging.h"
#include "mythdate.h"
#include "mythdb.h"

// Mythui headers
#include "mythsystemlegacy.h"
#include "mythxdisplay.h"

// X11 headers
#include <X11/Xlib.h>

extern "C" {
#include <X11/extensions/dpms.h>
}

#define LOC      QString("ScreenSaverX11Private: ")

class ScreenSaverX11Private
{
    friend class ScreenSaverX11;

  public:
    explicit ScreenSaverX11Private(ScreenSaverX11 *outer)
    {
        const uint flags = kMSDontBlockInputDevs | kMSDontDisableDrawing |
                           kMSProcessEvents;
        m_xscreensaverRunning =
                myth_system("xscreensaver-command -version >&- 2>&-",
                            flags) == 0;

        if (IsScreenSaverRunning())
        {
            m_resetTimer = new QTimer(outer);
            m_resetTimer->setSingleShot(false);
            QObject::connect(m_resetTimer, SIGNAL(timeout()),
                             outer, SLOT(resetSlot()));
            if (m_xscreensaverRunning)
                LOG(VB_GENERAL, LOG_INFO, LOC + "XScreenSaver support enabled");
        }

        m_display = MythXDisplay::OpenMythXDisplay(false);
        if (m_display)
        {
            int dummy0 = 0;
            int dummy1 = 0;
            m_dpmsaware = (DPMSQueryExtension(m_display->GetDisplay(),
                                            &dummy0, &dummy1) != 0);
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "Failed to open connection to X11 server");
        }

        if (m_dpmsaware && m_display)
        {
            CARD16 power_level = 0;

            /* If someone runs into X server weirdness that goes away when
             * they externally disable DPMS, then the 'dpmsenabled' test should
             * be short circuited by a call to 'DPMSCapable()'. Be sure to
             * manually initialize dpmsenabled to false.
             */

            DPMSInfo(m_display->GetDisplay(), &power_level, &m_dpmsenabled);

            if (static_cast<bool>(m_dpmsenabled))
                LOG(VB_GENERAL, LOG_INFO, LOC + "DPMS is active.");
            else
                LOG(VB_GENERAL, LOG_INFO, LOC + "DPMS is disabled.");
        }
        else
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + "DPMS is not supported.");
        }
    }

    ~ScreenSaverX11Private()
    {
        // m_resetTimer deleted by ScreenSaverX11 QObject dtor
        delete m_display;
    }

    bool IsScreenSaverRunning(void) const
    {
        return m_xscreensaverRunning;
    }

    bool IsDPMSEnabled(void) const { return static_cast<bool>(m_dpmsenabled); }

    void StopTimer(void)
    {
        LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "StopTimer");
        if (m_resetTimer)
            m_resetTimer->stop();
    }

    void StartTimer(void)
    {
        LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "StartTimer");
        if (m_resetTimer)
            m_resetTimer->start(m_timeoutInterval);
    }

    void ResetTimer(void)
    {
        LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "ResetTimer -- begin");

        StopTimer();

        if (m_timeoutInterval == -1)
        {
            m_timeoutInterval = GetMythDB()->GetNumSettingOnHost(
                "xscreensaverInterval", GetMythDB()->GetHostName(), 50) * 1000;
        }

        if (m_timeoutInterval > 0)
            StartTimer();

        LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "ResetTimer -- end");
    }

    // DPMS
    bool DeactivatedDPMS(void) const
    {
        return m_dpmsdeactivated;
    }

    void DisableDPMS(void)
    {
        if (IsDPMSEnabled() && m_display)
        {
            m_dpmsdeactivated = true;
            Status status = DPMSDisable(m_display->GetDisplay());
            m_display->Sync();
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("DPMS Deactivated %1").arg(status));
        }
    }

    void RestoreDPMS(void)
    {
        if (m_dpmsdeactivated && m_display)
        {
            m_dpmsdeactivated = false;
            Status status = DPMSEnable(m_display->GetDisplay());
            m_display->Sync();
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("DPMS Reactivated %1").arg(status));
        }
    }

    void SaveScreenSaver(void)
    {
        if (!m_state.m_saved && m_display)
        {
            XGetScreenSaver(m_display->GetDisplay(), &m_state.m_timeout,
                            &m_state.m_interval, &m_state.m_preferblank,
                            &m_state.m_allowexposure);
            m_state.m_saved = true;
        }
    }

    void RestoreScreenSaver(void)
    {
        if (m_state.m_saved && m_display)
        {
            XSetScreenSaver(m_display->GetDisplay(), m_state.m_timeout,
                            m_state.m_interval, m_state.m_preferblank,
                            m_state.m_allowexposure);
            m_display->Sync();
            m_state.m_saved = false;
        }
    }

    void ResetScreenSaver(void)
    {
        if (!IsScreenSaverRunning())
            return;

        QDateTime current_time = MythDate::current();
        if ((!m_lastDeactivated.isValid()) ||
            (m_lastDeactivated.secsTo(current_time) > 30))
        {
            if (m_xscreensaverRunning)
            {
                LOG(VB_PLAYBACK, LOG_INFO, LOC +
                    "Calling xscreensaver-command -deactivate");
                myth_system("xscreensaver-command -deactivate >&- 2>&- &",
                            kMSDontBlockInputDevs |
                            kMSDontDisableDrawing |
                            kMSRunBackground);
            }
            m_lastDeactivated = current_time;
        }
    }

  private:
    class ScreenSaverState
    {
      public:
        ScreenSaverState() = default;
        bool m_saved         {false};
        int  m_timeout       {-1};
        int  m_interval      {-1};
        int  m_preferblank   {-1};
        int  m_allowexposure {-1};
    };

  private:
    bool m_dpmsaware           {false};
    bool m_dpmsdeactivated     {false}; ///< true if we disabled DPMS
    bool m_xscreensaverRunning {false};
    BOOL m_dpmsenabled         {static_cast<BOOL>(false)};

    int m_timeoutInterval      {-1};
    QTimer *m_resetTimer       {nullptr};

    QDateTime m_lastDeactivated;

    ScreenSaverState m_state;
    MythXDisplay *m_display    {nullptr};
};

ScreenSaverX11::ScreenSaverX11() :
    d(new ScreenSaverX11Private(this))
{
}

ScreenSaverX11::~ScreenSaverX11()
{
    /* Ensure DPMS gets left as it was found. */
    if (d->DeactivatedDPMS())
        ScreenSaverX11::Restore();

    delete d;
}

void ScreenSaverX11::Disable(void)
{
    d->SaveScreenSaver();

    if (d->m_display)
    {
        XResetScreenSaver(d->m_display->GetDisplay());
        XSetScreenSaver(d->m_display->GetDisplay(), 0, 0, 0, 0);
        d->m_display->Sync();
    }

    d->DisableDPMS();

    if (d->IsScreenSaverRunning())
        d->ResetTimer();
}

void ScreenSaverX11::Restore(void)
{
    d->RestoreScreenSaver();
    d->RestoreDPMS();

    // One must reset after the restore
    if (d->m_display)
    {
        XResetScreenSaver(d->m_display->GetDisplay());
        d->m_display->Sync();
    }

    if (d->IsScreenSaverRunning())
        d->StopTimer();
}

void ScreenSaverX11::Reset(void)
{
    bool need_xsync = false;
    Display *dsp = nullptr;
    if (d->m_display)
        dsp = d->m_display->GetDisplay();

    if (dsp)
    {
        XResetScreenSaver(dsp);
        need_xsync = true;
    }

    if (d->IsScreenSaverRunning())
        resetSlot();

    if (Asleep() && dsp)
    {
        DPMSForceLevel(dsp, DPMSModeOn);
        need_xsync = true;
    }

    if (need_xsync && d->m_display)
        d->m_display->Sync();
}

bool ScreenSaverX11::Asleep(void)
{
    if (!d->IsDPMSEnabled())
        return false;

    if (d->DeactivatedDPMS())
        return false;

    BOOL on = static_cast<BOOL>(false);
    CARD16 power_level = DPMSModeOn;

    if (d->m_display)
        DPMSInfo(d->m_display->GetDisplay(), &power_level, &on);

    return (power_level != DPMSModeOn);
}

void ScreenSaverX11::resetSlot(void)
{
    d->ResetScreenSaver();
}
