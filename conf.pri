# qconf

PREFIX = /usr/local
BINDIR = /usr/local/bin
DATADIR = /usr/local/share

DEFINES += QCA_NO_PLUGINS HAVE_CONFIG YANDEX_EXTENSIONS
# HAVE_OPENSSL disabled: bundled qca-ossl is incompatible with OpenSSL 3.x
# Will be re-enabled in Task 13 when system QCA 2.3.x is used
INCLUDEPATH += /usr/local/include /opt/local/include
LIBS += -lssl -lcrypto -L/opt/local/lib -lz
CONFIG += qca-static release c++17
QMAKE_MACOSX_DEPLOYMENT_TARGET = 11.0
PSI_DATADIR=/usr/local/share/yachat

