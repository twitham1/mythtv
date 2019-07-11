/*
    MP3 encoding support using liblame for MythMusic

    (c) 2003 Stefan Frank
    
    Please send an e-mail to sfr@gmx.net if you have
    questions or comments.

    Project Website: out http://www.mythtv.org/

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
*/

// qt
#include <QApplication>
#include <QString>

// myth
#include <mythcontext.h>
#include <mythlogging.h>
#include "musicmetadata.h"
#include "metaioid3.h"

// mythmusic
#include "lameencoder.h"

// c++
#include <iostream>
using namespace std;

static int write_buffer(char *buf, int bufsize, FILE *fp)
{
    return fwrite(buf, 1, bufsize, fp);
}

void LameEncoder::init_id3tags(lame_global_flags *gf)
{
    // Very basic ID3v2 Header. Rewritten later, but libid3tag.
    id3tag_init(gf);

    // Dummy tag. It'll get overwritten later by the more flexible
    // libid3 (MAD). Unf. it wont write the id3 tag to the file if
    // none exists, hense this dummy entry.
    id3tag_set_title(gf, "Title");

    // write v2 tags.
    id3tag_v2_only(gf);
}

int LameEncoder::init_encoder(lame_global_flags *gf, int quality, bool vbr)
{
    int lameret = 0;
    int meanbitrate = 128;
    int preset = STANDARD;

    switch (quality)
    {
        case 0:                         // low, always use CBR
            meanbitrate = 128;
            vbr = false;
            break;
        case 1:                         // medium
            meanbitrate = 192;
            break;
        case 2:                         // high
            meanbitrate = 256;
            preset = EXTREME;
            break;
    }

    if (vbr)
        lame_set_preset(gf, preset);
    else
    {
        lame_set_preset(gf, meanbitrate);
        lame_set_VBR(gf, vbr_off);
    }

    if (m_channels == 1)
    {
        lame_set_mode(gf, MONO);
    }

    lameret = lame_init_params(gf);

    return lameret;
}

LameEncoder::LameEncoder(const QString &outfile, int qualitylevel,
                         MusicMetadata *metadata, bool vbr) :
    Encoder(outfile, qualitylevel, metadata),
    m_mp3buf(new char[m_mp3buf_size]),
    m_gf(lame_init())
{
    init_id3tags(m_gf);

    int lameret = init_encoder(m_gf, qualitylevel, vbr);
    if (lameret < 0)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Error initializing LAME encoder. Got return code: %1")
            .arg(lameret));
        return;
    }
}

LameEncoder::~LameEncoder()
{
    LameEncoder::addSamples(nullptr, 0); //flush

    if (m_gf && m_out)
        lame_mp3_tags_fid (m_gf, m_out);
    if (m_gf)
        lame_close(m_gf);
    delete[] m_mp3buf;

    // Need to close the file here.
    if (m_out)
    {
        fclose(m_out);

        // Make sure the base class doesn't do a double clear.
        m_out = nullptr;
    }

    // Now write the Metadata
    if (m_metadata)
        MetaIOID3().write(m_outfile, m_metadata);
}

int LameEncoder::addSamples(int16_t * bytes, unsigned int length)
{
    int lameret = 0;

    m_samples_per_channel = length / m_bytes_per_sample;

    if (length > 0)
    {
        lameret = lame_encode_buffer_interleaved(m_gf, bytes,
                                                 m_samples_per_channel,
                                                 (unsigned char *)m_mp3buf,
                                                 m_mp3buf_size);
    }
    else
    {
        lameret = lame_encode_flush(m_gf, (unsigned char *)m_mp3buf,
                                          m_mp3buf_size);
    }

    if (lameret < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("LAME encoder error."));
    } 
    else if (lameret > 0 && m_out)
    {
        if (write_buffer(m_mp3buf, lameret, m_out) != lameret)
        {
            LOG(VB_GENERAL, LOG_ERR, "Failed to write mp3 data. Aborting.");
            return EENCODEERROR;
        }
    }

    return 0;
}

