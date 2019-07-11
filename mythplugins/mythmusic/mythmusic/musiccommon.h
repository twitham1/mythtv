#ifndef MUSICCOMMON_H_
#define MUSICCOMMON_H_

// qt
#include <QKeyEvent>
#include <QObject>

// mythtv
#include <audiooutput.h>
#include <mythscreentype.h>
#include <musicmetadata.h>

// mythmusic
#include "playlist.h"
#include "musicplayer.h"

class Output;
class Decoder;
class QTimer;
class MythUIProgressBar;
class MythUIImage;
class MythUIText;
class MythUIStateType;
class MythUIButton;
class MythUIVideo;
class MythUIButton;
class MythUICheckBox;
class MythMenu;

enum MusicView
{
    MV_PLAYLIST,
    MV_LYRICS,
    MV_PLAYLISTEDITORTREE,
    MV_PLAYLISTEDITORGALLERY,
    MV_VISUALIZER,
    MV_SEARCH,
    MV_ARTISTINFO,
    MV_ALBUMINFO,
    MV_TRACKINFO,
    MV_RADIO,
    MV_MINIPLAYER
};

Q_DECLARE_METATYPE(MusicView);

class MPUBLIC MusicCommon : public MythScreenType
{
    Q_OBJECT

  protected:

    MusicCommon(MythScreenStack *parent, MythScreenType *parentScreen,
                const QString &name);
    ~MusicCommon(void);

    bool CreateCommon(void);

    void switchView(MusicView view);

    void customEvent(QEvent *event) override; // MythUIType
    bool keyPressEvent(QKeyEvent *e) override; // MythScreenType

    void ShowMenu(void) override; // MythScreenType

  protected slots:
    void viewExited(void);

    void play(void);
    void stop(void);
    void pause(void);
    void previous(void);
    void next(void);
    void seekforward(void);
    void seekback(void);
    void seek(int);
    void stopAll(void);
    void changeRating(bool increase);

    void searchButtonList(void);
    MythMenu* createMainMenu(void);
    MythMenu* createSubMenu(void);
    MythMenu* createPlaylistMenu(void);
    MythMenu* createPlayerMenu(void);
    MythMenu* createQuickPlaylistsMenu(void);
    MythMenu* createRepeatMenu(void);
    MythMenu* createShuffleMenu(void);
    MythMenu* createVisualizerMenu(void);
    MythMenu* createPlaylistOptionsMenu(void);

    void playlistItemClicked(MythUIButtonListItem *item);
    void playlistItemVisible(MythUIButtonListItem *item);

    void fromCD(void);
    void allTracks(void);
    void byArtist(void);
    void byAlbum(void);
    void byGenre(void);
    void byYear(void);
    void byTitle(void);
    void doUpdatePlaylist(void);

  protected:
    void showExitMenu(void);
    void showPlaylistOptionsMenu(bool addMainMenu = false);

  protected:
    void init(bool startPlayback = true);
    QString getTimeString(int exTime, int maxTime);
    void updateProgressBar(void);
    void setTrackOnLCD(MusicMetadata *mdata);
    void editTrackInfo(MusicMetadata *mdata);
    void updateTrackInfo(MusicMetadata *mdata);
    void showTrackInfo(MusicMetadata *mdata);
    void updateUIPlaylist(void);
    void updatePlaylistStats(void);
    void updateUIPlayedList(void);    // for streaming
    void updateRepeatMode(void);
    void updateShuffleMode(bool updateUIList = false);

    void changeVolume(bool up);
    void changeSpeed(bool up);
    void toggleMute(void);
    void toggleUpmix(void);
    void showVolume(void);
    void updateVolume(void);
    void showSpeed(bool show);

    void startVisualizer(void);
    void stopVisualizer(void);
    void cycleVisualizer(void);
    void switchVisualizer(const QString &visual);
    void switchVisualizer(int visual);

    void playFirstTrack();
    bool restorePosition(int trackID);

    MythScreenType        *m_parentScreen       {nullptr};
    MusicView              m_currentView;

    // visualiser stuff
    MainVisual            *m_mainvisual         {nullptr};
    bool                   m_fullscreenBlank    {false};
    bool                   m_cycleVisualizer    {false};
    bool                   m_randomVisualizer   {false};

    QStringList            m_visualModes;
    unsigned int           m_currentVisual      {0};

    bool                   m_moveTrackMode      {false};
    bool                   m_movingTrack        {false};

    bool                   m_controlVolume      {true};

    int                    m_currentTrack       {0};
    int                    m_currentTime        {0};
    int                    m_maxTime            {0};

    uint                   m_playlistTrackCount {0};
    uint                   m_playlistPlayedTime {0};
    uint                   m_playlistMaxTime    {0};

    // for quick playlists
    PlaylistOptions        m_playlistOptions;
    QString                m_whereClause;

    // for adding tracks from playlist editor
    QList<int>             m_songList;

    // UI widgets
    MythUIText            *m_timeText           {nullptr};
    MythUIText            *m_infoText           {nullptr};
    MythUIText            *m_visualText         {nullptr};
    MythUIText            *m_noTracksText       {nullptr};

    MythUIStateType       *m_shuffleState       {nullptr};
    MythUIStateType       *m_repeatState        {nullptr};

    MythUIStateType       *m_movingTracksState  {nullptr};

    MythUIStateType       *m_ratingState        {nullptr};

    MythUIProgressBar     *m_trackProgress      {nullptr};
    MythUIText            *m_trackProgressText  {nullptr};
    MythUIText            *m_trackSpeedText     {nullptr};
    MythUIStateType       *m_trackState         {nullptr};

    MythUIStateType       *m_muteState          {nullptr};
    MythUIText            *m_volumeText         {nullptr};

    MythUIProgressBar     *m_playlistProgress   {nullptr};

    MythUIButton          *m_prevButton         {nullptr};
    MythUIButton          *m_rewButton          {nullptr};
    MythUIButton          *m_pauseButton        {nullptr};
    MythUIButton          *m_playButton         {nullptr};
    MythUIButton          *m_stopButton         {nullptr};
    MythUIButton          *m_ffButton           {nullptr};
    MythUIButton          *m_nextButton         {nullptr};

    MythUIImage           *m_coverartImage      {nullptr};

    MythUIButtonList      *m_currentPlaylist    {nullptr};
    MythUIButtonList      *m_playedTracksList   {nullptr};

    MythUIVideo           *m_visualizerVideo    {nullptr};
};

class MPUBLIC MythMusicVolumeDialog : public MythScreenType
{
    Q_OBJECT
  public:
    MythMusicVolumeDialog(MythScreenStack *parent, const char *name)
        : MythScreenType(parent, name, false) {}
    ~MythMusicVolumeDialog(void);

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType

  protected:
    void increaseVolume(void);
    void decreaseVolume(void);
    void toggleMute(void);

    void updateDisplay(void);

    QTimer            *m_displayTimer {nullptr};
    MythUIText        *m_messageText  {nullptr};
    MythUIText        *m_volText      {nullptr};
    MythUIStateType   *m_muteState    {nullptr};
    MythUIProgressBar *m_volProgress  {nullptr};
};

class MPUBLIC TrackInfoDialog : public MythScreenType
{
  Q_OBJECT
  public:
    TrackInfoDialog(MythScreenStack *parent, MusicMetadata *mdata, const char *name)
        : MythScreenType(parent, name, false),
          m_metadata(mdata) {}
    ~TrackInfoDialog(void) = default;

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType

  protected:
    MusicMetadata *m_metadata {nullptr};
};

#endif
