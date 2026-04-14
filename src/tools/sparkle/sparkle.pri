SPARKLE_FRAMEWORK = /Library/Frameworks/Sparkle.framework
exists($$SPARKLE_FRAMEWORK) {
	DEFINES += HAVE_SPARKLE
	QMAKE_LFLAGS += -framework Sparkle

	INCLUDEPATH += $$PWD
	DEPENDPATH  += $$PWD
	OBJECTIVE_SOURCES += $$PWD/sparkle.mm
	HEADERS += $$PWD/sparkle.h
}
