/*
 * psigroupchatdlg.cpp
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

#include "psigroupchatdlg.h"

#include <QLabel>
#include <QLayout>
#include <QPushButton>
#include <QToolBar>
#include <QMessageBox>
#include <QPainter>
#include <QColorGroup>
#include <QSplitter>
#include <QTimer>
#include <QToolButton>
#include <QInputDialog>
#include <QPointer>
#include <QAction>
#include <QObject>
#include <QMenu>
#include <QCursor>
#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QKeyEvent>
#include <QMimeData>
#include <QResizeEvent>
#include <QScrollBar>
#include <QHBoxLayout>
#include <QFrame>
#include <QList>
#include <QTextBlockFormat>
#include <QVBoxLayout>
#include <QContextMenuEvent>
#include <QTextCursor>

#include "psicon.h"
#include "psiaccount.h"
#include "capsmanager.h"
#include "userlist.h"
#include "mucconfigdlg.h"
#include "textutil.h"
#include "statusdlg.h"
#include "xmpp_message.h"
#include "psiiconset.h"
#include "stretchwidget.h"
#include "mucmanager.h"
#include "busywidget.h"
#include "common.h"
#include "msgmle.h"
#include "iconwidget.h"
#include "iconselect.h"
#include "psitoolbar.h"
#include "iconaction.h"
#include "psitooltip.h"
#include "psioptions.h"
#include "shortcutmanager.h"
#include "psicontactlist.h"
#include "accountlabel.h"
#include "gcuserview.h"

#ifdef Q_WS_WIN
#include <windows.h>
#endif

//----------------------------------------------------------------------------
// PsiGroupchatDlg
//----------------------------------------------------------------------------
class PsiGroupchatDlg::Private : public QObject
{
	Q_OBJECT
public:
	Private(PsiGroupchatDlg *d) {
		dlg = d;
		nickSeparator = ":";
		typingStatus = Typing_Normal;

		trackBar = false;
		oldTrackBarPosition = 0;
	}

	PsiGroupchatDlg *dlg;
	IconAction *act_find, *act_clear, *act_icon, *act_configure;
#ifdef WHITEBOARDING
	IconAction *act_whiteboard;
#endif
	QMenu *pm_settings;

	QStringList hist;
	int histAt;

	QPointer<GCFindDlg> findDlg;
	QString lastSearch;

public:
	bool trackBar;
protected:
	int  oldTrackBarPosition;

private:
	ChatEdit* mle() const { return dlg->ui_.mle->chatEdit(); }
	ChatView* te_log() const { return dlg->ui_.log; }

public slots:
	void addEmoticon(const PsiIcon *icon) {
		if ( !dlg->isActiveTab() ) {
			return;
		}

		QString text = icon->defaultText();

		if (!text.isEmpty()) {
			mle()->insert(text + " ");
		}
	}

	void addEmoticon(QString text) {
		if ( !dlg->isActiveTab() ) {
			return;
		}

		mle()->insert( text + " " );
	}

	void deferredScroll() {
		//QTimer::singleShot(250, this, SLOT(slotScroll()));
		te_log()->scrollToBottom();
	}

protected slots:
	void slotScroll() {
		te_log()->scrollToBottom();
	}

public:
	bool internalFind(QString str, bool startFromBeginning = false)
	{
		if (startFromBeginning) {
			QTextCursor cursor = te_log()->textCursor();
			cursor.movePosition(QTextCursor::Start, QTextCursor::KeepAnchor);
			cursor.clearSelection();
			te_log()->setTextCursor(cursor);
		}

		bool found = te_log()->find(str);
		if(!found) {
			if (!startFromBeginning)
				return internalFind(str, true);

			return false;
		}

		return true;
	}

private:
	void removeTrackBar(QTextCursor &cursor)
	{
		if (oldTrackBarPosition) {
			cursor.setPosition(oldTrackBarPosition, QTextCursor::KeepAnchor);
			QTextBlockFormat blockFormat = cursor.blockFormat();
			blockFormat.clearProperty(QTextFormat::BlockTrailingHorizontalRulerWidth);
			cursor.clearSelection();
			cursor.setBlockFormat(blockFormat);
		}
	}

	void addTrackBar(QTextCursor &cursor)
	{
		cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
		oldTrackBarPosition = cursor.position();
		QTextBlockFormat blockFormat = cursor.blockFormat();
		blockFormat.setProperty(QTextFormat::BlockTrailingHorizontalRulerWidth, QVariant(true));
		cursor.clearSelection();
		cursor.setBlockFormat(blockFormat);
	}

public:
	void doTrackBar()
	{
		trackBar = false;

		// save position, because our manipulations could change it
		int scrollbarValue = te_log()->verticalScrollBar()->value();

		QTextCursor cursor = te_log()->textCursor();
		cursor.beginEditBlock();
		PsiTextView::Selection selection = te_log()->saveSelection(cursor);

		removeTrackBar(cursor);
		addTrackBar(cursor);

		te_log()->restoreSelection(cursor, selection);
		cursor.endEditBlock();
		te_log()->setTextCursor(cursor);

		te_log()->verticalScrollBar()->setValue(scrollbarValue);
	}

protected:
	// Nick auto-completion code follows...
	enum TypingStatus {
		Typing_Normal = 0,
		Typing_TabPressed,
		Typing_TabbingNicks,
		Typing_MultipleSuggestions
	};
	TypingStatus typingStatus;
	QString nickSeparator; // in case of "nick: ...", it equals ":"
	QStringList suggestedNicks;
	int  suggestedIndex;
	bool suggestedFromStart;

	QString beforeNickText(QString text) {
		int i;
		for (i = text.length() - 1; i >= 0; --i)
			if ( text[i].isSpace() )
				break;

		QString beforeNick = text.left(i+1);
		return beforeNick;
	}

	QStringList suggestNicks(QString text, bool fromStart) {
		QString nickText = text;
		QString beforeNick;
		if ( !fromStart ) {
			beforeNick = beforeNickText(text);
			nickText   = text.mid(beforeNick.length());
		}

		QStringList nicks = dlg->ui_.lv_users->nickList();
		QStringList::Iterator it = nicks.begin();
		QStringList suggestedNicks;
		for ( ; it != nicks.end(); ++it) {
			if ( (*it).left(nickText.length()).toLower() == nickText.toLower() ) {
				if ( fromStart )
					suggestedNicks << *it;
				else
					suggestedNicks << beforeNick + *it;
			}
		}

		return suggestedNicks;
	}

	QString longestSuggestedString(QStringList suggestedNicks) {
		QString testString = suggestedNicks.first();
		while ( testString.length() > 0 ) {
			bool found = true;
			QStringList::Iterator it = suggestedNicks.begin();
			for ( ; it != suggestedNicks.end(); ++it) {
				if ( (*it).left(testString.length()).toLower() != testString.toLower() ) {
					found = false;
					break;
				}
			}

			if ( found )
				break;

			testString = testString.left( testString.length() - 1 );
		}

		return testString;
	}

	QString insertNick(bool fromStart, QString beforeNick = "") {
		typingStatus = Typing_MultipleSuggestions;
		suggestedFromStart = fromStart;
		suggestedNicks = dlg->ui_.lv_users->nickList();
		QStringList::Iterator it = suggestedNicks.begin();
		for ( ; it != suggestedNicks.end(); ++it)
			*it = beforeNick + *it;

		QString newText;
		if ( !dlg->lastReferrer().isEmpty() ) {
			newText = beforeNick + dlg->lastReferrer();
			suggestedIndex = -1;
		}
		else {
			newText = suggestedNicks.first();
			suggestedIndex = 0;
		}

		if ( fromStart ) {
			newText += nickSeparator;
			newText += " ";
		}

		return newText;
	}

	QString suggestNick(bool fromStart, QString origText, bool *replaced) {
		suggestedFromStart = fromStart;
		suggestedNicks = suggestNicks(origText, fromStart);
		suggestedIndex = -1;

		QString newText;
		if ( suggestedNicks.count() ) {
			if ( suggestedNicks.count() == 1 ) {
				newText = suggestedNicks.first();
				if ( fromStart ) {
					newText += nickSeparator;
					newText += " ";
				}
			}
			else {
				newText = longestSuggestedString(suggestedNicks);
				if ( !newText.length() )
					return origText;

				typingStatus = Typing_MultipleSuggestions;
				// TODO: display a tooltip that will contain all suggestedNicks
			}

			*replaced = true;
		}

		return newText;
	}

public:
	void doAutoNickInsertion() {
		QTextCursor cursor = mle()->textCursor();

		// we need to get index from beginning of current block
		int index = cursor.position();
		cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::KeepAnchor);
		index -= cursor.position();

		QString paraText = cursor.block().text();
		QString origText = paraText.left(index);
		QString newText;

		bool replaced = false;

		if ( typingStatus == Typing_MultipleSuggestions ) {
			suggestedIndex++;
			if ( suggestedIndex >= (int)suggestedNicks.count() )
				suggestedIndex = 0;

			newText = suggestedNicks[suggestedIndex];
			if ( suggestedFromStart ) {
				newText += nickSeparator;
				newText += " ";
			}

			replaced = true;
		}

		if ( !cursor.block().position() && !replaced ) {
			if ( !index && typingStatus == Typing_TabbingNicks ) {
				newText = insertNick(true, "");
				replaced = true;
			}
			else {
				newText = suggestNick(true, origText, &replaced);
			}
		}

		if ( !replaced ) {
			if ( (!index || origText[index-1].isSpace()) && typingStatus == Typing_TabbingNicks ) {
				newText = insertNick(false, beforeNickText(origText));
				replaced = true;
			}
			else {
				newText = suggestNick(false, origText, &replaced);
			}
		}

		if ( replaced ) {
			mle()->setUpdatesEnabled( false );
			int position = cursor.position() + newText.length();

			cursor.beginEditBlock();
			cursor.select(QTextCursor::BlockUnderCursor);
			cursor.insertText(newText + paraText.mid(index, paraText.length() - index));
			cursor.setPosition(position, QTextCursor::KeepAnchor);
			cursor.clearSelection();
			cursor.endEditBlock();
			mle()->setTextCursor(cursor);

			mle()->setUpdatesEnabled( true );
			mle()->viewport()->update();
		}
	}

	bool eventFilter( QObject *obj, QEvent *ev ) {
		if (te_log()->handleCopyEvent(obj, ev, mle()))
			return true;

		if ( obj == mle() && ev->type() == QEvent::KeyPress ) {
			auto *e = static_cast<QKeyEvent *>(ev);

			if ( e->key() == Qt::Key_Tab ) {
				switch ( typingStatus ) {
				case Typing_Normal:
					typingStatus = Typing_TabPressed;
					break;
				case Typing_TabPressed:
					typingStatus = Typing_TabbingNicks;
					break;
				default:
					break;
				}

				doAutoNickInsertion();
				return true;
			}

			typingStatus = Typing_Normal;

			return false;
		}

		return QObject::eventFilter( obj, ev );
	}
};

PsiGroupchatDlg::PsiGroupchatDlg(const Jid& jid, PsiAccount* account, TabManager* tabManager)
	: GCMainDlg(jid, account, tabManager)
{
}

void PsiGroupchatDlg::initUi()
{
	if ( option.brushedMetal )
		setAttribute(Qt::WA_MacMetalStyle);
	nicknumber=0;
	d = new Private(this);

	options_ = PsiOptions::instance();

	d->histAt = 0;
	d->findDlg = 0;

	setAcceptDrops(true);

#ifndef Q_WS_MAC
	setWindowIcon(IconsetFactory::icon("psi/groupChat").icon());
#endif

	ui_.setupUi(this);
	ui_.lb_ident->setAccount(account());
	ui_.lb_ident->setShowJid(false);

	connect(ui_.pb_topic, SIGNAL(clicked()), SLOT(doTopic()));
	PsiToolTip::install(ui_.le_topic);

	connect(account()->psi(), SIGNAL(accountCountChanged()), this, SLOT(updateIdentityVisibility()));
	updateIdentityVisibility();

	d->act_find = new IconAction(tr("Find"), "psi/search", tr("&Find"), 0, this);
	connect(d->act_find, SIGNAL(activated()), SLOT(openFind()));
	ui_.tb_find->setDefaultAction(d->act_find);

#ifndef YAPSI
	ui_.tb_emoticons->setIcon(IconsetFactory::icon("psi/smile").icon());
#else
	ui_.tb_emoticons->setIcon(QIcon(":images/chat/smile.png"));
#endif

#ifdef Q_WS_MAC
	connect(ui_.log, SIGNAL(selectionChanged()), SLOT(logSelectionChanged()));
#endif

	ui_.lv_users->setMainDlg(this);
	connect(ui_.lv_users, SIGNAL(action(const QString &, const Status &, int)), SLOT(lv_action(const QString &, const Status &, int)));

	d->act_clear = new IconAction (tr("Clear chat window"), "psi/clearChat", tr("Clear chat window"), 0, this);
	connect( d->act_clear, SIGNAL( activated() ), SLOT( doClearButton() ) );

	d->act_configure = new IconAction(tr("Configure Room"), "psi/configure-room", tr("&Configure Room"), 0, this);
	connect(d->act_configure, SIGNAL(activated()), SLOT(configureRoom()));

#ifdef WHITEBOARDING
	d->act_whiteboard = new IconAction(tr("Open a whiteboard"), "psi/whiteboard", tr("Open a &whiteboard"), 0, this);
	connect(d->act_whiteboard, SIGNAL(activated()), SLOT(openWhiteboard()));
#endif

	connect(account()->psi()->iconSelectPopup(), SIGNAL(textSelected(QString)), d, SLOT(addEmoticon(QString)));
	d->act_icon = new IconAction( tr( "Select icon" ), "psi/smile", tr( "Select icon" ), 0, this );
	d->act_icon->setMenu( account()->psi()->iconSelectPopup() );
	ui_.tb_emoticons->setMenu(account()->psi()->iconSelectPopup());

	ui_.toolbar->setIconSize(QSize(16,16));
	ui_.toolbar->addAction(d->act_clear);
	ui_.toolbar->addAction(d->act_configure);
#ifdef WHITEBOARDING
	ui_.toolbar->addAction(d->act_whiteboard);
#endif
	ui_.toolbar->addWidget(new StretchWidget(ui_.toolbar));
	ui_.toolbar->addAction(d->act_icon);
	ui_.toolbar->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Maximum);

	connect(ui_.mle, SIGNAL(textEditCreated(QTextEdit*)), SLOT(chatEditCreated()));
	chatEditCreated();

	d->pm_settings = new QMenu(this);
	connect(d->pm_settings, SIGNAL(aboutToShow()), SLOT(buildMenu()));
	ui_.tb_actions->setMenu(d->pm_settings);

	// resize the horizontal splitter
	QList<int> list;
	list << 500;
	list << 80;
	ui_.hsplitter->setSizes(list);

	list.clear();
	list << 324;
	list << 10;
	ui_.vsplitter->setSizes(list);

	ui_.mle->chatEdit()->setFocus();
	resize(PsiOptions::instance()->getOption("options.ui.muc.size").toSize());

	setLooks();
	setShortcuts();
	invalidateTab();
}

PsiGroupchatDlg::~PsiGroupchatDlg()
{
	//QMimeSourceFactory *m = ui_.log->mimeSourceFactory();
	//ui_.log->setMimeSourceFactory(0);
	//delete m;

	delete d;
}

void PsiGroupchatDlg::resizeEvent(QResizeEvent* e)
{
	if (option.keepSizes)
		PsiOptions::instance()->setOption("options.ui.muc.size", e->size());
}

void PsiGroupchatDlg::deactivated()
{
	GCMainDlg::deactivated();

	d->trackBar = true;
}

void PsiGroupchatDlg::activated()
{
	GCMainDlg::activated();
	d->trackBar = false;
}

void PsiGroupchatDlg::updateIdentityVisibility()
{
	ui_.lb_ident->setVisible(account()->psi()->contactList()->enabledAccounts().count() > 1);
}

#ifdef WHITEBOARDING
void PsiGroupchatDlg::openWhiteboard()
{
	account()->actionOpenWhiteboardSpecific(jid(), jid().withResource(this->nick()), true);
}
#endif

/*void PsiGroupchatDlg::le_upPressed()
{
	if(d->histAt < (int)d->hist.count()) {
		++d->histAt;
		d->le_input->setText(d->hist[d->histAt-1]);
	}
}

void PsiGroupchatDlg::le_downPressed()
{
	if(d->histAt > 0) {
		--d->histAt;
		if(d->histAt == 0)
			d->le_input->clear();
		else
			d->le_input->setText(d->hist[d->histAt-1]);
	}
}*/

void PsiGroupchatDlg::doTopic()
{
	bool ok = false;
	QString str = QInputDialog::getText(
		tr("Set Groupchat Topic"),
		tr("Enter a topic:"),
		QLineEdit::Normal, ui_.le_topic->text(), &ok, this);

	if(ok) {
		Message m(jid());
		m.setType("groupchat");
		m.setSubject(str);
		m.setTimeStamp(QDateTime::currentDateTime());
		aSend(m);
	}
}

void PsiGroupchatDlg::doClear()
{
	ui_.log->clear();
	d->histAt = 0;
	// d->hist.prepend(str);
}

void PsiGroupchatDlg::doClearButton()
{
	const QMessageBox::StandardButton button = QMessageBox::question(
		this,
		tr("Warning"),
		tr("Are you sure you want to clear the chat window?\n(note: does not affect saved history)"),
		QMessageBox::Yes | QMessageBox::No,
		QMessageBox::No);
	if (button == QMessageBox::Yes)
		doClear();
}

void PsiGroupchatDlg::openFind()
{
	if(d->findDlg)
		::bringToFront(d->findDlg);
	else {
		d->findDlg = new GCFindDlg(d->lastSearch, this);
		connect(d->findDlg, SIGNAL(find(const QString &)), SLOT(doFind(const QString &)));
		d->findDlg->show();
	}
}

void PsiGroupchatDlg::doFind(const QString &str)
{
	d->lastSearch = str;
	if (d->internalFind(str))
		d->findDlg->found();
	else
		d->findDlg->error(str);
}

bool PsiGroupchatDlg::doDisconnect()
{
	bool result = GCMainDlg::doDisconnect();
	if (result) {
		ui_.pb_topic->setEnabled(false);
		appendSysMsg(tr("Disconnected."), true);
	}
	return result;
}

bool PsiGroupchatDlg::doConnect()
{
	bool result = GCMainDlg::doConnect();
	if (result) {
		appendSysMsg(tr("Reconnecting..."), true);
	}
	return result;
}

void PsiGroupchatDlg::dragEnterEvent(QDragEnterEvent *e)
{
	if (e->mimeData()->hasText())
		e->acceptProposedAction();
	else
		e->ignore();
}

void PsiGroupchatDlg::dropEvent(QDropEvent *e)
{
	if (!e->mimeData()->hasText()) {
		e->ignore();
		return;
	}

	const Jid jid(e->mimeData()->text());
	if (jid.isValid() && !ui_.lv_users->hasJid(jid)) {
		Message m;
		m.setTo(this->jid());
		m.addMUCInvite(MUCInvite(jid));
		if (!password().isEmpty())
			m.setMUCPassword(password());
		m.setTimeStamp(QDateTime::currentDateTime());
		account()->dj_sendMessage(m);
		e->acceptProposedAction();
		return;
	}

	e->ignore();
}

void PsiGroupchatDlg::error(int, const QString &str)
{
	ui_.pb_topic->setEnabled(false);

	if(gcState() == GC_Connecting)
		appendSysMsg(tr("Unable to join groupchat.  Reason: %1").arg(str), true);
	else
		appendSysMsg(tr("Unexpected groupchat error: %1").arg(str), true);

	setGcState(GC_Idle);
}

void PsiGroupchatDlg::presence(const QString &nick, const Status &s)
{
	if(s.hasError()) {
		QString message;
		if (s.errorCode() == 409) {
			message = tr("Please choose a different nickname");
			nickChangeFailure();
		}
		else
			message = tr("An error occurred");
		appendSysMsg(message, false, QDateTime::currentDateTime());
		return;
	}

	if ((nick.isEmpty()) && (s.mucStatus() == 100)) {
		setNonAnonymous(true);
	}

	if(s.isAvailable()) {
		// Available
		if (s.mucStatus() == 201) {
			appendSysMsg(tr("New room created"), false, QDateTime::currentDateTime());
			if (options_->getOption("options.muc.accept-defaults").toBool())
				mucManager()->setDefaultConfiguration();
			else if (options_->getOption("options.muc.auto-configure").toBool())
				QTimer::singleShot(0, this, SLOT(configureRoom()));
		}

		GCUserViewItem* contact = (GCUserViewItem*) ui_.lv_users->findEntry(nick);
		if (contact == NULL) {
			//contact joining
			if ( !isConnecting() && options_->getOption("options.muc.show-joins").toBool() ) {
				QString message = tr("%1 has joined the room");

				if ( options_->getOption("options.muc.show-role-affiliation").toBool() ) {
					if (s.mucItem().role() != MUCItem::NoRole) {
						if (s.mucItem().affiliation() != MUCItem::NoAffiliation) {
							message = tr("%3 has joined the room as %1 and %2").arg(MUCManager::roleToString(s.mucItem().role(),true)).arg(MUCManager::affiliationToString(s.mucItem().affiliation(),true));
						}
						else {
							message = tr("%2 has joined the room as %1").arg(MUCManager::roleToString(s.mucItem().role(),true));
						}
					}
					else if (s.mucItem().affiliation() != MUCItem::NoAffiliation) {
						message = tr("%2 has joined the room as %1").arg(MUCManager::affiliationToString(s.mucItem().affiliation(),true));
					}
				}
				if (!s.mucItem().jid().isEmpty())
					message = message.arg(QString("%1 (%2)").arg(nick).arg(s.mucItem().jid().full()));
				else
					message = message.arg(nick);
				appendSysMsg(message, false, QDateTime::currentDateTime());
			}
		}
		else {
			// Status change
			if ( !isConnecting() && options_->getOption("options.muc.show-role-affiliation").toBool() ) {
				QString message;
				if (contact->s.mucItem().role() != s.mucItem().role() && s.mucItem().role() != MUCItem::NoRole) {
					if (contact->s.mucItem().affiliation() != s.mucItem().affiliation()) {
						message = tr("%1 is now %2 and %3").arg(nick).arg(MUCManager::roleToString(s.mucItem().role(),true)).arg(MUCManager::affiliationToString(s.mucItem().affiliation(),true));
					}
					else {
						message = tr("%1 is now %2").arg(nick).arg(MUCManager::roleToString(s.mucItem().role(),true));
					}
				}
				else if (contact->s.mucItem().affiliation() != s.mucItem().affiliation()) {
					message += tr("%1 is now %2").arg(nick).arg(MUCManager::affiliationToString(s.mucItem().affiliation(),true));
				}

				if (!message.isEmpty())
					appendSysMsg(message, false, QDateTime::currentDateTime());
			}
			if ( !isConnecting() && options_->getOption("options.muc.show-status-changes").toBool() ) {
				if (s.status() != contact->s.status() || s.show() != contact->s.show())	{
					QString message;
					QString st;
					if (s.show().isEmpty())
						st=tr("online");
					else
						st=s.show();
					message = tr("%1 is now %2").arg(nick).arg(st);
					if (!s.status().isEmpty())
						message+=QString(" (%1)").arg(s.status());
					appendSysMsg(message, false, QDateTime::currentDateTime());
				}
			}
		}
		ui_.lv_users->updateEntry(nick, s);
	}
	else {
		// Unavailable
		if (s.hasMUCDestroy()) {
			// Room was destroyed
			QString message = tr("This room has been destroyed.");
			if (!s.mucDestroy().reason().isEmpty()) {
				message += "\n";
				message += tr("Reason: %1").arg(s.mucDestroy().reason());
			}
			if (!s.mucDestroy().jid().isEmpty()) {
				message += "\n";
				message += tr("Do you want to join the alternate venue '%1' ?").arg(s.mucDestroy().jid().full());
				int ret = QMessageBox::information(this, tr("Room Destroyed"), message, QMessageBox::Yes, QMessageBox::No);
				if (ret == QMessageBox::Yes) {
					account()->actionJoin(s.mucDestroy().jid().full());
				}
			}
			else {
				QMessageBox::information(this,tr("Room Destroyed"), message);
			}
			close();
		}
		if ( !isConnecting() && options_->getOption("options.muc.show-joins").toBool() ) {
			QString message;
			QString nickJid;
			GCUserViewItem *contact = (GCUserViewItem*) ui_.lv_users->findEntry(nick);
			if (contact && !contact->s.mucItem().jid().isEmpty())
				nickJid = QString("%1 (%2)").arg(nick).arg(contact->s.mucItem().jid().full());
			else
				nickJid = nick;

			switch (s.mucStatus()) {
				case 301:
					// Ban
					if (nick == this->nick()) {
						mucInfoDialog(tr("Banned"), tr("You have been banned from the room"), s.mucItem().actor(), s.mucItem().reason());
						close();
					}

					if (!s.mucItem().actor().isEmpty())
						message = tr("%1 has been banned by %2").arg(nickJid, s.mucItem().actor().full());
					else
						message = tr("%1 has been banned").arg(nickJid);

					if (!s.mucItem().reason().isEmpty())
						message += QString(" (%1)").arg(s.mucItem().reason());
					break;

				case 303:
					message = tr("%1 is now known as %2").arg(nick).arg(s.mucItem().nick());
					ui_.lv_users->updateEntry(s.mucItem().nick(), s);
					break;

				case 307:
					// Kick
					if (nick == this->nick()) {
						mucInfoDialog(tr("Kicked"), tr("You have been kicked from the room"), s.mucItem().actor(), s.mucItem().reason());
						close();
					}

					if (!s.mucItem().actor().isEmpty())
						message = tr("%1 has been kicked by %2").arg(nickJid).arg(s.mucItem().actor().full());
					else
						message = tr("%1 has been kicked").arg(nickJid);
					if (!s.mucItem().reason().isEmpty())
						message += QString(" (%1)").arg(s.mucItem().reason());
					break;

				case 321:
					// Remove due to affiliation change
					if (nick == this->nick()) {
						mucInfoDialog(tr("Removed"), tr("You have been removed from the room due to an affiliation change"), s.mucItem().actor(), s.mucItem().reason());
						close();
					}

					if (!s.mucItem().actor().isEmpty())
						message = tr("%1 has been removed from the room by %2 due to an affilliation change").arg(nickJid).arg(s.mucItem().actor().full());
					else
						message = tr("%1 has been removed from the room due to an affilliation change").arg(nickJid);

					if (!s.mucItem().reason().isEmpty())
						message += QString(" (%1)").arg(s.mucItem().reason());
					break;

				case 322:
					// Remove due to members only
					if (nick == this->nick()) {
						mucInfoDialog(tr("Removed"), tr("You have been removed from the room because the room was made members only"), s.mucItem().actor(), s.mucItem().reason());
						close();
					}

					if (!s.mucItem().actor().isEmpty())
						message = tr("%1 has been removed from the room by %2 because the room was made members-only").arg(nickJid).arg(s.mucItem().actor().full());
					else
						message = tr("%1 has been removed from the room because the room was made members-only").arg(nickJid);

					if (!s.mucItem().reason().isEmpty())
						message += QString(" (%1)").arg(s.mucItem().reason());
					break;

				default:
					//contact leaving
					message = tr("%1 has left the room").arg(nickJid);
					if (!s.status().isEmpty())
						message += QString(" (%1)").arg(s.status());
			}
			appendSysMsg(message, false, QDateTime::currentDateTime());
		}
		ui_.lv_users->removeEntry(nick);
	}

	if (!s.capsNode().isEmpty()) {
		Jid caps_jid(s.mucItem().jid().isEmpty() || !nonAnonymous() ? Jid(jid()).withResource(nick) : s.mucItem().jid());
		account()->capsManager()->updateCaps(caps_jid,s.capsNode(),s.capsVersion(),s.capsExt());
	}

	if (nick == this->nick()) {
		// Update configuration dialog
		configDlgUpdateSelfAffiliation();
		setConfigureEnabled(s.mucItem().affiliation() >= MUCItem::Member);
	}
}

void PsiGroupchatDlg::setConfigureEnabled(bool enabled)
{
	GCMainDlg::setConfigureEnabled(enabled);
	d->act_configure->setEnabled(enabled);
}

void PsiGroupchatDlg::message(const Message &_m)
{
	Message m = _m;
	QString from = m.from().resource();
	bool alert = false;

	if (hasMucMessage(m)) {
		return;
	}
	appendMucMessage(m);

	if(!m.subject().isEmpty()) {
		ui_.le_topic->setText(m.subject());
		ui_.le_topic->setCursorPosition(0);
		ui_.le_topic->setToolTip(QString("<qt><p>%1</p></qt>").arg(m.subject()));
		if(m.body().isEmpty()) {
			if (!from.isEmpty())
				m.setBody(QString("/me ") + tr("has set the topic to: %1").arg(m.subject()));
			else
				// The topic was set by the server
				m.setBody(tr("The topic has been set to: %1").arg(m.subject()));
		}
	}

	if(m.body().isEmpty())
		return;

	// code to determine if the speaker was addressing this client in chat
	if(m.body().contains(this->nick()))
		alert = true;

	if (m.body().left(this->nick().length()) == this->nick())
		setLastReferrer(m.from().resource());

	if(option.gcHighlighting) {
		for(QStringList::Iterator it=option.gcHighlights.begin();it!=option.gcHighlights.end();it++) {
			if(m.body().contains((*it), Qt::CaseInsensitive)) {
				alert = true;
			}
		}
	}

	// play sound?
	if(from == this->nick()) {
		if(!m.spooled())
			account()->playSound(eSend);
	}
	else {
		if(alert || (!option.noGCSound && !m.spooled() && !from.isEmpty()) )
			account()->playSound(eChat2);
	}

	if(from.isEmpty())
		appendSysMsg(m.body(), alert, m.timeStamp());
	else
		appendMessage(m, alert);
}

void PsiGroupchatDlg::doJoined()
{
	ui_.lv_users->clear();
	ui_.pb_topic->setEnabled(true);
	appendSysMsg(tr("Connected. Getting discussion history..."), true);
}

void PsiGroupchatDlg::appendSysMsg(const QString &str, bool alert, const QDateTime &ts)
{
	if (d->trackBar)
	 	d->doTrackBar();

	if (!option.gcHighlighting)
		alert=false;

	QDateTime time = QDateTime::currentDateTime();
	if(!ts.isNull())
		time = ts;

	QString timestr = ui_.log->formatTimeStamp(time);
	ui_.log->appendText(QString("<font color=\"#00A000\">[%1]").arg(timestr) + QString(" *** %1</font>").arg(str.toHtmlEscaped()));

	if(alert)
		doAlert();
}

QString PsiGroupchatDlg::getNickColor(QString nick)
{
	int sender;
	if(nick == this->nick()||nick.isEmpty())
		sender = -1;
	else {
		if (!nicks.contains(nick)) {
			//not found in map
			nicks.insert(nick,nicknumber);
			nicknumber++;
		}
		sender=nicks[nick];
	}

	if(!option.gcNickColoring || option.gcNickColors.empty()) {
		return "#000000";
	}
	else if(sender == -1 || option.gcNickColors.size() == 1) {
		return option.gcNickColors[option.gcNickColors.size()-1];
	}
	else {
		int n = sender % (option.gcNickColors.size()-1);
		return option.gcNickColors[n];
	}
}

void PsiGroupchatDlg::appendMessage(const Message &m, bool alert)
{
	//QString who, color;
	if (!option.gcHighlighting)
		alert=false;
	QString who, textcolor, nickcolor,alerttagso,alerttagsc;

	who = m.from().resource();
	if (d->trackBar&&m.from().resource() != this->nick()&&!m.spooled())
	 	d->doTrackBar();
	/*if(local) {
		color = "#FF0000";
	}
	else {
		color = "#0000FF";
	}*/
	nickcolor = getNickColor(who);
	textcolor = ui_.log->palette().active().text().name();
	if(alert) {
		textcolor = "#FF0000";
		alerttagso = "<b>";
		alerttagsc = "</b>";
	}
	if(m.spooled())
		nickcolor = "#008000"; //color = "#008000";

	QString timestr = ui_.log->formatTimeStamp(m.timeStamp());

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

	if(emote) {
		//ui_.log->append(QString("<font color=\"%1\">").arg(color) + QString("[%1]").arg(timestr) + QString(" *%1 ").arg(who.toHtmlEscaped()) + txt + "</font>");
		ui_.log->appendText(QString("<font color=\"%1\">").arg(nickcolor) + QString("[%1]").arg(timestr) + QString(" *%1 ").arg(who.toHtmlEscaped()) + alerttagso + txt + alerttagsc + "</font>");
	}
	else {
		if(option.chatSays) {
			//ui_.log->append(QString("<font color=\"%1\">").arg(color) + QString("[%1] ").arg(timestr) + QString("%1 says:").arg(who.toHtmlEscaped()) + "</font><br>" + txt);
			ui_.log->appendText(QString("<font color=\"%1\">").arg(nickcolor) + QString("[%1] ").arg(timestr) + QString("%1 says:").arg(who.toHtmlEscaped()) + "</font><br>" + QString("<font color=\"%1\">").arg(textcolor) + alerttagso + txt + alerttagsc + "</font>");
		}
		else {
			//ui_.log->append(QString("<font color=\"%1\">").arg(color) + QString("[%1] &lt;").arg(timestr) + who.toHtmlEscaped() + QString("&gt;</font> ") + txt);
			ui_.log->appendText(QString("<font color=\"%1\">").arg(nickcolor) + QString("[%1] &lt;").arg(timestr) + who.toHtmlEscaped() + QString("&gt;</font> ") + QString("<font color=\"%1\">").arg(textcolor) + alerttagso + txt + alerttagsc +"</font>");
		}
	}

	//if(local)
	if(m.from().resource() == this->nick())
		d->deferredScroll();

	// if we're not active, notify the user by changing the title
	if(!isActiveTab()) {
		setUnreadMessageCount(unreadMessageCount() + 1);
	}

	//if someone directed their comments to us, notify the user
	if(alert)
		doAlert();

	//if the message spoke to us, alert the user before closing this window
	//except that keepopen doesn't seem to be implemented for this class yet.
	/*if(alert) {
		d->keepOpen = true;
		QTimer::singleShot(1000, this, SLOT(setKeepOpenFalse()));
        }*/
}

void PsiGroupchatDlg::doAlert()
{
	if(!isActiveTab())
		if (PsiOptions::instance()->getOption("options.ui.flash-windows").toBool())
			doFlash(true);
}

QString PsiGroupchatDlg::desiredCaption() const
{
	QString cap = GCMainDlg::desiredCaption();
	return cap;
}

void PsiGroupchatDlg::setLooks()
{
	GCMainDlg::setLooks();
	ui_.vsplitter->optionsChanged();
	ui_.mle->optionsChanged();

	// update the fonts
	QFont f;
	f.fromString(option.font[fChat]);
	ui_.log->setFont(f);
	ui_.mle->chatEdit()->setFont(f);

	f.fromString(option.font[fRoster]);
	ui_.lv_users->setFont(f);

	if (PsiOptions::instance()->getOption("options.ui.chat.central-toolbar").toBool()) {
		ui_.toolbar->show();
		ui_.tb_actions->hide();
		ui_.tb_emoticons->hide();
	}
	else {
		ui_.toolbar->hide();
		ui_.tb_emoticons->setVisible(option.useEmoticons);
		ui_.tb_actions->show();
	}
}

void PsiGroupchatDlg::optionsUpdate()
{
	/*QMimeSourceFactory *m = ui_.log->mimeSourceFactory();
	ui_.log->setMimeSourceFactory(PsiIconset::instance()->emoticons.generateFactory());
	delete m;*/

	setLooks();
	setShortcuts();

	// update status icons
	ui_.lv_users->updateAll();
}

void PsiGroupchatDlg::lv_action(const QString &nick, const Status &s, int x)
{
	if(x == 0) {
		account()->invokeGCMessage(jid().withResource(nick));
	}
	else if(x == 1) {
		account()->invokeGCChat(jid().withResource(nick));
	}
	else if(x == 2) {
		UserListItem u;
		u.setJid(jid().withResource(nick));
		u.setName(nick);

		// make a resource so the contact appears online
		UserResource ur;
		ur.setName(nick);
		ur.setStatus(s);
		u.userResourceList().append(ur);

		StatusShowDlg *w = new StatusShowDlg(u);
		w->show();
	}
	else if(x == 3) {
		account()->invokeGCInfo(jid().withResource(nick));
	}
	else if(x == 4) {
		account()->invokeGCFile(jid().withResource(nick));
	}
	else if(x == 10) {
		mucManager()->kick(nick);
	}
	else if(x == 11) {
		GCUserViewItem *contact = (GCUserViewItem*) ui_.lv_users->findEntry(nick);
		mucManager()->ban(contact->s.mucItem().jid());
	}
	else if(x == 12) {
		GCUserViewItem *contact = (GCUserViewItem*) ui_.lv_users->findEntry(nick);
		if (contact->s.mucItem().role() != MUCItem::Visitor)
			mucManager()->setRole(nick, MUCItem::Visitor);
	}
	else if(x == 13) {
		GCUserViewItem *contact = (GCUserViewItem*) ui_.lv_users->findEntry(nick);
		if (contact->s.mucItem().role() != MUCItem::Participant)
			mucManager()->setRole(nick, MUCItem::Participant);
	}
	else if(x == 14) {
		GCUserViewItem *contact = (GCUserViewItem*) ui_.lv_users->findEntry(nick);
		if (contact->s.mucItem().role() != MUCItem::Moderator)
			mucManager()->setRole(nick, MUCItem::Moderator);
	}
	/*else if(x == 15) {
		GCUserViewItem *contact = (GCUserViewItem*) ui_.lv_users->findEntry(nick);
		if (contact->s.mucItem().affiliation() != MUCItem::NoAffiliation)
			mucManager()->setAffiliation(contact->s.mucItem().jid(), MUCItem::NoAffiliation);
	}
	else if(x == 16) {
		GCUserViewItem *contact = (GCUserViewItem*) ui_.lv_users->findEntry(nick);
		if (contact->s.mucItem().affiliation() != MUCItem::Member)
			mucManager()->setAffiliation(contact->s.mucItem().jid(), MUCItem::Member);
	}
	else if(x == 17) {
		GCUserViewItem *contact = (GCUserViewItem*) ui_.lv_users->findEntry(nick);
		if (contact->s.mucItem().affiliation() != MUCItem::Admin)
			mucManager()->setAffiliation(contact->s.mucItem().jid(), MUCItem::Admin);
	}
	else if(x == 18) {
		GCUserViewItem *contact = (GCUserViewItem*) ui_.lv_users->findEntry(nick);
		if (contact->s.mucItem().affiliation() != MUCItem::Owner)
			mucManager()->setAffiliation(contact->s.mucItem().jid(), MUCItem::Owner);
	}*/
}

void PsiGroupchatDlg::contextMenuEvent(QContextMenuEvent *)
{
	d->pm_settings->exec(QCursor::pos());
}

void PsiGroupchatDlg::buildMenu()
{
	// Dialog menu
	d->pm_settings->clear();

	d->act_clear->addTo( d->pm_settings );
	d->act_configure->addTo( d->pm_settings );
#ifdef WHITEBOARDING
	d->act_whiteboard->addTo( d->pm_settings );
#endif
	d->pm_settings->insertSeparator();

#ifndef YAPSI
	d->act_icon->addTo( d->pm_settings );
#endif
}

void PsiGroupchatDlg::chatEditCreated()
{
	ui_.log->setDialog(this);
	ui_.mle->chatEdit()->setDialog(this);
	ui_.mle->chatEdit()->setSendAction(actionSend());

	ui_.mle->chatEdit()->installEventFilter(d);
}

bool PsiGroupchatDlg::doSend()
{
	QString str = chatEdit()->text();
	bool result = GCMainDlg::doSend();
	if (result) {
		d->histAt = 0;
		d->hist.prepend(str);
	}
	return result;
}

void PsiGroupchatDlg::configDlgUpdateSelfAffiliation()
{
	if (configDlg()) {
		GCUserViewItem* c = (GCUserViewItem*)ui_.lv_users->findEntry(this->nick());
		configDlg()->setRoleAffiliation(c->s.mucItem().role(), c->s.mucItem().affiliation());
	}
}

//----------------------------------------------------------------------------
// GCFindDlg
//----------------------------------------------------------------------------
GCFindDlg::GCFindDlg(const QString &str, QWidget *parent)
	: QDialog(parent)
{
	setAttribute(Qt::WA_DeleteOnClose);
	setWindowTitle(tr("Find"));
	QVBoxLayout *vb = new QVBoxLayout(this, 4);
	QHBoxLayout *hb = new QHBoxLayout(vb);
	QLabel *l = new QLabel(tr("Find:"), this);
	hb->addWidget(l);
	le_input = new QLineEdit(this);
	hb->addWidget(le_input);
	vb->addStretch(1);

	QFrame *Line1 = new QFrame(this);
	Line1->setFrameShape( QFrame::HLine );
	Line1->setFrameShadow( QFrame::Sunken );
	Line1->setFrameShape( QFrame::HLine );
	vb->addWidget(Line1);

	hb = new QHBoxLayout(vb);
	hb->addStretch(1);
	QPushButton *pb_close = new QPushButton(tr("&Close"), this);
	connect(pb_close, SIGNAL(clicked()), SLOT(close()));
	hb->addWidget(pb_close);
	QPushButton *pb_find = new QPushButton(tr("&Find"), this);
	pb_find->setDefault(true);
	connect(pb_find, SIGNAL(clicked()), SLOT(doFind()));
	hb->addWidget(pb_find);
	pb_find->setAutoDefault(true);

	resize(200, minimumSizeHint().height());

	le_input->setText(str);
	le_input->setFocus();
}

GCFindDlg::~GCFindDlg()
{
}

void GCFindDlg::found()
{
	// nothing here to do...
}

void GCFindDlg::error(const QString &str)
{
	QMessageBox::warning(this, tr("Find"), tr("Search string '%1' not found.").arg(str));
	le_input->setFocus();
}

void GCFindDlg::doFind()
{
	emit find(le_input->text());
}

#include "psigroupchatdlg.moc"
