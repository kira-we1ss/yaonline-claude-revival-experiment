#!/bin/bash

copy_private_header() {
	qt_path="$QTDIR/src/$1"
	yapsi_path="tools/yastuff/yawidgets/private/$(basename $1)"
	echo "Copying private header '$qt_path' '$yapsi_path'..."
	cp "$qt_path" "$yapsi_path"
}

copy_private_header "corelib/kernel/qobject_p.h"
copy_private_header "gui/itemviews/qabstractitemview_p.h"
copy_private_header "gui/itemviews/qtreeview_p.h"
copy_private_header "gui/kernel/qwidget_p.h"
copy_private_header "gui/text/qtextcontrol_p.h"
copy_private_header "gui/text/qtextcontrol_p_p.h"
copy_private_header "gui/widgets/qabstractscrollarea_p.h"
copy_private_header "gui/widgets/qeffects_p.h"
copy_private_header "gui/widgets/qframe_p.h"
