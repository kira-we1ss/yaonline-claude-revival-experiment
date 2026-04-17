# qconf

PREFIX = /usr/local
BINDIR = /usr/local/bin
DATADIR = /usr/local/share

# QCA 2.3.7 — built at /tmp/qca-install with ossl plugin
# QCA_NO_PLUGINS removed so qca-ossl.dylib is loaded at runtime
DEFINES += HAVE_CONFIG YANDEX_EXTENSIONS HAVE_OPENSSL
INCLUDEPATH += /usr/local/include /opt/local/include
INCLUDEPATH += /tmp/qca-install/lib/qca-qt5.framework/Headers
LIBS += -lssl -lcrypto -L/opt/local/lib -lz
LIBS += -F/tmp/qca-install/lib -framework qca-qt5
QMAKE_LFLAGS += -Wl,-rpath,@executable_path/../Frameworks
CONFIG -= qca-static
CONFIG += release c++17
QMAKE_MACOSX_DEPLOYMENT_TARGET = 11.0
PSI_DATADIR=/usr/local/share/yachat

