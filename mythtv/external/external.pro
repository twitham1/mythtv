include (../settings.pro)

TEMPLATE = subdirs

win32-msvc* {

# Libraries without dependencies

!using_libbluray_external: SUBDIRS += libmythbluray
SUBDIRS += libmythdvdnav
SUBDIRS += libudfread
SUBDIRS += nzmqt/src/nzmqt.pro
SUBDIRS += libmythsoundtouch

}
