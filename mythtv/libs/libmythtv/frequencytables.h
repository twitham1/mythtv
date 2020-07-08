// -*- Mode: c++ -*-

#ifndef FREQUENCY_TABLE_H
#define FREQUENCY_TABLE_H

// C++ includes
#include <list>
#include <utility>
#include <vector>
using namespace std;

// Qt includes
#include <QString>
#include <QMap>

// MythTV includes
#include "mythtvexp.h"
#include "dtvconfparser.h"
#include "dtvconfparserhelpers.h"
#include "iptvtuningdata.h"

class FrequencyTable;
class TransportScanItem;

using freq_table_map_t  = QMap<QString, const FrequencyTable*>;
using freq_table_list_t = vector<const FrequencyTable*>;

bool teardown_frequency_tables(void);

MTV_PUBLIC freq_table_list_t get_matching_freq_tables(
    const QString &format, const QString &modulation, const QString &country);

MTV_PUBLIC long long get_center_frequency(
    const QString& format, const QString& modulation, const QString& country, int freqid);

MTV_PUBLIC int get_closest_freqid(
    const QString& format, QString modulation, const QString& country, long long centerfreq);

class FrequencyTable
{
  public:
    FrequencyTable(QString                 _name_format,
                   int                     _name_offset,
                   uint64_t                _frequencyStart,
                   uint64_t                _frequencyEnd,
                   uint                    _frequencyStep,
                   DTVModulation::Types    _modulation)
        : m_nameFormat(std::move(_name_format)), m_nameOffset(_name_offset),
          m_frequencyStart(_frequencyStart), m_frequencyEnd(_frequencyEnd),
          m_frequencyStep(_frequencyStep),   m_modulation(_modulation) {}

    FrequencyTable(uint64_t                _frequencyStart,
                   uint64_t                _frequencyEnd,
                   uint                    _frequencyStep,
                   QString                 _name_format,
                   int                     _name_offset,
                   DTVInversion::Types     _inversion,
                   DTVBandwidth::Types     _bandwidth,
                   DTVCodeRate::Types      _coderate_hp,
                   DTVCodeRate::Types      _coderate_lp,
                   DTVModulation::Types    _constellation,
                   DTVTransmitMode::Types  _trans_mode,
                   DTVGuardInterval::Types _guard_interval,
                   DTVHierarchy::Types     _hierarchy,
                   DTVModulation::Types    _modulation,
                   int                     _offset1,
                   int                     _offset2)
        : m_nameFormat(std::move(_name_format)), m_nameOffset(_name_offset),
          m_frequencyStart(_frequencyStart), m_frequencyEnd(_frequencyEnd),
          m_frequencyStep(_frequencyStep),   m_modulation(_modulation),
          m_offset1(_offset1),               m_offset2(_offset2),
          m_inversion(_inversion),           m_bandwidth(_bandwidth),
          m_coderateHp(_coderate_hp),        m_coderateLp(_coderate_lp),
          m_constellation(_constellation),   m_transMode(_trans_mode),
          m_guardInterval(_guard_interval),  m_hierarchy(_hierarchy) {}

    FrequencyTable(uint64_t                _frequencyStart,
                   uint64_t                _frequencyEnd,
                   uint                    _frequencyStep,
                   QString                 _name_format,
                   int                     _name_offset,
                   DTVCodeRate::Types      _fec_inner,
                   DTVModulation::Types    _modulation,
                   uint                    _symbol_rate,
                   int                     _offset1,
                   int                     _offset2)
        : m_nameFormat(std::move(_name_format)), m_nameOffset(_name_offset),
          m_frequencyStart(_frequencyStart), m_frequencyEnd(_frequencyEnd),
          m_frequencyStep(_frequencyStep),   m_modulation(_modulation),
          m_offset1(_offset1),               m_offset2(_offset2),
          m_symbolRate(_symbol_rate),        m_fecInner(_fec_inner) { ; }

    virtual ~FrequencyTable() { ; }

    // Common Stuff
    QString           m_nameFormat;     ///< pretty name format
    int               m_nameOffset;    ///< Offset to add to the pretty name
    uint64_t          m_frequencyStart; ///< The staring centre frequency
    uint64_t          m_frequencyEnd;   ///< The ending centre frequency
    uint              m_frequencyStep;  ///< The step in frequency
    DTVModulation     m_modulation;
    int               m_offset1 {0};    ///< The first  offset from the centre freq
    int               m_offset2 {0};    ///< The second offset from the centre freq

    // DVB OFDM stuff
    DTVInversion      m_inversion;
    DTVBandwidth      m_bandwidth;
    DTVCodeRate       m_coderateHp;
    DTVCodeRate       m_coderateLp;
    DTVModulation     m_constellation;
    DTVTransmitMode   m_transMode;
    DTVGuardInterval  m_guardInterval;
    DTVHierarchy      m_hierarchy;

    // DVB-C/DVB-S stuff
    uint              m_symbolRate {0};
    DTVCodeRate       m_fecInner;
};

/**
 *  \brief Class used for doing a list of frequencies / transports.
 *
 *   This is used for ATSC/NA Digital Cable and also scan all transports.
 */
class TransportScanItem
{
  public:
    TransportScanItem();
    TransportScanItem(uint           _sourceid,
                      const QString &_si_std,
                      QString        _name,
                      uint           _mplexid,
                      uint           _timeoutTune);

    TransportScanItem(uint           _sourceid,
                      QString        _name,
                      DTVMultiplex  &_tuning,
                      uint           _timeoutTune);

    TransportScanItem(uint                _sourceid,
                      QString             _name,
                      DTVTunerType        _tuner_type,
                      const DTVTransport &_tuning,
                      uint                _timeoutTune);

    TransportScanItem(uint                _sourceid,
                      const QString      &_si_std,
                      QString strFmt,  /* fmt for info shown to user  */
                      uint freqNum,
                      uint frequency,         /* center frequency to use     */
                      const FrequencyTable &ft,  /* freq table to get info from */
                      uint                _timeoutTune);

    TransportScanItem(uint                  _sourceid,
                      QString               _name,
                      IPTVTuningData        _tuning,
                      QString               _channel,
                      uint                  _timeoutTune);

    uint offset_cnt() const
        { return (m_freqOffsets[2]) ? 3 : ((m_freqOffsets[1]) ? 2 : 1); }

    uint64_t freq_offset(uint i) const;

    QString toString() const;

  private:
    uint GetMultiplexIdFromDB(void) const;

  public:
    uint               m_mplexid     {UINT_MAX}; ///< DB Mplexid

    QString            m_friendlyName;        ///< Name to display in scanner dialog
    uint               m_friendlyNum {0};     ///< Frequency number (freqid w/freq table)
    int                m_sourceID    {0};     ///< Associated SourceID
    bool               m_useTimer    {false}; /**< Set if timer is used after
                                              lock for getting PAT */

    bool               m_scanning    {false}; ///< Probably Unnecessary
    std::array<int,3>  m_freqOffsets {0,0,0}; ///< Frequency offsets
    unsigned           m_timeoutTune {1000};  ///< Timeout to tune to a frequency

    DTVMultiplex       m_tuning;              ///< Tuning info
    IPTVTuningData     m_iptvTuning;          ///< IPTV Tuning info
    QString            m_iptvChannel;         ///< IPTV base channel

    DTVChannelInfoList m_expectedChannels;

    int                m_signalStrength {0};
    uint               m_networkID      {0};
    uint               m_transportID    {0};
};

class transport_scan_items_it_t
{
  public:
    transport_scan_items_it_t() = default;
    transport_scan_items_it_t(const list<TransportScanItem>::iterator it)
        : m_it(it) {}

    transport_scan_items_it_t& operator++()
    {
        m_offset++;
        if ((uint)m_offset >= (*m_it).offset_cnt())
        {
            ++m_it;
            m_offset = 0;
        }
        return *this;
    }
    transport_scan_items_it_t& operator--()
    {
        m_offset--;
        if (m_offset < 0)
        {
            --m_it;
            m_offset = (*m_it).offset_cnt() - 1;
        }
        return *this;
    }

    // cert-dcl21-cpp says this function should be const.
    // readability-const-return-type says it shouldn't.
    // NOLINTNEXTLINE(readability-const-return-type)
    const transport_scan_items_it_t operator++(int)
    {
        transport_scan_items_it_t tmp = *this;
        operator++();
        return tmp;
    }

    // cert-dcl21-cpp says this function should be const.
    // readability-const-return-type says it shouldn't.
    // NOLINTNEXTLINE(readability-const-return-type)
    const transport_scan_items_it_t operator--(int)
    {
        transport_scan_items_it_t tmp = *this;
        operator--();
        return tmp;
    }

    transport_scan_items_it_t& operator+=(int incr)
        { for (int i = 0; i < incr; i++) ++(*this); return *this; }
    transport_scan_items_it_t& operator-=(int incr)
        { for (int i = 0; i < incr; i++) --(*this); return *this; }


    const TransportScanItem& operator*() const { return *m_it; }
    TransportScanItem&       operator*()       { return *m_it; }

    list<TransportScanItem>::iterator iter() { return m_it; }
    list<TransportScanItem>::const_iterator iter() const { return m_it; }
    uint offset() const { return (uint) m_offset; }
    transport_scan_items_it_t nextTransport() const
    {
        list<TransportScanItem>::iterator tmp = m_it;
        return {++tmp};
    }

  private:
    list<TransportScanItem>::iterator m_it;
    int m_offset {0};

    friend bool operator==(const transport_scan_items_it_t&A,
                           const transport_scan_items_it_t&B);
    friend bool operator!=(const transport_scan_items_it_t&A,
                           const transport_scan_items_it_t&B);

    friend bool operator==(const transport_scan_items_it_t&A,
                           const list<TransportScanItem>::iterator&B);
};

inline bool operator==(const transport_scan_items_it_t& A,
                       const transport_scan_items_it_t& B)
{
    return (A.m_it == B.m_it) && (A.m_offset == B.m_offset);
}

inline bool operator!=(const transport_scan_items_it_t &A,
                       const transport_scan_items_it_t &B)
{
    return (A.m_it != B.m_it) || (A.m_offset != B.m_offset);
}

inline bool operator==(const transport_scan_items_it_t& A,
                       const list<TransportScanItem>::iterator& B)
{
    return (A.m_it == B) && (0 == A.offset());
}

using transport_scan_items_t = list<TransportScanItem>;

#endif // FREQUENCY_TABLE_H
