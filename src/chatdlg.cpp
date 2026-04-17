/*
 * chatdlg.cpp - dialog for handling chats
 * Copyright (C) 2001-2007  Justin Karneges, Michail Pishchagin
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

#include "chatdlg.h"

#include <QLabel>
#include <QCursor>
#include <QLineEdit>
#include <QToolButton>
#include <QLayout>
#include <QSplitter>
#include <QToolBar>
#include <QTimer>
#include <QDateTime>
#include <QPixmap>
#include <QColor>
#include <Qt>
#include <QCloseEvent>
#include <QList>
#include <QKeyEvent>
#include <QHBoxLayout>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QList>
#include <QMessageBox>
#include <QShowEvent>
#include <QVBoxLayout>
#include <QContextMenuEvent>
#include <QResizeEvent>
#include <QMenu>
#include <QDragEnterEvent>
#include <QEvent>

#include "profiles.h"
#include "psiaccount.h"
#include "common.h"
#include "userlist.h"
#include "stretchwidget.h"
#include "psiiconset.h"
#include "iconwidget.h"
#include "textutil.h"
#include "xmpp_message.h"
#include "xmpp_htmlelement.h"
#include "fancylabel.h"
#include "msgmle.h"
#include "iconselect.h"
#include "psicon.h"
#include "iconlabel.h"
#include "capsmanager.h"
#include "iconaction.h"
#include "avatars.h"
#include "jidutil.h"
#include "tabdlg.h"
#include "psioptions.h"
#include "psitooltip.h"
#include "shortcutmanager.h"
#include "psicontactlist.h"
#include "accountlabel.h"
#include "psicontact.h"
#include "psilogger.h"
#ifdef HAVE_PGPUTIL
#include "pgputil.h"
#endif
#include "omemomanager.h"
#include "xmpp_omemo.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#ifdef YAPSI
#include "yachatdlg.h"
#include "yacommon.h"
#include "yachatviewmodel.h"
#else
#include "psichatdlg.h"
#endif

ChatDlg* ChatDlg::create(const Jid& jid, PsiAccount* account, TabManager* tabManager)
{
#ifdef YAPSI
	YaChatDlg* chat = new YaChatDlg(jid, account, tabManager);
#else
	ChatDlg* chat = new PsiChatDlg(jid, account, tabManager);
#endif
	chat->init();
	chat->setJid(jid);
#ifdef YAPSI
	chat->restoreLastMessages();
#endif
	return chat;
}

ChatDlg::ChatDlg(const Jid& jid, PsiAccount* pa, TabManager* tabManager)
	: ChatDlgBase(jid, pa, tabManager)
	, lastMessageUserAction_(false)
{
	if (option.brushedMetal) {
		setAttribute(Qt::WA_MacMetalStyle);
	}

	keepOpen_ = false;
	warnSend_ = false;
	selfDestruct_ = 0;
	transid_ = -1;
	key_ = "";
	lastWasEncrypted_ = false;

	status_ = -1;

	// Message events
	contactChatState_ = XMPP::StateNone;
	lastChatState_ = XMPP::StateNone;
	sendComposingEvents_ = false;
	isComposing_ = false;
	composingTimer_ = 0;

	// XEP-0085: idle timer — transitions Active → Inactive after 2 minutes of no typing
	inactiveTimer_ = new QTimer(this);
	inactiveTimer_->setSingleShot(true);
	inactiveTimer_->setInterval(120000); // 2 minutes per XEP-0085 §5.2
	connect(inactiveTimer_, SIGNAL(timeout()), SLOT(inactiveTimeout()));
}

void ChatDlg::init()
{
	ChatDlgBase::init();

	connect(this, SIGNAL(composing(bool)), SLOT(updateIsComposing(bool)));

	updateContact(jid(), true);

	X11WM_CLASS("chat");

	updatePGP();

	connect(account(), SIGNAL(pgpKeyChanged()), SLOT(updatePGP()));
	connect(account(), SIGNAL(encryptedMessageSent(int, bool, int)), SLOT(encryptedMessageSent(int, bool, int)));

#ifndef YAPSI
	// TODO: port to restoreSavedSize() (and adapt it from restoreSavedGeometry())
	resize(PsiOptions::instance()->getOption("options.ui.chat.size").toSize());
#endif
}

ChatDlg::~ChatDlg()
{
}

void ChatDlg::resizeEvent(QResizeEvent *e)
{
#ifdef YAPSI
	Q_UNUSED(e);
#else
	if (option.keepSizes) {
		PsiOptions::instance()->setOption("options.ui.chat.size", e->size());
	}
#endif
}

void ChatDlg::closeEvent(QCloseEvent *e)
{
	if (readyToHide()) {
		e->accept();
	}
	else {
		e->ignore();
	}
}

/**
 * Runs all the gumph necessary before hiding a chat.
 * (checking new messages, setting the autodelete, cancelling composing etc)
 * \return ChatDlg is ready to be hidden.
 */
bool ChatDlg::readyToHide()
{
	// really lame way of checking if we are encrypting
	if (!chatEdit()->isEnabled()) {
		return false;
	}

#ifndef YAPSI
	if (keepOpen_) {
		int n = QMessageBox::information(this, tr("Warning"), tr("A new chat message was just received.\nDo you still want to close the window?"), tr("&Yes"), tr("&No"));
		if (n != 0) {
			return false;
		}
	}
#endif

#ifndef YAPSI
	// destroy the dialog if delChats is dcClose
	if (option.delChats == dcClose) {
		setAttribute(Qt::WA_DeleteOnClose);
	}
	else {
		if (option.delChats == dcHour) {
			setSelfDestruct(60);
		}
		else if (option.delChats == dcDay) {
			setSelfDestruct(60 * 24);
		}
	}
#endif

	// Reset 'contact is composing' & cancel own composing event
	resetComposing();
	setChatState(StateGone);
	if (contactChatState_ == XMPP::StateComposing || contactChatState_ == XMPP::StateInactive) {
		setContactChatState(XMPP::StatePaused);
	}

#ifndef YAPSI
	emit messagesRead(jid());

	if (unreadMessageCount() > 0) {
		setUnreadMessageCount(0);
	}
	doFlash(false);

	chatEdit()->setFocus();
#endif
	return true;
}

void ChatDlg::capsChanged(const Jid& j)
{
	if (jid().compare(j, false)) {
		capsChanged();
	}
}

void ChatDlg::capsChanged()
{
}

void ChatDlg::hideEvent(QHideEvent* e)
{
	if (isMinimized()) {
		resetComposing();
		setChatState(StateInactive);
	}
	ChatDlgBase::hideEvent(e);

	// hideEvent is called both when current chat tab becomes inactive
	// and when TabDlg closes it but decides not to call close() on it
	slotScroll();
}

void ChatDlg::showEvent(QShowEvent* e)
{
	ChatDlgBase::showEvent(e);
	setSelfDestruct(0);
}

void ChatDlg::changeEvent(QEvent* event)
{
	ChatDlgBase::changeEvent(event);

	if (event->type() == QEvent::ActivationChange && isActiveTab()) {
		// if we're bringing it to the front, get rid of the '*' if necessary
		activated();
	}
}

void ChatDlg::deactivated()
{
	ChatDlgBase::deactivated();
}

void ChatDlg::activated()
{
	ChatDlgBase::activated();

	emit messagesRead(jid());

	// XEP-0333: send <displayed> marker for the last received message that requested markers
	if (!lastReceivedMsgId_.isEmpty()) {
		Message marker(jid());
		marker.setType("chat");
		marker.setChatMarker(XMPP::Message::MarkerDisplayed, lastReceivedMsgId_);
		emit aSend(marker);
		lastReceivedMsgId_.clear();
	}

	// XEP-0085: notify contact that we are now actively looking at the window
	if (contactChatState_ != XMPP::StateNone && sendComposingEvents_) {
		setChatState(XMPP::StateActive);
	}
}

void ChatDlg::dropEvent(QDropEvent* event)
{
	QStringList files;
	if (!account()->loggedIn() || !event->mimeData()->hasUrls())
		return;

	const QList<QUrl> urls = event->mimeData()->urls();
	for (const QUrl &url : urls) {
		if (url.isLocalFile())
			files << url.toLocalFile();
	}

	if (!files.isEmpty()) {
		account()->actionSendFiles(jid(), files);
		event->acceptProposedAction();
	}
}

void ChatDlg::dragEnterEvent(QDragEnterEvent* event)
{
	if (!account()->loggedIn() || !event->mimeData()->hasUrls()) {
		event->ignore();
		return;
	}

	for (const QUrl &url : event->mimeData()->urls()) {
		if (url.isLocalFile()) {
			event->acceptProposedAction();
			return;
		}
	}

	event->ignore();
}

void ChatDlg::setJid(const Jid &j)
{
	if (!j.compare(jid())) {
		account()->dialogUnregister(this);
		ChatDlgBase::setJid(j);
		account()->dialogRegister(this, jid());
		updateContact(jid(), false);
	}
}

QString ChatDlg::getDisplayName() const
{
	return dispNick_;
}

QSize ChatDlg::defaultSize()
{
	return QSize(320, 280);
}

struct UserStatus {
	UserStatus()
		: userListItem(0)
		, statusType(XMPP::Status::Offline)
	{}

	UserListItem* userListItem;
	XMPP::Status::Type statusType;
	QString status;
	QString publicKeyID;
};

UserStatus userStatusFor(const Jid& jid, QList<UserListItem*> ul, bool forceEmptyResource)
{
	if (ul.isEmpty())
		return UserStatus();

	UserStatus u;

	u.userListItem = ul.first();
	if (jid.resource().isEmpty() || forceEmptyResource) {
		// use priority
		if (u.userListItem->isAvailable()) {
			const UserResource &r = *u.userListItem->userResourceList().priority();
			u.statusType = r.status().type();
			u.status = r.status().status();
			u.publicKeyID = r.publicKeyID();
		}
	}
	else {
		// use specific
		UserResourceList::ConstIterator rit = u.userListItem->userResourceList().find(jid.resource());
		if (rit != u.userListItem->userResourceList().end()) {
			u.statusType = (*rit).status().type();
			u.status = (*rit).status().status();
			u.publicKeyID = (*rit).publicKeyID();
		}
	}

	if (u.statusType == XMPP::Status::Offline)
		u.status = u.userListItem->lastUnavailableStatus().status();

	return u;
}

void ChatDlg::updateContact(const Jid &j, bool fromPresence)
{
	// if groupchat, only update if the resource matches
	if (account()->findGCContact(j) && !jid().compare(j)) {
		return;
	}

	if (jid().compare(j, false)) {
		QList<UserListItem*> ul = account()->findRelevant(j);
		UserStatus userStatus = userStatusFor(jid(), ul, false);
#ifdef YAPSI
		if (userStatus.statusType == XMPP::Status::Offline)
			userStatus = userStatusFor(jid(), ul, true);
#endif
		if (userStatus.statusType == XMPP::Status::Offline)
			contactChatState_ = XMPP::StateNone;

		bool statusChanged = false;
		if (status_ != userStatus.statusType || statusString_ != userStatus.status) {
			statusChanged = true;
			status_ = userStatus.statusType;
			statusString_ = userStatus.status;
		}

		contactUpdated(userStatus.userListItem, userStatus.statusType, userStatus.status);

		{
			QString jidText = userStatus.userListItem ?
			                  userStatus.userListItem->jid().full() :
			                  jid().full();
			jidText = Ya::humanReadableJid(account()->psi(), jidText);
			dispNick_ = JIDUtil::nickOrJid(userStatus.userListItem ? userStatus.userListItem->name() : dispNick_, jidText);
#ifdef YAPSI
			dispNick_ = Ya::contactName(dispNick_, jidText);
#endif
			nicksChanged();
			invalidateTab();

			key_ = userStatus.publicKeyID;
			updatePGP();

			if (fromPresence && statusChanged) {
				QString msg = tr("%1 is %2").arg(TextUtil::escape(dispNick_)).arg(status2txt(status_));
				if (!statusString_.isEmpty()) {
					QString ss = TextUtil::linkify(TextUtil::plain2rich(statusString_));
					ss = TextUtil::emoticonify(ss);
					if (PsiOptions::instance()->getOption("options.ui.chat.legacy-formatting").toBool()) {
						ss = TextUtil::legacyFormat(ss);
					}
					msg += QString(" [%1]").arg(ss);
				}
#ifndef YAPSI
				appendSysMsg(msg);
#endif
			}
		}

		// Update capabilities
		capsChanged(jid());

		// Reset 'is composing' event if the status changed
		if (statusChanged && contactChatState_ != XMPP::StateNone) {
			if (contactChatState_ == XMPP::StateComposing || contactChatState_ == XMPP::StateInactive) {
				setContactChatState(XMPP::StatePaused);
			}
		}
	}
}

void ChatDlg::contactUpdated(UserListItem* u, int status, const QString& statusString)
{
	Q_UNUSED(u);
	Q_UNUSED(status);
	Q_UNUSED(statusString);

	if (!sendComposingEvents_) {
		sendComposingEvents_ = canChatState();
	}
}

void ChatDlg::doVoice()
{
	aVoice(jid());
}

void ChatDlg::updateAvatar(const Jid& j)
{
	if (j.compare(jid(), false))
		updateAvatar();
}

void ChatDlg::setLooks()
{
	ChatDlgBase::setLooks();

	// update contact info
	status_ = -2; // sick way of making it redraw the status
	updateContact(jid(), false);

	// update the widget icon
#ifndef Q_OS_MAC
	setWindowIcon(IconsetFactory::icon("psi/start-chat").icon());
#endif

	/*QBrush brush;
	brush.setPixmap( QPixmap( option.chatBgImage ) );
	chatView()->setPaper(brush);
	chatView()->setStaticBackground(true);*/

	setWindowOpacity(double(qMax(MINIMUM_OPACITY, PsiOptions::instance()->getOption("options.ui.chat.opacity").toInt())) / 100);
}

void ChatDlg::optionsUpdate()
{
	ChatDlgBase::optionsUpdate();

	if (isHidden()) {
		if (option.delChats == dcClose) {
			LOG_TRACE;
			deleteLater();
			return;
		}
		else {
			if (option.delChats == dcHour) {
				setSelfDestruct(60);
			}
			else if (option.delChats == dcDay) {
				setSelfDestruct(60 * 24);
			}
			else {
				setSelfDestruct(0);
			}
		}
	}
}

void ChatDlg::updatePGP()
{
}

void ChatDlg::doInfo()
{
	aInfo(jid());
}

void ChatDlg::doHistory()
{
	aHistory(jid());
}

void ChatDlg::doFile()
{
	aFile(jid());
}

void ChatDlg::setKeepOpenFalse()
{
	keepOpen_ = false;
}

void ChatDlg::setWarnSendFalse()
{
	warnSend_ = false;
}

void ChatDlg::setSelfDestruct(int minutes)
{
#ifdef YAPSI
	return;
#endif

	if (minutes <= 0) {
		if (selfDestruct_) {
			delete selfDestruct_;
			selfDestruct_ = 0;
		}
		return;
	}

	if (!selfDestruct_) {
		LOG_TRACE;
		selfDestruct_ = new QTimer(this);
		connect(selfDestruct_, SIGNAL(timeout()), SLOT(deleteLater()));
	}

	selfDestruct_->start(minutes * 60000);
}

QString ChatDlg::desiredCaption() const
{
	QString cap = ChatDlgBase::desiredCaption();

	if (contactChatState_ == XMPP::StateComposing) {
		cap = tr("%1 (Composing ...)").arg(cap);
	}
	else if (contactChatState_ == XMPP::StateInactive) {
		cap = tr("%1 (Inactive)").arg(cap);
	}

	return cap;
}

void ChatDlg::invalidateTab()
{
	ChatDlgBase::invalidateTab();
}

bool ChatDlg::isEncryptionEnabled() const
{
	return false;
}

bool ChatDlg::doSend()
{
	if (!ChatDlgBase::doSend()) {
		return false;
	}

	if (warnSend_) {
		warnSend_ = false;
		int n = QMessageBox::information(this, tr("Warning"), tr(
		                                     "<p>Encryption was recently disabled by the remote contact.  "
		                                     "Are you sure you want to send this message without encryption?</p>"
		                                 ), tr("&Yes"), tr("&No"));
		if (n != 0) {
			return false;
		}
	}

	Message m(jid());
	m.setType("chat");
	QString inputText = chatEdit()->toPlainText();
	// XEP-0308: '~' prefix triggers a correction of the last sent message
	if (inputText.startsWith("~") && !lastSentMsgId_.isEmpty()) {
		m.setBody(inputText.mid(1).trimmed());
		m.setReplaceId(lastSentMsgId_);
	} else {
		m.setBody(inputText);
	}
	sendMessage(m, true);
	return true;
}

void ChatDlg::sendMessage(XMPP::Message m, bool userAction)
{
	lastMessageUserAction_ = userAction;
	m.setTimeStamp(QDateTime::currentDateTime());
#ifdef YAPSI
	m.setYaFlags(YaChatViewModel::OutgoingMessage);
#endif
	if (isEncryptionEnabled()) {
		m.setWasEncrypted(true);
	}

	// we want id's to be readily available in case we need
	// to highlight an error
	m.setId(account()->client()->genUniqueId());

	// XEP-0184 Message Receipts
	// since it's fairly important to get delivery confirmation in as many
	// cases as possible, we always ask for confirmation even when
	// our chances of positive reply are slim
	m.setMessageReceipt(ReceiptRequest);

	// XEP-0333 Chat Markers: request markers from the recipient
	m.setChatMarkable(true);

	// Request events
	if (option.messageEvents) {
		// Only request more events when really necessary
		if (sendComposingEvents_) {
			m.addEvent(ComposingEvent);
		}
		m.setChatState(XMPP::StateActive);
	}

	// Update current state
	setChatState(XMPP::StateActive);

	m_ = m;

	if (isEncryptionEnabled()) {
		// XEP-0384 OMEMO: encrypt via OmemoManager if available
		OmemoManager* omemoMgr = account() ? account()->omemoManager() : nullptr;
		if (omemoMgr && omemoMgr->isEnabled(jid())) {
			// Disable chat edit while we fetch bundles + encrypt
			chatEdit()->setEnabled(false);

			// Step 1: fetch contact's device bundles (establishes sessions)
			// Step 2: after sessionsEstablished, encrypt
			// Step 3: after encryptDone, send
			connect(omemoMgr, &OmemoManager::sessionsEstablished,
			        this, [this, omemoMgr, m](const XMPP::Jid& to, bool ok) mutable {
				if (!to.compare(jid(), false))
					return;
				// One-shot: disconnect this lambda
				disconnect(omemoMgr, &OmemoManager::sessionsEstablished, this, nullptr);

				if (!ok) {
					qWarning("[OMEMO] Sessions not established — sending plaintext fallback");
					chatEdit()->setEnabled(true);
					emit aSend(m);
					doneSend(m);
					return;
				}

				// Connect encrypt callback
				connect(omemoMgr, &OmemoManager::encryptDone,
				        this, [this, omemoMgr, m](
				                const XMPP::Jid& to2, const QDomElement& enc,
				                const QString& /*plainBody*/, bool encOk) mutable {
					if (!to2.compare(jid(), false))
						return;
					disconnect(omemoMgr, &OmemoManager::encryptDone, this, nullptr);
					chatEdit()->setEnabled(true);
					if (encOk) {
						XMPP::Message em = m;
						em.setBody(QLatin1String(
						    "I sent you an OMEMO encrypted message "
						    "but your client doesn't seem to support that. "
						    "https://conversations.im/omemo"));
						em.addExtension(enc);
						// We used to attach <private xmlns='urn:xmpp:
						// carbons:2'/> here to prevent the fallback body
						// being carbon-copied in cleartext to our other
						// resources. That broke multi-device OMEMO:
						// <private/> suppresses the ENTIRE message from
						// the carbon stream, including the <encrypted>
						// extension — so our phone never sees our own
						// outgoing message in any form. The correct
						// behaviour (matching Conversations/Dino) is to
						// ALLOW the carbon: our other devices are part
						// of our own devicelist and have <key> entries
						// inside <encrypted>, so they decrypt the real
						// body via OMEMO. The fallback string is just a
						// polite note to non-OMEMO clients.
						emit aSend(em);
						doneSend(m); // log original plaintext locally
					} else {
						qWarning("[OMEMO] Encryption failed — sending plaintext fallback");
						emit aSend(m);
						doneSend(m);
					}
				});
				Q_UNUSED(to); // suppress warning about lambda capture
				omemoMgr->encrypt(jid(), m.body());
			});
			omemoMgr->fetchContactBundles(jid());
			chatEdit()->setFocus();
			return;
		}

		// PGP fallback (no OMEMO manager)
		chatEdit()->setEnabled(false);
		transid_ = account()->sendMessageEncrypted(m);
		if (transid_ == -1) {
			chatEdit()->setEnabled(true);
			chatEdit()->setFocus();
			return;
		}
	}
	else {
		emit aSend(m);
		doneSend(m);
	}

	chatEdit()->setFocus();
}

void ChatDlg::doSendMessage(const XMPP::Message& m)
{
	sendMessage(m, false);
}

void ChatDlg::doneSend(const XMPP::Message& m)
{
	// XEP-0308: track the id of the last message we sent for potential correction
	if (!m.replaceId().isEmpty()) {
		// This was itself a correction — update lastSentMsgId_ to the new message id
		// so the user can correct this correction if needed
		lastSentMsgId_ = m.id();
		lastSentBody_  = m.body();
	} else if (!m.body().isEmpty()) {
		lastSentMsgId_ = m.id();
		lastSentBody_  = m.body();
	}

	appendMessage(m, true);
	disconnect(chatEdit(), SIGNAL(textChanged()), this, SLOT(setComposing()));
	if (lastMessageUserAction_) {
		chatEdit()->clear();
	}

	// Reset composing timer
	connect(chatEdit(), SIGNAL(textChanged()), this, SLOT(setComposing()));
	// Reset composing timer
	resetComposing();

	// XEP-0085: message sent — stop the idle timer (next keystroke will restart it)
	inactiveTimer_->stop();
}

void ChatDlg::encryptedMessageSent(int x, bool b, int e)
{
#ifdef HAVE_PGPUTIL
	if (transid_ == -1 || transid_ != x) {
		return;
	}
	transid_ = -1;
	if (b) {
		doneSend(m_);
	}
	else {
		QMessageBox::critical(this, tr("Error"), tr("There was an error trying to send the message encrypted.\nReason: %1.").arg(PGPUtil::instance().messageErrorString((QCA::SecureMessage::Error) e)));
	}
	chatEdit()->setEnabled(true);
	chatEdit()->setFocus();
#else
	Q_ASSERT(false);
	Q_UNUSED(x);
	Q_UNUSED(b);
	Q_UNUSED(e);
#endif
}

void ChatDlg::incomingMessage(const Message &m)
{
	if (m.body().isEmpty()) {
		// Event message
		if (m.containsEvent(CancelEvent)) {
			setContactChatState(XMPP::StatePaused);
		}
		else if (m.containsEvent(ComposingEvent)) {
			setContactChatState(XMPP::StateComposing);
		}

		if (m.chatState() != XMPP::StateNone) {
			setContactChatState(m.chatState());
		}
	}
	else {
		// Normal message
		// Check if user requests event messages
		sendComposingEvents_ = m.containsEvent(ComposingEvent);
		if (!m.eventId().isEmpty()) {
			eventId_ = m.eventId();
		}
		if (m.containsEvents() || m.chatState() != XMPP::StateNone) {
			setContactChatState(XMPP::StateActive);
		}
		else {
			setContactChatState(XMPP::StateNone);
		}

		// XEP-0333: track id of last incoming message supporting markers
		if (m.isChatMarkable() && !m.id().isEmpty()) {
			lastReceivedMsgId_ = m.id();
		}

		// XEP-0308: if this is a correction, append a notice
		if (!m.replaceId().isEmpty()) {
			Message corrected = m;
			corrected.setBody(m.body() + tr(" *(edited)*"));
			appendMessage(corrected);
		} else {
			appendMessage(m);
		}
	}
}

void ChatDlg::setPGPEnabled(bool enabled)
{
	Q_UNUSED(enabled);
}

QString ChatDlg::whoNick(bool local) const
{
	QString result;

	if (local) {
		result = account()->nick();
	}
	else {
		result = dispNick_;
	}

	return TextUtil::escape(result);
}

void ChatDlg::receivedPendingMessage()
{
	if (isActiveTab())
		return;

	setUnreadMessageCount(unreadMessageCount() + 1);
	invalidateTab();
	if (PsiOptions::instance()->getOption("options.ui.flash-windows").toBool()) {
		doFlash(true);
	}
	if (option.raiseChatWindow) {
		if (option.useTabs) {
			TabDlg* tabSet = getManagingTabDlg();
			tabSet->selectTab(this);
			::bringToFront(tabSet, false);
		}
		else {
			::bringToFront(this, false);
		}
	}
}

void ChatDlg::appendMessage(const Message &m, bool local)
{
	// figure out the encryption state
	bool encChanged = false;
	bool encEnabled = false;
	if (lastWasEncrypted_ != m.wasEncrypted()) {
		encChanged = true;
	}
	lastWasEncrypted_ = m.wasEncrypted();
	encEnabled = lastWasEncrypted_;

	if (encChanged) {
		if (encEnabled) {
			appendSysMsg(QString("<icon name=\"psi/cryptoYes\"> ") + tr("Encryption Enabled"));
			if (!local) {
				setPGPEnabled(true);
			}
		}
		else {
			appendSysMsg(QString("<icon name=\"psi/cryptoNo\"> ") + tr("Encryption Disabled"));
			if (!local) {
				setPGPEnabled(false);

				// enable warning
				warnSend_ = true;
				QTimer::singleShot(3000, this, SLOT(setWarnSendFalse()));
			}
		}
	}

	QString txt = messageText(m);

	ChatDlg::SpooledType spooledType = m.spooled() ?
	                                   ChatDlg::Spooled_OfflineStorage :
	                                   ChatDlg::Spooled_None;
#ifdef YAPSI
	if (isEmoteMessage(m))
		appendEmoteMessage(spooledType, m.timeStamp(), local, m.spamFlag(), m.id(), m.messageReceipt(), txt, XMPP::YaDateTime::fromYaTime_t(m.yaMessageId()), m.yaFlags());
	else
		appendNormalMessage(spooledType, m.timeStamp(), local, m.spamFlag(), m.id(), m.messageReceipt(), txt, XMPP::YaDateTime::fromYaTime_t(m.yaMessageId()), m.yaFlags());
#else
	if (isEmoteMessage(m))
		appendEmoteMessage(spooledType, m.timeStamp(), local, txt);
	else
		appendNormalMessage(spooledType, m.timeStamp(), local, txt);
#endif

	appendMessageFields(m);

#ifndef YAPSI
	if (local) {
#endif
		deferredScroll();
#ifndef YAPSI
	}
#endif

	// if we're not active, notify the user by changing the title
	receivedPendingMessage();

	if (!local) {
		keepOpen_ = true;
		QTimer::singleShot(1000, this, SLOT(setKeepOpenFalse()));
	}
}

void ChatDlg::deferredScroll()
{
	QTimer::singleShot(250, this, SLOT(slotScroll()));
}

void ChatDlg::slotScroll()
{
	chatView()->scrollToBottom();
}

void ChatDlg::updateIsComposing(bool b)
{
	setChatState(b ? XMPP::StateComposing : XMPP::StatePaused);
}

void ChatDlg::setChatState(ChatState state)
{
	if (option.messageEvents && (sendComposingEvents_ || (contactChatState_ != XMPP::StateNone))) {
		// Don't send to offline resource
		QList<UserListItem*> ul = account()->findRelevant(jid());
		if (ul.isEmpty()) {
			sendComposingEvents_ = false;
			lastChatState_ = XMPP::StateNone;
			return;
		}

		UserListItem *u = ul.first();
		if (!u->isAvailable()) {
			sendComposingEvents_ = canChatState();
			lastChatState_ = XMPP::StateNone;
			return;
		}

		// Transform to more privacy-enabled chat states if necessary
		if (!option.inactiveEvents && (state == XMPP::StateGone || state == XMPP::StateInactive)) {
			state = XMPP::StatePaused;
		}

		if (lastChatState_ == XMPP::StateNone && (state != XMPP::StateActive && state != XMPP::StateComposing && state != XMPP::StateGone)) {
			//this isn't a valid transition, so don't send it, and don't update laststate
			return;
		}

		// Check if we should send a message
		if (state == lastChatState_ || state == XMPP::StateActive || (lastChatState_ == XMPP::StateActive && state == XMPP::StatePaused)) {
			lastChatState_ = state;
			return;
		}

		// Build event message
		Message m(jid());
		if (sendComposingEvents_) {
			m.setEventId(eventId_);
			if (state == XMPP::StateComposing) {
				m.addEvent(ComposingEvent);
			}
			else if (lastChatState_ == XMPP::StateComposing) {
				m.addEvent(CancelEvent);
			}
		}
		if (contactChatState_ != XMPP::StateNone) {
			if (lastChatState_ != XMPP::StateGone) {
				if ((state == XMPP::StateInactive && lastChatState_ == XMPP::StateComposing) || (state == XMPP::StateComposing && lastChatState_ == XMPP::StateInactive)) {
					// First go to the paused state
					Message m(jid());
					m.setType("chat");
					m.setChatState(XMPP::StatePaused);
					if (account()->isAvailable()) {
						account()->dj_sendMessage(m, false);
					}
				}
				m.setChatState(state);
			}
		}

		// Send event message
		if (m.containsEvents() || m.chatState() != XMPP::StateNone) {
			m.setType("chat");
			if (account()->isAvailable()) {
				account()->dj_sendMessage(m, false);
			}
		}

		// Save last state
		if (lastChatState_ != XMPP::StateGone || state == XMPP::StateActive)
			lastChatState_ = state;

		// XEP-0085: no need to track idleness once we've gone Gone or Inactive
		if (state == XMPP::StateGone || state == XMPP::StateInactive) {
			inactiveTimer_->stop();
		}
	}
}

void ChatDlg::inactiveTimeout()
{
	// XEP-0085 §5.2: user has been idle for 2 minutes — transition to Inactive
	setChatState(XMPP::StateInactive);
}

void ChatDlg::setContactChatState(ChatState state)
{
	contactChatState_ = state;
	if (state == XMPP::StateGone) {
#ifndef YAPSI
		appendSysMsg(tr("%1 ended the conversation").arg(TextUtil::escape(dispNick_)));
#endif
	}
	else {
		// Activate ourselves
		if (lastChatState_ == XMPP::StateGone) {
			setChatState(XMPP::StateActive);
		}
	}
	invalidateTab();
}

/**
 * Records that the user is composing
 */
void ChatDlg::setComposing()
{
	if (!composingTimer_) {
		/* User (re)starts composing */
		composingTimer_ = new QTimer(this);
		connect(composingTimer_, SIGNAL(timeout()), SLOT(checkComposing()));
		composingTimer_->start(2000); // FIXME: magic number
		emit composing(true);
	}
	isComposing_ = true;

	// XEP-0085 §5.2: reset the 2-minute idle timer on every keystroke
	inactiveTimer_->start();
}

/**
 * Checks if the user is still composing
 */
void ChatDlg::checkComposing()
{
	if (!isComposing_) {
		// User stopped composing
		composingTimer_->deleteLater();
		composingTimer_ = 0;
		emit composing(false);
	}
	isComposing_ = false; // Reset composing
}

void ChatDlg::resetComposing()
{
	if (composingTimer_) {
		delete composingTimer_;
		composingTimer_ = 0;
		isComposing_ = false;
	}
}

void ChatDlg::nicksChanged()
{
	// this function is intended to be reimplemented in subclasses
}

static const QString me_cmd = "/me ";

bool ChatDlg::isEmoteText(const QString& text)
{
	return text.startsWith(me_cmd);
}

bool ChatDlg::isEmoteMessage(const XMPP::Message& m)
{
	if (isEmoteText(m.body()) || isEmoteText(m.html().text().trimmed()))
		return true;

	return false;
}

QString ChatDlg::messageText(const XMPP::Message& m)
{
	bool emote = isEmoteMessage(m);
	QString txt;

	// TMP: ONLINE-1885
	// if (m.containsHTML() && PsiOptions::instance()->getOption("options.html.chat.render").toBool() && !m.html().text().isEmpty()) {
	// 	return messageText(m.html().toString("span"), emote, true);
	// }

	return messageText(m.body(), emote, false);
}

QString ChatDlg::messageText(const QString& text, bool isEmote, bool isHtml)
{
	QString txt = text;

	if (isHtml) {
		if (isEmote) {
			int cmd = txt.indexOf(me_cmd);
			txt = txt.remove(cmd, me_cmd.length());
		}
	}
	else {
		if (isEmote) {
			txt = txt.mid(me_cmd.length());
		}

		txt = TextUtil::plain2rich(txt);
		txt = TextUtil::linkify(txt);
	}

	txt = TextUtil::emoticonify(txt);
	if (PsiOptions::instance()->getOption("options.ui.chat.legacy-formatting").toBool())
		txt = TextUtil::legacyFormat(txt);

	return txt;
}

TabbableWidget::State ChatDlg::state() const
{
	return contactChatState_ == XMPP::StateComposing ?
	       TabbableWidget::StateComposing :
	       TabbableWidget::StateNone;
}

bool ChatDlg::canChatState() const
{
	if (!account() || !account()->capsManager())
		return false;

	XMPP::Jid j = jid();
	if (j.resource().isEmpty()) {
		QList<UserListItem*> ul = account()->findRelevant(jid());
		if (!ul.isEmpty()) {
			UserListItem* u = ul.first();
			UserResourceList::Iterator rit = u->userResourceList().priority();
			if (rit != u->userResourceList().end()) {
				j.setResource((*rit).name());
			}
		}
	}

	return account()->capsManager()->features(j).canChatState();
}

void ChatDlg::chatEditCreated()
{
	ChatDlgBase::chatEditCreated();

	if (highlightersInstalled()) {
		connect(chatEdit(), SIGNAL(textChanged()), this, SLOT(setComposing()));
	}
}
