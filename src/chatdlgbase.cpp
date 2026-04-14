/*
 * chatdlgbase.cpp
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

#include "chatdlgbase.h"

#include <QTimer>
#include <QScrollBar>

#include "psiaccount.h"
#include "psicon.h"
#include "yachatviewmodel.h"
#include "yachatdlgshared.h"
#include "psioptions.h"
#include "yachatview.h"
#include "msgmle.h"
#include "shortcutmanager.h"
#include "xmpp_xmlcommon.h"
#include "fileutil.h"
#ifdef YAPSI
#include "yadayuse.h"
#endif

static const QString textColorOptionPath = "options.ya.chat-window.text-color";

ChatDlgBase::ChatDlgBase(const Jid& jid, PsiAccount* acc, TabManager* tabManager)
	: TabbableWidget(jid, acc, tabManager)
	, highlightersInstalled_(false)
#ifdef YAPSI
	, model_(0)
#endif
	, pending_(0)
{
#ifdef YAPSI
	model_ = new YaChatViewModel(account()->deliveryConfirmationManager(), account()->psi()->yaNarodDiskManager());

	connect(model_, SIGNAL(uploadFinished(const QString&, const QString&, qint64)), SLOT(uploadFinished(const QString&, const QString&, qint64)));
	connect(YaChatDlgShared::instance(account()->psi()), SIGNAL(uploadFile()), SLOT(uploadFile()));
	connect(YaChatDlgShared::instance(account()->psi()), SIGNAL(uploadRecentFile(const QString&, const QString&, qint64)), SLOT(uploadRecentFile(const QString&, const QString&, qint64)));

	connect(PsiOptions::instance(), SIGNAL(optionChanged(const QString&)), SLOT(optionChanged(const QString&)));
	connect(account(), SIGNAL(updatedActivity()), SLOT(updateModelNotices()));
	connect(account(), SIGNAL(enabledChanged()), SLOT(updateModelNotices()));
	QTimer::singleShot(0, this, SLOT(updateModelNotices()));
#endif
}

ChatDlgBase::~ChatDlgBase()
{
	account()->dialogUnregister(this);
	delete model_;
}

void ChatDlgBase::init()
{
	initActions();
	setShortcuts();
	initUi();
	setLooks();

	connect(account(), SIGNAL(updatedActivity()), SLOT(updateSendAction()));
	connect(account(), SIGNAL(enabledChanged()), SLOT(updateSendAction()));
	updateSendAction();

	// SyntaxHighlighters modify the QTextEdit in a QTimer::singleShot(0, ...) call
	// so we need to install our hooks after it fired for the first time
	QTimer::singleShot(10, this, SLOT(initComposing()));

	setAcceptDrops(true);

	chatView()->setFocusPolicy(Qt::NoFocus);
	chatEdit()->setFocus();

	account()->dialogRegister(this, jid());
}

QAction* ChatDlgBase::actionSend() const
{
	return act_send_;
}

void ChatDlgBase::initActions()
{
	act_send_ = new QAction(tr("Send"), this);
	addAction(act_send_);
	connect(act_send_, SIGNAL(activated()), SLOT(doSend()));

	act_close_ = new QAction(this);
	addAction(act_close_);
	connect(act_close_, SIGNAL(activated()), SLOT(close()));

	act_scrollup_ = new QAction(this);
	addAction(act_scrollup_);
	connect(act_scrollup_, SIGNAL(activated()), SLOT(scrollUp()));

	act_scrolldown_ = new QAction(this);
	addAction(act_scrolldown_);
	connect(act_scrolldown_, SIGNAL(activated()), SLOT(scrollDown()));
}

void ChatDlgBase::setShortcuts()
{
	//act_send_->setShortcuts(ShortcutManager::instance()->shortcuts("chat.send"));
	act_scrollup_->setShortcuts(ShortcutManager::instance()->shortcuts("common.scroll-up"));
	act_scrolldown_->setShortcuts(ShortcutManager::instance()->shortcuts("common.scroll-down"));

	//if(!option.useTabs) {
	//	act_close_->setShortcuts(ShortcutManager::instance()->shortcuts("common.close"));
	//}
}

bool ChatDlgBase::doSend()
{
	if (!act_send_->isEnabled()) {
		return false;
	}

	if (chatEdit()->text().trimmed().isEmpty()) {
		chatEdit()->clear();
		return false;
	}

	if (chatEdit()->text() == "/clear") {
		chatEdit()->clear();
		doClear();
		return false;
	}

	return true;
}

void ChatDlgBase::scrollUp()
{
	chatView()->verticalScrollBar()->setValue(chatView()->verticalScrollBar()->value() - chatView()->verticalScrollBar()->pageStep() / 2);
}

void ChatDlgBase::scrollDown()
{
	chatView()->verticalScrollBar()->setValue(chatView()->verticalScrollBar()->value() + chatView()->verticalScrollBar()->pageStep() / 2);
}

bool ChatDlgBase::couldSendMessages() const
{
	return chatEdit()->isEnabled() &&
	       !chatEdit()->text().trimmed().isEmpty() &&
	       account()->isAvailable();
}

void ChatDlgBase::updateSendAction()
{
#ifndef YAPSI
	act_send_->setEnabled(couldSendMessages());
#endif
}

void ChatDlgBase::chatEditCreated()
{
	chatView()->setDialog(this);
	chatEdit()->setDialog(this);
	chatEdit()->setSendAction(act_send_);
	chatEdit()->installEventFilter(this);
	disconnect(chatEdit(), SIGNAL(textChanged()), this, SLOT(updateSendAction()));
	connect(chatEdit(), SIGNAL(textChanged()), this, SLOT(updateSendAction()));

#ifdef YAPSI
	chatEdit()->setController(account()->psi());
	chatEdit()->setUploadFileAction(YaChatDlgShared::instance(account()->psi())->uploadFileAction());
	chatEdit()->setRecentFilesMenu(YaChatDlgShared::instance(account()->psi())->recentFilesMenu());
	chatEdit()->setTypographyAction(YaChatDlgShared::instance(account()->psi())->typographyAction());
	chatEdit()->setEmoticonsAction(YaChatDlgShared::instance(account()->psi())->emoticonsAction());
	chatEdit()->setCheckSpellingAction(YaChatDlgShared::instance(account()->psi())->checkSpellingAction());
	chatEdit()->setSendButtonEnabledAction(YaChatDlgShared::instance(account()->psi())->sendButtonEnabledAction());
	optionChanged(textColorOptionPath);
#endif
}

void ChatDlgBase::initComposing()
{
	highlightersInstalled_ = true;
	chatEditCreated();
}

bool ChatDlgBase::highlightersInstalled() const
{
	return highlightersInstalled_;
}

void ChatDlgBase::doClear()
{
#ifndef YAPSI
	chatView()->clear();
#else
	model_->doClear();
#endif
}

void ChatDlgBase::optionChanged(const QString& option)
{
	if (option == textColorOptionPath) {
		chatView()->setTextColor(PsiOptions::instance()->getOption(textColorOptionPath).value<QColor>());
	}
}

void ChatDlgBase::updateModelNotices()
{
	model_->setAccountIsOfflineNoticeVisible(!account()->isAvailable());
	model_->setAccountIsDisabledNoticeVisible(!account()->enabled());
}

YaChatViewModel* ChatDlgBase::model() const
{
	return model_;
}

void ChatDlgBase::setLooks()
{
	// update the font
	QFont f;
	f.fromString(option.font[fChat]);
	chatView()->setFont(f);
	chatEdit()->setFont(f);
}

void ChatDlgBase::uploadFile()
{
	if (!isActiveTab())
		return;

	QStringList fileNames = FileUtil::getOpenAnyFileNames(this);
	if (!fileNames.isEmpty()) {
		uploadFiles(fileNames);
	}
}

void ChatDlgBase::uploadRecentFile(const QString& fileName, const QString& url, qint64 size)
{
	if (!isActiveTab())
		return;

	uploadFinished(fileName, url, size);
}

void ChatDlgBase::uploadFile(const QString& fileName)
{
	account()->psi()->yaNarodDiskManager()->uploadFile(fileName, this, "uploadFileStarted");
}

void ChatDlgBase::uploadFileStarted(const QString& id)
{
	if (id.isEmpty())
		return;

	// QMap<QString, YaNarodDiskManager::RecentFile> files = account()->psi()->yaNarodDiskManager()->recentFiles();
	// if (files.contains(id)) {
	// 	uploadFinished(files[id].fileName, files[id].url, files[id].size);
	// }
	// else {
		model()->addFileUpload(id);
	// }
}

void ChatDlgBase::uploadFiles(const QStringList& fileNames)
{
	foreach(const QString& fileName, fileNames) {
		uploadFile(fileName);
	}
}

void ChatDlgBase::uploadFinished(const QString& fileName, const QString& url, qint64 size)
{
#ifdef YAPSI
	YaDayUse::instance(account()->psi())->stat(135, 70521);
#endif

	QDomDocument doc;
	QDomElement body = doc.createElementNS("http://www.w3.org/1999/xhtml", "body");
	doc.appendChild(body);
	QDomElement html = textTag(&doc, "a", YaNarodDiskManager::humanReadableName(fileName, size));
	html.setAttribute("href", url);
	body.appendChild(html);

	Message m(jid());
	m.setType("chat");
	m.setHTML(body);
	m.setBody(QString("%1 (%2)")
	          .arg(url)
	          .arg(YaNarodDiskManager::humanReadableSize(size)));
	doSendMessage(m);
}

void ChatDlgBase::addEmoticon(QString text)
{
	if (!isActiveTab())
		return;

	chatEdit()->insert(text + " ");
}


// FIXME: This should be unnecessary, since these keys are all registered as
// actions in the constructor. Somehow, Qt ignores this sometimes (i think
// just for actions that have no modifier).
void ChatDlgBase::keyPressEvent(QKeyEvent *e)
{
	QKeySequence key = e->key() + (e->modifiers() & ~Qt::KeypadModifier);
	if (!option.useTabs && ShortcutManager::instance()->shortcuts("common.close").contains(key)) {
		close();
	}
	else if (ShortcutManager::instance()->shortcuts("chat.send").contains(key)) {
		doSend();
	}
	else if (ShortcutManager::instance()->shortcuts("common.scroll-up").contains(key)) {
		scrollUp();
	}
	else if (ShortcutManager::instance()->shortcuts("common.scroll-down").contains(key)) {
		scrollDown();
	}
	else if (key.toString(QKeySequence::PortableText).contains("Enter") ||
	         key.toString(QKeySequence::PortableText).contains("Return"))
	{
		chatEdit()->insert("\n");
	}
	else {
		e->ignore();
	}
}

void ChatDlgBase::activated()
{
	doFlash(false);
	chatEdit()->setFocus();

	if (unreadMessageCount() > 0) {
		setUnreadMessageCount(0);
	}
}

QString ChatDlgBase::desiredCaption() const
{
	QString cap = "";

	if (unreadMessageCount() > 0) {
		cap += "* ";
		if (unreadMessageCount() > 1) {
			cap += QString("[%1] ").arg(unreadMessageCount());
		}
	}
	cap += getDisplayName();
	return cap;
}

int ChatDlgBase::unreadMessageCount() const
{
	return pending_;
}

void ChatDlgBase::setUnreadMessageCount(int pending)
{
	if (pending_ != pending) {
		pending_ = pending;
		invalidateTab();
	}
}

bool ChatDlgBase::eventFilter(QObject *obj, QEvent *event)
{
	if (event->type() == QEvent::KeyPress) {
		keyPressEvent(static_cast<QKeyEvent*>(event));
		if (event->isAccepted())
			return true;
	}

	if (chatView()->handleCopyEvent(obj, event, chatEdit()))
		return true;

	return QWidget::eventFilter(obj, event);
}

void ChatDlgBase::optionsUpdate()
{
	setLooks();
	setShortcuts();
}

void ChatDlgBase::showEvent(QShowEvent* e)
{
	optionsUpdate();
	TabbableWidget::showEvent(e);
}
