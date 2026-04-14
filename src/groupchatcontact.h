/*
 * groupchatcontact.h
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

#ifndef GROUPCHATCONTACT_H
#define GROUPCHATCONTACT_H

#include "psicontact.h"
#include "conferencebookmark.h"
#include "xmpp_status.h"
#include <QPointer>

class GroupchatContact : public PsiContact
{
	Q_OBJECT
public:
	GroupchatContact(PsiAccount* parent);
	~GroupchatContact();

	const ConferenceBookmark& bookmark() const;
	void setBookmark(const ConferenceBookmark& bookmark);

	XMPP::Status::Type active() const;
	void setActive(XMPP::Status::Type active);

	// reimplemented
	bool isEditable() const;
	bool isDragEnabled() const;
	bool isRemovable() const;
	bool inList() const;
	bool isOnline() const;
	bool isHidden() const;
	QStringList groups() const;
	void remove();
	const QString& name() const;
	const QString& comparisonName() const;
	void setName(const QString& name);
	virtual XMPP::Status status() const;
	virtual QIcon picture() const;
	virtual const XMPP::Jid& jid() const;
	virtual void activate();
	virtual ContactListItemMenu* contextMenu(ContactListModel* model);

	static QString groupName();

private:
	QPointer<PsiAccount> account_;
	ConferenceBookmark bookmark_;
	XMPP::Status::Type active_;
	XMPP::Status status_;
	QString comparisonName_;
};

#endif
