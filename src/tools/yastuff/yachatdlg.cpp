/*
 * yachatdlg.cpp - custom chat dialog
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

#include <QPainter>
#include <QIcon>
#include <QTextDocument> // for Qt::escape()
#include <QMouseEvent>
#include <QMimeData>

#include "yachatdlg.h"
#include "yatrayicon.h"

#include "iconselect.h"
#include "textutil.h"
#include "iconselect.h"
#include "yacommon.h"
#include "yacontactlabel.h"
#include "yaselflabel.h"
#include "psicon.h"
#include "psicontact.h"
#include "psicon.h"
#include "psiaccount.h"
#include "userlist.h"
#include "yachattooltip.h"
#include "iconset.h"
#include "yachatviewmodel.h"
#include "globaleventqueue.h"
#include "xmpp_message.h"
#include "yavisualutil.h"
#include "yaprivacymanager.h"
#include "applicationinfo.h"
#include "tabdlg.h"
#include "yapushbutton.h"
#include "psioptions.h"
#include "psiiconset.h"
#include "deliveryconfirmationmanager.h"
#include "yahistorycachemanager.h"
#include "psilogger.h"
#include "vcardfactory.h"
#include "xmpp_yadatetime.h"
#include "yachatseparator.h"
#include "msgmle.h"
#include "yanaroddiskmanager.h"
#include "xmpp_xmlcommon.h"

//----------------------------------------------------------------------------
// YaChatDlg
//----------------------------------------------------------------------------

YaChatDlg::YaChatDlg(const Jid& jid, PsiAccount* acc, TabManager* tabManager)
	: ChatDlg(jid, acc, tabManager)
	, selfProfile_(YaProfile::create(acc))
	, contactProfile_(YaProfile::create(acc, jid))
	, showAuthButton_(true)
{
	connect(this, SIGNAL(invalidateTabInfo()), SLOT(updateComposingMessage()));
}

YaChatDlg::~YaChatDlg()
{
	delete fakeContact_;
}

void YaChatDlg::initUi()
{
	ui_.setupUi(this);

	YaChatContactStatus* contactStatus = new YaChatContactStatus(ui_.contactStatus->parentWidget());
	replaceWidget(ui_.contactStatus, contactStatus);
	ui_.contactStatus = contactStatus;

	// connect(ui_.mle, SIGNAL(textEditCreated(QTextEdit*)), SLOT(chatEditCreated()));
	chatEditCreated();

	ui_.contactInfo->setMode(YaChatContactInfoExtra::Button);

	connect(ui_.contactInfo, SIGNAL(clicked()), SLOT(showContactProfile()));
	connect(ui_.contactInfo, SIGNAL(alternateClicked()), SLOT(showAlternateContactProfile()));

	ui_.contactUserpic->setProfile(contactProfile_);
	ui_.contactName->setProfile(contactProfile_);
	ui_.chatView->setModel(model());
	QTimer::singleShot(0, this, SLOT(updateContactName()));

	ui_.contactName->setMinimumSize(100, 30);

	connect(ui_.bottomFrame->separator(), SIGNAL(textSelected(QString)), SLOT(addEmoticon(QString)));
	connect(ui_.bottomFrame->separator(), SIGNAL(addContact()), SLOT(addContact()));
	connect(ui_.bottomFrame->separator(), SIGNAL(authContact()), SLOT(authContact()));
	connect(ui_.bottomFrame->separator(), SIGNAL(sendFile()), SLOT(uploadFile()));

	{
		if (PsiIconset::instance()->yaEmoticonSelectorIconset()) {
			ui_.bottomFrame->separator()->setIconset(*PsiIconset::instance()->yaEmoticonSelectorIconset());
		}
	}

	ui_.bottomFrame->setSendAction(actionSend());

	resize(sizeHint());
	doClear();
}

void YaChatDlg::restoreLastMessages()
{
	account()->psi()->yaHistoryCacheManager()->getMessagesFor(account(), jid(), this, "retrieveHistoryFinished");

	// we need to run this after PsiAccount::processChatsHelper() finishes its work
	// so we won't end up with dupes in chatlog
	QTimer::singleShot(0, this, SLOT(retrieveHistoryFinishedHelper()));
}

void YaChatDlg::retrieveHistoryFinished()
{
	retrieveHistoryFinishedHelper();
	model()->setHistoryReceived(true);
}

void YaChatDlg::retrieveHistoryFinishedHelper()
{
	SpooledType spooled = model()->historyReceived() ? Spooled_Sync : Spooled_History;

	QList<YaHistoryCacheManager::Message> list = account()->psi()->yaHistoryCacheManager()->getCachedMessagesFor(account(), jid());
	qSort(list.begin(), list.end(), yaHistoryCacheManagerMessageMoreThan);
	foreach(const YaHistoryCacheManager::Message& msg, list) {
		if (!msg.isMood) {
			if (isEmoteText(msg.body))
				appendEmoteMessage(spooled, msg.timeStamp, msg.originLocal, false, QString(), XMPP::ReceiptNone, messageText(msg.body, true), msg.timeStamp, YaChatViewModel::NoFlags);
			else
				appendNormalMessage(spooled, msg.timeStamp, msg.originLocal, false, QString(), XMPP::ReceiptNone, messageText(msg.body, false), msg.timeStamp, YaChatViewModel::NoFlags);
		}
		else {
			Q_ASSERT(!msg.originLocal);
			addMoodChange(spooled, msg.body, msg.timeStamp);
		}
	}
}

void YaChatDlg::doHistory()
{
	Ya::showHistory(selfProfile_->account(), contactProfile_->jid());
}

bool YaChatDlg::doSend()
{
	if (!couldSendMessages()) {
		return false;
	}

#if 0 // ONLINE-1988
	YaPrivacyManager* privacyManager = dynamic_cast<YaPrivacyManager*>(account()->privacyManager());
	if (privacyManager && privacyManager->isContactBlocked(jid())) {
		privacyManager->setContactBlocked(jid(), false);
	}
#endif

	return ChatDlg::doSend();
}

void YaChatDlg::doneSend(const XMPP::Message& m)
{
	account()->deliveryConfirmationManager()->start(m);
	ChatDlg::doneSend(m);
}

void YaChatDlg::capsChanged()
{
}

bool YaChatDlg::isEncryptionEnabled() const
{
	return false;
}

void YaChatDlg::contactUpdated(UserListItem* u, int status, const QString& statusString)
{
	ChatDlg::contactUpdated(u, status, statusString);
	PsiContact* contact = this->contact();

	XMPP::Status::Type statusType = XMPP::Status::Offline;
	if (status != -1)
		statusType = static_cast<XMPP::Status::Type>(status);

	ui_.contactStatus->realWidget()->setStatus(contact ? contact->status().type() : statusType, statusString, gender());
	ui_.contactUserpic->setStatus(statusType);

	if (lastStatus_.type() != statusType) {
		model()->setStatusTypeChangedNotice(statusType);
	}

	lastStatus_ = XMPP::Status(statusType, statusString);
	alternateContactProfile_ = QString("<div style='white-space:pre'>Self JID: %1</div>").arg(Qt::escape(account()->jid().full()));
	alternateContactProfile_ += QString("<div style='white-space:pre'>Paired JID: %1</div><br>").arg(Qt::escape(jid().full()));
	alternateContactProfile_ += u ? u->makeTip(true, false) : QString();

	updateModelNotices();
}

void YaChatDlg::addMoodChange(SpooledType spooled, const QString& mood, const QDateTime& timeStamp)
{
	Q_UNUSED(spooled);
	Q_UNUSED(mood);
	Q_UNUSED(timeStamp);
#if 0 // ONLINE-1932
	model()->addMoodChange(static_cast<YaChatViewModel::SpooledType>(spooled), mood, timeStamp);
#endif
}

void YaChatDlg::updateModelNotices()
{
	ChatDlg::updateModelNotices();
	PsiContact* contact = this->contact();

	model()->setUserIsBlockedNoticeVisible(contact && contact->isBlocked());
	model()->setUserIsOfflineNoticeVisible(ui_.contactStatus->realWidget()->status() == XMPP::Status::Offline);
	model()->setUserNotInListNoticeVisible(!contact || !contact->inList());
	model()->setNotAuthorizedToSeeUserStatusNoticeVisible(contact && !contact->authorizesToSeeStatus());

	ui_.bottomFrame->separator()->setShowAddButton(contact && (contact->addAvailable() || (contact->isBlocked() && account()->isAvailable())));
	ui_.bottomFrame->separator()->setShowAuthButton(!ui_.bottomFrame->separator()->showAddButton() && contact && contact->authAvailable() && showAuthButton_);
}

void YaChatDlg::updateAvatar()
{
	// TODO
}

void YaChatDlg::optionsUpdate()
{
	ChatDlg::optionsUpdate();
}

QString YaChatDlg::colorString(bool local, ChatDlg::SpooledType spooled) const
{
	return Ya::colorString(local, spooled);
}

void YaChatDlg::appendSysMsg(const QString &str)
{
	// TODO: add a new type to the model
#ifndef YAPSI
	appendNormalMessage(Spooled_None, QDateTime::currentDateTime(), false, str);
#else
	appendNormalMessage(Spooled_None, QDateTime::currentDateTime(), false, 0, QString(), XMPP::ReceiptNone, str, XMPP::YaDateTime(), YaChatViewModel::NoFlags);
#endif
}

void YaChatDlg::appendEmoteMessage(ChatDlg::SpooledType spooled, const QDateTime& time, bool local, int spamFlag, QString id, XMPP::MessageReceipt messageReceipt, QString txt, const XMPP::YaDateTime& yaTime, int yaFlags)
{
	model()->addEmoteMessage(static_cast<YaChatViewModel::SpooledType>(spooled), time, local, spamFlag, id, messageReceipt, txt, yaTime, yaFlags);
}

void YaChatDlg::appendNormalMessage(ChatDlg::SpooledType spooled, const QDateTime& time, bool local, int spamFlag, QString id, XMPP::MessageReceipt messageReceipt, QString txt, const XMPP::YaDateTime& yaTime, int yaFlags)
{
	model()->addMessage(static_cast<YaChatViewModel::SpooledType>(spooled), time, local, spamFlag, id, messageReceipt, txt, yaTime, yaFlags);
}

void YaChatDlg::appendMessageFields(const Message& m)
{
	QString txt;
	if (!m.subject().isEmpty()) {
		txt += QString("<b>") + tr("Subject:") + "</b> " + QString("%1").arg(Qt::escape(m.subject())) + "<br>";
	}
	if (!m.urlList().isEmpty()) {
		UrlList urls = m.urlList();
		txt += QString("<i>") + tr("-- Attached URL(s) --") + "</i>" + "<br>";
		for (QList<Url>::ConstIterator it = urls.begin(); it != urls.end(); ++it) {
			const Url &u = *it;
			txt += QString("<b>") + tr("URL:") + "</b> " + QString("%1").arg(TextUtil::linkify(Qt::escape(u.url()))) + "<br>";
			txt += QString("<b>") + tr("Desc:") + "</b> " + QString("%1").arg(u.desc()) + "<br>";
		}
	}

	if (txt.isEmpty()) {
		return;
	}

	ChatDlg::SpooledType spooledType = m.spooled() ?
	        Spooled_OfflineStorage :
	        Spooled_None;
#ifndef YAPSI
	appendNormalMessage(spooledType, m.timeStamp(), false, txt);
#else
	appendNormalMessage(spooledType, m.timeStamp(), false, m.spamFlag(), m.id(), m.messageReceipt(), txt, XMPP::YaDateTime::fromYaTime_t(m.yaMessageId()), m.yaFlags());
#endif
}

ChatViewClass* YaChatDlg::chatView() const
{
	return ui_.chatView;
}

ChatEdit* YaChatDlg::chatEdit() const
{
	return ui_.bottomFrame->chatEdit();
}

void YaChatDlg::showAlternateContactProfile()
{
	QPoint global = ui_.contactInfo->parentWidget()->mapToGlobal(ui_.contactInfo->geometry().topLeft());
	PsiToolTip::showText(global, alternateContactProfile_, this);
}

void YaChatDlg::showContactProfile()
{
	PsiContact* contact = this->contact();
	if (contact) {
		QRect rect = QRect(ui_.contactInfo->mapToGlobal(ui_.contactInfo->rect().topLeft()),
		                   ui_.contactInfo->mapToGlobal(ui_.contactInfo->rect().bottomRight()));
		YaChatToolTip::instance()->showText(rect, contact, 0, 0);
	}
}

bool YaChatDlg::eventFilter(QObject* obj, QEvent* e)
{
	return ChatDlg::eventFilter(obj, e);
}

void YaChatDlg::nicksChanged()
{
	ui_.chatView->nicksChanged(whoNick(true), whoNick(false));
}

XMPP::VCard::Gender YaChatDlg::gender() const
{
	PsiContact* contact = account()->findContact(jid());
	return contact ? contact->gender() : XMPP::VCard::UnknownGender;
}

PsiContact* YaChatDlg::contact() const
{
	delete fakeContact_;
	PsiContact* contact = account()->findContact(jid().bare());
	if (!contact) {
		fakeContact_ = account()->psi()->contactList()->addFakeTempContact(jid(), account());
		contact = fakeContact_;
	}
	return contact;
}

void YaChatDlg::updateContact(const Jid &j, bool fromPresence)
{
	ChatDlg::updateContact(j, fromPresence);
	model()->setUserGender(gender());
}

void YaChatDlg::updateComposingMessage()
{
	bool enable = state() == TabbableWidget::StateComposing;
	// if (enable != model()->composingEventVisible())
	// 	model()->setComposingEventVisible(enable);
	if (enable != ui_.chatView->composingEventVisible())
		ui_.chatView->setComposingEventVisible(enable);
}

/**
 * Makes sure widget don't nastily overlap when trying to resize the dialog
 * to the smallest possible size
 */
QSize YaChatDlg::minimumSizeHint() const
{
	QSize sh = ChatDlg::minimumSizeHint();
	sh.setWidth(qMax(200, sh.width()));
	return sh;
}

void YaChatDlg::setJid(const Jid& j)
{
	ChatDlg::setJid(j);
	model()->setJid(j);
}

void YaChatDlg::aboutToShow()
{
	ChatDlg::aboutToShow();

	// FIXME: it'd be good to have updateChatEditHeight() here for cases
	// when number of tab rows changed since this YaChatDlg was last
	// visible to avoid jump when it appears, but it doesn't actually work
	// when YaChatDlg is created for the first time due to font size
	// not instantly propagated to LineEdit (due to QCSS?)
	// ui_.bottomFrame->separator()->updateChatEditHeight();

	ui_.bottomFrame->layout()->activate();
	ui_.chatSplitter->layout()->activate();
}

void YaChatDlg::receivedPendingMessage()
{
#ifndef YAPSI_ACTIVEX_SERVER
	ChatDlg::receivedPendingMessage();
#endif
}

void YaChatDlg::addPendingMessage()
{
	ChatDlg::receivedPendingMessage();
}

void YaChatDlg::resizeEvent(QResizeEvent* e)
{
	ChatDlg::resizeEvent(e);
	updateContactName();
}

void YaChatDlg::updateContactName()
{
	if (getManagingTabDlg()) {
		ui_.contactName->updateEffectiveWidth(getManagingTabDlg()->windowExtra());
	}
}

void YaChatDlg::chatEditCreated()
{
	ChatDlg::chatEditCreated();

	ui_.bottomFrame->separator()->setChatWidgets(chatEdit(), chatView());
}

void YaChatDlg::setLooks()
{
	ChatDlg::setLooks();

	ui_.bottomFrame->separator()->updateChatEditHeight();
}

void YaChatDlg::addContact()
{
	// TODO: need some sort of central dispatcher for this kind of stuff
	emit YaRosterToolTip::instance()->addContact(jid().bare(), account(), QStringList(), QString());
}

void YaChatDlg::authContact()
{
	PsiContact* contact = account()->findContact(jid().bare());
	if (contact) {
		contact->rerequestAuthorizationFrom();
	}

	showAuthButton_ = false;
	updateModelNotices();
}

void YaChatDlg::closed()
{
	showAuthButton_ = true;
	updateModelNotices();

	ChatDlg::closed();
}

void YaChatDlg::activated()
{
	ChatDlg::activated();
	updateContactName();

	if (model()->historyReceived()) {
		account()->psi()->yaHistoryCacheManager()->getMessagesFor(account(), jid(), this, "retrieveHistoryFinished");
	}
}

void YaChatDlg::dropEvent(QDropEvent* event)
{
	QList<QUrl> files = event->mimeData()->urls();
	if (account()->loggedIn() && !files.isEmpty()) {
		// FIXME: zip several files into one
		foreach(const QUrl& f, files) {
			uploadFile(f.toLocalFile());
		}
	}
}
