/*
 * yacontactlabel.cpp - contact's name widget
 * Copyright (C) 2008  Yandex LLC (Michail Pishchagin)
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

#include "yacontactlabel.h"

#include <QTimer>
#include <QPainter>
#include <QPen>
#include <QBrush>

#include "psiaccount.h"
#include "psitooltip.h"
#include "textutil.h"
#include "vcardfactory.h"
#include "yacommon.h"
#include "yavisualutil.h"

static QPixmap servicePixmapForDomain(const QString& domain)
{
	static QPixmap spritePixmap;
	static QList<QPixmap> splitPixmaps;
	static QMap<QString, int> domains;
	if (spritePixmap.isNull()) {
		spritePixmap = QPixmap(":images/chat/sprite-16x16.png");
		if (!spritePixmap.width() || !spritePixmap.height())
			return QPixmap();

		int size = spritePixmap.width();
		Q_ASSERT(size > 0);
		int numPixmaps = spritePixmap.height() / size;
		for (int i = 0; i < numPixmaps; ++i) {
			QPixmap pix = spritePixmap.copy(0, i * size, size, size);
			splitPixmaps << pix;
		}

		// http://mail.yandex.ru/api/service_icons
		static QMap<QString, int> services;
		services["yaru"]        = 0;
		services["yandex-team"] = 1;
		services["mail.ru"]     = 2;
		services["qip"]         = 3;
		services["gmail"]       = 4;
		services["vkontakte"]   = 5;
		services["facebook"]    = 6;
		services["jabber.ru"]   = 7;

		QMap<QString, QString> tmpDomains;
		tmpDomains["ya.ru"] = "yaru";
		// also in yacommon.cpp: yandexTeamHostnameRegExp()
		tmpDomains["yandex-team.ru"] = "yandex-team";
		tmpDomains["yandex-team.com.ua"] = "yandex-team";
		tmpDomains["mrim.ya.ru"] = "mail.ru";
		tmpDomains["qip.ru"] = "qip";
		tmpDomains["gmail.com"] = "gmail";
		tmpDomains["vk.com"] = "vkontakte";
		tmpDomains["chat.facebook.com"] = "facebook";
		tmpDomains["jabber.ru"] = "jabber.ru";

		QMapIterator<QString, QString> it(tmpDomains);
		while (it.hasNext()) {
			it.next();
			domains[it.key()] = services[it.value()];
		}
	}

	if (domains.contains(domain)) {
		return splitPixmaps.at(domains[domain]);
	}

	return QPixmap();
}

YaContactLabel::YaContactLabel(QWidget* parent)
	: YaLabel(parent)
	, profile_(0)
{
	setEditable(false);
}

QString YaContactLabel::text() const
{
	if (!forcedText_.isEmpty()) {
		return forcedText_;
	}

	if (!profile_) {
		return Ya::ellipsis();
	}
	return Ya::contactName(profile_->name(), profile_->jid().full());
}

QPixmap YaContactLabel::servicePixmap() const
{
	if (!profile_)
		return QPixmap();
	return servicePixmapForDomain(profile_->jid().domain());
}

void YaContactLabel::setProfile(const YaProfile* profile)
{
	if (profile_ != profile) {
		profile_ = (YaProfile*)profile;
		if (profile_)
			connect(profile_, SIGNAL(nameChanged()), SLOT(update()));
		update();
	}
}

QString YaContactLabel::forcedText() const
{
	return forcedText_;
}

void YaContactLabel::setForcedText(const QString forcedText)
{
	forcedText_ = forcedText;
	updateGeometry();
}

const YaProfile* YaContactLabel::profile() const
{
	return profile_;
}

void YaContactLabel::paintEvent(QPaintEvent* e)
{
	if (!backgroundColor_.isValid()) {
		YaLabel::paintEvent(e);
		return;
	}

	QPainter p(this);
	QRect r = rect();
	QPixmap servicePixmap = this->servicePixmap();
	if (!servicePixmap.isNull()) {
		p.drawPixmap(r.left(), r.top() + (r.height() - servicePixmap.height())/2, servicePixmap);
		r.setLeft(r.left() + servicePixmap.width() + 2);
	}

	// p.setRenderHint(QPainter::TextAntialiasing, true);
	QFont f = p.font();
	// f.setStyleStrategy(QFont::PreferAntialias);
	p.setFont(f);
	p.setPen(textColor());
	p.drawText(r, Qt::AlignLeft | Qt::AlignVCenter, text());
	if (fontMetrics().width(text()) > r.width()) {
		Ya::VisualUtil::drawTextFadeOut(&p, r, backgroundColor_, 15);
	}
}

QColor YaContactLabel::backgroundColor() const
{
	return backgroundColor_;
}

void YaContactLabel::setBackgroundColor(QColor backgroundColor)
{
	backgroundColor_ = backgroundColor;
	update();
}
