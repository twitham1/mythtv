#ifndef _SATIP_CHANNEL_H_
#define _SATIP_CHANNEL_H_

// Qt headers
#include <QString>
#include <QMutex>

// MythTV headers
#include "dtvchannel.h"
#include "satipstreamhandler.h"

class SatIPChannel : public DTVChannel
{
  public:
    SatIPChannel(TVRec *parent, QString  device);
    ~SatIPChannel(void) override;

    // Commands
    bool Open(void) override;   // ChannelBase
    void Close(void) override;  // ChannelBase

    using DTVChannel::Tune;
    bool Tune(const DTVMultiplex& /*tuning*/) override;    // DTVChannel
    bool Tune(const QString &channum) override; // DTVChannel

    // Gets
    bool IsOpen(void) const override;               // ChannelBase
    QString GetDevice(void) const override          // ChannelBase
       { return m_device; }
    bool IsPIDTuningSupported(void) const override  // DTVChannel
        { return true; }
    bool IsMaster(void) const override;             // DTVChannel
    
    SatIPStreamHandler *GetStreamHandler(void) const { return m_streamHandler; }

  private:
    QString             m_device;
    QStringList         m_args;
    mutable QMutex      m_tuneLock;
    mutable QMutex      m_streamLock;
    SatIPStreamHandler *m_streamHandler       {nullptr};
    MPEGStreamData     *m_streamData          {nullptr};
    QString             m_videodev;
};

#endif // _SATIP_CHANNEL_H_
