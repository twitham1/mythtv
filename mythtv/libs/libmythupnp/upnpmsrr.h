
#ifndef UPnpMSRR_H_
#define UPnpMSRR_H_

#include <QDomDocument>
#include <QString>

#include "httpserver.h"
#include "eventing.h"
              
class UPnpMSRR;
                          
typedef enum 
{
    MSRR_Unknown                = 0,
    MSRR_GetServiceDescription  = 1,
    MSRR_IsAuthorized           = 2,
    MSRR_RegisterDevice         = 3,
    MSRR_IsValidated            = 4

} UPnpMSRRMethod;

//////////////////////////////////////////////////////////////////////////////
//
// UPnpMSRR Class Definition
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC  UPnpMSRR : public Eventing
{
    private:

        QString         m_sServiceDescFileName;
        QString         m_sControlUrl;

        UPnpMSRRMethod  GetMethod                  ( const QString &sURI  );

        void            HandleIsAuthorized         ( HTTPRequest *pRequest );
        void            HandleRegisterDevice       ( HTTPRequest *pRequest );
        void            HandleIsValidated          ( HTTPRequest *pRequest );

    protected:

        // Implement UPnpServiceImpl methods that we can

        QString GetServiceType() override // UPnpServiceImpl
            { return "urn:microsoft.com:service:X_MS_MediaReceiverRegistrar:1"; }
        QString GetServiceId() override // UPnpServiceImpl
            { return "urn:microsoft.com:serviceId:X_MS_MediaReceiverRegistrar"; }
        QString GetServiceControlURL() override // UPnpServiceImpl
            { return m_sControlUrl.mid( 1 ); }
        QString GetServiceDescURL() override // UPnpServiceImpl
            { return m_sControlUrl.mid( 1 ) + "/GetServDesc"; }

    public:
                 UPnpMSRR( UPnpDevice *pDevice,
                           const QString &sSharePath ); 

        virtual ~UPnpMSRR() = default;

        QStringList GetBasePaths() override; // Eventing

        bool ProcessRequest( HTTPRequest *pRequest ) override; // Eventing
};

#endif
