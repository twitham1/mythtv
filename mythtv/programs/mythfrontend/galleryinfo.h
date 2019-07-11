//! \file
//! \brief The info/details overlay that shows image metadata

#ifndef GALLERYINFO_H
#define GALLERYINFO_H

#include <QTimer>

#include "mythuibuttonlist.h"
#include "imagemanager.h"


class MythScreenType;

//! Displayed info/details about an image.
enum InfoVisibleState { kNoInfo,    //!< Details not displayed
                        kBasicInfo, //!< Shows just the most useful exif tags
                        kFullInfo   //!< Shows all exif tags
                      };

//! The image info/details buttonlist overlay that displays exif tags
class InfoList : QObject
{
    Q_OBJECT
public:
    explicit InfoList(MythScreenType &screen);

    bool             Create(bool focusable);
    void             Toggle(const ImagePtrK&);
    bool             Hide();
    void             Update(const ImagePtrK&);
    void             Display(ImageItemK &im, const QStringList &tagStrings);
    InfoVisibleState GetState() const   { return m_infoVisible; }

private slots:
    void Clear()   { m_btnList->Reset(); }

private:
    void CreateButton(const QString&, const QString&);
    void CreateCount(ImageItemK &);

    MythScreenType   &m_screen;      //!< Parent screen
    MythUIButtonList *m_btnList     {nullptr};  //!< Overlay buttonlist
    InfoVisibleState  m_infoVisible {kNoInfo};  //!< Info list state
    ImageManagerFe   &m_mgr;         //!< Image Manager
    QTimer            m_timer;       //!< Clears list if no new metadata arrives
};

#endif // GALLERYINFO_H
