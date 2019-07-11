#ifndef PLAYLISTVIEW_H_
#define PLAYLISTVIEW_H_

// qt
#include <QEvent>
#include <QVector>

// mythui
#include <mythscreentype.h>

// mythmusic
#include <musiccommon.h>

class MythUIButtonList;
class MythUIText;

class PlaylistView : public MusicCommon
{
    Q_OBJECT
  public:
    PlaylistView(MythScreenStack *parent, MythScreenType *parentScreen);
    ~PlaylistView(void) = default;

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *) override; // MusicCommon

  protected:
    void customEvent(QEvent *event) override; // MusicCommon
};

#endif
