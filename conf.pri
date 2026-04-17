# qconf

PREFIX = /usr/local
BINDIR = /usr/local/bin
DATADIR = /usr/local/share

# QCA 2.3.7 bundled at third-party/qca-qt5-install/ (moved from the
# previous /tmp/qca-install — /tmp is cleared on macOS reboot). The
# framework directory is checked into the repo so the build is
# reproducible and survives restarts.
# QCA_NO_PLUGINS removed so qca-ossl.dylib is loaded at runtime.
DEFINES += HAVE_CONFIG YANDEX_EXTENSIONS HAVE_OPENSSL
INCLUDEPATH += /usr/local/include /opt/local/include
INCLUDEPATH += $$PWD/third-party/qca-qt5-install/qca-qt5.framework/Headers
LIBS += -lssl -lcrypto -L/opt/local/lib -lz
LIBS += -F$$PWD/third-party/qca-qt5-install -framework qca-qt5
QMAKE_LFLAGS += -Wl,-rpath,@executable_path/../Frameworks
CONFIG -= qca-static
CONFIG += release c++17
QMAKE_MACOSX_DEPLOYMENT_TARGET = 11.0
PSI_DATADIR=/usr/local/share/yachat

