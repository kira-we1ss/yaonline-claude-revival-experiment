#ifndef YACHAT_YAPSI_REVISION_H
#define YACHAT_YAPSI_REVISION_H

#include <QString>

#if defined(__has_include)
#  if __has_include("tools/yastuff/yapsi_revision.h")
#    include "tools/yastuff/yapsi_revision.h"
#  else
static QString YAPSI_VERSION = QString::fromLatin1("3.2.2");
static const int YAPSI_REVISION = 0;
#  endif
#else
static QString YAPSI_VERSION = QString::fromLatin1("3.2.2");
static const int YAPSI_REVISION = 0;
#endif

#endif
