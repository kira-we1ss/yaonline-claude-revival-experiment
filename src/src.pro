#
# Psi qmake profile
#

# Configuration
TEMPLATE = app
TARGET   = yachat
CONFIG += qt thread c++17

unix:!mac {
	exists($$[QT_INSTALL_LIBS]/libQtCore.a) {
		CONFIG += qt_static
	}
}
windows: {
	exists($$[QT_INSTALL_LIBS]/QtCore.lib) {
		CONFIG += qt_static
	}
}

windows: include($$PWD/../conf_windows.pri)

yapsi_activex_server {
	CONFIG  += qaxserver qaxcontainer
	contains(CONFIG, static): DEFINES += QT_NODLL

	# DEF_FILE = ../win32/yapsiserver.def
	RC_FILE  = ../win32/yapsiserver.rc
}
else {
	windows: RC_FILE = ../win32/psi_win32.rc
}

QT += xml network widgets concurrent sql

#CONFIG += use_crash
CONFIG += pep
DEFINES += QT_STATICPLUGIN

# Import several very useful Makefile targets 
# as well as set up default directories for 
# generated files
include(../qa/valgrind/valgrind.pri)
include(../qa/oldtest/unittest.pri)

# qconf

exists(../conf.pri) {
	include(../conf.pri)

	# Target
	target.path = $$BINDIR
	INSTALLS += target

	# Shared files
	sharedfiles.path  = $$PSI_DATADIR
	sharedfiles.files = ../COPYING ../sound ../certs
	INSTALLS += sharedfiles

	# Widgets
	#widgets.path = $$PSI_DATADIR/designer
	#widgets.files = ../libpsi/psiwidgets/libpsiwidgets.so
	#INSTALLS += widgets

	# emoticons
	emoticons.path = $$PSI_DATADIR/iconsets
	emoticons.files = tools/yastuff/iconsets/emoticons
	INSTALLS += emoticons

	# icons and desktop files
	dt.path=$$PREFIX/share/applications/
	dt.extra    = cp -f ../psi.desktop $(INSTALL_ROOT)$$dt.path/yachat.desktop
	icon1.path=$$PREFIX/share/icons/hicolor/16x16/apps
	icon1.extra = cp -f ../iconsets/system/default/logo_16.png $(INSTALL_ROOT)$$icon1.path/yachat.png
	icon2.path=$$PREFIX/share/icons/hicolor/32x32/apps
	icon2.extra = cp -f ../iconsets/system/default/logo_32.png $(INSTALL_ROOT)$$icon2.path/yachat.png
	icon3.path=$$PREFIX/share/icons/hicolor/48x48/apps
	icon3.extra = cp -f ../iconsets/system/default/logo_48.png $(INSTALL_ROOT)$$icon3.path/yachat.png
	icon4.path=$$PREFIX/share/icons/hicolor/64x64/apps
	icon4.extra = cp -f ../iconsets/system/default/logo_64.png $(INSTALL_ROOT)$$icon4.path/yachat.png
	icon5.path=$$PREFIX/share/icons/hicolor/128x128/apps
	icon5.extra = cp -f ../iconsets/system/default/logo_128.png $(INSTALL_ROOT)$$icon5.path/yachat.png
	INSTALLS += dt icon1 icon2 icon3 icon4 icon5
}

windows {
	LIBS += -lWSock32 -lUser32 -lShell32 -lGdi32 -lAdvAPI32
	INCLUDEPATH += . # otherwise MSVC will fail to find "common.h" when compiling options/* stuff
}

qt_static {
	DEFINES += STATIC_IMAGEFORMAT_PLUGINS
	QTPLUGIN += qjpeg qgif
}

# Configuration
# IPv6 ?
#DEFINES += NO_NDNS

# Psi sources
include(src.pri)

# don't clash with unittests
SOURCES += main.cpp
HEADERS += main.h

################################################################################
# Translation
################################################################################

LANG_PATH = ../lang

TRANSLATIONS = \
	$$LANG_PATH/psi_ru.ts

OPTIONS_TRANSLATIONS_FILE=$$PWD/option_translations.cpp

QMAKE_EXTRA_TARGETS += translate_options
translate_options.commands = $$PWD/../admin/update_options_ts.py $$PWD/../options/default.xml > $$OPTIONS_TRANSLATIONS_FILE

# In case lupdate doesn't work
QMAKE_EXTRA_TARGETS += translate
translate.commands = lupdate . options widgets tools/grepshortcutkeydlg ../cutestuff/network ../iris/xmpp-im -ts $$TRANSLATIONS


exists($$OPTIONS_TRANSLATIONS_FILE) {
	SOURCES += $$OPTIONS_TRANSLATIONS_FILE
}
QMAKE_CLEAN += $$OPTIONS_TRANSLATIONS_FILE

################################################################################

# Resources

RESOURCES += ../psi.qrc ../iconsets.qrc

!win32 {
	system(./yapsi_translations.sh)

	# Revision number
	system(./yapsi_revision.sh)

	# Qt translations
	system(./yapsi_qt_translations.sh)
}

breakpad {
	BREAKPAD_PATH = $$PWD/../../vendor/google-breakpad/src
	BREAKPAD_PRI = $$PWD/tools/breakpad/breakpad.pri
	include($$BREAKPAD_PRI)
}

# Platform specifics
unix:!mac {
	QMAKE_POST_LINK = rm -f ../yachat ; ln -s src/yachat ../yachat
}
mac {
	QMAKE_MACOSX_DEPLOYMENT_TARGET = 11.0
	QMAKE_INFO_PLIST = ../mac/Info.plist
	RC_FILE = ../mac/application.icns
	QMAKE_POST_LINK = \
		mkdir -p `dirname $(TARGET)`/../Resources/iconsets/emoticons; \
		cp -R tools/yastuff/iconsets/emoticons/* `dirname $(TARGET)`/../Resources/iconsets/emoticons; \
		cp -R ../certs ../sound `dirname $(TARGET)`/../Resources; \
		echo "APPLyach" > `dirname $(TARGET)`/../PkgInfo;

	CARBONCOCOA_PRI = $$PWD/tools/carboncocoa/carboncocoa.pri
	include($$CARBONCOCOA_PRI)
}
