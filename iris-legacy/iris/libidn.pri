libidn {
	INCLUDEPATH += $$PWD
	DEPENDPATH  += $$PWD

	unix:{
		QMAKE_CFLAGS_WARN_ON -= -W
		# Bundled libidn vendored C: don't lint it. The big
		# RFC 3454 stringprep tables in rfc3454.c / profiles.c
		# use sparse designated initializers that trigger
		# -Wmissing-field-initializers ~970 times (Layer 7
		# audit). Appending to _WARN_ON puts it *after*
		# -Wall -Wextra in the final clang line so the
		# suppression wins.
		QMAKE_CFLAGS_WARN_ON   += -Wno-missing-field-initializers
		QMAKE_CXXFLAGS_WARN_ON += -Wno-missing-field-initializers
	}
	win32:{
		QMAKE_CFLAGS += -Zm400
	}

	SOURCES += \
		$$LIBIDN_BASE/profiles.c \
		#$$LIBIDN_BASE/toutf8.c \
		$$LIBIDN_BASE/rfc3454.c \
		$$LIBIDN_BASE/nfkc.c \
		$$LIBIDN_BASE/stringprep.c
}

