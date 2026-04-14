INCLUDEPATH += $$PWD
DEPENDPATH += $$PWD
DEFINES += YAPSI
# DEFINES += YAPSI_DEV
# DEFINES += DEFAULT_XMLCONSOLE
# DEFINES += YAPSI_NO_STYLESHEETS
# DEFINES += YAPSI_STRESSTEST_ACCOUNTS

DEFINES += USE_GENERAL_CONTACT_GROUP

include($$PWD/syntaxhighlighters/syntaxhighlighters.pri)
include($$PWD/../slickwidgets/slickwidgets.pri)
include($$PWD/../smoothscrollbar/smoothscrollbar.pri)
include($$PWD/../animationhelpers/animationhelpers.pri)
include($$PWD/../cutejson/cutejson.pri)
include($$PWD/../../../third-party/JsonQt/jsonqt.pri)

# there's also a section in src.pro
yapsi_activex_server {
	DEFINES += YAPSI_ACTIVEX_SERVER

	HEADERS += \
		$$PWD/yapsiserver.h \
		$$PWD/yaonline.h \
		$$PWD/ycuapiwrapper.h \
		$$PWD/yaaddcontacthelper.h

	SOURCES += \
		$$PWD/yapsiserver.cpp \
		$$PWD/yaonline.cpp \
		$$PWD/ycuapiwrapper.cpp \
		$$PWD/yaaddcontacthelper.cpp
}

debug {
	MODELTEST_PRI = $$PWD/../../../../../modeltest/modeltest.pri
	exists($$MODELTEST_PRI) {
		include($$MODELTEST_PRI)
		DEFINES += MODELTEST
	}
}

HEADERS += \
	$$PWD/yaprofile.h \
	$$PWD/yarostertooltip.h \
	$$PWD/yachattooltip.h \
	$$PWD/yachattiplabel.h \
	$$PWD/yarostertiplabel.h \
	$$PWD/yamainwin.h \
	$$PWD/yaonlinemainwin.h \
	$$PWD/yaroster.h \
	$$PWD/yarostertoolbutton.h \
	$$PWD/yachatdlg.h \
	$$PWD/yachatdlgshared.h \
	$$PWD/yagroupchatdlg.h \
	$$PWD/yagroupchatroomlist.h \
	$$PWD/yagroupchatcombobox.h \
	$$PWD/yagroupchatcontactlistmodel.h \
	$$PWD/yagroupchatcontactlistview.h \
	$$PWD/yagroupchatcontactlistmenu.h \
	$$PWD/yatrayicon.h \
	$$PWD/yaeventnotifier.h \
	$$PWD/yatabbednotifier.h \
	$$PWD/yaloginpage.h \
	$$PWD/yacontactlistmodel.h \
	$$PWD/yacontactlistmodelselection.h \
	$$PWD/fakegroupcontact.h \
	$$PWD/yainformersmodel.h \
	$$PWD/yacontactlistcontactsmodel.h \
	$$PWD/yacommon.h \
	$$PWD/yastyle.h \
	$$PWD/yatoster.h \
	$$PWD/yapopupnotification.h \
	$$PWD/yaabout.h \
	$$PWD/yapreferences.h \
	$$PWD/yaprivacymanager.h \
	$$PWD/yaipc.h \
	$$PWD/yaremoveconfirmationmessagebox.h \
	$$PWD/yatoastercentral.h \
	$$PWD/yadayuse.h \
	$$PWD/yalicense.h \
	$$PWD/delayedvariablebase.h \
	$$PWD/delayedvariable.h \
	$$PWD/yahistorycachemanager.h \
	$$PWD/yalogeventsmanager.h \
	$$PWD/yaunreadmessagesmanager.h \
	$$PWD/yaexception.h \
	$$PWD/yadebugconsole.h \
	$$PWD/yatokenauth.h \
	$$PWD/yanaroddiskmanager.h \
	$$PWD/yatransportmanager.h \
	$$PWD/yamrimtransport.h \
	$$PWD/yaj2jtransport.h \
	$$PWD/yamucmanager.h \
	$$PWD/yapddmanager.h

SOURCES += \
	$$PWD/yaprofile.cpp \
	$$PWD/yarostertooltip.cpp \
	$$PWD/yachattooltip.cpp \
	$$PWD/yachattiplabel.cpp \
	$$PWD/yarostertiplabel.cpp \
	$$PWD/yamainwin.cpp \
	$$PWD/yaonlinemainwin.cpp \
	$$PWD/yaroster.cpp \
	$$PWD/yarostertoolbutton.cpp \
	$$PWD/yachatdlg.cpp \
	$$PWD/yachatdlgshared.cpp \
	$$PWD/yagroupchatdlg.cpp \
	$$PWD/yagroupchatroomlist.cpp \
	$$PWD/yagroupchatcombobox.cpp \
	$$PWD/yagroupchatcontactlistmodel.cpp \
	$$PWD/yagroupchatcontactlistview.cpp \
	$$PWD/yagroupchatcontactlistmenu.cpp \
	$$PWD/yatrayicon.cpp \
	$$PWD/yaeventnotifier.cpp \
	$$PWD/yatabbednotifier.cpp \
	$$PWD/yaloginpage.cpp \
	$$PWD/yacontactlistmodel.cpp \
	$$PWD/yacontactlistmodelselection.cpp \
	$$PWD/fakegroupcontact.cpp \
	$$PWD/yainformersmodel.cpp \
	$$PWD/yacontactlistcontactsmodel.cpp \
	$$PWD/yacommon.cpp \
	$$PWD/yastyle.cpp \
	$$PWD/yatoster.cpp \
	$$PWD/yapopupnotification.cpp \
	$$PWD/yaabout.cpp \
	$$PWD/yapreferences.cpp \
	$$PWD/yaprivacymanager.cpp \
	$$PWD/yaipc.cpp \
	$$PWD/yaremoveconfirmationmessagebox.cpp \
	$$PWD/yatoastercentral.cpp \
	$$PWD/yadayuse.cpp \
	$$PWD/yalicense.cpp \
	$$PWD/delayedvariablebase.cpp \
	$$PWD/yahistorycachemanager.cpp \
	$$PWD/yalogeventsmanager.cpp \
	$$PWD/yaunreadmessagesmanager.cpp \
	$$PWD/yaexception.cpp \
	$$PWD/yadebugconsole.cpp \
	$$PWD/yatokenauth.cpp \
	$$PWD/yanaroddiskmanager.cpp \
	$$PWD/yatransportmanager.cpp \
	$$PWD/yamrimtransport.cpp \
	$$PWD/yaj2jtransport.cpp \
	$$PWD/yamucmanager.cpp \
	$$PWD/yapddmanager.cpp

RESOURCES += \
	$$PWD/yastuff.qrc \
	$$PWD/yaiconsets.qrc

FORMS += \
	$$PWD/yaloginpage.ui \
	$$PWD/yamainwindow.ui \
	$$PWD/yachatdialog.ui \
	$$PWD/yagroupchatdialog.ui \
	$$PWD/yarostertiplabel.ui \
	$$PWD/yaabout.ui \
	$$PWD/yapreferences.ui \
	$$PWD/yalicense.ui \
	$$PWD/yadebugconsole.ui
