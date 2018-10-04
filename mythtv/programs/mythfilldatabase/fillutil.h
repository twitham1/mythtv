#ifndef _FILLUTIL_H_
#define _FILLUTIL_H_

// Std C headers
#include <cstdio>

// Qt headers
#include <QString>

class QFile;

bool dash_open(QFile &file, const QString &filename,
               int m, FILE *handle = nullptr);

QString SetupIconCacheDirectory(void);

#endif // _FILLUTIL_H_
