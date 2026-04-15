/*
 * groupchatcontact.cpp
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

#include "groupchatcontact.h"

#include <QMutableListIterator>

#include "psiaccount.h"
#include "userlist.h"
#include "yavisualutil.h"
#include "bookmarkmanager.h"
#include "groupchatcontactmenu.h"

static QStringList GROUPCHAT_CONTACT_GROUPS;

GroupchatContact::GroupchatContact(PsiAccount* parent)
	: PsiContact(UserListItem(), parent)
	, account_(parent)
	, bookmark_(QString(), XMPP::Jid(), false)
{
	setParent(parent);
	if (GROUPCHAT_CONTACT_GROUPS.isEmpty()) {
		GROUPCHAT_CONTACT_GROUPS << groupName();
	}

	setActive(XMPP::Status::GC_Inactive);
	setActive(XMPP::Status::GC_Favorited);
}

GroupchatContact::~GroupchatContact()
{
}

QString GroupchatContact::groupName()
{
	return tr("Favorite Groupchats");
}

const ConferenceBookmark& GroupchatContact::bookmark() const
{
	return bookmark_;
}

void GroupchatContact::setBookmark(const ConferenceBookmark& bookmark)
{
	if (bookmark_ != bookmark) {
		bookmark_ = bookmark;
		comparisonName_ = name().toLower();
		emit updated();
	}
}

XMPP::Status::Type GroupchatContact::active() const
{
	return active_;
}

void GroupchatContact::setActive(XMPP::Status::Type active)
{
	if (active_ != active) {
		active_ = active;
		status_ = XMPP::Status(active);
		emit updated();
	}
}

bool GroupchatContact::isEditable() const
{
	return !account_.isNull() && account_->isAvailable();
}

bool GroupchatContact::isDragEnabled() const
{
	return false;
}

bool GroupchatContact::isRemovable() const
{
	return isEditable();
}

bool GroupchatContact::inList() const
{
	return true;
}

bool GroupchatContact::isOnline() const
{
	return true;
}

bool GroupchatContact::isHidden() const
{
	return false;
}

QStringList GroupchatContact::groups() const
{
	return GROUPCHAT_CONTACT_GROUPS;
}

void GroupchatContact::remove()
{
	QList<ConferenceBookmark> conferences = account()->bookmarkManager()->conferences();
	QMutableListIterator<ConferenceBookmark> it(conferences);
	while (it.hasNext()) {
		ConferenceBookmark bookmark = it.next();
		if (bookmark.jid() == jid()) {
			it.remove();
			break;
		}
	}
	account()->bookmarkManager()->setBookmarks(conferences);
}

void GroupchatContact::setName(const QString& name)
{
	QList<ConferenceBookmark> conferences = account()->bookmarkManager()->conferences();
	QMutableListIterator<ConferenceBookmark> it(conferences);
	while (it.hasNext()) {
		ConferenceBookmark bookmark = it.next();
		if (bookmark.jid() == jid()) {
			bookmark.setName(name);
			it.setValue(bookmark);
			break;
		}
	}
	account()->bookmarkManager()->setBookmarks(conferences);
}

const QString& GroupchatContact::name() const
{
	return bookmark_.name();
}

const QString& GroupchatContact::comparisonName() const
{
	return comparisonName_;
}

XMPP::Status GroupchatContact::status() const
{
	return status_;
}

QIcon GroupchatContact::picture() const
{
	return Ya::VisualUtil::groupchatAvatar(active_);
}

const XMPP::Jid& GroupchatContact::jid() const
{
	return bookmark_.jid();
}

void GroupchatContact::activate()
{
	account_->actionJoin(bookmark_, true);
}

ContactListItemMenu* GroupchatContact::contextMenu(ContactListModel* model)
{
	if (!account())
		return 0;
	return new GroupchatContactMenu(this, model);
}
