// -*- Mode: c++ -*-
// vim: set expandtab tabstop=4 shiftwidth=4

#include "mythsorthelper.h"
#include "mythcorecontext.h"

/**
 *  \brief Common code for creating a MythSortHelper object.
 *
 *  Given an object with the three user specified parameters
 *  initialized, this function initializes the rest of the object.
 */
void MythSortHelper::MythSortHelperCommon(void)
{
    m_prefixes = tr("^(The |A |An )",
                    "Regular Expression for what to ignore when sorting");
    m_prefixes = m_prefixes.trimmed();
    if (not hasPrefixes()) {
        // This language doesn't ignore any words when sorting
        m_prefix_mode = SortPrefixKeep;
        return;
    }
    if (not m_prefixes.startsWith("^"))
        m_prefixes = "^" + m_prefixes;

    if (m_case_sensitive == Qt::CaseInsensitive)
    {
        m_prefixes = m_prefixes.toLower();
        m_exclusions = m_exclusions.toLower();
    }
    m_prefixes_regex = QRegularExpression(m_prefixes);
    m_prefixes_regex2 = QRegularExpression(m_prefixes + "(.*)");
    m_excl_list = m_exclusions.split(";", QString::SkipEmptyParts);
    for (int i = 0; i < m_excl_list.size(); i++)
      m_excl_list[i] = m_excl_list[i].trimmed();
}

/**
 *  \brief Create a MythSortHelper object.
 *
 *  Create the object and read settings from the core context.
 *
 *  \note This handles the case where the gCoreContext object doesn't
 *  exists (i.e. running under the Qt test harness) which allows this
 *  code and the objects using it to be tested.
 */
MythSortHelper::MythSortHelper()
{
    if (gCoreContext) {
#if 0
        // Last minute change.  QStringRef::localeAwareCompare appears to
        // always do case insensitive sorting, so there's no point in
        // presenting this option to a user.
        m_case_sensitive =
            gCoreContext->GetBoolSetting("SortCaseSensitive", false)
            ? Qt::CaseSensitive : Qt::CaseInsensitive;
#endif
        m_prefix_mode =
            gCoreContext->GetBoolSetting("SortStripPrefixes", true)
            ? SortPrefixRemove : SortPrefixKeep;
        m_exclusions =
            gCoreContext->GetSetting("SortPrefixExceptions", "");
    }
    MythSortHelperCommon();
}

/**
 *  \brief Fully specify the creation of a MythSortHelper object.
 *
 *  This function creates a MythSortHelper object based on the
 *  parameters provided.  It does not attempt to retrieve the user's
 *  preferred preferences from the database.
 *
 *  \warning This function should never be called directly.  It exists
 *  solely for use in test code.
 *
 *  \return A pointer to the MythSortHelper singleton.
 */
MythSortHelper::MythSortHelper(
    Qt::CaseSensitivity case_sensitive,
    SortPrefixMode prefix_mode,
    const QString &exclusions) :
    m_case_sensitive(case_sensitive),
    m_prefix_mode(prefix_mode),
    m_exclusions(exclusions)
{
    MythSortHelperCommon();
}

/**
 *  \brief Copy a MythSortHelper object.
 *
 *  This function is required for the shared pointer code to work.
 *
 *  \warning This function should never be called directly.
 *
 *  \return A new MythSortHelper object.
 */
MythSortHelper::MythSortHelper(MythSortHelper *other)
{
    m_case_sensitive  = other->m_case_sensitive;
    m_prefix_mode     = other->m_prefix_mode;
    m_prefixes        = other->m_prefixes;
    m_prefixes_regex  = other->m_prefixes_regex;
    m_prefixes_regex2 = other->m_prefixes_regex2;
    m_exclusions      = other->m_exclusions;
    m_excl_list       = other->m_excl_list;
    m_excl_mode       = other->m_excl_mode;
}

static std::shared_ptr<MythSortHelper> singleton = nullptr;

/**
 *  \brief Get a pointer to the MythSortHelper singleton.
 *
 *  This function creates the MythSortHelper singleton, and returns a
 *  shared pointer to it.
 *
 *  \return A pointer to the MythSortHelper singleton.
 */
std::shared_ptr<MythSortHelper> getMythSortHelper(void)
{
    if (singleton == nullptr)
        singleton = std::make_shared<MythSortHelper>(new MythSortHelper());
    return singleton;
}

/**
 *  \brief Delete the MythSortHelper singleton.
 *
 *  This function deletes the pointer to the MythSortHelper singleton.
 *  This will force the next call to get the singleton to create a new
 *  one, which will pick up any modified parameters.  The old one will
 *  be deleted when the last reference goes away (which probably
 *  happens with this assignment).
 */
void resetMythSortHelper(void)
{
    singleton = nullptr;
}

/**
 *  \brief Create the sortable form of an title string.
 *
 *  This function converts a title string to a version that can be
 *  used for sorting. Depending upon user settings, it may ignore the
 *  case of the string (by forcing all strings to lower case) and may
 *  strip the common prefix words "A", "An" and "The" from the
 *  beginning of the string.
 *
 * \param title The title of an item.
 * \return The conversion of the title to use when sorting.
 */
QString MythSortHelper::doTitle(const QString& title) const
{
    QString ltitle = title;
    if (m_case_sensitive == Qt::CaseInsensitive)
        ltitle = ltitle.toLower();
    if (m_prefix_mode == SortPrefixKeep)
	return ltitle;
    if (m_excl_mode == SortExclusionMatch)
    {
        if (m_excl_list.contains(ltitle))
            return ltitle;
    } else {
        foreach (const QString &str, m_excl_list)
            if (ltitle.startsWith(str))
                return ltitle;
    }
    if (m_prefix_mode == SortPrefixRemove)
        return ltitle.remove(m_prefixes_regex);
    return ltitle.replace(m_prefixes_regex2, "\\2, \\1").trimmed();
}

/**
 *  \brief Create the sortable form of an item.
 *
 *  This function converts a pathname to a version that can be used
 *  for sorting. Depending upon user settings, it may ignore the case
 *  of the string (by forcing all strings to lower case) and may strip
 *  the common prefix words "A", "An" and "The" from the beginning of
 *  each component in the pathname.
 *
 * \param pathname The pathname of an item.
 * \return The conversion of the pathname to use when sorting.
 */
QString MythSortHelper::doPathname(const QString& pathname) const
{
    QString lpathname = pathname;
    if (m_case_sensitive == Qt::CaseInsensitive)
        lpathname = lpathname.toLower();
    if (m_prefix_mode == SortPrefixKeep)
	return lpathname;
    QStringList parts = lpathname.split("/");
    for (int i = 0; i < parts.size(); ++i) {
        bool excluded = false;
        for (int j = 0; j < m_excl_list.size(); ++j) {
            if (parts[i].startsWith(m_excl_list[j])) {
                excluded = true;
                break;
            }
        }
        if (excluded)
            continue;
        if (m_prefix_mode == SortPrefixRemove)
            parts[i] = parts[i].remove(m_prefixes_regex);
        else
            parts[i] = parts[i].replace(m_prefixes_regex2, "\\2, \\1").trimmed();
    }
    return parts.join("/");
}
