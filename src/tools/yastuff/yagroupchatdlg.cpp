/*
 * yagroupchatdlg.cpp
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

#include "yagroupchatdlg.h"

#include <QMessageBox>
#include <QMutableListIterator>

#include "yachatviewmodel.h"
#include "psiaccount.h"
#include "psicon.h"
#include "psioptions.h"
#include "yachatseparator.h"
#include "psiiconset.h"
#include "yagroupchatcontactlistmodel.h"
#include "capsmanager.h"
#include "textutil.h"
#include "mucconfigdlg.h"
#include "bookmarkmanager.h"
#include "yavisualutil.h"
#include "psievent.h"
#include "contactupdatesmanager.h"
#include "psitooltip.h"
#include "xmpp_discoinfotask.h"
#include "xmpp_discoitem.h"

YaGroupchatDlg::YaGroupchatDlg(const Jid& jid, PsiAccount* acc, TabManager* tabManager)
	: GCMainDlg(jid, acc, tabManager)
	, doTrackbar_(false)
	, contactStatusRecreated_(false)
	, subjectCanBeModified_(true)
{
	contactList_ = new YaGroupchatContactListModel(this);
	connect(contactList_, SIGNAL(insertNick(const QString&)), SLOT(insertNick(const QString&)));
}

YaGroupchatDlg::~YaGroupchatDlg()
{
}

void YaGroupchatDlg::initUi()
{
	// setAttribute(Qt::WA_DeleteOnClose, false);

	ui_.setupUi(this);
	ui_.contactList->setModel(contactList_);
	ui_.contactList->setAccount(account());
	ui_.contactList->setGroupchat(jid().full());
	for (int i = 0; i < contactList_->rowCount(); i++) {
		ui_.contactList->setExpanded(contactList_->index(i, 0), true);
	}

	recreateContactStatus();

	ui_.addContact->init();
	ui_.addContact->setCompactSize(QSize(16, 16));
	ui_.addContact->hide(); // TODO FIXME

	QList<int> list;
	list << 500;
	list << 180;
	ui_.splitter->setSizes(list);

//	YaChatContactStatus* contactStatus = new YaChatContactStatus(ui_.contactStatus->parentWidget());
//	replaceWidget(ui_.contactStatus, contactStatus);
//	ui_.contactStatus = contactStatus;

	ui_.contactInfo->setMode(YaChatContactInfoExtra::Button);

	ui_.contactInfo->setForcedIcon(QIcon(":images/window/buttons/academic/gear.png"));
	ui_.contactInfo->setForcedText(tr("Settings"));
	connect(ui_.contactInfo, SIGNAL(clicked()), SLOT(showContactProfile()));
	connect(ui_.contactInfo, SIGNAL(alternateClicked()), SLOT(showAlternateContactProfile()));

	connect(ui_.favoriteButton, SIGNAL(clicked()), SLOT(toggleFavorite()));
	connect(ui_.editSubjectButton, SIGNAL(clicked()), SLOT(editSubject()));
	connect(account()->bookmarkManager(), SIGNAL(conferencesChanged(const QList<ConferenceBookmark>&)), SLOT(updateFavorite()));
	updateFavorite();

	ui_.contactUserpic->setForcedIcon(Ya::VisualUtil::groupchatAvatar(XMPP::Status::GC_Active));
	ui_.contactName->setForcedText(jid().bare());

	ui_.chatView->setModel(model());
	ui_.chatView->setGroupchatMode(true);

	ui_.contactName->setMinimumSize(10, 30);

	connect(ui_.bottomFrame->separator(), SIGNAL(textSelected(QString)), SLOT(addEmoticon(QString)));
	connect(ui_.bottomFrame->separator(), SIGNAL(sendFile()), SLOT(uploadFile()));

	{
		if (PsiIconset::instance()->yaEmoticonSelectorIconset()) {
			ui_.bottomFrame->separator()->setIconset(*PsiIconset::instance()->yaEmoticonSelectorIconset());
		}
	}

	ui_.bottomFrame->setSendAction(actionSend());

	PsiToolTip::install(ui_.favoriteButton);
	PsiToolTip::install(ui_.editSubjectButton);
	ui_.favoriteButton->setToolTip(tr("Add to Favorites"));
	ui_.editSubjectButton->setToolTip(tr("Edit Subject"));

	resize(sizeHint());
	doClear();
}

void YaGroupchatDlg::recreateContactStatus()
{
	if (contactStatusRecreated_)
		return;

	setUpdatesEnabled(false);

	// YaSelfMood must always be created the last, otherwise interaction with its
	// expanding menus will be severely limited on non-osx platforms
	YaSelfMood *selfMoodOld = ui_.contactStatus;
	ui_.contactStatus = new YaSelfMood(selfMoodOld->parentWidget(), true);
	replaceWidget(selfMoodOld, ui_.contactStatus);
	ui_.contactStatus->raiseExtraInWidgetStack();
	connect(ui_.contactStatus, SIGNAL(statusChanged(const QString&)), SLOT(subjectChanged()));
	setConfigureEnabled(configureEnabled());
	subjectChanged(currentSubject());
	ui_.contactStatus->ensureVisible();

	setUpdatesEnabled(true);
}

void YaGroupchatDlg::error(int, const QString& str)
{
	if (gcState() == GC_Connecting)
		appendSysMsg(tr("Unable to join groupchat.  Reason: %1").arg(str), true);
	else
		appendSysMsg(tr("Unexpected groupchat error: %1").arg(str), true);

	setGcState(GC_Idle);
}

void YaGroupchatDlg::presence(const QString& nick, const Status& s)
{
	if (s.hasError()) {
		QString message;
		if (s.errorCode() == 409) {
			message = tr("Please choose a different nickname");
			nickChangeFailure();
		}
		else {
			message = tr("An error occurred");
		}

		appendSysMsg(message, false, QDateTime::currentDateTime());
		return;
	}

	if ((nick.isEmpty()) && (s.getMUCStatuses().contains(100))) {
		setNonAnonymous(true);
	}

	PsiOptions* options_ = PsiOptions::instance();

	if (s.isAvailable()) {
		// Available
		if (s.getMUCStatuses().contains(201)) {
			appendSysMsg(tr("New room created"), false, QDateTime::currentDateTime());
			mucManager()->setDefaultConfiguration();
		}

		QStandardItem* contact = contactList_->findEntry(nick);
		if (!contact) {
			//contact joining
			if (!isConnecting() && options_->getOption("options.muc.show-joins").toBool()) {
				QString message = tr("%1 has joined the room");

				if (options_->getOption("options.muc.show-role-affiliation").toBool()) {
					if (s.mucItem().role() != MUCItem::NoRole) {
						if (s.mucItem().affiliation() != MUCItem::NoAffiliation) {
							message = tr("%3 has joined the room as %1 and %2")
							          .arg(MUCManager::roleToString(s.mucItem().role(), true))
							          .arg(MUCManager::affiliationToString(s.mucItem().affiliation(), true));
						}
						else {
							message = tr("%2 has joined the room as %1")
							          .arg(MUCManager::roleToString(s.mucItem().role(), true));
						}
					}
					else if (s.mucItem().affiliation() != MUCItem::NoAffiliation) {
						message = tr("%2 has joined the room as %1")
						          .arg(MUCManager::affiliationToString(s.mucItem().affiliation(), true));
					}
				}
				if (!s.mucItem().jid().isEmpty()) {
					message = message
					          .arg(QString("%1 (%2)")
					               .arg(nick)
					               .arg(s.mucItem().jid().bare()));
				}
				else {
					message = message.arg(nick);
				}
				appendSysMsg(message, false, QDateTime::currentDateTime());
			}
		}
		else {
			// Status change
			if (!isConnecting() && options_->getOption("options.muc.show-role-affiliation").toBool()) {
				QString message;
				if (YaGroupchatContactListModel::status(contact).mucItem().role() != s.mucItem().role() && s.mucItem().role() != MUCItem::NoRole) {
					if (YaGroupchatContactListModel::status(contact).mucItem().affiliation() != s.mucItem().affiliation()) {
						message = tr("%1 is now %2 and %3")
						          .arg(nick)
						          .arg(MUCManager::roleToString(s.mucItem().role(), true))
						          .arg(MUCManager::affiliationToString(s.mucItem().affiliation(), true));
					}
					else {
						message = tr("%1 is now %2")
						          .arg(nick)
						          .arg(MUCManager::roleToString(s.mucItem().role(), true));
					}
				}
				else if (YaGroupchatContactListModel::status(contact).mucItem().affiliation() != s.mucItem().affiliation()) {
					message += tr("%1 is now %2")
					           .arg(nick)
					           .arg(MUCManager::affiliationToString(s.mucItem().affiliation(), true));
				}

				if (!message.isEmpty()) {
					appendSysMsg(message, false, QDateTime::currentDateTime());
				}
			}

			if (!isConnecting() && options_->getOption("options.muc.show-status-changes").toBool()) {
				if (s.status() != YaGroupchatContactListModel::status(contact).status() || s.show() != YaGroupchatContactListModel::status(contact).show()) {
					QString message;
					QString st;
					if (s.show().isEmpty())
						st = tr("online");
					else
						st = s.show();
					message = tr("%1 is now %2").arg(nick).arg(st);
					if (!s.status().isEmpty())
						message += QString(" (%1)").arg(s.status());
					appendSysMsg(message, false, QDateTime::currentDateTime());
				}
			}
		}

		contactList_->updateEntry(nick, s);
	}
	else {
		// Unavailable
		if (s.hasMUCDestroy()) {
			// Room was destroyed
			QString message = tr("This room has been destroyed.");
			if (!s.mucDestroy().reason().isEmpty()) {
				message += "\n";
				message += tr("Reason: %1")
				           .arg(s.mucDestroy().reason());
			}
			if (!s.mucDestroy().jid().isEmpty()) {
				message += "\n";
				message += tr("Do you want to join the alternate venue '%1' ?")
				           .arg(s.mucDestroy().jid().full());
				int ret = QMessageBox::information(this, tr("Room Destroyed"), message, QMessageBox::Yes, QMessageBox::No);
				if (ret == QMessageBox::Yes) {
					account()->actionJoin(s.mucDestroy().jid().full());
				}
			}
			else {
				QMessageBox::information(this, tr("Room Destroyed"), message);
			}
			doForcedLeave();
		}

		QString message;
		QString nickJid;
		QStandardItem* contact = contactList_->findEntry(nick);
		if (contact && !YaGroupchatContactListModel::status(contact).mucItem().jid().isEmpty()) {
			nickJid = QString("%1 (%2)")
			          .arg(nick)
			          .arg(YaGroupchatContactListModel::status(contact).mucItem().jid().bare());
		}
		else {
			nickJid = nick;
		}

		bool suppressDefault = false;

		if (s.getMUCStatuses().contains(301)) {
			// Ban
			mucKickMsgHelper(nick, s, nickJid, tr("Banned"), tr("You have been banned from the room"),
						 tr("You have been banned from the room by %1"),
						 tr("%1 has been banned"),
						 tr("%1 has been banned by %2"));
			suppressDefault = true;
		}
		if (s.getMUCStatuses().contains(307)) {
			// Kick
			mucKickMsgHelper(nick, s, nickJid, tr("Kicked"), tr("You have been kicked from the room"),
						  tr("You have been kicked from the room by %1"),
						  tr("%1 has been kicked"),
						  tr("%1 has been kicked by %2"));
			suppressDefault = true;
		}
		if (s.getMUCStatuses().contains(321)) {
			// Remove due to affiliation change
			mucKickMsgHelper(nick, s, nickJid, tr("Removed"),
						 tr("You have been removed from the room due to an affiliation change"),
						 tr("You have been removed from the room by %1 due to an affiliation change"),
						 tr("%1 has been removed from the room due to an affilliation change"),
						 tr("%1 has been removed from the room by %2 due to an affilliation change"));
			suppressDefault = true;
		}
		if (s.getMUCStatuses().contains(322)) {
			mucKickMsgHelper(nick, s, nickJid, tr("Removed"),
						 tr("You have been removed from the room because the room was made members only"),
						 tr("You have been removed from the room by %1 because the room was made members only"),
						 tr("%1 has been removed from the room because the room was made members-only"),
						 tr("%1 has been removed from the room by %2 because the room was made members-only"));
			suppressDefault = true;
		}

		if ( !isConnecting() && !suppressDefault && options_->getOption("options.muc.show-joins").toBool() ) {
			if (s.getMUCStatuses().contains(303)) {
				message = tr("%1 is now known as %2").arg(nick).arg(s.mucItem().nick());
				contactList_->updateEntry(nick, s, s.mucItem().nick());
			} else {
				//contact leaving
				message = tr("%1 has left the room").arg(nickJid);
				if (!s.status().isEmpty()) {
					message += QString(" (%1)").arg(s.status());
				}
			}
			appendSysMsg(message, false, QDateTime::currentDateTime());
		}
		contactList_->removeEntry(nick);
	}

	if (!s.capsNode().isEmpty()) {
		Jid caps_jid(s.mucItem().jid().isEmpty() || !nonAnonymous() ? Jid(jid()).withResource(nick) : s.mucItem().jid());
		account()->capsManager()->updateCaps(caps_jid, s.capsNode(), s.capsVersion(), s.capsExt());
	}

	if (nick == this->nick()) {
		// Update configuration dialog
		configDlgUpdateSelfAffiliation();
		setConfigureEnabled(s.mucItem().affiliation() >= MUCItem::Member);
	}
}

void YaGroupchatDlg::message(const Message& _m)
{
	Message m = _m;
	QString from = m.from().resource();
	bool alert = false;

	if (hasMucMessage(m)) {
		return;
	}
	appendMucMessage(m);

	if (!m.subject().isEmpty()) {
		subjectChanged(m.subject());
		if (m.body().isEmpty()) {
			if (!from.isEmpty())
				m.setBody(QString("/me ") + tr("has set the topic to: %1").arg(m.subject()));
			else
				// The topic was set by the server
				m.setBody(tr("The topic has been set to: %1").arg(m.subject()));
		}
	}

	if (m.body().isEmpty())
		return;

	// code to determine if the speaker was addressing this client in chat
	if (m.body().contains(this->nick()))
		alert = true;

	if (m.body().left(this->nick().length()) == this->nick())
		setLastReferrer(m.from().resource());

	// if (option.gcHighlighting) {
	// 	for (QStringList::Iterator it = option.gcHighlights.begin();it != option.gcHighlights.end();it++) {
	// 		if (m.body().contains((*it), Qt::CaseInsensitive)) {
	// 			alert = true;
	// 		}
	// 	}
	// }

	// play sound?
	if (from == this->nick()) {
		if (!m.spooled())
			account()->playSound(eSend);
	}
	else {
#ifndef YAPSI
		if (alert || (!option.noGCSound && !m.spooled() && !from.isEmpty()))
			account()->playSound(eChat2);
#endif

		if (alert && !m.spooled() && m.subject().isEmpty()) {
			XMPP::Jid j;
			j.set(jid().domain(), jid().node(), from);
			GroupchatAlertEvent* event = new GroupchatAlertEvent(j, m.body(), account());
			emit eventCreated(event);
		}
	}

	QString error;
	if (m.error().condition != XMPP::Stanza::Error::UndefinedCondition) {
		if (from.isEmpty()) {
			from = this->nick();
		}
		error = m.error().description().first;
	}

	if (from.isEmpty()) {
		appendSysMsg(m.body(), alert, m.timeStamp());
	}
	else {
		if (from != this->nick() && !m.spooled()) {
			doTrackbar();
		}

		ChatDlgBase::SpooledType spooledType = m.spooled() ?
		                                       ChatDlgBase::Spooled_OfflineStorage :
		                                       ChatDlgBase::Spooled_None;

		QStandardItem* contact = contactList_->findEntry(from);
		if (!contact) {
			XMPP::Status dummy;
			contactList_->updateEntry(from, dummy);
			contact = contactList_->findEntry(from);
		}
		Q_ASSERT(contact);

		bool emote = false;
		if(m.body().left(4) == "/me ")
			emote = true;

		QString txt;
		if(emote)
			txt = TextUtil::plain2rich(m.body().mid(4));
		else
			txt = TextUtil::plain2rich(m.body());

		txt = TextUtil::linkify(txt);

		txt = TextUtil::emoticonify(txt);
		if( PsiOptions::instance()->getOption("options.ui.chat.legacy-formatting").toBool() )
			txt = TextUtil::legacyFormat(txt);

		model()->addGroupchatMessage(static_cast<YaChatViewModel::SpooledType>(spooledType), m.timeStamp(),
			from, contact ? contact->data(YaGroupchatContactListModel::IdRole).toInt() : -1,
			txt, emote, error, 0);

		if (!isActiveTab()) {
			setUnreadMessageCount(unreadMessageCount() + 1);
		}
	}
}

void YaGroupchatDlg::appendSysMsg(const QString& str, bool alert, const QDateTime &ts)
{
	doTrackbar();
	GCMainDlg::appendSysMsg(str, alert, ts);
}

QString YaGroupchatDlg::getDisplayName() const
{
	return ui_.contactName->forcedText();
}

ChatViewClass* YaGroupchatDlg::chatView() const
{
	return ui_.chatView;
}

ChatEdit* YaGroupchatDlg::chatEdit() const
{
	return ui_.bottomFrame->chatEdit();
}

void YaGroupchatDlg::configDlgUpdateSelfAffiliation()
{
	if (configDlg()) {
		QStandardItem* contact = contactList_->findEntry(this->nick());
		configDlg()->setRoleAffiliation(YaGroupchatContactListModel::status(contact).mucItem().role(),
		                                YaGroupchatContactListModel::status(contact).mucItem().affiliation());
	}
}

QStringList YaGroupchatDlg::nickList() const
{
	return contactList_->nickList();
}

void YaGroupchatDlg::ban(const QString& nick)
{
	QStandardItem* contact = contactList_->findEntry(nick);
	mucManager()->ban(YaGroupchatContactListModel::status(contact).mucItem().jid());
}

void YaGroupchatDlg::changeRole(const QString& nick, XMPP::MUCItem::Role role)
{
	QStandardItem* contact = contactList_->findEntry(nick);
	if (YaGroupchatContactListModel::status(contact).mucItem().role() != role)
		mucManager()->setRole(nick, role);
}

void YaGroupchatDlg::updateFavorite()
{
	bool favorited = false;
	foreach(const ConferenceBookmark bookmark, account()->bookmarkManager()->conferences()) {
		if (bookmark.jid() == jid()) {
			favorited = true;
			break;
		}
	}
	ui_.favoriteButton->setChecked(favorited);
}

void YaGroupchatDlg::toggleFavorite()
{
	bool favorited = ui_.favoriteButton->isChecked();
	QList<ConferenceBookmark> conferences = account()->bookmarkManager()->conferences();
	QMutableListIterator<ConferenceBookmark> it(conferences);
	while (it.hasNext()) {
		ConferenceBookmark bookmark = it.next();
		if (bookmark.jid() == jid()) {
			it.remove();
			break;
		}
	}

	if (favorited) {
		ConferenceBookmark bookmark(getDisplayName(), jid(), false, nick(), password());
		conferences << bookmark;
	}

	account()->bookmarkManager()->setBookmarks(conferences);
}

void YaGroupchatDlg::editSubject()
{
	ui_.contactStatus->setCustomMood();
}

void YaGroupchatDlg::setConfigureEnabled(bool enabled)
{
	GCMainDlg::setConfigureEnabled(enabled);

	bool changeSubjectEnabled = gcState() == GC_Connected;
	changeSubjectEnabled &= configureEnabled() || subjectCanBeModified_;
	ui_.editSubjectButton->setEnabled(changeSubjectEnabled);
	ui_.contactStatus->setStatusType(changeSubjectEnabled ? XMPP::Status::Online : XMPP::Status::Offline);
}

void YaGroupchatDlg::subjectChanged(const QString& subject)
{
	GCMainDlg::subjectChanged(subject);
	ui_.contactStatus->setStatusText(subject);
}

void YaGroupchatDlg::subjectChanged()
{
	if (currentSubject() != ui_.contactStatus->statusText()) {
		setSubject(ui_.contactStatus->statusText());
	}
}

void YaGroupchatDlg::showContactProfile()
{
	configureRoom();
}

void YaGroupchatDlg::showAlternateContactProfile()
{
	// TODO: ???
}

void YaGroupchatDlg::deactivated()
{
	GCMainDlg::deactivated();
	doTrackbar_ = true;
}

void YaGroupchatDlg::activated()
{
	GCMainDlg::activated();
	doTrackbar_ = false;

	account()->psi()->contactUpdatesManager()->groupchatActivated(account(), jid());
	recreateContactStatus();
	contactStatusRecreated_ = true;
}

void YaGroupchatDlg::doTrackbar()
{
	if (!doTrackbar_)
		return;

	doTrackbar_ = false;
	model()->doTrackbar();
}

void YaGroupchatDlg::chatEditCreated()
{
	GCMainDlg::chatEditCreated();
	ui_.bottomFrame->separator()->setChatWidgets(chatEdit(), chatView());
}

void YaGroupchatDlg::doJoined()
{
	GCMainDlg::doJoined();
	delete getRoomTitleTask_;
	getRoomTitleTask_ = new XMPP::DiscoInfoTask(account()->client()->rootTask());
	connect(getRoomTitleTask_, SIGNAL(finished()), SLOT(discoInfoFinished()));
	getRoomTitleTask_->get(jid().bare());
	getRoomTitleTask_->go();
}

void YaGroupchatDlg::discoInfoFinished()
{
	title_ = QString();
	XMPP::DiscoInfoTask* task = static_cast<XMPP::DiscoInfoTask*>(sender());
	foreach(const XMPP::DiscoItem::Identity& i, task->item().identities()) {
		if (!i.name.isEmpty()) {
			title_ = i.name;
			break;
		}
	}

	foreach(const XMPP::XData::Field& f, task->xdata().fields()) {
		if (f.var() == "muc#roomconfig_changesubject") {
			subjectCanBeModified_ = f.value().join("").toInt() > 0;
			setConfigureEnabled(configureEnabled());
			break;
		}
	}

	updateRoomTitle();
}

void YaGroupchatDlg::updateRoomTitle()
{
	QString title = bookmarkName_;
	if (title.isEmpty())
		title = title_;
	if (title.isEmpty())
		title = jid().bare();

	ui_.contactName->setForcedText(title);
	emit invalidateTabInfo();
}

void YaGroupchatDlg::setTitle(const QString& title)
{
	delete getRoomTitleTask_;
	GCMainDlg::setTitle(title);
	title_ = title;
	updateRoomTitle();
}

void YaGroupchatDlg::setBookmarkName(const QString& bookmarkName)
{
	GCMainDlg::setBookmarkName(bookmarkName);
	bookmarkName_ = bookmarkName;
	updateRoomTitle();
}
