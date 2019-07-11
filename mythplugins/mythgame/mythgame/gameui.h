#ifndef GAMEUI_H_
#define GAMEUI_H_

#include <QString>
#include <QObject>

// myth
#include <mythscreentype.h>
#include <metadata/metadatacommon.h>
#include <metadata/metadatadownload.h>
#include <metadata/metadataimagedownload.h>
#include <mythprogressdialog.h>

class MythUIButtonTree;
class MythGenericTree;
class MythUIText;
class MythUIStateType;
class RomInfo;
class QTimer;
class QKeyEvent;
class QEvent;
class GameScanner;

class GameUI : public MythScreenType
{
    Q_OBJECT

  public:
    explicit GameUI(MythScreenStack *parentStack);
    ~GameUI() = default;

    bool Create() override; // MythScreenType
    void BuildTree();
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType

  public slots:
    void nodeChanged(MythGenericTree* node);
    void itemClicked(MythUIButtonListItem* item);
    void showImages(void);
    void searchComplete(const QString&);
    void gameSearch(MythGenericTree *node = nullptr,
                     bool automode = false);
    void OnGameSearchListSelection(RefCountHandler<MetadataLookup> lookup);
    void OnGameSearchDone(MetadataLookup *lookup);
    void StartGameImageSet(MythGenericTree *node, QStringList coverart,
                           QStringList fanart, QStringList screenshot);
    void doScan(void);
    void reloadAllData(bool dbchanged);

  private:
    void updateRomInfo(RomInfo *rom);
    void clearRomInfo(void);
    void edit(void);
    void showInfo(void);
    void ShowMenu(void) override; // MythScreenType
    void searchStart(void);
    void toggleFavorite(void);
    void customEvent(QEvent *event) override; // MythUIType
    void createBusyDialog(const QString& title);

    QString getFillSql(MythGenericTree* node) const;
    QString getChildLevelString(MythGenericTree *node) const;
    QString getFilter(MythGenericTree *node) const;
    int     getLevelsOnThisBranch(MythGenericTree *node) const;
    bool    isLeaf(MythGenericTree *node) const;
    void    fillNode(MythGenericTree *node);
    void    resetOtherTrees(MythGenericTree *node);
    void    updateChangedNode(MythGenericTree *node, RomInfo *romInfo);
    void    handleDownloadedImages(MetadataLookup *lookup);

  private:
    bool m_showHashed                      {false};
    bool m_gameShowFileName                {false};

    MythGenericTree  *m_gameTree           {nullptr};
    MythGenericTree  *m_favouriteNode      {nullptr};

    MythUIBusyDialog *m_busyPopup          {nullptr};
    MythScreenStack  *m_popupStack         {nullptr};

    MythUIButtonTree *m_gameUITree         {nullptr};
    MythUIText       *m_gameTitleText      {nullptr};
    MythUIText       *m_gameSystemText     {nullptr};
    MythUIText       *m_gameYearText       {nullptr};
    MythUIText       *m_gameGenreText      {nullptr};
    MythUIText       *m_gamePlotText       {nullptr};
    MythUIStateType  *m_gameFavouriteState {nullptr};
    MythUIImage      *m_gameImage          {nullptr};
    MythUIImage      *m_fanartImage        {nullptr};
    MythUIImage      *m_boxImage           {nullptr};

    MetadataDownload      *m_query         {nullptr};
    MetadataImageDownload *m_imageDownload {nullptr};

    GameScanner      *m_scanner            {nullptr};
};

#endif
