include(../conf.pri)
windows:include(../conf_windows.pri)
CONFIG += iris_bundle

# don't build iris apps
CONFIG += no_tests

# use qca from psi if necessary
qca-static {
	DEFINES += QCA_STATIC
	INCLUDEPATH += $$PWD/../third-party/qca/qca/include/QtCrypto
}
else {
	CONFIG += crypto
}

# use zlib from psi if necessary
psi-zip {
	INCLUDEPATH += $$PWD/tools/zip/minizip/win32
}

mac {
	# Universal binaries
	qc_universal {
		CONFIG += x86 x86_64
		QMAKE_MAC_SDK=/Developer/SDKs/MacOSX10.5.sdk
		QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.5
	}
}
