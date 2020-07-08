/****************************************************************************
** Filename: unzip_p.h
** Last updated [dd/mm/yyyy]: 28/01/2007
**
** pkzip 2.0 decompression.
**
** Some of the code has been inspired by other open source projects,
** (mainly Info-Zip and Gilles Vollant's minizip).
** Compression and decompression actually uses the zlib library.
**
** Copyright (C) 2007-2008 Angius Fabrizio. All rights reserved.
**
** This file is part of the OSDaB project (http://osdab.sourceforge.net/).
**
** This file may be distributed and/or modified under the terms of the
** GNU General Public License version 2 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
** See the file LICENSE.GPL that came with this software distribution or
** visit http://www.gnu.org/copyleft/gpl.html for GPL licensing information.
**
**********************************************************************/

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Zip/UnZip API.  It exists purely as an
// implementation detail. This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#ifndef OSDAB_UNZIP_P_H
#define OSDAB_UNZIP_P_H

#include "unzip.h"
#include "zipentry_p.h"

#include <QtGlobal>

// zLib authors suggest using larger buffers (128K or 256K) for (de)compression (especially for inflate())
// we use a 256K buffer here - if you want to use this code on a pre-iceage mainframe please change it ;)
#define UNZIP_READ_BUFFER (256*1024)

class UnzipPrivate
{
public:
	UnzipPrivate();

	// Replace this with whatever else you use to store/retrieve the password.
	QString password;

	bool skipAllEncrypted {false};

	QMap<QString,ZipEntryP*>* headers {nullptr};

	QIODevice* device {nullptr};

	char buffer1[UNZIP_READ_BUFFER] {0};
	char buffer2[UNZIP_READ_BUFFER] {0};

	unsigned char* uBuffer;
	const quint32* crcTable;

	// Central Directory (CD) offset
	quint32 cdOffset {0};
	// End of Central Directory (EOCD) offset
	quint32 eocdOffset {0};

	// Number of entries in the Central Directory (as to the EOCD record)
	quint16 cdEntryCount {0};

	// The number of detected entries that have been skipped because of a non compatible format
	quint16 unsupportedEntryCount {0};

	QString comment;

	UnZip::ErrorCode openArchive(QIODevice* device);

	UnZip::ErrorCode seekToCentralDirectory();
	UnZip::ErrorCode parseCentralDirectoryRecord();
	UnZip::ErrorCode parseLocalHeaderRecord(const QString& path, ZipEntryP& entry);

	void closeArchive();

	UnZip::ErrorCode extractFile(const QString& path, ZipEntryP& entry, const QDir& dir, UnZip::ExtractionOptions options);
	UnZip::ErrorCode extractFile(const QString& path, ZipEntryP& entry, QIODevice* device, UnZip::ExtractionOptions options);

	UnZip::ErrorCode testPassword(quint32* keys, const QString& file, const ZipEntryP& header);
	bool testKeys(const ZipEntryP& header, quint32* keys);

	bool createDirectory(const QString& path);

	inline void decryptBytes(quint32* keys, char* buffer, qint64 read) const;

	static inline quint32 getULong(const unsigned char* data, quint32 offset) ;
	static inline quint64 getULLong(const unsigned char* data, quint32 offset) ;
	static inline quint16 getUShort(const unsigned char* data, quint32 offset) ;
	static inline int decryptByte(quint32 key2) ;
	inline void updateKeys(quint32* keys, int c) const;
	inline void initKeys(const QString& pwd, quint32* keys) const;

	static inline QDateTime convertDateTime(const unsigned char date[2], const unsigned char time[2]) ;
};

#endif // OSDAB_UNZIP_P_H
