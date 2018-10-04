#include <QStringList>

#include "mythlogging.h"

#include "mythuibuttonlist.h"
#include "mythuitext.h"
#include "mythuibutton.h"
#include "videometadata.h"
#include "videoutils.h"

#include "videopopups.h"

CastDialog::CastDialog(MythScreenStack *lparent, VideoMetadata *metadata) :
    MythScreenType(lparent, "videocastpopup"), m_metadata(metadata)
{
}

bool CastDialog::Create()
{
    if (!LoadWindowFromXML("video-ui.xml", "castpopup", this))
        return false;

    MythUIButtonList *castList = nullptr;
    MythUIButton     *okButton = nullptr;

    bool err = false;
    UIUtilE::Assign(this, castList, "cast", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'castpopup'");
        return false;
    }

    UIUtilW::Assign(this, okButton, "ok");

    if (okButton)
        connect(okButton, SIGNAL(Clicked()), SLOT(Close()));

    QStringList cast = GetDisplayCast(*m_metadata);
    QStringListIterator castIterator(cast);
    while (castIterator.hasNext())
    {
        new MythUIButtonListItem(castList, castIterator.next());
    }

    BuildFocusList();

    return true;
}

/////////////////////////////////////////////////////////////

PlotDialog::PlotDialog(MythScreenStack *lparent, VideoMetadata *metadata) :
    MythScreenType(lparent, "videoplotpopup"), m_metadata(metadata)
{
}

bool PlotDialog::Create()
{
    if (!LoadWindowFromXML("video-ui.xml", "descriptionpopup", this))
        return false;

    MythUIText   *plotText = nullptr;
    MythUIButton *okButton = nullptr;

    bool err = false;
    UIUtilE::Assign(this, plotText, "description", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'descriptionpopup'");
        return false;
    }

    UIUtilW::Assign(this, okButton, "ok");

    plotText->SetText(m_metadata->GetPlot());

    if (okButton)
        connect(okButton, SIGNAL(Clicked()), SLOT(Close()));

    BuildFocusList();

    return true;
}
