// -*- Mode: c++ -*-

#include <cerrno>
#include <cstring>
#include <cmath>

#include <unistd.h>

//Qt headers
#include <QCoreApplication>

// MythTV headers
#include "mythcontext.h"
#include "mythdbcon.h"
#include "dvbsignalmonitor.h"
#include "dvbchannel.h"
#include "dvbstreamdata.h"
#include "atscstreamdata.h"
#include "mpegtables.h"
#include "atsctables.h"
#include "cardutil.h"

#include "dvbtypes.h"
#include "dvbchannel.h"
#include "dvbrecorder.h"
#include "dvbstreamhandler.h"

#define LOC QString("DVBSigMon[%1](%2): ") \
            .arg(m_inputid).arg(m_channel->GetDevice())

/**
 *  \brief Initializes signal lock and signal values.
 *
 *   Start() must be called to actually begin continuous
 *   signal monitoring. The timeout is set to 3 seconds,
 *   and the signal threshold is initialized to 0%.
 *
 *  \param db_cardnum Recorder number to monitor,
 *                    if this is less than 0, SIGNAL events will not be
 *                    sent to the frontend even if SetNotifyFrontend(true)
 *                    is called.
 *  \param _channel DVBChannel for card
 *  \param _flags   Flags to start with
 */
DVBSignalMonitor::DVBSignalMonitor(int db_cardnum, DVBChannel* _channel,
                                   bool _release_stream,
                                   uint64_t _flags)
    : DTVSignalMonitor(db_cardnum, _channel, _release_stream, _flags),
      // This snr setup is incorrect for API 3.x but works better
      // than int16_t range in practice, however this is correct
      // for the 4.0 DVB API which uses a uint16_t for the snr
      signalToNoise    (QCoreApplication::translate("(Common)",
                        "Signal To Noise"),  "snr",
                        0,      true,      0, 65535, 0),
      bitErrorRate     (tr("Bit Error Rate"),     "ber",
                        65535,  false,     0, 65535, 0),
      uncorrectedBlocks(tr("Uncorrected Blocks"), "ucb",
                        65535,  false,     0, 65535, 0),
      rotorPosition    (tr("Rotor Progress"),     "pos",
                        100,    true,      0,   100, 0),
      streamHandlerStarted(false),
      streamHandler(nullptr)
{
    // These two values should probably come from the database...
    int wait = 3000; // timeout when waiting on signal
    int threshold = 0; // signal strength threshold

    m_signalLock.SetTimeout(wait);
    m_signalStrength.SetTimeout(wait);
    m_signalStrength.SetThreshold(threshold);

    // This is incorrect for API 3.x but works better than int16_t range
    // in practice, however this is correct for the 4.0 DVB API
    m_signalStrength.SetRange(0, 65535);

    uint64_t rmflags = 0;

#define DVB_IO(FLAG, METHOD, MSG) \
  do { if (HasFlags(FLAG)) { bool mok; _channel->METHOD(&mok); \
          if (!mok) { \
              LOG(VB_GENERAL, LOG_WARNING, LOC+"Cannot "+(MSG)+ENO); \
              rmflags |= (FLAG); } \
          else { \
              LOG(VB_CHANNEL, LOG_INFO, LOC + "Can " + (MSG)); } } } while (false)

    DVB_IO(kSigMon_WaitForSig, GetSignalStrength,
           "measure Signal Strength");
    DVB_IO(kDVBSigMon_WaitForSNR, GetSNR,
           "measure S/N");
    DVB_IO(kDVBSigMon_WaitForBER, GetBitErrorRate,
           "measure Bit Error Rate");
    DVB_IO(kDVBSigMon_WaitForUB, GetUncorrectedBlockCount,
           "count Uncorrected Blocks");

#undef DVB_IO

    RemoveFlags(rmflags);

    LOG(VB_CHANNEL, LOG_INFO, LOC + "DVBSignalMonitor::ctor " +
            QString("initial flags %1").arg(sm_flags_to_string(m_flags)));

    m_minimum_update_rate = _channel->GetMinSignalMonitorDelay();
    if (m_minimum_update_rate > 30)
        usleep(m_minimum_update_rate * 1000);

    streamHandler = DVBStreamHandler::Get(_channel->GetCardNum(), m_inputid);
}

/** \fn DVBSignalMonitor::~DVBSignalMonitor()
 *  \brief Stops signal monitoring and table monitoring threads.
 */
DVBSignalMonitor::~DVBSignalMonitor()
{
    DVBSignalMonitor::Stop();
    DVBStreamHandler::Return(streamHandler, m_inputid);
}

// documented in dtvsignalmonitor.h
void DVBSignalMonitor::SetRotorTarget(float target)
{
    QMutexLocker locker(&m_statusLock);
    rotorPosition.SetThreshold((int)roundf(100 * target));
}

void DVBSignalMonitor::GetRotorStatus(bool &was_moving, bool &is_moving)
{
    DVBChannel *dvbchannel = GetDVBChannel();
    if (!dvbchannel)
        return;

    const DiSEqCDevRotor *rotor = dvbchannel->GetRotor();
    if (!rotor)
        return;

    QMutexLocker locker(&m_statusLock);
    was_moving = rotorPosition.GetValue() < 100;
    int pos    = (int)truncf(rotor->GetProgress() * 100);
    rotorPosition.SetValue(pos);
    is_moving  = rotorPosition.GetValue() < 100;
}

/** \fn DVBSignalMonitor::Stop(void)
 *  \brief Stop signal monitoring and table monitoring threads.
 */
void DVBSignalMonitor::Stop(void)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Stop() -- begin");
    SignalMonitor::Stop();
    if (GetStreamData())
        streamHandler->RemoveListener(GetStreamData());
    streamHandlerStarted = false;
    streamHandler->SetRetuneAllowed(false, nullptr, nullptr);
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Stop() -- end");
}

QStringList DVBSignalMonitor::GetStatusList(void) const
{
    QStringList list = DTVSignalMonitor::GetStatusList();
    m_statusLock.lock();
    if (HasFlags(kDVBSigMon_WaitForSNR))
        list<<signalToNoise.GetName()<<signalToNoise.GetStatus();
    if (HasFlags(kDVBSigMon_WaitForBER))
        list<<bitErrorRate.GetName()<<bitErrorRate.GetStatus();
    if (HasFlags(kDVBSigMon_WaitForUB))
        list<<uncorrectedBlocks.GetName()<<uncorrectedBlocks.GetStatus();
    if (HasFlags(kDVBSigMon_WaitForPos))
        list<<rotorPosition.GetName()<<rotorPosition.GetStatus();
    m_statusLock.unlock();
    return list;
}

void DVBSignalMonitor::HandlePMT(uint program_num, const ProgramMapTable *pmt)
{
    DTVSignalMonitor::HandlePMT(program_num, pmt);

    if (pmt->ProgramNumber() == (uint)m_programNumber)
    {
        DVBChannel *dvbchannel = GetDVBChannel();
        if (dvbchannel)
            dvbchannel->SetPMT(pmt);
    }
}

void DVBSignalMonitor::HandleSTT(const SystemTimeTable *stt)
{
    DTVSignalMonitor::HandleSTT(stt);
    DVBChannel *dvbchannel = GetDVBChannel();
    if (dvbchannel)
        dvbchannel->SetTimeOffset(GetStreamData()->TimeOffset());
}

void DVBSignalMonitor::HandleTDT(const TimeDateTable *tdt)
{
    DTVSignalMonitor::HandleTDT(tdt);
    DVBChannel *dvbchannel = GetDVBChannel();
    if (dvbchannel)
        dvbchannel->SetTimeOffset(GetStreamData()->TimeOffset());
}

DVBChannel *DVBSignalMonitor::GetDVBChannel(void)
{
    return dynamic_cast<DVBChannel*>(m_channel);
}

/** \fn DVBSignalMonitor::UpdateValues()
 *  \brief Fills in frontend stats and emits status Qt signals.
 *
 *   This is automatically called by run(), after Start()
 *   has been used to start the signal monitoring thread.
 */
void DVBSignalMonitor::UpdateValues(void)
{
    if (!m_running || m_exit)
        return;

    if (streamHandlerStarted)
    {
        if (!streamHandler->IsRunning())
        {
            m_error = tr("Error: stream handler died");
            m_update_done = true;
            return;
        }

        // Update signal status for display
        DVBChannel *dvbchannel = GetDVBChannel();
        if (dvbchannel)
        {
            // Get info from card
            uint sig = (uint) (dvbchannel->GetSignalStrength() * 65535);
            uint snr = (uint) (dvbchannel->GetSNR() * 65535);

            // Set SignalMonitorValues from info from card.
            {
                QMutexLocker locker(&m_statusLock);

                if (HasFlags(kSigMon_WaitForSig))
                    m_signalStrength.SetValue(sig);
                if (HasFlags(kDVBSigMon_WaitForSNR))
                    signalToNoise.SetValue(snr);

                // LOG(VB_CHANSCAN, LOG_DEBUG, LOC +
                //     QString("Update sig:%1 snr:%2").arg(sig).arg(snr));
            }
        }

        EmitStatus();
        if (IsAllGood())
            SendMessageAllGood();

        m_update_done = true;
        return;
    }

    AddFlags(kSigMon_WaitForSig);

    DVBChannel *dvbchannel = GetDVBChannel();
    if (!dvbchannel)
        return;

    // Handle retuning after rotor has turned
    if (HasFlags(SignalMonitor::kDVBSigMon_WaitForPos))
    {
        if (dvbchannel->GetRotor())
        {
            if (!streamHandler->IsRetuneAllowed())
                streamHandler->SetRetuneAllowed(true, this, dvbchannel);
            streamHandler->RetuneMonitor();
        }
        else
            RemoveFlags(SignalMonitor::kDVBSigMon_WaitForPos);
    }

    bool wasLocked = false, isLocked = false;
    uint sig = 0, snr = 0, ber = 0, ublocks = 0;

    // Get info from card
    bool has_lock = dvbchannel->HasLock();
    if (HasFlags(kSigMon_WaitForSig))
        sig = (uint) (dvbchannel->GetSignalStrength() * 65535);
    if (HasFlags(kDVBSigMon_WaitForSNR))
        snr = (uint) (dvbchannel->GetSNR() * 65535);
    if (HasFlags(kDVBSigMon_WaitForBER))
        ber = (uint) dvbchannel->GetBitErrorRate();
    if (HasFlags(kDVBSigMon_WaitForUB))
        ublocks = (uint) dvbchannel->GetUncorrectedBlockCount();

    has_lock |= streamHandler->IsRunning();

    // Set SignalMonitorValues from info from card.
    {
        QMutexLocker locker(&m_statusLock);

        // BER and UB are actually uint32 values, but we
        // clamp them at 64K. This is because these values
        // are actually cumulative, but we don't try to
        // normalize these to a time period.

        wasLocked = m_signalLock.IsGood();
        m_signalLock.SetValue((has_lock) ? 1 : 0);
        isLocked = m_signalLock.IsGood();

        if (HasFlags(kSigMon_WaitForSig))
            m_signalStrength.SetValue(sig);
        if (HasFlags(kDVBSigMon_WaitForSNR))
            signalToNoise.SetValue(snr);
        if (HasFlags(kDVBSigMon_WaitForBER))
            bitErrorRate.SetValue(ber);
        if (HasFlags(kDVBSigMon_WaitForUB))
            uncorrectedBlocks.SetValue(ublocks);
    }

    // Debug output
    if (wasLocked != isLocked)
    {
        LOG(VB_CHANNEL, LOG_INFO, LOC + "UpdateValues -- Signal " +
                (isLocked ? "Locked" : "Lost"));
    }

    EmitStatus();
    if (IsAllGood())
        SendMessageAllGood();

    // Start table monitoring if we are waiting on any table
    // and we have a lock.
    if (isLocked && GetStreamData() &&
        (!HasFlags(kDVBSigMon_WaitForPos) || rotorPosition.IsGood()) &&
        HasAnyFlag(kDTVSigMon_WaitForPAT | kDTVSigMon_WaitForPMT |
                   kDTVSigMon_WaitForMGT | kDTVSigMon_WaitForVCT |
                   kDTVSigMon_WaitForNIT | kDTVSigMon_WaitForSDT))
    {
        GetStreamData()->AddListeningPID(MPEG_PAT_PID);
        streamHandler->AddListener(GetStreamData(), true, false);
        streamHandlerStarted = true;
    }

    m_update_done = true;
}

/** \fn DVBSignalMonitor::EmitStatus(void)
 *  \brief Emits signals for lock, signal strength, etc.
 */
void DVBSignalMonitor::EmitStatus(void)
{
    // Emit signals..
    DTVSignalMonitor::EmitStatus();
    if (HasFlags(kDVBSigMon_WaitForSNR))
        SendMessage(kStatusSignalToNoise,     signalToNoise);
    if (HasFlags(kDVBSigMon_WaitForBER))
        SendMessage(kStatusBitErrorRate,      bitErrorRate);
    if (HasFlags(kDVBSigMon_WaitForUB))
        SendMessage(kStatusUncorrectedBlocks, uncorrectedBlocks);
    if (HasFlags(kDVBSigMon_WaitForPos))
        SendMessage(kStatusRotorPosition,     rotorPosition);
}
