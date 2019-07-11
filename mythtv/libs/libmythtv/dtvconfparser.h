/* -*- Mode: c++ -*-
 * vim: set expandtab tabstop=4 shiftwidth=4:
 *
 * Original Project
 *      MythTV      http://www.mythtv.org
 *
 * Author(s):
 *      John Pullan  (john@pullan.org)
 *
 * Description:
 *     Collection of classes to provide dvb channel scanning
 *     functionallity
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 *
 */

#ifndef _DTVCONFPARSER_H_
#define _DTVCONFPARSER_H_

// C++ headers
#include <cstdint>
#include <unistd.h>
#include <vector>
using namespace std;

// Qt headers
#include <QString>

// MythTV headers
#include "dtvmultiplex.h"

class QStringList;

class DTVChannelInfo
{
  public:
    DTVChannelInfo() = default;
    QString toString() const;

 public:
    QString m_name;
    uint    m_serviceid {0};
    int     m_lcn       {-1};
};
typedef vector<DTVChannelInfo> DTVChannelInfoList;

class DTVTransport : public DTVMultiplex
{
  public:
    explicit DTVTransport(const DTVMultiplex &other) : DTVMultiplex(other) { }

  public:
    DTVChannelInfoList channels;
};
typedef vector<DTVTransport> DTVChannelList;

/** \class DTVConfParser
 *  \brief Parses dvb-utils channel scanner output files.
 */
class DTVConfParser
{
  public:
    enum return_t   { ERROR_CARDTYPE, ERROR_OPEN, ERROR_PARSE, OK };
    enum cardtype_t { ATSC, OFDM, QPSK, QAM, DVBS2, UNKNOWN };

    DTVConfParser(enum cardtype_t type, uint sourceid, const QString &file)
        : m_type(type), m_sourceid(sourceid), m_filename(file) {}
    virtual ~DTVConfParser() = default;

    return_t Parse(void);

    DTVChannelList GetChannels(void) const { return m_channels; }

  private:
    bool ParseVDR(     const QStringList &tokens, int channelNo = -1);
    bool ParseConf(    const QStringList &tokens);
    bool ParseConfOFDM(const QStringList &tokens);
    bool ParseConfQPSK(const QStringList &tokens);
    bool ParseConfQAM( const QStringList &tokens);
    bool ParseConfATSC(const QStringList &tokens);

  private:
    cardtype_t     m_type;
    uint           m_sourceid;
    QString        m_filename;

    void AddChannel(const DTVMultiplex &mux, DTVChannelInfo &chan);

    DTVChannelList m_channels;
};

#endif // _DTVCONFPARSER_H_
