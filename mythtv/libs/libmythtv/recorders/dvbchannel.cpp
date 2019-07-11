/*
 *  Class DVBChannel
 *
 * Copyright (C) Kenneth Aafloy 2003
 *
 *  Description:
 *      Has the responsibility of opening the Frontend device and
 *      setting the options to tune a channel. It also keeps other
 *      channel options used by the dvb hierarcy.
 *
 *  Author(s):
 *      Taylor Jacob (rtjacob at earthlink.net)
 *          - Changed tuning and DB structure
 *      Kenneth Aafloy (ke-aa at frisurf.no)
 *          - Rewritten for faster tuning.
 *      Ben Bucksch
 *          - Wrote the original implementation
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

// POSIX headers
#include <fcntl.h>
#include <unistd.h>
#include <sys/poll.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>

// MythTV headers
#include "mythconfig.h"
#include "mythdb.h"
#include "cardutil.h"
#include "channelutil.h"
#include "dvbtypes.h"
#include "dvbchannel.h"
#include "dvbcam.h"
#include "tv_rec.h"

// Returned by drivers on unsupported DVBv3 ioctl calls
#ifndef ENOTSUPP
#define ENOTSUPP 524
#endif

// Local functions
static void drain_dvb_events(int fd);
static bool wait_for_backend(int fd, int timeout_ms);
static struct dvb_frontend_parameters dtvmultiplex_to_dvbparams(
    DTVTunerType /*tuner_type*/, const DTVMultiplex& /*tuning*/, int intermediate_freq, bool can_fec_auto);
static DTVMultiplex dvbparams_to_dtvmultiplex(
    DTVTunerType /*tuner_type*/, const dvb_frontend_parameters& /*params*/);

int64_t concurrent_tunings_delay = 1000;
QDateTime DVBChannel::s_last_tuning = QDateTime::currentDateTime();

#define LOC QString("DVBChan[%1](%2): ").arg(m_inputid).arg(DVBChannel::GetDevice())

/** \class DVBChannel
 *  \brief Provides interface to the tuning hardware when using DVB drivers
 *
 *  \bug Only supports single input cards.
 */
DVBChannel::DVBChannel(const QString &aDevice, TVRec *parent)
    : DTVChannel(parent), m_device(aDevice)
{
    s_master_map_lock.lockForWrite();
    QString key = CardUtil::GetDeviceName(DVB_DEV_FRONTEND, m_device);
    if (m_pParent)
        key += QString(":%1")
            .arg(CardUtil::GetSourceID(m_pParent->GetInputId()));
    s_master_map[key].push_back(this); // == RegisterForMaster
    DVBChannel *master = static_cast<DVBChannel*>(s_master_map[key].front());
    if (master == this)
    {
        m_dvbcam = new DVBCam(m_device);
        m_has_crc_bug = CardUtil::HasDVBCRCBug(m_device);
    }
    else
    {
        m_dvbcam       = master->m_dvbcam;
        m_has_crc_bug  = master->m_has_crc_bug;
    }
    s_master_map_lock.unlock();

    m_sigmon_delay = CardUtil::GetMinSignalMonitoringDelay(m_device);
}

DVBChannel::~DVBChannel()
{
    // set a new master if there are other instances and we're the master
    // whether we are the master or not remove us from the map..
    s_master_map_lock.lockForWrite();
    QString key = CardUtil::GetDeviceName(DVB_DEV_FRONTEND, m_device);
    if (m_pParent)
        key += QString(":%1")
            .arg(CardUtil::GetSourceID(m_pParent->GetInputId()));
    DVBChannel *master = static_cast<DVBChannel*>(s_master_map[key].front());
    if (master == this)
    {
        s_master_map[key].pop_front();
        DVBChannel *new_master = nullptr;
        if (!s_master_map[key].empty())
            new_master = dynamic_cast<DVBChannel*>(s_master_map[key].front());
        if (new_master)
        {
            QMutexLocker master_locker(&(master->m_hw_lock));
            QMutexLocker new_master_locker(&(new_master->m_hw_lock));
            new_master->m_is_open = master->m_is_open;
        }
    }
    else
    {
        s_master_map[key].removeAll(this);
    }
    s_master_map_lock.unlock();

    DVBChannel::Close();

    // if we're the last one out delete dvbcam
    s_master_map_lock.lockForRead();
    MasterMap::iterator mit = s_master_map.find(key);
    if ((*mit).empty())
        delete m_dvbcam;
    m_dvbcam = nullptr;
    s_master_map_lock.unlock();

    // diseqc_tree is managed elsewhere
}

void DVBChannel::Close(DVBChannel *who)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Closing DVB channel");

    QMutexLocker locker(&m_hw_lock);

    IsOpenMap::iterator it = m_is_open.find(who);
    if (it == m_is_open.end())
        return; // this caller didn't have it open in the first place..

    m_is_open.erase(it);

    DVBChannel *master = GetMasterLock();
    if (master != nullptr && master != this)
    {
        if (m_dvbcam->IsRunning())
            m_dvbcam->SetPMT(this, nullptr);
        master->Close(this);
        m_fd_frontend = -1;
        ReturnMasterLock(master);
        return;
    }
    ReturnMasterLock(master); // if we're the master we don't need this lock..

    if (!m_is_open.empty())
        return; // not all callers have closed the DVB channel yet..

    if (m_diseqc_tree)
        m_diseqc_tree->Close();

    if (m_fd_frontend >= 0)
    {
        close(m_fd_frontend);
        m_fd_frontend = -1;

        m_dvbcam->Stop();
    }
}

bool DVBChannel::Open(DVBChannel *who)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Opening DVB channel");

    if (!m_inputid)
    {
        if (!InitializeInput())
            return false;
    }

    QMutexLocker locker(&m_hw_lock);

    if (m_fd_frontend >= 0)
    {
        m_is_open[who] = true;
        return true;
    }

    DVBChannel *master = GetMasterLock();
    if (master != this)
    {
        if (!master->Open(who))
        {
            ReturnMasterLock(master);
            return false;
        }

        m_fd_frontend         = master->m_fd_frontend;
        m_frontend_name       = master->m_frontend_name;
        m_tunerType           = master->m_tunerType;
        m_capabilities        = master->m_capabilities;
        m_ext_modulations     = master->m_ext_modulations;
        m_frequency_minimum   = master->m_frequency_minimum;
        m_frequency_maximum   = master->m_frequency_maximum;
        m_symbol_rate_minimum = master->m_symbol_rate_minimum;
        m_symbol_rate_maximum = master->m_symbol_rate_maximum;

        m_is_open[who] = true;

        if (!InitializeInput())
        {
            Close();
            ReturnMasterLock(master);
            return false;
        }

        ReturnMasterLock(master);
        return true;
    }
    ReturnMasterLock(master); // if we're the master we don't need this lock..

    QString dvbdev = CardUtil::GetDeviceName(DVB_DEV_FRONTEND, m_device);
    QByteArray devn = dvbdev.toLatin1();

    for (int tries = 1; ; ++tries)
    {
        m_fd_frontend = open(devn.constData(), O_RDWR | O_NONBLOCK);
        if (m_fd_frontend >= 0)
            break;
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Opening DVB frontend device failed." + ENO);
        if (tries >= 20 || (errno != EBUSY && errno != EAGAIN))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Failed to open DVB frontend device due to "
                        "fatal error or too many attempts."));
            return false;
        }
        usleep(50000);
    }

    dvb_frontend_info info;
    memset(&info, 0, sizeof(info));
    if (ioctl(m_fd_frontend, FE_GET_INFO, &info) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Failed to get frontend information." + ENO);

        close(m_fd_frontend);
        m_fd_frontend = -1;
        return false;
    }

    m_frontend_name       = info.name;
    m_capabilities        = info.caps;
    m_frequency_minimum   = info.frequency_min;
    m_frequency_maximum   = info.frequency_max;
    m_symbol_rate_minimum = info.symbol_rate_min;
    m_symbol_rate_maximum = info.symbol_rate_max;

    CardUtil::SetDefaultDeliverySystem(m_inputid, m_fd_frontend);
    m_tunerType = CardUtil::ProbeTunerType(m_fd_frontend);

    LOG(VB_RECORD, LOG_INFO, LOC +
        QString("Frontend '%2' tunertype: %3")
            .arg(m_frontend_name).arg(m_tunerType.toString()));

    // Turn on the power to the LNB
    if (m_tunerType.IsDiSEqCSupported())
    {

        m_diseqc_tree = m_diseqc_dev.FindTree(m_inputid);
        if (m_diseqc_tree)
        {
            bool is_SCR = false;

            DiSEqCDevSCR *scr = m_diseqc_tree->FindSCR(m_diseqc_settings);
            if (scr)
            {
                is_SCR = true;
                LOG(VB_CHANNEL, LOG_INFO, LOC + "Requested DVB channel is on SCR system");
            }
            else
                LOG(VB_CHANNEL, LOG_INFO, LOC + "Requested DVB channel is on non-SCR system");

            m_diseqc_tree->Open(m_fd_frontend, is_SCR);
        }
    }

    m_first_tune = true;

    if (!InitializeInput())
    {
        Close();
        return false;
    }

    if (m_fd_frontend >= 0)
        m_is_open[who] = true;

    return (m_fd_frontend >= 0);
}

bool DVBChannel::IsOpen(void) const
{
    // Have to acquire the hw lock to prevent is_open being modified whilst we're searching it
    QMutexLocker locker(&m_hw_lock);
    IsOpenMap::const_iterator it = m_is_open.find(this);
    return it != m_is_open.end();
}

bool DVBChannel::Init(QString &startchannel, bool setchan)
{
    if (setchan && !IsOpen())
        Open(this);

    return ChannelBase::Init(startchannel, setchan);
}

/** \fn DVBChannel::CheckFrequency(uint64_t) const
 *  \brief Checks tuning frequency
 */
void DVBChannel::CheckFrequency(uint64_t frequency) const
{
    if (m_frequency_minimum && m_frequency_maximum &&
        (m_frequency_minimum <= m_frequency_maximum) &&
        (frequency < m_frequency_minimum || frequency > m_frequency_maximum))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            QString("Your frequency setting (%1) is out of range. "
                    "(min/max:%2/%3)")
                .arg(frequency).arg(m_frequency_minimum).arg(m_frequency_maximum));
    }
}

void DVBChannel::CheckOptions(DTVMultiplex &tuning) const
{
    if ((tuning.m_inversion == DTVInversion::kInversionAuto) &&
        ((m_capabilities & FE_CAN_INVERSION_AUTO) == 0U))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "'Auto' inversion parameter unsupported by this driver, "
            "falling back to 'off'.");
        tuning.m_inversion = DTVInversion::kInversionOff;
    }

    // DVB-S needs a fully initialized diseqc tree and is checked later in Tune
    if (!m_diseqc_tree)
    {
        const DVBChannel *master = GetMasterLock();
        if (master == nullptr || !master->m_diseqc_tree)
            CheckFrequency(tuning.m_frequency);
        ReturnMasterLock(master);
    }

    if (m_tunerType.IsFECVariable() &&
        m_symbol_rate_minimum && m_symbol_rate_maximum &&
        (m_symbol_rate_minimum <= m_symbol_rate_maximum) &&
        (tuning.m_symbolrate < m_symbol_rate_minimum ||
         tuning.m_symbolrate > m_symbol_rate_maximum))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            QString("Symbol Rate setting (%1) is out of range (min/max:%2/%3)")
                .arg(tuning.m_symbolrate)
                .arg(m_symbol_rate_minimum).arg(m_symbol_rate_maximum));
    }

    if (m_tunerType.IsFECVariable() && !CheckCodeRate(tuning.m_fec))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Selected fec_inner parameter unsupported by this driver.");
    }

    if (m_tunerType.IsModulationVariable() && !CheckModulation(tuning.m_modulation))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
           "Selected modulation parameter unsupported by this driver.");
    }

    if (DTVTunerType::kTunerTypeDVBT != m_tunerType &&
        DTVTunerType::kTunerTypeDVBT2 != m_tunerType)
    {
        LOG(VB_CHANNEL, LOG_INFO, LOC + tuning.toString());
        return;
    }

    // Check OFDM Tuning params

    if (!CheckCodeRate(tuning.m_hp_code_rate))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Selected code_rate_hp parameter unsupported by this driver.");
    }

    if (!CheckCodeRate(tuning.m_lp_code_rate))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Selected code_rate_lp parameter unsupported by this driver.");
    }

    if ((tuning.m_bandwidth == DTVBandwidth::kBandwidthAuto) &&
        ((m_capabilities & FE_CAN_BANDWIDTH_AUTO) == 0U))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "'Auto' bandwidth parameter unsupported by this driver.");
    }

    if ((tuning.m_trans_mode == DTVTransmitMode::kTransmissionModeAuto) &&
        ((m_capabilities & FE_CAN_TRANSMISSION_MODE_AUTO) == 0U))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "'Auto' transmission_mode parameter unsupported by this driver.");
    }

    if ((tuning.m_guard_interval == DTVGuardInterval::kGuardIntervalAuto) &&
        ((m_capabilities & FE_CAN_GUARD_INTERVAL_AUTO) == 0U))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "'Auto' guard_interval parameter unsupported by this driver.");
    }

    if ((tuning.m_hierarchy == DTVHierarchy::kHierarchyAuto) &&
        ((m_capabilities & FE_CAN_HIERARCHY_AUTO) == 0U))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "'Auto' hierarchy parameter unsupported by this driver. ");
    }

    if (!CheckModulation(tuning.m_modulation))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Selected modulation parameter unsupported by this driver.");
    }

    LOG(VB_CHANNEL, LOG_INFO, LOC + tuning.toString());
}

/**
 *  \brief Return true iff rate is supported rate on the frontend
 */
bool DVBChannel::CheckCodeRate(DTVCodeRate rate) const
{
    const uint64_t caps = m_capabilities;
    return
        ((DTVCodeRate::kFECNone == rate))                            ||
        ((DTVCodeRate::kFEC_1_2 == rate) && ((caps & FE_CAN_FEC_1_2) != 0U)) ||
        ((DTVCodeRate::kFEC_2_3 == rate) && ((caps & FE_CAN_FEC_2_3) != 0U)) ||
        ((DTVCodeRate::kFEC_3_4 == rate) && ((caps & FE_CAN_FEC_3_4) != 0U)) ||
        ((DTVCodeRate::kFEC_4_5 == rate) && ((caps & FE_CAN_FEC_4_5) != 0U)) ||
        ((DTVCodeRate::kFEC_5_6 == rate) && ((caps & FE_CAN_FEC_5_6) != 0U)) ||
        ((DTVCodeRate::kFEC_6_7 == rate) && ((caps & FE_CAN_FEC_6_7) != 0U)) ||
        ((DTVCodeRate::kFEC_7_8 == rate) && ((caps & FE_CAN_FEC_7_8) != 0U)) ||
        ((DTVCodeRate::kFEC_8_9 == rate) && ((caps & FE_CAN_FEC_8_9) != 0U)) ||
        ((DTVCodeRate::kFECAuto == rate) && ((caps & FE_CAN_FEC_AUTO) != 0U));
}

/**
 *  \brief Return true iff modulation is supported modulation on the frontend
 */
bool DVBChannel::CheckModulation(DTVModulation modulation) const
{
    const DTVModulation m = modulation;
    const uint64_t      c = m_capabilities;

    return
        ((DTVModulation::kModulationQPSK    == m) && ((c & FE_CAN_QPSK) != 0U))     ||
#if HAVE_FE_CAN_2G_MODULATION
        ((DTVModulation::kModulation8PSK    == m) && ((c & FE_CAN_2G_MODULATION) != 0U)) ||
        ((DTVModulation::kModulation16APSK  == m) && ((c & FE_CAN_2G_MODULATION) != 0U)) ||
        ((DTVModulation::kModulation32APSK  == m) && ((c & FE_CAN_2G_MODULATION) != 0U)) ||
#endif //HAVE_FE_CAN_2G_MODULATION
        ((DTVModulation::kModulationQAM16   == m) && ((c & FE_CAN_QAM_16) != 0U))   ||
        ((DTVModulation::kModulationQAM32   == m) && ((c & FE_CAN_QAM_32) != 0U))   ||
        ((DTVModulation::kModulationQAM64   == m) && ((c & FE_CAN_QAM_64) != 0U))   ||
        ((DTVModulation::kModulationQAM128  == m) && ((c & FE_CAN_QAM_128) != 0U))  ||
        ((DTVModulation::kModulationQAM256  == m) && ((c & FE_CAN_QAM_256) != 0U))  ||
        ((DTVModulation::kModulationQAMAuto == m) && ((c & FE_CAN_QAM_AUTO) != 0U)) ||
        ((DTVModulation::kModulation8VSB    == m) && ((c & FE_CAN_8VSB) != 0U))     ||
        ((DTVModulation::kModulation16VSB   == m) && ((c & FE_CAN_16VSB) != 0U));
}

/** \fn DVBChannel::SetPMT(const ProgramMapTable*)
 *  \brief Tells the Conditional Access Module which streams we wish to decode.
 */
void DVBChannel::SetPMT(const ProgramMapTable *pmt)
{
    if (!m_dvbcam->IsRunning())
        m_dvbcam->Start();
    if (pmt && m_dvbcam->IsRunning())
        m_dvbcam->SetPMT(this, pmt);
}

/** \fn DVBChannel::SetTimeOffset(double)
 *  \brief Tells the Conditional Access Module the offset
 *         from the computers utc time to dvb time.
 */
void DVBChannel::SetTimeOffset(double offset)
{
    if (m_dvbcam->IsRunning())
        m_dvbcam->SetTimeOffset(offset);
}

bool DVBChannel::Tune(const DTVMultiplex &tuning)
{
    if (!m_inputid)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Tune(): Invalid input."));
        return false;
    }
    return Tune(tuning, false, false);
}

static struct dtv_properties *dtvmultiplex_to_dtvproperties(
    DTVTunerType tuner_type, const DTVMultiplex &tuning, int intermediate_freq,
    bool can_fec_auto, bool do_tune = true)
{
    uint c = 0;
    struct dtv_properties *cmdseq;

    if (tuner_type != DTVTunerType::kTunerTypeDVBT  &&
        tuner_type != DTVTunerType::kTunerTypeDVBC  &&
        tuner_type != DTVTunerType::kTunerTypeDVBS1 &&
        tuner_type != DTVTunerType::kTunerTypeDVBS2 &&
        tuner_type != DTVTunerType::kTunerTypeDVBT2)
    {
        LOG(VB_GENERAL, LOG_ERR, "DVBChan: Unsupported tuner type " +
            tuner_type.toString());
        return nullptr;
    }

    LOG(VB_CHANNEL, LOG_DEBUG, "DVBChan: modsys " + tuning.m_mod_sys.toString());

    cmdseq = (struct dtv_properties*) calloc(1, sizeof(*cmdseq));
    if (!cmdseq)
        return nullptr;

    cmdseq->props = (struct dtv_property*) calloc(20, sizeof(*(cmdseq->props)));
    if (!(cmdseq->props))
    {
        free(cmdseq);
        return nullptr;
    }

    // The cx24116 DVB-S2 demod anounce FE_CAN_FEC_AUTO but has apparently
    // trouble with FEC_AUTO on DVB-S2 transponders
    if (tuning.m_mod_sys == DTVModulationSystem::kModulationSystem_DVBS2)
        can_fec_auto = false;

    if (tuner_type == DTVTunerType::kTunerTypeDVBS2 ||
        tuner_type == DTVTunerType::kTunerTypeDVBT ||
        tuner_type == DTVTunerType::kTunerTypeDVBT2)
    {
        cmdseq->props[c].cmd      = DTV_DELIVERY_SYSTEM;
        cmdseq->props[c++].u.data = tuning.m_mod_sys;
    }

    cmdseq->props[c].cmd      = DTV_FREQUENCY;
    cmdseq->props[c++].u.data = intermediate_freq ? intermediate_freq : tuning.m_frequency;
    cmdseq->props[c].cmd      = DTV_MODULATION;
    cmdseq->props[c++].u.data = tuning.m_modulation;
    cmdseq->props[c].cmd      = DTV_INVERSION;
    cmdseq->props[c++].u.data = tuning.m_inversion;

    if (tuner_type == DTVTunerType::kTunerTypeDVBS1 ||
        tuner_type == DTVTunerType::kTunerTypeDVBS2 ||
        tuner_type == DTVTunerType::kTunerTypeDVBC)
    {
        cmdseq->props[c].cmd      = DTV_SYMBOL_RATE;
        cmdseq->props[c++].u.data = tuning.m_symbolrate;
    }

    if (tuner_type.IsFECVariable())
    {
        cmdseq->props[c].cmd      = DTV_INNER_FEC;
        cmdseq->props[c++].u.data = can_fec_auto ? FEC_AUTO
            : (fe_code_rate_t) (int) tuning.m_fec;
    }

    if (tuner_type == DTVTunerType::kTunerTypeDVBT ||
        tuner_type == DTVTunerType::kTunerTypeDVBT2)
    {
        cmdseq->props[c].cmd      = DTV_BANDWIDTH_HZ;
        cmdseq->props[c++].u.data = (8-tuning.m_bandwidth) * 1000000;
        cmdseq->props[c].cmd      = DTV_CODE_RATE_HP;
        cmdseq->props[c++].u.data = tuning.m_hp_code_rate;
        cmdseq->props[c].cmd      = DTV_CODE_RATE_LP;
        cmdseq->props[c++].u.data = tuning.m_lp_code_rate;
        cmdseq->props[c].cmd      = DTV_TRANSMISSION_MODE;
        cmdseq->props[c++].u.data = tuning.m_trans_mode;
        cmdseq->props[c].cmd      = DTV_GUARD_INTERVAL;
        cmdseq->props[c++].u.data = tuning.m_guard_interval;
        cmdseq->props[c].cmd      = DTV_HIERARCHY;
        cmdseq->props[c++].u.data = tuning.m_hierarchy;
    }

    if (tuning.m_mod_sys == DTVModulationSystem::kModulationSystem_DVBS2)
    {
        cmdseq->props[c].cmd      = DTV_PILOT;
        cmdseq->props[c++].u.data = PILOT_AUTO;
        cmdseq->props[c].cmd      = DTV_ROLLOFF;
        cmdseq->props[c++].u.data = tuning.m_rolloff;
    }
    else if (tuning.m_mod_sys == DTVModulationSystem::kModulationSystem_DVBS)
    {
        cmdseq->props[c].cmd      = DTV_ROLLOFF;
        cmdseq->props[c++].u.data = DTVRollOff::kRollOff_35;
    }

    if (do_tune)
        cmdseq->props[c++].cmd    = DTV_TUNE;

    cmdseq->num = c;

    return cmdseq;
}

/*****************************************************************************
           Tuning functions for each of the five types of cards.
 *****************************************************************************/

/**
 *  \brief Tunes the card to a frequency but does not deal with PIDs.
 *
 *   This is used by DVB Channel Scanner, the EIT Parser, and by TVRec.
 *
 *  \param tuning      Info on transport to tune to
 *  \param force_reset If true, frequency tuning is done
 *                     even if it should not be needed.
 *  \param same_input  Optional, doesn't change input (for retuning).
 *  \return true on success, false on failure
 */
bool DVBChannel::Tune(const DTVMultiplex &tuning,
                      bool force_reset,
                      bool same_input)
{
    QMutexLocker lock(&m_tune_lock);
    QMutexLocker locker(&m_hw_lock);

    DVBChannel *master = GetMasterLock();
    if (master != this)
    {
        LOG(VB_CHANNEL, LOG_INFO, LOC + "Tuning on slave channel");
        SetSIStandard(tuning.m_sistandard);
        bool ok = master->Tune(tuning, force_reset, false);
        ReturnMasterLock(master);
        return ok;
    }
    ReturnMasterLock(master); // if we're the master we don't need this lock..


    int intermediate_freq = 0;
    bool can_fec_auto = false;
    bool reset = (force_reset || m_first_tune);

    if (m_tunerType.IsDiSEqCSupported() && !m_diseqc_tree)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "DVB-S/S2 needs device tree for LNB handling");
        return false;
    }

    m_desired_tuning = tuning;

    if (m_fd_frontend < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Tune(): Card not open!");

        return false;
    }

    // Remove any events in queue before tuning.
    drain_dvb_events(m_fd_frontend);

    LOG(VB_CHANNEL, LOG_INFO, LOC + "\nOld Params: " + m_prev_tuning.toString() +
            "\nNew Params: " + tuning.toString());

    // DVB-S is in kHz, other DVB is in Hz
    bool is_dvbs = ((DTVTunerType::kTunerTypeDVBS1 == m_tunerType) ||
                    (DTVTunerType::kTunerTypeDVBS2 == m_tunerType));
    int     freq_mult = (is_dvbs) ? 1 : 1000;
    QString suffix    = (is_dvbs) ? "kHz" : "Hz";

    if (reset || !m_prev_tuning.IsEqual(m_tunerType, tuning, 500 * freq_mult))
    {
        LOG(VB_CHANNEL, LOG_INFO, LOC + QString("Tune(): Tuning to %1%2")
                .arg(intermediate_freq ? intermediate_freq : tuning.m_frequency)
                .arg(suffix));

        m_tune_delay_lock.lock();

        if (QDateTime::currentDateTime() < s_last_tuning)
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + QString("Next tuning after less than %1ms. Delaying by %1ms")
                .arg(concurrent_tunings_delay));
            usleep(concurrent_tunings_delay * 1000);
        }

        s_last_tuning = QDateTime::currentDateTime();
        s_last_tuning = s_last_tuning.addMSecs(concurrent_tunings_delay);

        m_tune_delay_lock.unlock();

        // send DVB-S setup
        if (m_diseqc_tree)
        {
            // configure for new input
            if (!same_input)
                m_diseqc_settings.Load(m_inputid);

            // execute diseqc commands
            if (!m_diseqc_tree->Execute(m_diseqc_settings, tuning))
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "Tune(): Failed to setup DiSEqC devices");
                return false;
            }

            // retrieve actual intermediate frequency
            DiSEqCDevLNB *lnb = m_diseqc_tree->FindLNB(m_diseqc_settings);
            if (!lnb)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "Tune(): No LNB for this configuration");
                return false;
            }

            if (lnb->GetDeviceID() != m_last_lnb_dev_id)
            {
                m_last_lnb_dev_id = lnb->GetDeviceID();
                // make sure we tune to frequency, if the lnb has changed
                m_first_tune = true;
            }

            intermediate_freq = lnb->GetIntermediateFrequency(
                m_diseqc_settings, tuning);

            // retrieve scr intermediate frequency
            DiSEqCDevSCR *scr = m_diseqc_tree->FindSCR(m_diseqc_settings);
            if (lnb && scr)
            {
                intermediate_freq = scr->GetIntermediateFrequency(intermediate_freq);
            }

            // if card can auto-FEC, use it -- sometimes NITs are inaccurate
            if (m_capabilities & FE_CAN_FEC_AUTO)
                can_fec_auto = true;

            // Check DVB-S intermediate frequency here since it requires a fully
            // initialized diseqc tree
            CheckFrequency(intermediate_freq);
        }

        if (DTVTunerType::kTunerTypeDVBS2 == m_tunerType ||
            DTVTunerType::kTunerTypeDVBT2 == m_tunerType)
        {
            struct dtv_property p_clear;
            struct dtv_properties cmdseq_clear;

            p_clear.cmd        = DTV_CLEAR;
            cmdseq_clear.num   = 1;
            cmdseq_clear.props = &p_clear;

            if ((ioctl(m_fd_frontend, FE_SET_PROPERTY, &cmdseq_clear)) < 0)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "Tune(): Clearing DTV properties cache failed." + ENO);
                return false;
            }

            struct dtv_properties *cmds = dtvmultiplex_to_dtvproperties(
                m_tunerType, tuning, intermediate_freq, can_fec_auto);

            if (!cmds) {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "Failed to convert DTVMultiplex to DTV_PROPERTY sequence");
                return false;
            }

            if (VERBOSE_LEVEL_CHECK(VB_CHANNEL, LOG_DEBUG))
            {
                for (uint i = 0; i < cmds->num; i++)
                {
                    LOG(VB_CHANNEL, LOG_DEBUG, LOC +
                        QString("prop %1: cmd = %2, data %3")
                            .arg(i).arg(cmds->props[i].cmd)
                            .arg(cmds->props[i].u.data));
                }
            }

            int res = ioctl(m_fd_frontend, FE_SET_PROPERTY, cmds);

            free(cmds->props);
            free(cmds);

            if (res < 0)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "Tune(): Setting Frontend tuning parameters failed." + ENO);
                return false;
            }
        }
        else
        {
            struct dvb_frontend_parameters params = dtvmultiplex_to_dvbparams(
                m_tunerType, tuning, intermediate_freq, can_fec_auto);

            if (ioctl(m_fd_frontend, FE_SET_FRONTEND, &params) < 0)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "Tune(): Setting Frontend tuning parameters failed." + ENO);
                return false;
            }
        }

        // Extra delay to add for broken DVB drivers
        if (m_tuning_delay)
            usleep(m_tuning_delay * 1000);

        wait_for_backend(m_fd_frontend, 50 /* msec */);

        m_prev_tuning = tuning;
        m_first_tune = false;
    }

    SetSIStandard(tuning.m_sistandard);

    LOG(VB_CHANNEL, LOG_INFO, LOC + "Tune(): Frequency tuning successful");

    return true;
}

bool DVBChannel::Retune(void)
{
    return Tune(m_desired_tuning, true, true);
}

/** \fn DVBChannel::IsTuningParamsProbeSupported(void) const
 *  \brief Returns true iff tuning info probing is working.
 */
bool DVBChannel::IsTuningParamsProbeSupported(void) const
{
    QMutexLocker locker(&m_hw_lock);

    if (m_fd_frontend < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Card not open!");

        return false;
    }

    const DVBChannel *master = GetMasterLock();
    if (master != this)
    {
        bool ok = master->IsTuningParamsProbeSupported();
        ReturnMasterLock(master);
        return ok;
    }
    ReturnMasterLock(master); // if we're the master we don't need this lock..

    if (m_diseqc_tree)
    {
        // TODO We need to implement the inverse of
        // lnb->GetIntermediateFrequency() for ProbeTuningParams()
        // to accurately reflect the frequency before LNB transform.
        return false;
    }

    dvb_frontend_parameters params;

    int res = ioctl(m_fd_frontend, FE_GET_FRONTEND, &params);
    if (res < 0)
    {
        LOG(VB_CHANNEL, LOG_ERR, LOC + "Getting device frontend failed." + ENO);
    }

    return (res >= 0);
}

/** \fn DVBChannel::ProbeTuningParams(DTVMultiplex&) const
 *  \brief Fetches DTVMultiplex params from driver
 *
 *   Note: Only updates tuning on success.
 *
 *  \return true on success, false on failure
 */
bool DVBChannel::ProbeTuningParams(DTVMultiplex &tuning) const
{
    QMutexLocker locker(&m_hw_lock);

    if (m_fd_frontend < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Card not open!");

        return false;
    }

    const DVBChannel *master = GetMasterLock();
    if (master != this)
    {
        bool ok = master->ProbeTuningParams(tuning);
        ReturnMasterLock(master);
        return ok;
    }
    ReturnMasterLock(master); // if we're the master we don't need this lock..

    if (m_diseqc_tree)
    {
        // TODO We need to implement the inverse of
        // lnb->GetIntermediateFrequency() for ProbeTuningParams()
        // to accurately reflect the frequency before LNB transform.
        return false;
    }

    if (m_tunerType == DTVTunerType::kTunerTypeDVBS2)
    {
        // TODO implement probing of tuning parameters with FE_GET_PROPERTY
        return false;
    }

    dvb_frontend_parameters params;
    if (ioctl(m_fd_frontend, FE_GET_FRONTEND, &params) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Getting Frontend tuning parameters failed." + ENO);

        return false;
    }

    uint    mplex      = tuning.m_mplex;
    QString sistandard = tuning.m_sistandard;

    tuning = dvbparams_to_dtvmultiplex(m_tunerType, params);

    tuning.m_mplex       = mplex;
    tuning.m_sistandard  = sistandard;

    return true;
}

/** \fn DVBChannel::GetChanID() const
 *  \brief Returns Channel ID
 *  \bug This only works if there is only one input on the card.
 */
int DVBChannel::GetChanID() const
{
    int found = 0;
    int id = -1;
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT chanid,visible "
                  "FROM channel, capturecard "
                  "WHERE capturecard.sourceid = channel.sourceid AND "
                  "      channel.channum = :CHANNUM AND "
                  "      capturecard.cardid = :INPUTID");

    query.bindValue(":CHANNUM", m_curchannelname);
    query.bindValue(":INPUTID", m_inputid);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("fetching chanid", query);
        return -1;
    }

    while (query.next())
    {
        found += query.value(1).toInt();
        if (id == -1 || found)
            id = query.value(0).toInt();
    }

    if (!found)
    {
        LOG(VB_GENERAL, LOG_INFO,
            QString("No visible channel ids for %1")
            .arg(m_curchannelname));
    }

    if (found > 1)
    {
        LOG(VB_GENERAL, LOG_WARNING,
            QString("Found multiple visible channel ids for %1")
            .arg(m_curchannelname));
    }

    return id;
}

const DiSEqCDevRotor *DVBChannel::GetRotor(void) const
{
    if (m_diseqc_tree)
        return m_diseqc_tree->FindRotor(m_diseqc_settings);

    return nullptr;
}

// documented in dvbchannel.h
bool DVBChannel::HasLock(bool *ok) const
{
    const DVBChannel *master = GetMasterLock();
    if (master != this)
    {
        bool haslock = master->HasLock(ok);
        ReturnMasterLock(master);
        return haslock;
    }
    ReturnMasterLock(master); // if we're the master we don't need this lock..

    fe_status_t status;
    memset(&status, 0, sizeof(status));

    int ret = ioctl(m_fd_frontend, FE_READ_STATUS, &status);
    if (ret < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "FE_READ_STATUS failed" + ENO);
    }

    if (ok)
        *ok = (0 == ret);

    return (status & FE_HAS_LOCK) != 0;
}

// documented in dvbchannel.h
double DVBChannel::GetSignalStrengthDVBv5(bool *ok) const
{
    struct dtv_property prop;
    struct dtv_properties cmd;

    memset(&prop, 0, sizeof(prop));
    prop.cmd = DTV_STAT_SIGNAL_STRENGTH;
    cmd.num = 1;
    cmd.props = &prop;
    int ret = ioctl(m_fd_frontend, FE_GET_PROPERTY, &cmd);
    LOG(VB_RECORD, LOG_DEBUG, LOC +
        QString("FE DTV signal strength ret=%1 res=%2 len=%3 scale=%4 val=%5")
        .arg(ret)
        .arg(cmd.props->result)
        .arg(cmd.props->u.st.len)
        .arg(cmd.props->u.st.stat[0].scale)
        .arg(cmd.props->u.st.stat[0].svalue)
        );
    bool tmpOk = (ret == 0) && (cmd.props->u.st.len > 0);
    if (ok)
        *ok = tmpOk;
    double value = 0;
    if (tmpOk)
    {
        if (cmd.props->u.st.stat[0].scale == FE_SCALE_DECIBEL)
        {
            // -20dBm is a great signal so make that 100%
            // -100dBm lower than the noise floor so that is 0%
            // svalue is in 0.001 dBm
            // If the value is outside the range -100 to 0 dBm
            // we do not believe it.
            int svalue = cmd.props->u.st.stat[0].svalue;
            if (svalue >= -100000 && svalue <= -0)
            {
                // convert value from -100dBm to -20dBm to a 0-1 range
                value = svalue + 100000.0;
                value = value / 80000.0;
                if (value > 1.0)
                    value = 1.0;
            }
        }
        else if (cmd.props->u.st.stat[0].scale == FE_SCALE_RELATIVE)
        {
            // returned as 16 bit unsigned
            value = cmd.props->u.st.stat[0].uvalue / 65535.0;
        }
    }
    else
    {
        LOG(VB_RECORD, LOG_ERR, LOC +
            "Getting DVBv5 Frontend signal strength failed." + ENO);
    }
    return value;
}

// documented in dvbchannel.h
double DVBChannel::GetSignalStrength(bool *ok) const
{
    const DVBChannel *master = GetMasterLock();
    if (master != this)
    {
        double val = master->GetSignalStrength(ok);
        ReturnMasterLock(master);
        return val;
    }
    ReturnMasterLock(master); // if we're the master we don't need this lock..

    // We use uint16_t for sig because this is correct for DVB API 4.0,
    // and works better than the correct int16_t for the 3.x API
    uint16_t sig = 0;
    int ret = ioctl(m_fd_frontend, FE_READ_SIGNAL_STRENGTH, &sig);
    if (ret < 0)
    {
        if (errno == EOPNOTSUPP || errno == ENOTSUPP)
        {
            return GetSignalStrengthDVBv5(ok);
        }
        LOG(VB_RECORD, LOG_ERR, LOC +
            "Getting Frontend signal strength failed." + ENO);
    }

    if (ok)
        *ok = (0 == ret);

    return sig * (1.0 / 65535.0);
}

// documented in dvbchannel.h
double DVBChannel::GetSNRDVBv5(bool *ok) const
{
    struct dtv_property prop;
    struct dtv_properties cmd;

    memset(&prop, 0, sizeof(prop));
    prop.cmd = DTV_STAT_CNR;
    cmd.num = 1;
    cmd.props = &prop;
    int ret = ioctl(m_fd_frontend, FE_GET_PROPERTY, &cmd);
    LOG(VB_RECORD, LOG_DEBUG, LOC +
        QString("FE DTV cnr ret=%1 res=%2 len=%3 scale=%4 val=%5")
        .arg(ret)
        .arg(cmd.props->result)
        .arg(cmd.props->u.st.len)
        .arg(cmd.props->u.st.stat[0].scale)
        .arg(cmd.props->u.st.stat[0].svalue)
        );
    bool tmpOk = (ret == 0) && (cmd.props->u.st.len > 0);
    if (ok)
        *ok = tmpOk;
    double value = 0;
    if (tmpOk)
    {
        if (cmd.props->u.st.stat[0].scale == FE_SCALE_DECIBEL)
        {
            // svalue is in 0.001 dB
            value = cmd.props->u.st.stat[0].svalue;
            // let 50dB+ CNR be 100% quality and 0dB be 0%
            // convert 0.001 dB from 0-50000 to a 0-1 range
            value = value / 50000.0;
            if (value > 1.0)
                value = 1.0;
            else if (value < 0)
                value = 0.0;
        }
        else if (cmd.props->u.st.stat[0].scale == FE_SCALE_RELATIVE)
        {
            // returned as 16 bit unsigned
            value = cmd.props->u.st.stat[0].uvalue / 65535.0;
        }
    }
    else
    {
        LOG(VB_RECORD, LOG_ERR, LOC +
            "Getting DVBv5 Frontend signal/noise ratio failed." + ENO);
    }
    return value;
}

// documented in dvbchannel.h
double DVBChannel::GetSNR(bool *ok) const
{
    const DVBChannel *master = GetMasterLock();
    if (master != this)
    {
        double val = master->GetSNR(ok);
        ReturnMasterLock(master);
        return val;
    }
    ReturnMasterLock(master); // if we're the master we don't need this lock..

    // We use uint16_t for sig because this is correct for DVB API 4.0,
    // and works better than the correct int16_t for the 3.x API
    uint16_t snr = 0;
    int ret = ioctl(m_fd_frontend, FE_READ_SNR, &snr);
    if (ret < 0)
    {
        if (errno == EOPNOTSUPP || errno == ENOTSUPP)
        {
            return GetSNRDVBv5(ok);
        }
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Getting Frontend signal/noise ratio failed." + ENO);
    }

    if (ok)
        *ok = (0 == ret);

    return snr * (1.0 / 65535.0);
}

// documented in dvbchannel.h
double DVBChannel::GetBitErrorRateDVBv5(bool *ok) const
{
    struct dtv_property prop[2];
    struct dtv_properties cmd;

    memset(&prop, 0, sizeof(prop));
    prop[0].cmd = DTV_STAT_POST_ERROR_BIT_COUNT;
    prop[1].cmd = DTV_STAT_POST_TOTAL_BIT_COUNT;
    cmd.num = 2;
    cmd.props = prop;
    int ret = ioctl(m_fd_frontend, FE_GET_PROPERTY, &cmd);
    LOG(VB_RECORD, LOG_DEBUG, LOC +
        QString("FE DTV bit error rate ret=%1 res=%2 len=%3 scale=%4 val=%5 res=%6 len=%7 scale=%8 val=%9")
        .arg(ret)
        .arg(cmd.props[0].result)
        .arg(cmd.props[0].u.st.len)
        .arg(cmd.props[0].u.st.stat[0].scale)
        .arg(cmd.props[0].u.st.stat[0].uvalue)
        .arg(cmd.props[1].result)
        .arg(cmd.props[1].u.st.len)
        .arg(cmd.props[1].u.st.stat[0].scale)
        .arg(cmd.props[1].u.st.stat[0].uvalue)
        );
    bool tmpOk = (ret == 0) &&
            (cmd.props[0].u.st.len > 0) &&
            (cmd.props[1].u.st.len > 0);
    if (ok)
        *ok = tmpOk;
    double value = 0;
    if (tmpOk)
    {
        if ((cmd.props[0].u.st.stat[0].scale == FE_SCALE_COUNTER) &&
            (cmd.props[1].u.st.stat[0].scale == FE_SCALE_COUNTER) &&
            (cmd.props[1].u.st.stat[0].uvalue != 0))
        {
            value = static_cast<double>(
                    static_cast<long double>(cmd.props[0].u.st.stat[0].uvalue) /
                    cmd.props[1].u.st.stat[0].uvalue);
        }
    }
    else
    {
        LOG(VB_RECORD, LOG_ERR, LOC +
            "Getting DVBv5 Frontend bit error rate failed." + ENO);
    }
    return value;
}

// documented in dvbchannel.h
double DVBChannel::GetBitErrorRate(bool *ok) const
{
    const DVBChannel *master = GetMasterLock();
    if (master != this)
    {
        double val = master->GetBitErrorRate(ok);
        ReturnMasterLock(master);
        return val;
    }
    ReturnMasterLock(master); // if we're the master we don't need this lock..

    uint32_t ber = 0;
    int ret = ioctl(m_fd_frontend, FE_READ_BER, &ber);
    if (ret < 0)
    {
        if (errno == EOPNOTSUPP || errno == ENOTSUPP)
        {
            return GetBitErrorRateDVBv5(ok);
        }
        LOG(VB_RECORD, LOG_ERR, LOC +
            "Getting Frontend bit error rate failed." + ENO);
    }

    if (ok)
        *ok = (0 == ret);

    return (double) ber;
}

// documented in dvbchannel.h
double DVBChannel::GetUncorrectedBlockCountDVBv5(bool *ok) const
{
    struct dtv_property prop;
    struct dtv_properties cmd;

    memset(&prop, 0, sizeof(prop));
    prop.cmd = DTV_STAT_ERROR_BLOCK_COUNT;
    cmd.num = 1;
    cmd.props = &prop;
    int ret = ioctl(m_fd_frontend, FE_GET_PROPERTY, &cmd);
    LOG(VB_RECORD, LOG_DEBUG, LOC +
        QString("FE DTV uncorrected block count ret=%1 res=%2 len=%3 scale=%4 val=%5")
        .arg(ret)
        .arg(cmd.props[0].result)
        .arg(cmd.props[0].u.st.len)
        .arg(cmd.props[0].u.st.stat[0].scale)
        .arg(cmd.props[0].u.st.stat[0].svalue)
        );
    bool tmpOk = (ret == 0) && (cmd.props->u.st.len > 0);
    if (ok)
        *ok = tmpOk;
    double value = 0;
    if (tmpOk)
    {
        if (cmd.props->u.st.stat[0].scale == FE_SCALE_COUNTER)
            value = cmd.props->u.st.stat[0].uvalue;
    }
    else
    {
        LOG(VB_RECORD, LOG_DEBUG, LOC +
            "Getting DVBv5 Frontend uncorrected block count failed." + ENO);
    }
    return value;
}

// documented in dvbchannel.h
double DVBChannel::GetUncorrectedBlockCount(bool *ok) const
{
    const DVBChannel *master = GetMasterLock();
    if (master != this)
    {
        double val = master->GetUncorrectedBlockCount(ok);
        ReturnMasterLock(master);
        return val;
    }
    ReturnMasterLock(master); // if we're the master we don't need this lock..

    uint32_t ublocks = 0;
    int ret = ioctl(m_fd_frontend, FE_READ_UNCORRECTED_BLOCKS, &ublocks);
    if (ret < 0)
    {
        if (errno == EOPNOTSUPP || errno == ENOTSUPP)
        {
            return GetUncorrectedBlockCountDVBv5(ok);
        }
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Getting Frontend uncorrected block count failed." + ENO);
    }

    if (ok)
        *ok = (0 == ret);

    return (double) ublocks;
}

DVBChannel *DVBChannel::GetMasterLock(void)
{
    QString key = CardUtil::GetDeviceName(DVB_DEV_FRONTEND, m_device);
    if (m_pParent)
        key += QString(":%1")
            .arg(CardUtil::GetSourceID(m_pParent->GetInputId()));
    DTVChannel *master = DTVChannel::GetMasterLock(key);
    DVBChannel *dvbm = dynamic_cast<DVBChannel*>(master);
    if (master && !dvbm)
        DTVChannel::ReturnMasterLock(master);
    return dvbm;
}

void DVBChannel::ReturnMasterLock(DVBChannelP &dvbm)
{
    DTVChannel *chan = static_cast<DTVChannel*>(dvbm);
    DTVChannel::ReturnMasterLock(chan);
    dvbm = nullptr;
}

const DVBChannel *DVBChannel::GetMasterLock(void) const
{
    QString key = CardUtil::GetDeviceName(DVB_DEV_FRONTEND, m_device);
    if (m_pParent)
        key += QString(":%1")
            .arg(CardUtil::GetSourceID(m_pParent->GetInputId()));
    DTVChannel *master = DTVChannel::GetMasterLock(key);
    DVBChannel *dvbm = dynamic_cast<DVBChannel*>(master);
    if (master && !dvbm)
        DTVChannel::ReturnMasterLock(master);
    return dvbm;
}

void DVBChannel::ReturnMasterLock(DVBChannelCP &dvbm)
{
    DTVChannel *chan =
        static_cast<DTVChannel*>(const_cast<DVBChannel*>(dvbm));
    DTVChannel::ReturnMasterLock(chan);
    dvbm = nullptr;
}

bool DVBChannel::IsMaster(void) const
{
    const DVBChannel *master = GetMasterLock();
    bool is_master = (master == this);
    ReturnMasterLock(master);
    return is_master;
}

/** \fn drain_dvb_events(int)
 *  \brief Reads all the events off the queue, so we can use select
 *         in wait_for_backend(int,int).
 */
static void drain_dvb_events(int fd)
{
    struct dvb_frontend_event event;
    int ret = 0;
    while ((ret = ioctl(fd, FE_GET_EVENT, &event)) == 0);
    if ((ret < 0) && (EAGAIN != errno))
    {
        LOG(VB_CHANNEL, LOG_DEBUG, "Draining DVB Event failed. " + ENO);
    }
}

/** \fn wait_for_backend(int,int)
 *  \brief Waits for backend to get tune message.
 *
 *   With linux 2.6.12 or later this should block
 *   until the backend is tuned to the proper frequency.
 *
 *   With earlier kernels sleeping for timeout_ms may save us.
 *
 *   Waiting for DVB events has been tried, but this fails
 *   with several DVB cards. They either don't send the
 *   required events or delete them from the event queue
 *   before we can read the event.
 *
 *   Using a FE_GET_FRONTEND has also been tried, but returns
 *   the old data until a 2 sec timeout elapses on at least
 *   one DVB card.
 *
 *   We do not block until we have a lock, the signal
 *   monitor does that.
 *
 *  \param fd         frontend file descriptor
 *  \param timeout_ms timeout before FE_READ_STATUS in milliseconds
 */
static bool wait_for_backend(int fd, int timeout_ms)
{
    struct timeval select_timeout = { timeout_ms/1000, (timeout_ms % 1000) * 1000 /*usec*/};
    fd_set fd_select_set;
    FD_ZERO(    &fd_select_set);
    FD_SET (fd, &fd_select_set);

    // Try to wait for some output like an event, unfortunately
    // this fails on several DVB cards, so we have a timeout.
    int ret = 0;
    do ret = select(fd+1, &fd_select_set, nullptr, nullptr, &select_timeout);
    while ((-1 == ret) && (EINTR == errno));

    if (-1 == ret)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "DVBChan: wait_for_backend: Failed to wait on output" + ENO);

        return false;
    }

    // This is supposed to work on all cards, post 2.6.12...
    fe_status_t status;
    memset(&status, 0, sizeof(status));

    if (ioctl(fd, FE_READ_STATUS, &status) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "DVBChan: wait_for_backend: Failed to get status" + ENO);

        return false;
    }

    LOG(VB_CHANNEL, LOG_INFO, QString("DVBChan: wait_for_backend: Status: %1")
            .arg(toString(status)));

    return true;
}

static struct dvb_frontend_parameters dtvmultiplex_to_dvbparams(
    DTVTunerType tuner_type, const DTVMultiplex &tuning,
    int intermediate_freq, bool can_fec_auto)
{
    dvb_frontend_parameters params;
    memset(&params, 0, sizeof(params));

    params.frequency = tuning.m_frequency;
    params.inversion = (fe_spectral_inversion_t) (int) tuning.m_inversion;

    if (DTVTunerType::kTunerTypeDVBS1 == tuner_type)
    {
        if (tuning.m_mod_sys == DTVModulationSystem::kModulationSystem_DVBS2)
            LOG(VB_GENERAL, LOG_ERR,
                "DVBChan: Error, Tuning of a DVB-S2 transport "
                "with a DVB-S card will fail.");

        params.frequency = intermediate_freq;
        params.u.qpsk.symbol_rate = tuning.m_symbolrate;
        params.u.qpsk.fec_inner   = can_fec_auto ? FEC_AUTO
            : (fe_code_rate_t) (int) tuning.m_fec;
    }

    if (DTVTunerType::kTunerTypeDVBS2 == tuner_type)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "DVBChan: Error, MythTV was compiled without "
            "DVB-S2 headers being present so DVB-S2 tuning will fail.");
    }

    if (DTVTunerType::kTunerTypeDVBC == tuner_type)
    {
        params.u.qam.symbol_rate  = tuning.m_symbolrate;
        params.u.qam.fec_inner    = (fe_code_rate_t) (int) tuning.m_fec;
        params.u.qam.modulation   = (fe_modulation_t) (int) tuning.m_modulation;
    }

    if (DTVTunerType::kTunerTypeDVBT == tuner_type ||
        DTVTunerType::kTunerTypeDVBT2 == tuner_type)
    {
        params.u.ofdm.bandwidth             =
            (fe_bandwidth_t) (int) tuning.m_bandwidth;
        params.u.ofdm.code_rate_HP          =
            (fe_code_rate_t) (int) tuning.m_hp_code_rate;
        params.u.ofdm.code_rate_LP          =
            (fe_code_rate_t) (int) tuning.m_lp_code_rate;
        params.u.ofdm.constellation         =
            (fe_modulation_t) (int) tuning.m_modulation;
        params.u.ofdm.transmission_mode     =
            (fe_transmit_mode_t) (int) tuning.m_trans_mode;
        params.u.ofdm.guard_interval        =
            (fe_guard_interval_t) (int) tuning.m_guard_interval;
        params.u.ofdm.hierarchy_information =
            (fe_hierarchy_t) (int) tuning.m_hierarchy;
    }

    if (DTVTunerType::kTunerTypeATSC == tuner_type)
    {
        params.u.vsb.modulation   =
            (fe_modulation_t) (int) tuning.m_modulation;
    }

    return params;
}

static DTVMultiplex dvbparams_to_dtvmultiplex(
    DTVTunerType tuner_type, const dvb_frontend_parameters &params)
{
    DTVMultiplex tuning;

    tuning.m_frequency = params.frequency;
    tuning.m_inversion = params.inversion;

    if ((DTVTunerType::kTunerTypeDVBS1 == tuner_type) ||
        (DTVTunerType::kTunerTypeDVBS2 == tuner_type))
    {
        tuning.m_symbolrate     = params.u.qpsk.symbol_rate;
        tuning.m_fec            = params.u.qpsk.fec_inner;
    }

    if (DTVTunerType::kTunerTypeDVBC   == tuner_type)
    {
        tuning.m_symbolrate     = params.u.qam.symbol_rate;
        tuning.m_fec            = params.u.qam.fec_inner;
        tuning.m_modulation     = params.u.qam.modulation;
    }

    if (DTVTunerType::kTunerTypeDVBT   == tuner_type ||
        DTVTunerType::kTunerTypeDVBT2  == tuner_type)
    {
        tuning.m_bandwidth      = params.u.ofdm.bandwidth;
        tuning.m_hp_code_rate   = params.u.ofdm.code_rate_HP;
        tuning.m_lp_code_rate   = params.u.ofdm.code_rate_LP;
        tuning.m_modulation     = params.u.ofdm.constellation;
        tuning.m_trans_mode     = params.u.ofdm.transmission_mode;
        tuning.m_guard_interval = params.u.ofdm.guard_interval;
        tuning.m_hierarchy      = params.u.ofdm.hierarchy_information;
    }

    if (DTVTunerType::kTunerTypeATSC    == tuner_type)
    {
        tuning.m_modulation     = params.u.vsb.modulation;
    }

    return tuning;
}
