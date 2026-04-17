TEMPLATE = subdirs

include(conf.pri)
windows:include(conf_windows.pri)

# iris/ (the non-legacy, Qt5-style iris tree) is no longer compiled. The
# main app is built with `CONFIG += iris_legacy` in src/src.pri and links
# directly against iris-legacy/iris sources inline — there's no separate
# library output. We also no longer need qmake to configure iris/ nor to
# descend into it.
#
# qca-static is also no longer supported: QCA is bundled as a prebuilt
# framework at third-party/qca-qt5-install/qca-qt5.framework (see
# conf.pri) and linked via -F / -framework qca-qt5, not via in-tree
# compilation. Both dead branches removed.

sub_src.subdir = src

SUBDIRS += sub_src
