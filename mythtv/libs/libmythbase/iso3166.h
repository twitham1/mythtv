
// -*- Mode: c++ -*-
#ifndef ISO_3166_1_H
#define ISO_3166_1_H

#include <QString>
#include <QMap>

#include "mythbaseexp.h"

/** \file iso3166.h
 *  \brief ISO 3166-1 support functions
 *
 *   ISO 3166-1 alpha-2 is the two letter standard for specifying a country.
 *   This is used by MythTV for locale support.
 *
 *   In many contexts, such as with translations, these country codes can
 *   be prefixed with a 2 digit ISO639 language code and an underscore.
 *
 *   \sa iso639.h
 */

using ISO3166ToNameMap = QMap<QString, QString>;

// WARNING: These functions are not thread-safe and sould only be
// called from the main UI thread.

MBASE_PUBLIC  ISO3166ToNameMap GetISO3166EnglishCountryMap(void);
MBASE_PUBLIC  QString GetISO3166EnglishCountryName(const QString &iso3166Code);
MBASE_PUBLIC  ISO3166ToNameMap GetISO3166CountryMap(void);
MBASE_PUBLIC  QString GetISO3166CountryName(const QString &iso3166Code);

#endif // ISO_3166_1_H
