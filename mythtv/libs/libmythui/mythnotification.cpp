//
//  mythnotification.cpp
//  MythTV
//
//  Created by Jean-Yves Avenard on 25/06/13.
//  Copyright (c) 2013 Bubblestuff Pty Ltd. All rights reserved.
//

// libmyth headers
#include "mythlogging.h"
#include "mythnotification.h"

#include <QCoreApplication>

QEvent::Type MythNotification::New =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type MythNotification::Update =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type MythNotification::Info =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type MythNotification::Error =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type MythNotification::Warning =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type MythNotification::Check =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type MythNotification::Busy =
    (QEvent::Type) QEvent::registerEventType();

void MythNotification::SetId(int id)
{
    m_id = id;
    // default registered notification is to not expire
    if (m_id > 0 && m_duration == 0)
    {
        m_duration = -1;
    }
}

void MythNotification::ToStringList(void)
{
    m_extradata.clear();

    m_extradata << QString::number(Type())
                << QString::number(m_fullScreen)
                << m_description
                << QString::number(m_duration)
                << m_style
                << QString::number(m_visibility)
                << QString::number(m_priority)
                << m_metadata.value("minm")
                << m_metadata.value("asar")
                << m_metadata.value("asal")
                << m_metadata.value("asfm");
}

bool MythNotification::FromStringList(void)
{
    if (m_extradata.size() != 11)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("MythNotification::FromStringList called with %1 items, "
                    "expecting 11. '%2'")
            .arg(m_extradata.size()).arg(m_extradata.join(",")));
        return false;
    }

    QStringList::const_iterator Istr = m_extradata.begin();

    Type type     = static_cast<Type>((*Istr++).toInt());
    if (type != Type())
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("MythNotification::FromStringList called with type '%1' "
                    "in StringList, expected '%2' as set in constructor.")
            .arg(type).arg(Type()));
        return false;
    }
    m_fullScreen  = ((*Istr++).toInt() != 0);
    m_description = *Istr++;
    m_duration    = (*Istr++).toInt();
    m_style       = *Istr++;
    m_visibility  = static_cast<VNMask>((*Istr++).toInt());
    m_priority    = static_cast<Priority>((*Istr++).toInt());
    m_metadata["minm"] = *Istr++;
    m_metadata["asar"] = *Istr++;
    m_metadata["asal"] = *Istr++;
    m_metadata["asfm"] = *Istr++;

    return true;
}


/**
 * stringFromSeconds:
 *
 * Usage: stringFromSeconds(seconds).
 * Description: create a string in the format HH:mm:ss from a duration in seconds.
 * HH: will not be displayed if there's less than one hour.
 */
QString MythPlaybackNotification::stringFromSeconds(int time)
{
    int   hour    = time / 3600;
    int   minute  = (time - hour * 3600) / 60;
    int seconds   = time - hour * 3600 - minute * 60;
    QString str;

    if (hour)
    {
        str += QString("%1:").arg(hour);
    }
    if (minute < 10)
    {
        str += "0";
    }
    str += QString("%1:").arg(minute);
    if (seconds < 10)
    {
        str += "0";
    }
    str += QString::number(seconds);
    return str;
}

MythNotification::Type MythNotification::TypeFromString(const QString &type)
{
    if (type == "error")
    {
        return MythNotification::Error;
    }
    if (type == "warning")
    {
        return MythNotification::Warning;
    }
    if (type == "check")
    {
        return MythNotification::Check;
    }
    if (type == "busy")
    {
        return MythNotification::Busy;
    }
    return MythNotification::New;
}
