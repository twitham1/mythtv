
// myth
#include <mythmainwindow.h>
#include <mythdate.h>
#include <mythcontext.h>
#include <mythdbcon.h>

// mythbrowser
#include "bookmarkeditor.h"
#include "bookmarkmanager.h"
#include "browserdbutil.h"

/** \brief Creates a new BookmarkEditor Screen
 *  \param site   The bookmark we are adding/editing
 *  \param edit   If true we are editing an existing bookmark
 *  \param parent Pointer to the screen stack
 *  \param name   The name of the window
 */
BookmarkEditor::BookmarkEditor(Bookmark *site, bool edit,
                               MythScreenStack *parent, const char *name)
    : MythScreenType (parent, name),
      m_site(site),                  m_siteName(""),
      m_siteCategory(),              m_editing(edit),
      m_titleText(nullptr),          m_categoryEdit(nullptr),
      m_nameEdit(nullptr),           m_urlEdit(nullptr),
      m_isHomepage(nullptr),
      m_okButton(nullptr),           m_cancelButton(nullptr),
      m_findCategoryButton(nullptr), m_searchDialog(nullptr)
{
    if (m_editing)
    {
        m_siteCategory = m_site->category;
        m_siteName = m_site->name;
    }
}

bool BookmarkEditor::Create()
{
    if (!LoadWindowFromXML("browser-ui.xml", "bookmarkeditor", this))
        return false;

    bool err = false;

    UIUtilW::Assign(this, m_titleText, "title",  &err);
    UIUtilE::Assign(this, m_categoryEdit, "category", &err);
    UIUtilE::Assign(this, m_nameEdit, "name", &err);
    UIUtilE::Assign(this, m_urlEdit, "url", &err);
    UIUtilE::Assign(this, m_isHomepage, "homepage", &err);
    UIUtilE::Assign(this, m_okButton, "ok", &err);
    UIUtilE::Assign(this, m_cancelButton, "cancel", &err);
    UIUtilE::Assign(this, m_findCategoryButton, "findcategory", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'bookmarkeditor'");
        return false;
    }

    if (m_titleText)
    {
      if (m_editing)
          m_titleText->SetText(tr("Edit Bookmark Details"));
      else
          m_titleText->SetText(tr("Enter Bookmark Details"));
    }

    connect(m_okButton, SIGNAL(Clicked()), this, SLOT(Save()));
    connect(m_cancelButton, SIGNAL(Clicked()), this, SLOT(Exit()));
    connect(m_findCategoryButton, SIGNAL(Clicked()), this, SLOT(slotFindCategory()));

    if (m_editing && m_site)
    {
        m_categoryEdit->SetText(m_site->category);
        m_nameEdit->SetText(m_site->name);
        m_urlEdit->SetText(m_site->url);

        if (m_site->isHomepage)
            m_isHomepage->SetCheckState(MythUIStateType::Full);
    }

    BuildFocusList();

    SetFocusWidget(m_categoryEdit);

    return true;
}

bool BookmarkEditor::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("News", event, actions);

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void BookmarkEditor::Exit()
{
    Close();
}

void BookmarkEditor::Save()
{
    if (m_editing && m_siteCategory != "" && m_siteName != "")
        RemoveFromDB(m_siteCategory, m_siteName);

    ResetHomepageFromDB();

    bool isHomepage = (m_isHomepage->GetCheckState() == MythUIStateType::Full) ? true : false;
    InsertInDB(m_categoryEdit->GetText(), m_nameEdit->GetText(), m_urlEdit->GetText(), isHomepage );
    
    if (m_site)
    {
        m_site->category = m_categoryEdit->GetText();
        m_site->name = m_nameEdit->GetText();
        m_site->url = m_urlEdit->GetText();
        m_site->isHomepage = isHomepage;
    }

    Exit();
}

void BookmarkEditor::slotFindCategory(void)
{
    QStringList list;

    GetCategoryList(list);

    QString title = tr("Select a category");

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    m_searchDialog = new MythUISearchDialog(popupStack, title, list,
                                            true, m_categoryEdit->GetText());

    if (!m_searchDialog->Create())
    {
        delete m_searchDialog;
        m_searchDialog = nullptr;
        return;
    }

    connect(m_searchDialog, SIGNAL(haveResult(QString)), SLOT(slotCategoryFound(QString)));

    popupStack->AddScreen(m_searchDialog);
}

void BookmarkEditor::slotCategoryFound(QString category)
{
    m_categoryEdit->SetText(category);
}
