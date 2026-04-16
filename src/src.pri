# modules
include($$PWD/protocol/protocol.pri)
include($$PWD/irisprotocol/irisprotocol.pri)
include($$PWD/privacy/privacy.pri)
include($$PWD/capabilities/capabilities.pri)
include($$PWD/utilities/utilities.pri)
include($$PWD/tabs/tabs.pri)

# tools
include($$PWD/tools/trayicon/trayicon.pri)
include($$PWD/tools/iconset/iconset.pri)
include($$PWD/tools/idle/idle.pri)
include($$PWD/tools/systemwatch/systemwatch.pri)
include($$PWD/tools/zip/zip.pri)
include($$PWD/tools/optionstree/optionstree.pri)
include($$PWD/tools/globalshortcut/globalshortcut.pri)
include($$PWD/tools/advwidget/advwidget.pri)
include($$PWD/tools/spellchecker/spellchecker.pri)
include($$PWD/tools/grepshortcutkeydlg/grepshortcutkeydlg.pri)
include($$PWD/tools/atomicxmlfile/atomicxmlfile.pri)
include($$PWD/tools/httphelper/httphelper.pri)
include($$PWD/tools/simplecli/simplecli.pri)

# Growl (stub always included on mac)
mac {
	include($$PWD/tools/growlnotifier/growlnotifier.pri)
}

# Mac dock
mac { include($$PWD/tools/mac_dock/mac_dock.pri) }

# Tune
pep {
	DEFINES += USE_PEP
	CONFIG += tc_psifile
	mac { CONFIG += tc_itunes }
	windows { CONFIG += tc_winamp }
}
include($$PWD/tools/tunecontroller/tunecontroller.pri)

# Crash
use_crash {
	DEFINES += USE_CRASH
	include($$PWD/tools/crash/crash.pri)
}

# qca
qca-static {
	# QCA
	DEFINES += QCA_STATIC
	include($$PWD/../third-party/qca/qca.pri)

	# QCA-OpenSSL
	contains(DEFINES, HAVE_OPENSSL) {
		include($$PWD/../third-party/qca/qca-ossl.pri)
	}
	
	# QCA-SASL
	contains(DEFINES, HAVE_CYRUSSASL) {
		include($$PWD/../third-party/qca/qca-cyrus-sasl.pri)
	}

	# QCA-GnuPG
	include($$PWD/../third-party/qca/qca-gnupg.pri)
}
else {
	CONFIG += crypto	
}

# Widgets
include($$PWD/widgets/widgets.pri)

# Google FT
google_ft {
	DEFINES += GOOGLE_FT
	HEADERS += $$PWD/googleftmanager.h
	SOURCES += $$PWD/googleftmanager.cpp
	include(../third-party/libjingle.new/libjingle.pri)
}

# Jingle
jingle {
	HEADERS += $$PWD/jinglevoicecaller.h
	SOURCES += $$PWD/jinglevoicecaller.cpp
	DEFINES += HAVE_JINGLE POSIX

	JINGLE_CPP = $$PWD/../third-party/libjingle
	LIBS += -L$$JINGLE_CPP -ljingle_psi
	INCLUDEPATH += $$JINGLE_CPP

	contains(DEFINES, HAVE_PORTAUDIO) {
		LIBS += -framework CoreAudio -framework AudioToolbox
	}
}

# include Iris XMPP library
CONFIG += iris_bundle
CONFIG += iris_legacy

!iris_legacy {
	include($$PWD/../iris/iris.pri)
}
iris_legacy {
	DEFINES += IRIS_LEGACY
	include($$PWD/../iris-legacy/cutestuff/cutestuff.pri)
	include($$PWD/../iris-legacy/iris/iris.pri)
}

# Header files
HEADERS += \
	$$PWD/psilogger.h \
	$$PWD/varlist.h \ 
	$$PWD/jidutil.h \
#	$$PWD/showtextdlg.h \ 
	$$PWD/profiles.h \
	$$PWD/activeprofiles.h \
#	$$PWD/profiledlg.h \
#	$$PWD/aboutdlg.h \
	$$PWD/desktoputil.h \
	$$PWD/textutil.h \
	$$PWD/pixmaputil.h \
	$$PWD/psiaccount.h \
	$$PWD/psicon.h \
	$$PWD/accountscombobox.h \
	$$PWD/psievent.h \
	$$PWD/globaleventqueue.h \
	$$PWD/xmlconsole.h \
	$$PWD/contactview.h \
	$$PWD/psiiconset.h \
	$$PWD/applicationinfo.h \
	$$PWD/pgptransaction.h \
	$$PWD/userlist.h \
	$$PWD/mainwin.h \
	$$PWD/mainwin_p.h \
	$$PWD/psitrayicon.h \
	$$PWD/rtparse.h \
	$$PWD/systeminfo.h \
	$$PWD/common.h \
	$$PWD/proxy.h \
	$$PWD/miniclient.h \
#	$$PWD/accountmanagedlg.h \
#	$$PWD/accountadddlg.h \
#	$$PWD/accountregdlg.h \
#	$$PWD/accountmodifydlg.h \
#	$$PWD/changepwdlg.h \
	$$PWD/msgmle.h \
#	$$PWD/statusdlg.h \
	$$PWD/certutil.h \
#	$$PWD/eventdlg.h \
	$$PWD/chatdlg.h \
	$$PWD/chatdlgbase.h \
#	$$PWD/psichatdlg.h \
	$$PWD/chatsplitter.h \
#	$$PWD/chateditproxy.h \
#	$$PWD/psichatdlg.h \
#	$$PWD/adduserdlg.h \
	$$PWD/infodlg.h \
	$$PWD/translationmanager.h \
	$$PWD/eventdb.h \
#	$$PWD/historydlg.h \
#	$$PWD/tipdlg.h \
	$$PWD/searchdlg.h \
	$$PWD/registrationdlg.h \
#	$$PWD/psitoolbar.h \
#	$$PWD/passphrasedlg.h \
	$$PWD/vcardfactory.h \
	$$PWD/sslcertdlg.h \
	$$PWD/tasklist.h \
	$$PWD/discodlg.h \
	$$PWD/alerticon.h \
	$$PWD/alertable.h \
#	$$PWD/psipopup.h \
	$$PWD/psinotifier.h \
	$$PWD/psinotifierbase.h \
	$$PWD/psiapplication.h \
	$$PWD/avatars.h \
	$$PWD/actionlist.h \
	$$PWD/serverinfomanager.h \
#	$$PWD/psiactionlist.h \
	$$PWD/xdata_widget.h \
#	$$PWD/statuspreset.h \
	$$PWD/lastactivitytask.h \
	$$PWD/mucmanager.h \
	$$PWD/mucjoindlg.h \
	$$PWD/mucconfigdlg.h \
	$$PWD/mucaffiliationsmodel.h \
	$$PWD/mucaffiliationsproxymodel.h \
	$$PWD/mucaffiliationsview.h \
	$$PWD/rosteritemexchangetask.h \
	$$PWD/mood.h \
	$$PWD/moodcatalog.h \
	$$PWD/mooddlg.h \
	$$PWD/geolocation.h \
	$$PWD/physicallocation.h \
	$$PWD/pepmanager.h \
	$$PWD/pubsubsubscription.h \
	$$PWD/rc.h \
	$$PWD/psihttpauthrequest.h \
	$$PWD/httpauthmanager.h \
	$$PWD/ahcommand.h \
	$$PWD/pongserver.h \
	$$PWD/ahcommandserver.h \
	$$PWD/ahcommanddlg.h \
	$$PWD/ahcformdlg.h \
	$$PWD/ahcexecutetask.h \
	$$PWD/ahcservermanager.h \
	$$PWD/serverlistquerier.h \
#	$$PWD/psioptionseditor.h \
	$$PWD/psioptions.h \
#	$$PWD/voicecaller.h \
#	$$PWD/voicecalldlg.h \
	$$PWD/resourcemenu.h \
	$$PWD/statusmenu.h \
	$$PWD/shortcutmanager.h \
	$$PWD/psicontactlist.h \
	$$PWD/accountlabel.h \
	$$PWD/psiactions.h \
#	$$PWD/buzzer.h \
	$$PWD/dummystream.h \
	$$PWD/contactupdatesmanager.h \
	$$PWD/networkinterfacemanager.h \
	$$PWD/psicli.h

# Source files
SOURCES += \
	$$PWD/psilogger.cpp \
	$$PWD/varlist.cpp \
	$$PWD/jidutil.cpp \
#	$$PWD/showtextdlg.cpp \
	$$PWD/psi_profiles.cpp \
	$$PWD/activeprofiles.cpp \
#	$$PWD/profiledlg.cpp \
#	$$PWD/aboutdlg.cpp \
	$$PWD/desktoputil.cpp \
	$$PWD/textutil.cpp \
	$$PWD/pixmaputil.cpp \
	$$PWD/accountscombobox.cpp \
	$$PWD/psievent.cpp \
	$$PWD/globaleventqueue.cpp \
	$$PWD/xmlconsole.cpp \
	$$PWD/contactview.cpp \
	$$PWD/psiiconset.cpp \
	$$PWD/applicationinfo.cpp \
	$$PWD/pgptransaction.cpp \
	$$PWD/serverinfomanager.cpp \
	$$PWD/userlist.cpp \
	$$PWD/mainwin.cpp \
	$$PWD/mainwin_p.cpp \
	$$PWD/psitrayicon.cpp \
	$$PWD/rtparse.cpp \
	$$PWD/systeminfo.cpp \
	$$PWD/common.cpp \
	$$PWD/proxy.cpp \
	$$PWD/miniclient.cpp \
#	$$PWD/accountmanagedlg.cpp \
#	$$PWD/accountadddlg.cpp \
#	$$PWD/accountregdlg.cpp \
#	$$PWD/accountmodifydlg.cpp \
#	$$PWD/changepwdlg.cpp \
	$$PWD/msgmle.cpp \
#	$$PWD/statusdlg.cpp \
#	$$PWD/eventdlg.cpp \
	$$PWD/chatdlg.cpp \
	$$PWD/chatdlgbase.cpp \
#	$$PWD/psichatdlg.cpp \
	$$PWD/chatsplitter.cpp \
#	$$PWD/chateditproxy.cpp \
#	$$PWD/psichatdlg.cpp \
#	$$PWD/tipdlg.cpp \
#	$$PWD/adduserdlg.cpp \
	$$PWD/infodlg.cpp \
	$$PWD/translationmanager.cpp \
	$$PWD/certutil.cpp \
	$$PWD/eventdb.cpp \
#	$$PWD/historydlg.cpp \
	$$PWD/searchdlg.cpp \
	$$PWD/registrationdlg.cpp \
#	$$PWD/psitoolbar.cpp \
#	$$PWD/passphrasedlg.cpp \
	$$PWD/vcardfactory.cpp \
	$$PWD/sslcertdlg.cpp \
	$$PWD/discodlg.cpp \
	$$PWD/alerticon.cpp \
	$$PWD/alertable.cpp \
#	$$PWD/psipopup.cpp \
	$$PWD/psinotifier.cpp \
	$$PWD/psinotifierbase.cpp \
	$$PWD/psiapplication.cpp \
	$$PWD/avatars.cpp \
	$$PWD/actionlist.cpp \
#	$$PWD/psiactionlist.cpp \
	$$PWD/xdata_widget.cpp \
	$$PWD/lastactivitytask.cpp \
#	$$PWD/statuspreset.cpp \
	$$PWD/mucmanager.cpp \
	$$PWD/mucjoindlg.cpp \
	$$PWD/mucconfigdlg.cpp \
	$$PWD/mucaffiliationsmodel.cpp \
	$$PWD/mucaffiliationsproxymodel.cpp \
	$$PWD/mucaffiliationsview.cpp \
	$$PWD/rosteritemexchangetask.cpp \
	$$PWD/mood.cpp \
	$$PWD/moodcatalog.cpp \
	$$PWD/mooddlg.cpp \
	$$PWD/geolocation.cpp \
	$$PWD/physicallocation.cpp \
	$$PWD/pepmanager.cpp \
	$$PWD/pubsubsubscription.cpp \
	$$PWD/rc.cpp \
	$$PWD/httpauthmanager.cpp \
	$$PWD/ahcommand.cpp \
	$$PWD/pongserver.cpp \
	$$PWD/ahcommandserver.cpp \
	$$PWD/ahcommanddlg.cpp \
	$$PWD/ahcformdlg.cpp \
	$$PWD/ahcexecutetask.cpp \
	$$PWD/ahcservermanager.cpp \
	$$PWD/serverlistquerier.cpp \
	$$PWD/psioptions.cpp \
#	$$PWD/psioptionseditor.cpp \
#	$$PWD/voicecalldlg.cpp \
	$$PWD/resourcemenu.cpp \
	$$PWD/statusmenu.cpp \
	$$PWD/shortcutmanager.cpp \
	$$PWD/psicontactlist.cpp \
	$$PWD/accountlabel.cpp \
#	$$PWD/buzzer.cpp \
	$$PWD/dummystream.cpp \
	$$PWD/contactupdatesmanager.cpp \
	$$PWD/networkinterfacemanager.cpp

HEADERS += \
	$$PWD/psicontact.h \
	$$PWD/psiselfcontact.h \
	$$PWD/psicontactmenu.h \
	$$PWD/contactlistgroupstate.h \
	$$PWD/contactlistgroupcache.h \
	$$PWD/contactlistgroup.h \
	$$PWD/contactlistnestedgroup.h \
	$$PWD/contactlistgroupmenu.h \
#	$$PWD/psicontactlistview.h \
	$$PWD/contactlistviewdelegate.h \
	$$PWD/contactlistitem.h \
	$$PWD/contactlistitemmenu.h \
	$$PWD/contactlistmodel.h \
	$$PWD/contactlistmodelupdater.h \
	$$PWD/contactlistproxymodel.h \
	$$PWD/contactlistview.h \
	$$PWD/hoverabletreeview.h \
	$$PWD/psiaccountmenu.h \
	$$PWD/deliveryconfirmationmanager.h \
	$$PWD/fileutil.h \
	$$PWD/tabcompletion.h

SOURCES += \
	$$PWD/psicontact.cpp \
	$$PWD/psiselfcontact.cpp \
	$$PWD/psicontactmenu.cpp \
	$$PWD/contactlistgroupstate.cpp \
	$$PWD/contactlistgroupcache.cpp \
	$$PWD/contactlistgroup.cpp \
	$$PWD/contactlistnestedgroup.cpp \
	$$PWD/contactlistgroupmenu.cpp \
#	$$PWD/psicontactlistview.cpp \
	$$PWD/contactlistviewdelegate.cpp \
	$$PWD/contactlistitem.cpp \
	$$PWD/contactlistitemmenu.cpp \
	$$PWD/contactlistmodel.cpp \
	$$PWD/contactlistmodelupdater.cpp \
	$$PWD/contactlistproxymodel.cpp \
	$$PWD/contactlistview.cpp \
	$$PWD/hoverabletreeview.cpp \
	$$PWD/psiaccountmenu.cpp \
	$$PWD/psicon.cpp \
	$$PWD/psiaccount.cpp \
	$$PWD/deliveryconfirmationmanager.cpp \
	$$PWD/fileutil.cpp \
	$$PWD/tabcompletion.cpp

# CONFIG += filetransfer
filetransfer {
	DEFINES += FILETRANSFER

	HEADERS += \
		$$PWD/filetransdlg.h

	SOURCES += \
		$$PWD/filetransdlg.cpp
}

CONFIG += groupchat
groupchat {
	DEFINES += GROUPCHAT

	HEADERS += \
		$$PWD/groupchatdlg.h \
#		$$PWD/gcuserview.h \
#		$$PWD/psigroupchatdlg.h
		$$PWD/tabcompletionmuc.h

	SOURCES += \
		$$PWD/groupchatdlg.cpp
#		$$PWD/gcuserview.cpp \
#		$$PWD/psigroupchatdlg.cpp

#	FORMS += \
#		$$PWD/groupchatdlg.ui
}

SOURCES += \
	$$PWD/urlbookmark.cpp \
	$$PWD/conferencebookmark.cpp \
	$$PWD/groupchatcontact.cpp \
	$$PWD/groupchatcontactmenu.cpp \
	$$PWD/bookmarkmanager.cpp \
	$$PWD/bookmarkmanagedlg.cpp
HEADERS += \
	$$PWD/urlbookmark.h \
	$$PWD/conferencebookmark.h \
	$$PWD/groupchatcontact.h \
	$$PWD/groupchatcontactmenu.h \
	$$PWD/bookmarkmanager.h \
	$$PWD/bookmarkmanagedlg.h
FORMS += \
	$$PWD/bookmarkmanage.ui

# CONFIG += whiteboarding
whiteboarding {
	# Whiteboarding support. Still experimental.
	DEFINES += WHITEBOARDING

	HEADERS += \
		$$PWD/wbmanager.h \
		$$PWD/wbdlg.h \
		$$PWD/wbwidget.h \
		$$PWD/wbscene.h \
		$$PWD/wbitems.h

	SOURCES += \
		$$PWD/wbmanager.cpp \
		$$PWD/wbdlg.cpp \
		$$PWD/wbwidget.cpp \
		$$PWD/wbscene.cpp \
		$$PWD/wbitems.cpp
}

mac {
	HEADERS += $$PWD/psigrowlnotifier.h
	SOURCES += $$PWD/psigrowlnotifier.cpp
}

# Qt Designer interfaces
FORMS += \
	$$PWD/info.ui \
#	$$PWD/profileopen.ui \
#	$$PWD/profilemanage.ui \
#	$$PWD/profilenew.ui \
#	$$PWD/proxy.ui \
#	$$PWD/accountmanage.ui \
#	$$PWD/accountadd.ui \
#	$$PWD/accountreg.ui \
#	$$PWD/accountremove.ui \
#	$$PWD/accountmodify.ui \
#	$$PWD/changepw.ui \
#	$$PWD/addurl.ui \
#	$$PWD/adduser.ui \
#	$$PWD/mucjoin.ui \
	$$PWD/search.ui \
#	$$PWD/about.ui \
#	$$PWD/optioneditor.ui \
#	$$PWD/passphrase.ui \
	$$PWD/sslcert.ui \
	$$PWD/mucconfig.ui \
	$$PWD/xmlconsole.ui \
	$$PWD/disco.ui \
#	$$PWD/tip.ui \
#	$$PWD/filetrans.ui \
	$$PWD/mood.ui \
#	$$PWD/voicecall.ui \
	$$PWD/chatdlg.ui

# options dialog
#include($$PWD/options/options.pri)

include($$PWD/tools/yastuff/yastuff.pri)
include($$PWD/tools/yastuff/yawidgets/yawidgets.pri)
# include($$PWD/../../exceptionhelper/exceptionhelper.pri)

# Plugins
psi_plugins {
	HEADERS += $$PWD/pluginmanager.h \
				$$PWD/psiplugin.h
	SOURCES += $$PWD/pluginmanager.cpp
}

# CONFIG -= dbus
dbus {
	HEADERS += \
		$$PWD/dbus.h \
		$$PWD/psidbusnotifier.h
	SOURCES += \
		$$PWD/dbus.cpp \
		$$PWD/activeprofiles_dbus.cpp \
		$$PWD/psidbusnotifier.cpp

	DEFINES += USE_DBUS
	CONFIG += qdbus
}

win32:!dbus {
	SOURCES += $$PWD/activeprofiles_win.cpp
	LIBS += -lUser32
}


unix:!dbus {
	SOURCES += $$PWD/activeprofiles_stub.cpp
}

mac {
	QMAKE_LFLAGS += -framework Carbon -framework IOKit
}

# CONFIG += pgputil
pgputil {
	DEFINES += HAVE_PGPUTIL
	HEADERS += \
		$$PWD/pgputil.h \
		$$PWD/pgpkeydlg.h

	SOURCES += \
		$$PWD/pgputil.cpp \
		$$PWD/pgpkeydlg.cpp

	FORMS += \
		$$PWD/pgpkey.ui
}

INCLUDEPATH += $$PWD
DEPENDPATH += $$PWD
