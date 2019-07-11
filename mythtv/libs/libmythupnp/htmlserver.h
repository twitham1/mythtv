//////////////////////////////////////////////////////////////////////////////
// Program Name: htmlserver.h
// Created     : Mar. 9, 2011
//
// Purpose     : Http server extension to serve up static html content
//
// Copyright (c) 2011 David Blain <dblain@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details                    
//
//////////////////////////////////////////////////////////////////////////////

#ifndef __HTMLSERVER_H__
#define __HTMLSERVER_H__

#include "httpserver.h"
#include "serverSideScripting.h"

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// HtmlExtension Class Definition
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC HtmlServerExtension : public HttpServerExtension
{
    private:

        ServerSideScripting m_Scripting;
        QString             m_IndexFilename;

    public:
                 HtmlServerExtension( const QString &sSharePath,
                                      const QString &sApplicationPrefix);
        virtual ~HtmlServerExtension( ) = default;

        // Special case, this extension is called if no other extension
        // processes the request.  

        QStringList GetBasePaths() override // HttpServerExtension
            { return QStringList(); }

        bool ProcessRequest( HTTPRequest *pRequest ) override; // HttpServerExtension

        QScriptEngine* ScriptEngine()
        {
            return &(m_Scripting.m_engine);
        }

};

#endif
