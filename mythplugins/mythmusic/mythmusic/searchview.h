#ifndef SEARCHVIEW_H_
#define SEARCHVIEW_H_

// qt
#include <QEvent>
#include <QVector>

// mythui
#include <mythscreentype.h>

// mythmusic
#include <musiccommon.h>

class MythUIButtonList;
class MythUIText;
class MythUITextEdit;

class SearchView : public MusicCommon
{
    Q_OBJECT
  public:
    SearchView(MythScreenStack *parent, MythScreenType *parentScreen);
    ~SearchView(void) = default;

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *) override; // MusicCommon

    void ShowMenu(void) override; // MusicCommon

  protected:
    void customEvent(QEvent *event) override; // MusicCommon
    void updateTracksList(void);

  protected slots:
    void fieldSelected(MythUIButtonListItem *item);
    void criteriaChanged(void);

    void trackClicked(MythUIButtonListItem *item);
    void trackVisible(MythUIButtonListItem *item);

  private:
    bool                 m_playTrack    {false};
    MythUIButtonList    *m_fieldList    {nullptr};
    MythUITextEdit      *m_criteriaEdit {nullptr};
    MythUIText          *m_matchesText  {nullptr};
    MythUIButtonList    *m_tracksList   {nullptr};
};

#endif
