include ( ../../mythconfig.mak )
include ( ../../settings.pro )
   
QMAKE_STRIP = echo

TARGET = themenop
TEMPLATE = app
CONFIG -= qt moc

defaultfiles.path = $${PREFIX}/share/mythtv/themes/default
defaultfiles.files = default/*.xml default/images/*.png

widefiles.path = $${PREFIX}/share/mythtv/themes/default-wide
widefiles.files = default-wide/*.xml default-wide/images/*.png

menufiles.path = $${PREFIX}/share/mythtv/
menufiles.files = menus/*.xml

htmlfiles.path = $${PREFIX}/share/mythtv/themes/default/htmls
htmlfiles.files = htmls/*.html

INSTALLS += defaultfiles widefiles menufiles htmlfiles

# Input
SOURCES += ../../themedummy.c
