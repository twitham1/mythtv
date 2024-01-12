# -*- Mode: python; coding: utf-8; indent-tabs-mode: nil; -*-

info = {}
from common import main
from lib.culrcscrapers.lyricscom.lyricsScraper import LyricsFetcher
# make sure this-------^^^^^^^^^ matches this file's basename
#       and this-------vvvvvvvvv one too:
info['command']     = 'lyricscom.py'

info['name']        = 'Lyrics.Com'
info['description'] = 'Search https://lyrics.com for lyrics'
info['author']      = "Paul Harrison and ronie"
info['priority']    = '240'
info['version']     = '2.0'
info['syncronized'] = False

info['artist']      = 'Dire Straits' # v33-
info['title']       = 'Money For Nothing'
info['album']       = ''

info['artist']      = 'Blur' # v34+
info['title']       = "You're so Great"

if __name__ == '__main__':
    main.main(info, LyricsFetcher)
