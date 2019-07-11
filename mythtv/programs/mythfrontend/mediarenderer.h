//////////////////////////////////////////////////////////////////////////////
// Program Name: mediarenderer.h
//                                                                            
// Purpose - uPnp Media Renderer main Class
//                                                                            
// Created By  : David Blain                    Created On : Jan. 25, 2007
// Modified By :                                Modified On:                  
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#ifndef __MEDIARENDERER_H__
#define __MEDIARENDERER_H__

#include <QObject>
#include <QMutex>

#include "upnp.h"
#include "upnpcmgr.h"
#include "mythxmlclient.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

class MediaRenderer : public UPnp
{
    protected:

      //UPnpControl     *m_pUPnpControl {nullptr};  // Do not delete (auto deleted)
        UPnpCMGR        *m_pUPnpCMGR    {nullptr};  // Do not delete (auto deleted)

    public:
                 MediaRenderer();
        virtual ~MediaRenderer();

        DeviceLocation *GetDefaultMaster();
        void            SetDefaultMaster( DeviceLocation *pDeviceLoc,
                                          const QString  &sPin );
};

#endif
