#ifndef MYTHEVENT_H_
#define MYTHEVENT_H_

#include <QStringList>
#include <QEvent>
#include <utility>
#include "mythtypes.h"
#include "mythbaseexp.h"

/** \class MythEvent
    \brief This class is used as a container for messages.

    Any subclass of this that adds data to the event should override
    the clone method. As example, see OutputEvent in output.h.
 */
class MBASE_PUBLIC MythEvent : public QEvent
{
  public:
    explicit MythEvent(int type) : QEvent((QEvent::Type)type)
    { }

    // lmessage is passed by value for thread safety reasons per DanielK
    MythEvent(int type, QString lmessage) : QEvent((QEvent::Type)type),
        m_message(::std::move(lmessage)),    m_extradata("empty")
    {
    }

    // lmessage is passed by value for thread safety reasons per DanielK
    MythEvent(int type, QString lmessage, const QStringList &lextradata)
           : QEvent((QEvent::Type)type),
            m_message(::std::move(lmessage)),    m_extradata(lextradata)
    {
    }

    // lmessage is passed by value for thread safety reasons per DanielK
    explicit MythEvent(QString lmessage) : QEvent(MythEventMessage),
            m_message(::std::move(lmessage)),    m_extradata("empty")
    {
    }

    // lmessage is passed by value for thread safety reasons per DanielK
    MythEvent(QString lmessage, const QStringList &lextradata)
           : QEvent(MythEventMessage),
           m_message(::std::move(lmessage)),    m_extradata(lextradata)
    {
    }

    // lmessage is passed by value for thread safety reasons per DanielK
    MythEvent(QString lmessage, const QString lextradata)
           : QEvent(MythEventMessage),
           m_message(::std::move(lmessage)),    m_extradata(lextradata)
    {
    }


    virtual ~MythEvent() = default;

    const QString& Message() const { return m_message; }
    const QString& ExtraData(int idx = 0) const { return m_extradata[idx]; }
    const QStringList& ExtraDataList() const { return m_extradata; }
    int ExtraDataCount() const { return m_extradata.size(); }

    virtual MythEvent *clone() const
    { return new MythEvent(m_message, m_extradata); }

    static Type MythEventMessage;
    static Type MythUserMessage;
    static Type kUpdateTvProgressEventType;
    static Type kExitToMainMenuEventType;
    static Type kMythPostShowEventType;
    static Type kPushDisableDrawingEventType;
    static Type kPopDisableDrawingEventType;
    static Type kLockInputDevicesEventType;
    static Type kUnlockInputDevicesEventType;
    static Type kUpdateBrowseInfoEventType;
    static Type kDisableUDPListenerEventType;
    static Type kEnableUDPListenerEventType;

  protected:
    QString m_message;
    QStringList m_extradata;
};

class MBASE_PUBLIC ExternalKeycodeEvent : public QEvent
{
  public:
    explicit ExternalKeycodeEvent(const int key) :
        QEvent(kEventType), m_keycode(key) {}

    int getKeycode() { return m_keycode; }

    static Type kEventType;

  private:
    int m_keycode;
};

class MBASE_PUBLIC UpdateBrowseInfoEvent : public QEvent
{
  public:
    explicit UpdateBrowseInfoEvent(const InfoMap &infoMap) :
        QEvent(MythEvent::kUpdateBrowseInfoEventType), im(infoMap) {}
    InfoMap im;
};

// TODO combine with UpdateBrowseInfoEvent above
class MBASE_PUBLIC MythInfoMapEvent : public MythEvent
{
  public:
    MythInfoMapEvent(const QString &lmessage,
                     const InfoMap &linfoMap)
      : MythEvent(lmessage), m_infoMap(linfoMap) { }

    MythInfoMapEvent *clone() const override // MythEvent
        { return new MythInfoMapEvent(Message(), m_infoMap); }
    const InfoMap* GetInfoMap(void) { return &m_infoMap; }

  private:
    InfoMap m_infoMap;
};

#endif /* MYTHEVENT_H */
