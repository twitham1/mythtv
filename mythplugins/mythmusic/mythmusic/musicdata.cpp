// qt
#include <QApplication>

// mythtv
#include <mythscreenstack.h>
#include <mythmainwindow.h>
#include <mythprogressdialog.h>
#include <musicmetadata.h>
#include <musicfilescanner.h>
#include <musicutils.h>
#include <mthreadpool.h>

// mythmusic
#include "musicdata.h"
#include "musicplayer.h"

#include <unistd.h> // for usleep()

// this is the global MusicData object shared thoughout MythMusic
MusicData  *gMusicData = nullptr;


///////////////////////////////////////////////////////////////////////////////


MusicData::MusicData(void)
{
    all_playlists = nullptr;
    all_music = nullptr;
    all_streams = nullptr;
    initialized = false;
}

MusicData::~MusicData(void)
{
    if (all_playlists)
    {
        delete all_playlists;
        all_playlists = nullptr;
    }

    if (all_music)
    {
        delete all_music;
        all_music = nullptr;
    }

    if (all_streams)
    {
        delete all_streams;
        all_streams = nullptr;
    }
}

void MusicData::scanMusic (void)
{
    QStringList strList("SCAN_MUSIC");
    SendStringListThread *thread = new SendStringListThread(strList);
    MThreadPool::globalInstance()->start(thread, "Send SCAN_MUSIC");

    LOG(VB_GENERAL, LOG_INFO, "Requested a music file scan");
}

/// reload music after a scan, rip or import
void MusicData::reloadMusic(void)
{
    if (!all_music || !all_playlists)
        return;

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    QString message = tr("Rebuilding music tree");

    MythUIBusyDialog *busy = new MythUIBusyDialog(message, popupStack,
                                                  "musicscanbusydialog");

    if (busy->Create())
        popupStack->AddScreen(busy, false);
    else
        busy = nullptr;

    // TODO make it so the player isn't interupted
    // for the moment stop playing and try to resume after reloading
    bool wasPlaying = false;
    if (gPlayer->isPlaying())
    {
        gPlayer->savePosition();
        gPlayer->stop(true);
        wasPlaying = true;
    }

    all_music->startLoading();
    while (!all_music->doneLoading())
    {
        qApp->processEvents();
        usleep(50000);
    }

    all_playlists->resync();

    if (busy)
        busy->Close();

    if (wasPlaying)
        gPlayer->restorePosition();
}

void MusicData::loadMusic(void)
{
    // only do this once
    if (initialized)
        return;

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    QString message = qApp->translate("(MythMusicMain)",
                                      "Loading Music. Please wait ...");

    MythUIBusyDialog *busy = new MythUIBusyDialog(message, popupStack,
                                                  "musicscanbusydialog");
    if (busy->Create())
        popupStack->AddScreen(busy, false);
    else
        busy = nullptr;

    // Set the various track formatting modes
    MusicMetadata::setArtistAndTrackFormats();

    AllMusic *all_music = new AllMusic();

    //  Load all playlists into RAM (once!)
    PlaylistContainer *all_playlists = new PlaylistContainer(all_music);

    gMusicData->all_music = all_music;
    gMusicData->all_streams = new AllStream();
    gMusicData->all_playlists = all_playlists;

    gMusicData->initialized = true;

    while (!gMusicData->all_playlists->doneLoading() || !gMusicData->all_music->doneLoading())
    {
        qApp->processEvents();
        usleep(50000);
    }

    gPlayer->loadStreamPlaylist();
    gPlayer->loadPlaylist();

    if (busy)
        busy->Close();
}
