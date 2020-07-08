// Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>
//
// Use, modification and distribution is allowed without limitation,
// warranty, or liability of any kind.
//

// mythmusic
#include "decoder.h"
#include "constants.h"
#include "musicplayer.h"

// qt
#include <QDir>

// libmyth
#include <mythcontext.h>
#include <output.h>
#include <visual.h>

// libmythmetadata
#include "musicmetadata.h"
#include "metaio.h"

QEvent::Type DecoderEvent::Decoding =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type DecoderEvent::Stopped =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type DecoderEvent::Finished =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type DecoderEvent::Error =
    (QEvent::Type) QEvent::registerEventType();

Decoder::~Decoder()
{
    m_fctry = nullptr;
    m_out = nullptr;
}

/*
QString Decoder::getURL(void)
{
    return gPlayer->getDecoderHandler()->getUrl();
}
*/

void Decoder::setOutput(AudioOutput *o)
{
    lock();
    m_out = o;
    unlock();
}

void Decoder::error(const QString &e)
{
    auto *str = new QString(e.toUtf8());
    DecoderEvent ev(str);
    dispatch(ev);
}

// static methods
static QList<DecoderFactory*> *factories = nullptr;

static void checkFactories()
{
    if (!factories)
    {
        factories = new QList<DecoderFactory*>;

#ifdef HAVE_CDIO
        Decoder::registerFactory(new CdDecoderFactory);
#endif
        Decoder::registerFactory(new avfDecoderFactory);
    }
}

QStringList Decoder::all()
{
    checkFactories();

    QStringList l;

    for (const auto & factory : qAsConst(*factories))
        l += factory->description();

    return l;
}

bool Decoder::supports(const QString &source)
{
    checkFactories();

    for (const auto & factory : qAsConst(*factories))
    {
        if (factory->supports(source))
            return true;
    }

    return false;
}

void Decoder::registerFactory(DecoderFactory *fact)
{
    factories->push_back(fact);
}

Decoder *Decoder::create(const QString &source, AudioOutput *output, bool deletable)
{
    checkFactories();

    for (const auto & factory : qAsConst(*factories))
    {
        if (factory->supports(source))
            return factory->create(source, output, deletable);
    }

    return nullptr;
}
