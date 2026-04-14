# qconf

PREFIX = /usr/local
BINDIR = /usr/local/bin
DATADIR = /usr/local/share

DEFINES += QCA_NO_PLUGINS HAVE_OPENSSL HAVE_CONFIG YANDEX_EXTENSIONS
INCLUDEPATH += /usr/local/include /opt/local/include
LIBS += -lssl -lcrypto -L/opt/local/lib -lz
CONFIG += qca-static release c++17
QMAKE_MACOSX_DEPLOYMENT_TARGET = 11.0
PSI_DATADIR=/usr/local/share/yachat

