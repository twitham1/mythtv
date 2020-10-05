// C++ includes
#include <utility>

// Qt includes
#include <QMutexLocker>
#include <QString>

// MythTV includes
#include "mythlogging.h"
#include "mpegtables.h"
#include "tv_rec.h"
#include "satiputils.h"
#include "satipchannel.h"

#define LOC  QString("SatIPChan[%1](%2): ").arg(m_inputId).arg(SatIPChannel::GetDevice())

SatIPChannel::SatIPChannel(TVRec *parent, QString  device) :
    DTVChannel(parent), m_device(std::move(device))
{
    RegisterForMaster(m_device);
}

SatIPChannel::~SatIPChannel(void)
{
    SatIPChannel::Close();
    DeregisterForMaster(m_device);
}

bool SatIPChannel::IsMaster(void) const
{
    DTVChannel *master = DTVChannel::GetMasterLock(m_device);
    bool is_master = (master == this);
    DTVChannel::ReturnMasterLock(master);
    return is_master;
}

bool SatIPChannel::Open(void)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Open()");

    if (IsOpen())
        return true;

    QMutexLocker locker(&m_tuneLock);

    m_tunerType = SatIP::toTunerType(m_device);

    LOG(VB_CHANNEL, LOG_DEBUG, LOC + QString("Open() m_tunerType:%1 %2")
        .arg(m_tunerType).arg(m_tunerType.toString()));

    if (!InitializeInput())
    {
        LOG(VB_CHANNEL, LOG_INFO, LOC + "InitializeInput() failed");
        Close();
        return false;
    }

    m_streamHandler = SatIPStreamHandler::Get(m_device, GetInputID());

    return true;
}

void SatIPChannel::Close()
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Close()");

    QMutexLocker locker(&m_streamLock);
    if (m_streamHandler)
    {
        if (m_streamData)
        {
            m_streamHandler->RemoveListener(m_streamData);
        }
        SatIPStreamHandler::Return(m_streamHandler, m_inputId);
    }
}

bool SatIPChannel::Tune(const QString &channum)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("Tune(channum=%1) TODO").arg(channum));
    if (!IsOpen())
    {
        LOG(VB_CHANNEL, LOG_ERR, LOC + "Tune failed, not open");
        return false;
    }
    return false;
}

bool SatIPChannel::Tune(const DTVMultiplex &tuning)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("Tune(frequency=%1)").arg(tuning.m_frequency));
    m_streamHandler->Tune(tuning);
    return true;
}

bool SatIPChannel::IsOpen(void) const
{
    QMutexLocker locker(&m_streamLock);
    return m_streamHandler;
}
