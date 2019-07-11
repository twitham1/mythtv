#ifndef FILESELECTOR_H_
#define FILESELECTOR_H_

#include <iostream>

// qt
#include <QString>
#include <QStringList>
#include <QKeyEvent>

// myth
#include <mythscreentype.h>

// mytharchive
#include "archiveutil.h"

typedef struct
{
    bool directory;
    bool selected;
    QString filename;
    int64_t size;
} FileData;

typedef enum
{
    FSTYPE_FILELIST = 0,
    FSTYPE_FILE = 1,
    FSTYPE_DIRECTORY = 2
} FSTYPE;

class MythUIText;
class MythUITextEdit;
class MythUIButton;
class MythUIButtonList;
class MythUIButtonListItem;

class FileSelector : public MythScreenType
{
    Q_OBJECT

  public:
    FileSelector(MythScreenStack *parent, QList<ArchiveItem *> *archiveList,
                 FSTYPE type, const QString &startDir, const QString &filemask)
        : MythScreenType(parent, "FileSelector"),
          m_selectorType(type),
          m_filemask(filemask),
          m_curDirectory(startDir),
          m_archiveList(archiveList) {}
    ~FileSelector();

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *e) override; // MythScreenType

    QString getSelected(void);

  signals:
    void haveResult(bool ok);            // used in FSTYPE_FILELIST mode
    void haveResult(QString filename);   // used in FSTYPE_FILE or FSTYPE_DIRECTORY mode

  protected slots:
    void OKPressed(void);
    void cancelPressed(void);
    void backPressed(void);
    void homePressed(void);
    void itemClicked(MythUIButtonListItem *item);
    void locationEditLostFocus(void);

  protected:
    void updateFileList(void);
    void updateSelectedList(void);
    void updateWidgets(void);
    void wireUpTheme(void);
    void updateScrollArrows(void);

    FSTYPE                m_selectorType;
    QString               m_filemask;
    QString               m_curDirectory;
    QList<FileData *>     m_fileData;
    QStringList           m_selectedList;
    QList<ArchiveItem *> *m_archiveList;
    //
    //  GUI stuff
    //
    MythUIText       *m_titleText      {nullptr};
    MythUIButtonList *m_fileButtonList {nullptr};
    MythUITextEdit   *m_locationEdit   {nullptr};
    MythUIButton     *m_okButton       {nullptr};
    MythUIButton     *m_cancelButton   {nullptr};
    MythUIButton     *m_backButton     {nullptr};
    MythUIButton     *m_homeButton     {nullptr};
};

Q_DECLARE_METATYPE(FileData *)

#endif
