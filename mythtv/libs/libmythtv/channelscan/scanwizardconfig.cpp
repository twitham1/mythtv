#include "scanwizardconfig.h"

#include "videosource.h"
#include "cardutil.h"
#include "frequencytablesetting.h"

#include "channelscanmiscsettings.h"
#include "inputselectorsetting.h"
#include "scanwizard.h"

#include "panedvbs.h"
#include "panedvbs2.h"
#include "panedvbc.h"
#include "panedvbt.h"
#include "panedvbt2.h"
#include "paneatsc.h"
#include "paneanalog.h"
#include "panesingle.h"
#include "paneall.h"
#include "panedvbutilsimport.h"
#include "paneexistingscanimport.h"

void ScanWizard::SetupConfig(
    uint    default_sourceid,  uint default_cardid,
    const QString& default_inputname)
{
    m_videoSource = new VideoSourceSelector(
                    default_sourceid, CardUtil::GetScanableInputTypes(), false);
    m_input = new InputSelector(default_cardid, default_inputname);
    m_scanType = new ScanTypeSetting(),
    m_scanConfig = new ScanOptionalConfig(m_scanType);
    m_services = new DesiredServices();
    m_ftaOnly = new FreeToAirOnly();
    m_lcnOnly = new ChannelNumbersOnly();
    m_completeOnly = new CompleteChannelsOnly();
    m_addFullTS = new AddFullTS();
    m_trustEncSI = new TrustEncSISetting();

    setLabel(tr("Channel Scan"));

    addChild(m_services);
    addChild(m_ftaOnly);
    addChild(m_lcnOnly);
    addChild(m_completeOnly);
    addChild(m_addFullTS);
    addChild(m_trustEncSI);

    addChild(m_videoSource);
    addChild(m_input);
    addChild(m_scanType);
    addChild(m_scanConfig);

    connect(m_videoSource, SIGNAL(valueChanged(const QString&)),
            m_scanConfig,  SLOT(  SetSourceID( const QString&)));

    connect(m_videoSource, SIGNAL(valueChanged(const QString&)),
            m_input,       SLOT(  SetSourceID( const QString&)));

    connect(m_input,       SIGNAL(valueChanged(const QString&)),
            m_scanType,    SLOT(  SetInput(    const QString&)));

    connect(m_input,       SIGNAL(valueChanged(const QString&)),
            this,          SLOT(  SetInput(    const QString&)));
}

uint ScanWizard::GetSourceID(void) const
{
    return m_videoSource->getValue().toUInt();
}

ServiceRequirements ScanWizard::GetServiceRequirements(void) const
{
    return m_services->GetServiceRequirements();
}

bool ScanWizard::DoFreeToAirOnly(void) const
{
    return m_ftaOnly->boolValue();
}

bool ScanWizard::DoChannelNumbersOnly(void) const
{
    return m_lcnOnly->boolValue();
}

bool ScanWizard::DoCompleteChannelsOnly(void) const
{
    return m_completeOnly->boolValue();
}

bool ScanWizard::DoAddFullTS(void) const
{
    return m_addFullTS->boolValue();
}

bool ScanWizard::DoTestDecryption(void) const
{
    return m_trustEncSI->boolValue();
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

void ScanTypeSetting::SetInput(const QString &cardids_inputname)
{
    uint    cardid    = 0;
    QString inputname;
    if (!InputSelector::Parse(cardids_inputname, cardid, inputname))
        return;

    // Only refresh if we really have to. If we do it too often
    // Then we end up fighting the scan routine when we want to
    // check the type of dvb card :/
    if (cardid == m_hw_cardid)
        return;

    m_hw_cardid     = cardid;
    QString subtype = CardUtil::ProbeSubTypeName(m_hw_cardid);
    int nCardType   = CardUtil::toInputType(subtype);
    clearSelections();

    switch (nCardType)
    {
        case CardUtil::V4L:
        case CardUtil::MPEG:
            addSelection(tr("Full Scan"),
                         QString::number(FullScan_Analog), true);
            addSelection(tr("Import existing scan"),
                         QString::number(ExistingScanImport));
            return;
        case CardUtil::DVBT:
            addSelection(tr("Full Scan"),
                         QString::number(FullScan_DVBT), true);
            addSelection(tr("Full Scan (Tuned)"),
                         QString::number(NITAddScan_DVBT));
//             addSelection(tr("Import channels.conf"),
//                          QString::number(DVBUtilsImport));
            addSelection(tr("Import existing scan"),
                         QString::number(ExistingScanImport));
            break;
        case CardUtil::DVBT2:
            addSelection(tr("Full Scan"),
                         QString::number(FullScan_DVBT2), true);
            addSelection(tr("Full Scan (Tuned)"),
                         QString::number(NITAddScan_DVBT2));
//             addSelection(tr("Import channels.conf"),
//                          QString::number(DVBUtilsImport));
            addSelection(tr("Import existing scan"),
                         QString::number(ExistingScanImport));
            break;
        case CardUtil::DVBS:
            addSelection(tr("Full Scan (Tuned)"),
                         QString::number(NITAddScan_DVBS));
//             addSelection(tr("Import channels.conf"),
//                          QString::number(DVBUtilsImport));
            addSelection(tr("Import existing scan"),
                         QString::number(ExistingScanImport));
            break;
        case CardUtil::DVBS2:
            addSelection(tr("Full Scan (Tuned)"),
                         QString::number(NITAddScan_DVBS2));
//             addSelection(tr("Import channels.conf"),
//                          QString::number(DVBUtilsImport));
            addSelection(tr("Import existing scan"),
                         QString::number(ExistingScanImport));
            break;
        case CardUtil::QAM:
            addSelection(tr("Full Scan (Tuned)"),
                         QString::number(NITAddScan_DVBC));
            addSelection(tr("Full Scan"),
                         QString::number(FullScan_DVBC));
//             addSelection(tr("Import channels.conf"),
//                          QString::number(DVBUtilsImport));
            addSelection(tr("Import existing scan"),
                         QString::number(ExistingScanImport));
            break;
        case CardUtil::ATSC:
            addSelection(tr("Full Scan"),
                         QString::number(FullScan_ATSC), true);
//             addSelection(tr("Import channels.conf"),
//                          QString::number(DVBUtilsImport));
            addSelection(tr("Import existing scan"),
                         QString::number(ExistingScanImport));
            break;
        case CardUtil::HDHOMERUN:
            if (CardUtil::HDHRdoesDVB(CardUtil::GetVideoDevice(cardid)))
            {
                addSelection(tr("Full Scan"),
                             QString::number(FullScan_DVBT), true);
                addSelection(tr("Full Scan (Tuned)"),
                             QString::number(NITAddScan_DVBT));
            }
            else
                addSelection(tr("Full Scan"),
                             QString::number(FullScan_ATSC), true);
//             addSelection(tr("Import channels.conf"),
//                          QString::number(DVBUtilsImport));
            addSelection(tr("Import existing scan"),
                         QString::number(ExistingScanImport));
            break;
        case CardUtil::VBOX:
            addSelection(tr("VBox Channel Import"),
                         QString::number(VBoxImport), true);
            return;
        case CardUtil::FREEBOX:
            addSelection(tr("M3U Import with MPTS"),
                         QString::number(IPTVImportMPTS), true);
            addSelection(tr("M3U Import"),
                         QString::number(IPTVImport), true);
            return;
        case CardUtil::ASI:
            addSelection(tr("ASI Scan"),
                         QString::number(CurrentTransportScan), true);
            return;
        case CardUtil::EXTERNAL:
            addSelection(tr("MPTS Scan"),
                         QString::number(CurrentTransportScan), true);
            addSelection(tr("Import from ExternalRecorder"),
                         QString::number(ExternRecImport), true);
            return;
        case CardUtil::ERROR_PROBE:
            addSelection(tr("Failed to probe the card"),
                         QString::number(Error_Probe), true);
            return;
        default:
            addSelection(tr("Failed to open the card"),
                         QString::number(Error_Open), true);
            return;
    }

    addSelection(tr("Scan of all existing transports"),
                 QString::number(FullTransportScan));
    addSelection(tr("Scan of single existing transport"),
                 QString::number(TransportScan));
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

ScanOptionalConfig::ScanOptionalConfig(ScanTypeSetting *_scan_type) :
    m_scanType(_scan_type),
    m_dvbTCountry(new ScanCountry()),
    m_dvbT2Country(new ScanCountry()),
    m_network(new ScanNetwork()),
    m_paneDVBT(new PaneDVBT(QString::number(ScanTypeSetting::NITAddScan_DVBT), m_scanType)),
    m_paneDVBT2(new PaneDVBT2(QString::number(ScanTypeSetting::NITAddScan_DVBT2), m_scanType)),
    m_paneDVBS(new PaneDVBS(QString::number(ScanTypeSetting::NITAddScan_DVBS), m_scanType)),
    m_paneDVBS2(new PaneDVBS2(QString::number(ScanTypeSetting::NITAddScan_DVBS2), m_scanType)),
    m_paneATSC(new PaneATSC(QString::number(ScanTypeSetting::FullScan_ATSC), m_scanType)),
    m_paneDVBC(new PaneDVBC(QString::number(ScanTypeSetting::NITAddScan_DVBC), m_scanType)),
    m_paneAnalog(new PaneAnalog(QString::number(ScanTypeSetting::FullScan_Analog), m_scanType)),
    m_paneSingle(new PaneSingle(QString::number(ScanTypeSetting::TransportScan), m_scanType)),
    m_paneAll(new PaneAll(QString::number(ScanTypeSetting::FullTransportScan), m_scanType)),
    m_paneDVBUtilsImport(new PaneDVBUtilsImport()),
    m_paneExistingScanImport(new PaneExistingScanImport(QString::number(ScanTypeSetting::ExistingScanImport), m_scanType))
{
    setVisible(false);

    m_scanType->addTargetedChild(QString::number(ScanTypeSetting::Error_Open),
              new PaneError(tr("Failed to open the card")));
    m_scanType->addTargetedChild(QString::number(ScanTypeSetting::Error_Probe),
              new PaneError(tr("Failed to probe the card")));
    m_scanType->addTargetedChild(QString::number(ScanTypeSetting::FullScan_DVBC),
              m_network);
    m_scanType->addTargetedChild(QString::number(ScanTypeSetting::FullScan_DVBT),
              m_dvbTCountry);
    m_scanType->addTargetedChild(QString::number(ScanTypeSetting::FullScan_DVBT2),
              m_dvbT2Country);
//   m_scanType->addTargetedChild(QString::number(ScanTypeSetting::DVBUtilsImport),
//               paneDVBUtilsImport);
}

void ScanOptionalConfig::SetSourceID(const QString &sourceid)
{
    m_paneAnalog->SetSourceID(sourceid.toUInt());
    m_paneSingle->SetSourceID(sourceid.toUInt());
    m_paneExistingScanImport->SetSourceID(sourceid.toUInt());
}

QString ScanOptionalConfig::GetFrequencyStandard(void) const
{
    int     st =  m_scanType->getValue().toInt();

    switch (st)
    {
        case ScanTypeSetting::FullScan_ATSC:
            return "atsc";
        case ScanTypeSetting::FullScan_DVBC:
            return "dvbc";
        case ScanTypeSetting::FullScan_DVBT:
        case ScanTypeSetting::FullScan_DVBT2:
            return "dvbt";
        case ScanTypeSetting::FullScan_Analog:
            return "analog";
        default:
            return "unknown";
    }
}

QString ScanOptionalConfig::GetModulation(void) const
{
    int     st =  m_scanType->getValue().toInt();

    switch (st)
    {
        case ScanTypeSetting::FullScan_ATSC:
            return m_paneATSC->GetModulation();
        case ScanTypeSetting::FullScan_DVBC:
            return "qam";
        case ScanTypeSetting::FullScan_DVBT:
        case ScanTypeSetting::FullScan_DVBT2:
            return "ofdm";
        case ScanTypeSetting::FullScan_Analog:
            return "analog";
        default:
            return "unknown";
    }
}

QString ScanOptionalConfig::GetFrequencyTable(void) const
{
    int st =  m_scanType->getValue().toInt();

    switch (st)
    {
        case ScanTypeSetting::FullScan_ATSC:
            return m_paneATSC->GetFrequencyTable();
        case ScanTypeSetting::FullScan_DVBC:
            return m_network->getValue();
        case ScanTypeSetting::FullScan_DVBT:
            return m_dvbTCountry->getValue();
        case ScanTypeSetting::FullScan_DVBT2:
            return m_dvbT2Country->getValue();
        case ScanTypeSetting::FullScan_Analog:
            return m_paneAnalog->GetFrequencyTable();
        default:
            return "unknown";
    }
}

bool ScanOptionalConfig::GetFrequencyTableRange(
    QString &start, QString &end) const
{
    start.clear();
    end.clear();

    int st = m_scanType->getValue().toInt();
    if (ScanTypeSetting::FullScan_ATSC == st)
        return m_paneATSC->GetFrequencyTableRange(start, end);

    return false;
}

bool ScanOptionalConfig::DoIgnoreSignalTimeout(void) const
{
    int  st  = m_scanType->getValue().toInt();

    switch (st)
    {
    case ScanTypeSetting::TransportScan:
        return m_paneSingle->ignoreSignalTimeout();
    case ScanTypeSetting::FullTransportScan:
        return m_paneAll->ignoreSignalTimeout();
    case ScanTypeSetting::DVBUtilsImport:
        return m_paneDVBUtilsImport->DoIgnoreSignalTimeout();
    default:
        return false;
    }
}

bool ScanOptionalConfig::DoFollowNIT(void) const
{
    int  st  = m_scanType->getValue().toInt();
    switch (st)
    {
    case ScanTypeSetting::TransportScan:
        return m_paneSingle->GetFollowNIT();
    case ScanTypeSetting::FullTransportScan:
        return m_paneAll->GetFollowNIT();
    default:
        return false;
    }
}

QString ScanOptionalConfig::GetFilename(void) const
{
    return m_paneDVBUtilsImport->GetFilename();
}

uint ScanOptionalConfig::GetMultiplex(void) const
{
    int mplexid = m_paneSingle->GetMultiplex();
    return (mplexid <= 0) ? 0 : mplexid;
}

uint ScanOptionalConfig::GetScanID(void) const
{
    return m_paneExistingScanImport->GetScanID();
}

QMap<QString,QString> ScanOptionalConfig::GetStartChan(void) const
{
    QMap<QString,QString> startChan;

    int st = m_scanType->getValue().toInt();
    if (ScanTypeSetting::NITAddScan_DVBT == st)
    {
        const PaneDVBT *pane = m_paneDVBT;

        startChan["std"]            = "dvb";
        startChan["type"]           = "OFDM";
        startChan["frequency"]      = pane->frequency();
        startChan["inversion"]      = pane->inversion();
        startChan["bandwidth"]      = pane->bandwidth();
        startChan["coderate_hp"]    = pane->coderate_hp();
        startChan["coderate_lp"]    = pane->coderate_lp();
        startChan["constellation"]  = pane->constellation();
        startChan["trans_mode"]     = pane->trans_mode();
        startChan["guard_interval"] = pane->guard_interval();
        startChan["hierarchy"]      = pane->hierarchy();
    }
    else if (ScanTypeSetting::NITAddScan_DVBT2 == st)
    {
        const PaneDVBT2 *pane = m_paneDVBT2;

        startChan["std"]            = "dvb";
        startChan["type"]           = "DVB_T2";
        startChan["frequency"]      = pane->frequency();
        startChan["inversion"]      = pane->inversion();
        startChan["bandwidth"]      = pane->bandwidth();
        startChan["coderate_hp"]    = pane->coderate_hp();
        startChan["coderate_lp"]    = pane->coderate_lp();
        startChan["constellation"]  = pane->constellation();
        startChan["trans_mode"]     = pane->trans_mode();
        startChan["guard_interval"] = pane->guard_interval();
        startChan["hierarchy"]      = pane->hierarchy();
        startChan["mod_sys"]        = pane->mod_sys();
    }
    else if (ScanTypeSetting::NITAddScan_DVBS == st)
    {
        const PaneDVBS *pane = m_paneDVBS;

        startChan["std"]        = "dvb";
        startChan["type"]       = "QPSK";
        startChan["modulation"] = "qpsk";
        startChan["frequency"]  = pane->frequency();
        startChan["inversion"]  = pane->inversion();
        startChan["symbolrate"] = pane->symbolrate();
        startChan["fec"]        = pane->fec();
        startChan["polarity"]   = pane->polarity();
    }
    else if (ScanTypeSetting::NITAddScan_DVBC == st)
    {
        const PaneDVBC *pane = m_paneDVBC;

        startChan["std"]        = "dvb";
        startChan["type"]       = "QAM";
        startChan["frequency"]  = pane->frequency();
        startChan["inversion"]  = pane->inversion();
        startChan["symbolrate"] = pane->symbolrate();
        startChan["fec"]        = pane->fec();
        startChan["modulation"] = pane->modulation();
    }
    else if (ScanTypeSetting::NITAddScan_DVBS2 == st)
    {
        const PaneDVBS2 *pane = m_paneDVBS2;

        startChan["std"]        = "dvb";
        startChan["type"]       = "DVB_S2";
        startChan["frequency"]  = pane->frequency();
        startChan["inversion"]  = pane->inversion();
        startChan["symbolrate"] = pane->symbolrate();
        startChan["fec"]        = pane->fec();
        startChan["modulation"] = pane->modulation();
        startChan["polarity"]   = pane->polarity();
        startChan["mod_sys"]    = pane->mod_sys();
        startChan["rolloff"]    = pane->rolloff();
    }

    return startChan;
}
