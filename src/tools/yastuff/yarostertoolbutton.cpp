/*
 * yarostertoolbutton.cpp
 * Copyright (C) 2010  Yandex LLC (Michail Pishchagin)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "yarostertoolbutton.h"

#include <QCoreApplication>
#include <QPainter>

#include "yaroster.h"
#include "shortcutmanager.h"

//----------------------------------------------------------------------------
// YaRosterToolButton
//----------------------------------------------------------------------------

YaRosterToolButton::YaRosterToolButton(QWidget* parent)
	: QToolButton(parent)
	, underMouse_(false)
{
	setAttribute(Qt::WA_Hover, true);
}

void YaRosterToolButton::setUnderMouse(bool underMouse)
{
	underMouse_ = underMouse;
	if (!underMouse) {
		setDown(false);
	}
	update();
}

void YaRosterToolButton::paintEvent(QPaintEvent*)
{
	QPainter p(this);
	// p.fillRect(rect(), Qt::red);

	QPixmap pixmap = icon().pixmap(size(), iconMode());

	if ((!isDown() && !underMouse() && !isChecked() && !underMouse_) ||
	    (!isEnabled() && !isChecked()) ||
	    (!pressable()))
	{
		p.setOpacity(0.5);
	}

	int yOffset = (isDown() ? 1 : 0);
	if (!pressable())
		yOffset = 0;

	if (!textVisible()) {
		p.drawPixmap((width() - pixmap.width()) / 2,
		             (height() - pixmap.height()) / 2 + yOffset,
		             pixmap);
	}
	else {
		QRect r = rect();
		r.setWidth(sizeHint().width());
		r.moveLeft((rect().width() - r.width()) / 2);

		p.drawPixmap(r.left(),
		             (r.height() - pixmap.height()) / 2 + yOffset,
		             pixmap);

		QFont f = p.font();
		f.setUnderline(true);
		p.setFont(f);
		if (!pressable())
			p.setPen(Qt::gray);
		else
			p.setPen(QColor(0x03, 0x94, 0x00));
		p.drawText(r.adjusted(0, yOffset, 0, yOffset),
		           Qt::AlignRight | Qt::AlignVCenter, text());
	}
}

QIcon::Mode YaRosterToolButton::iconMode() const
{
	return /* isEnabled() ? */ QIcon::Normal /* : QIcon::Disabled */;
}

bool YaRosterToolButton::textVisible() const
{
	return !text().isEmpty() && isEnabled();
}

void YaRosterToolButton::setCompactSize(const QSize& size)
{
	compactSize_ = size;

	QEvent e(QEvent::EnabledChange);
	QCoreApplication::instance()->sendEvent(this, &e);
}

QSize YaRosterToolButton::minimumSizeHint() const
{
	QSize sh = compactSize_;
	if (textVisible()) {
		// sh.setWidth(sh.width() + 5);
		sh.setWidth(sh.width() + fontMetrics().width(text()));
	}
	return sh;
}

QSize YaRosterToolButton::sizeHint() const
{
	return minimumSizeHint();
}

void YaRosterToolButton::changeEvent(QEvent* e)
{
	QToolButton::changeEvent(e);

	if (e->type() == QEvent::EnabledChange) {
		if (toolTip_.isEmpty() && !toolTip().isEmpty()) {
			toolTip_ = toolTip();
		}

		setToolTip(isEnabled() ? toolTip_ : QString());

		if (textVisible()) {
			setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
			setMinimumWidth(0);
			setMaximumWidth(1000);
			setFixedHeight(compactSize_.height());
		}
		else {
			// setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
			setFixedSize(compactSize_);
		}
		updateGeometry();

		if (isEnabled())
			setCursor(Qt::PointingHandCursor);
		else
			unsetCursor();
	}
}

bool YaRosterToolButton::pressable() const
{
	return true;
}

//----------------------------------------------------------------------------
// YaRosterAddContactToolButton
//----------------------------------------------------------------------------

YaRosterAddContactToolButton::YaRosterAddContactToolButton(QWidget* parent)
	: YaRosterToolButton(parent)
{
}

void YaRosterAddContactToolButton::init()
{
	setText(tr("Add a contact"));
	setCheckable(true);

	// contactsPageButton()->addButton()->setIcon(enabled ?
	//         QPixmap(":images/addcontactok.png") :
	//         QPixmap(":images/addcontact.png"));
	setIcon(QPixmap(":images/addcontact.png"));

	QList<QKeySequence> keys = ShortcutManager::instance()->shortcuts("appwide.add-contact");
	if (keys.isEmpty())
		setToolTip(tr("Add a contact"));
	else
		setToolTip(tr("Add a contact (%1)")
		           .arg(keys.first().toString(QKeySequence::NativeText)));
}

void YaRosterAddContactToolButton::setRoster(YaRoster* yaRoster)
{
	yaRoster_ = yaRoster;
	connect(yaRoster_, SIGNAL(availableAccountsChanged(bool)), SLOT(update()));
}

QIcon::Mode YaRosterAddContactToolButton::iconMode() const
{
	return pressable() ? QIcon::Normal : QIcon::Disabled;
}

bool YaRosterAddContactToolButton::pressable() const
{
	return yaRoster_ && yaRoster_->haveAvailableAccounts();
}
