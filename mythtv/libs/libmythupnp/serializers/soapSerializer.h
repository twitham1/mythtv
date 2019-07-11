//////////////////////////////////////////////////////////////////////////////
// Program Name: soapSerializer.h
// Created     : Dec. 30, 2009
//
// Purpose     : Serializer Implementation for SOAP 
//                                                                            
// Copyright (c) 2005 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details                    
//
//////////////////////////////////////////////////////////////////////////////

#ifndef __SOAPSERIALIZER_H__
#define __SOAPSERIALIZER_H__

#include "upnpexp.h"
#include "xmlSerializer.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC SoapSerializer : public XmlSerializer
{
    public:

        SoapSerializer( QIODevice     *pDevice, 
                        const QString &sNamespace, 
                        const QString &sRequestName ) : XmlSerializer( pDevice, sRequestName ) 
        {
            m_sNamespace = sNamespace;
        }

        virtual ~SoapSerializer() = default;

        // ------------------------------------------------------------------
        //
        // ------------------------------------------------------------------

        void AddHeaders( QStringMap &headers ) override // Serializer
        {
            XmlSerializer::AddHeaders( headers );

            if (!headers.contains( "EXT" ))
                headers.insert( "EXT", "" );
        }


    protected:

        QString     m_sNamespace;

        // ------------------------------------------------------------------
        //
        // ------------------------------------------------------------------

        void BeginSerialize( QString &sName)  override // Serializer
        {
            m_pXmlWriter->writeStartDocument( "1.0" );

            m_pXmlWriter->writeStartElement("http://schemas.xmlsoap.org/soap/envelope/", "Envelope" );

            m_pXmlWriter->writeAttribute( "xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance" );
           // m_pXmlWriter->writeAttribute( "encodingStyle", "http://schemas.xmlsoap.org/soap/encoding/" );

            m_pXmlWriter->writeStartElement( "http://schemas.xmlsoap.org/soap/envelope/", "Body" );

            m_pXmlWriter->writeStartElement( m_sRequestName + "Response" );
            m_pXmlWriter->writeAttribute( "xmlns", m_sNamespace );

            m_bIsRoot = false;
    
            sName = m_sRequestName + "Result";
        }
};

#endif

