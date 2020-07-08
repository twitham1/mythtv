//////////////////////////////////////////////////////////////////////////////
// Program Name: eventing.cpp
// Created     : Dec. 22, 2006
//
// Purpose     : uPnp Eventing Base Class Implementation
//                                                                            
// Copyright (c) 2006 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details                    
//
//////////////////////////////////////////////////////////////////////////////

#include <cmath>

#include <QStringList>
#include <QTextCodec>
#include <QTextStream>

#include "upnp.h"
#include "eventing.h"
#include "upnptaskevent.h"
#include "mythlogging.h"

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

uint StateVariables::BuildNotifyBody(
    QTextStream &ts, TaskTime ttLastNotified) const
{
    uint nCount = 0;

    ts << "<?xml version=\"1.0\"?>" << endl
       << "<e:propertyset xmlns:e=\"urn:schemas-upnp-org:event-1-0\">" << endl;

    for (auto *prop : qAsConst(m_map))
    {
        if ( ttLastNotified < prop->m_ttLastChanged )
        {
            nCount++;

            ts << "<e:property>" << endl;
            ts <<   "<" << prop->m_sName << ">";
            ts <<     prop->ToString();
            ts <<   "</" << prop->m_sName << ">";
            ts << "</e:property>" << endl;
        }
    }

    ts << "</e:propertyset>" << endl;
    ts << flush;

    return nCount;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

Eventing::Eventing(const QString &sExtensionName,
                   QString sEventMethodName,
                   const QString &sSharePath) :
    HttpServerExtension(sExtensionName, sSharePath),
    m_sEventMethodName(std::move(sEventMethodName)),
    m_nSubscriptionDuration(
        UPnp::GetConfiguration()->GetValue("UPnP/SubscriptionDuration", 1800))
{
    m_nSupportedMethods |= (RequestTypeSubscribe | RequestTypeUnsubscribe);
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

Eventing::~Eventing()
{
    for (const auto *subscriber : qAsConst(m_Subscribers))
        delete subscriber;
    m_Subscribers.clear();
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

inline short Eventing::HoldEvents()
{
    // -=>TODO: Should use an Atomic increment... 
    //          need to research available functions.

    m_mutex.lock();
    bool err = (m_nHoldCount >= 127);
    short nVal = (m_nHoldCount++);
    m_mutex.unlock();

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Exceeded maximum guarranteed range of "
                                 "m_nHoldCount short [-128..127]");
        LOG(VB_GENERAL, LOG_ERR,
                "UPnP may not exhibit strange behavior or crash mythtv");
    }

    return nVal;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

inline short Eventing::ReleaseEvents()
{
    // -=>TODO: Should use an Atomic decrement... 

    m_mutex.lock();
    short nVal = (m_nHoldCount--);
    m_mutex.unlock();

    if (nVal == 0)
        Notify();

    return nVal;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QStringList Eventing::GetBasePaths()
{
    // -=>TODO: This isn't very efficient... Need to find out if we can make 
    //          this something unique, other than root.

    return QStringList( "/" );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool Eventing::ProcessRequest( HTTPRequest *pRequest )
{
    if (pRequest)
    {
        if ( pRequest->m_sBaseUrl != "/" )
            return false;

        if ( pRequest->m_sMethod != m_sEventMethodName )
            return false;

        LOG(VB_UPNP, LOG_INFO, QString("Eventing::ProcessRequest - Method (%1)")
                                   .arg(pRequest->m_sMethod ));

        switch( pRequest->m_eType )
        {
            case RequestTypeSubscribe   : HandleSubscribe   ( pRequest ); break;
            case RequestTypeUnsubscribe : HandleUnsubscribe ( pRequest ); break;
            default:
                UPnp::FormatErrorResponse( pRequest, UPnPResult_InvalidAction );
                break;
        }       
    }

    return( true );

}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void Eventing::ExecutePostProcess( )
{
    // Use PostProcessing Hook to perform Initial Notification
    // to make sure they receive it AFTER the subscription results

    if (m_pInitializeSubscriber != nullptr)
    {
        NotifySubscriber( m_pInitializeSubscriber );
        
        m_pInitializeSubscriber = nullptr;
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void Eventing::HandleSubscribe( HTTPRequest *pRequest ) 
{
    pRequest->m_eResponseType   = ResponseTypeXML;
    pRequest->m_nResponseStatus = 412;

    QString sCallBack = pRequest->GetRequestHeader( "CALLBACK", "" );
    QString sNT       = pRequest->GetRequestHeader( "NT"      , "" );
    QString sTimeout  = pRequest->GetRequestHeader( "TIMEOUT"  , "" );
    QString sSID      = pRequest->GetRequestHeader( "SID"     , "" );

    SubscriberInfo *pInfo = nullptr;

    // ----------------------------------------------------------------------
    // Validate Header Values...
    // ----------------------------------------------------------------------

    // -=>TODO: Need to add support for more than one CallBack URL.

    if ( sCallBack.length() != 0 )
    {
        // ------------------------------------------------------------------
        // New Subscription
        // ------------------------------------------------------------------

        if ( sSID.length() != 0 )   
        { 
            pRequest->m_nResponseStatus = 400; 
            return; 
        }

        if ( sNT != "upnp:event" )   
            return;

        // ----------------------------------------------------------------------
        // Process Subscription
        // ----------------------------------------------------------------------

        // -=>TODO: Temp code until support for multiple callbacks are supported.

        sCallBack = sCallBack.mid( 1, sCallBack.indexOf(">") - 1);

        int nDuration = m_nSubscriptionDuration;
        if ( sTimeout.startsWith("Second-") )
        {
            bool ok = false;
            int nValue = sTimeout.section("-", 1).toInt(&ok);
            if (ok)
                nDuration = nValue;
        }

        pInfo = new SubscriberInfo( sCallBack, nDuration );

        Subscribers::iterator it = m_Subscribers.find(pInfo->m_sUUID);
        if (it != m_Subscribers.end())
        {
            delete *it;
            m_Subscribers.erase(it);
        }
        m_Subscribers[pInfo->m_sUUID] = pInfo;

        // Use PostProcess Hook to Send Initial FULL Notification...
        //      *** Must send this response first then notify.

        m_pInitializeSubscriber = pInfo;
        pRequest->m_pPostProcess   = (IPostProcess *)this;

    }
    else
    {
        // ------------------------------------------------------------------
        // Renewal
        // ------------------------------------------------------------------

        if ( sSID.length() != 0 )   
        {
            sSID  = sSID.mid( 5 );
            pInfo = m_Subscribers[sSID];
        }

    }
    
    if (pInfo != nullptr)
    {
        pRequest->m_mapRespHeaders[ "SID"    ] = QString( "uuid:%1" )
                                                    .arg( pInfo->m_sUUID );

        pRequest->m_mapRespHeaders[ "TIMEOUT"] = QString( "Second-%1" )
                                                    .arg( pInfo->m_nDuration );

        pRequest->m_nResponseStatus = 200;

    }

}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void Eventing::HandleUnsubscribe( HTTPRequest *pRequest ) 
{
    pRequest->m_eResponseType   = ResponseTypeXML;
    pRequest->m_nResponseStatus = 412;

    QString sCallBack = pRequest->GetRequestHeader( "CALLBACK", "" );
    QString sNT       = pRequest->GetRequestHeader( "NT"      , "" );
    QString sSID      = pRequest->GetRequestHeader( "SID"     , "" );

    if ((sCallBack.length() != 0) || (sNT.length() != 0))     
    {
        pRequest->m_nResponseStatus = 400;
        return;
    }

    sSID = sSID.mid( 5 );

    Subscribers::iterator it = m_Subscribers.find(sSID);
    if (it != m_Subscribers.end())
    {
        delete *it;
        m_Subscribers.erase(it);
        pRequest->m_nResponseStatus = 200;
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void Eventing::Notify()
{
    TaskTime tt;
    gettimeofday( (&tt), nullptr );

    m_mutex.lock();

    Subscribers::iterator it = m_Subscribers.begin();
    while (it != m_Subscribers.end())
    { 
        if (!(*it))
        {   // This should never happen, but if someone inserted bad data...
            ++it;
            continue;
        }

        if (tt < (*it)->m_ttExpires)
        {
            // Subscription not expired yet. Send event notification.
            NotifySubscriber(*it);
            ++it;
        }
        else
        {
            // Time to expire this subscription. Remove subscriber from list.
            delete *it;
            it = m_Subscribers.erase(it);
        }
    }

    m_mutex.unlock();
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void Eventing::NotifySubscriber( SubscriberInfo *pInfo )
{
    if (pInfo == nullptr)
        return;

    QByteArray   aBody;
    QTextStream  tsBody( &aBody, QIODevice::WriteOnly );

    tsBody.setCodec(QTextCodec::codecForName("UTF-8"));

    // ----------------------------------------------------------------------
    // Build Body... Only send if there are changes
    // ----------------------------------------------------------------------

    uint nCount = BuildNotifyBody(tsBody, pInfo->m_ttLastNotified);
    if (nCount)
    {

        // -=>TODO: Need to add support for more than one CallBack URL.

        auto *pBuffer = new QByteArray();    // UPnpEventTask will delete this pointer.
        QTextStream  tsMsg( pBuffer, QIODevice::WriteOnly );

        tsMsg.setCodec(QTextCodec::codecForName("UTF-8"));

        // ----------------------------------------------------------------------
        // Build Message Header 
        // ----------------------------------------------------------------------

        int     nPort = (pInfo->m_qURL.port()>=0) ? pInfo->m_qURL.port() : 80;
        QString sHost = QString( "%1:%2" ).arg( pInfo->m_qURL.host() )
                                          .arg( nPort );

        tsMsg << "NOTIFY " << pInfo->m_qURL.path() << " HTTP/1.1\r\n";
        tsMsg << "HOST: " << sHost << "\r\n";
        tsMsg << "CONTENT-TYPE: \"text/xml\"\r\n";
        tsMsg << "Content-Length: " << QString::number( aBody.size() ) << "\r\n";
        tsMsg << "NT: upnp:event\r\n";
        tsMsg << "NTS: upnp:propchange\r\n";
        tsMsg << "SID: uuid:" << pInfo->m_sUUID << "\r\n";
        tsMsg << "SEQ: " << QString::number( pInfo->m_nKey ) << "\r\n";
        tsMsg << "\r\n";
        tsMsg << aBody;
        tsMsg << flush;

        // ------------------------------------------------------------------
        // Add new EventTask to the TaskQueue to do the actual sending.
        // ------------------------------------------------------------------

        LOG(VB_UPNP, LOG_INFO,
            QString("UPnp::Eventing::NotifySubscriber( %1 ) : %2 Variables")
                .arg( sHost ).arg(nCount));

        auto *pEventTask = new UPnpEventTask(QHostAddress(pInfo->m_qURL.host()),
                                             nPort, pBuffer);

        TaskQueue::Instance()->AddTask( 250, pEventTask );

        pEventTask->DecrRef();

        // ------------------------------------------------------------------
        // Update the subscribers Key & last Notified fields
        // ------------------------------------------------------------------

        pInfo->IncrementKey();

        gettimeofday( (&pInfo->m_ttLastNotified), nullptr );
    }
}

