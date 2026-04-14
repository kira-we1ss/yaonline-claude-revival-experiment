/*
 * yachatdlgshared.cpp
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

#include "yachatdlgshared.h"

#include <QAction>
#include <QMenu>

#include "yanaroddiskmanager.h"
#include "psioptions.h"
#include "common.h"
#include "psicon.h"

static const QString emoticonsEnabledOptionPath = "options.ya.emoticons-enabled";
static const QString enableTypographyOptionPath = "options.ya.typography.enable";
static const QString spellCheckEnabledOptionPath = "options.ui.spell-check.enabled";
static const QString sendButtonEnabledOptionPath = "options.ya.chat-window.send-button.enabled";

YaChatDlgShared* YaChatDlgShared::instance_ = 0;

YaChatDlgShared* YaChatDlgShared::instance(PsiCon* controller)
{
	if (!instance_ || instance_->controller_ != controller) {
		instance_ = new YaChatDlgShared(controller);
	}
	return instance_;
}

void YaChatDlgShared::typographyActionTriggered(bool enabled)
{
	PsiOptions::instance()->setOption(enableTypographyOptionPath, enabled);
}

void YaChatDlgShared::emoticonsActionTriggered(bool enabled)
{
	option.useEmoticons = enabled;
	PsiOptions::instance()->setOption(emoticonsEnabledOptionPath, enabled);
}

void YaChatDlgShared::checkSpellingActionTriggered(bool enabled)
{
	PsiOptions::instance()->setOption(spellCheckEnabledOptionPath, enabled);
}

void YaChatDlgShared::sendButtonEnabledActionTriggered(bool enabled)
{
	PsiOptions::instance()->setOption(sendButtonEnabledOptionPath, enabled);
}

void YaChatDlgShared::optionChanged(const QString& option)
{
	if (option == emoticonsEnabledOptionPath) {
		emoticonsAction_->setChecked(PsiOptions::instance()->getOption(emoticonsEnabledOptionPath).toBool());
	}
	else if (option == enableTypographyOptionPath) {
		typographyAction_->setChecked(PsiOptions::instance()->getOption(enableTypographyOptionPath).toBool());
	}
	else if (option == spellCheckEnabledOptionPath) {
		checkSpellingAction_->setChecked(PsiOptions::instance()->getOption(spellCheckEnabledOptionPath).toBool());
	}
	else if (option == sendButtonEnabledOptionPath) {
		sendButtonEnabledAction_->setChecked(PsiOptions::instance()->getOption(sendButtonEnabledOptionPath).toBool());
	}
}

void YaChatDlgShared::updateRecentFiles()
{
	Q_ASSERT(!recentFilesMenu_.isNull());
	if (recentFilesMenu_.isNull())
		return;

	QList<YaNarodDiskManager::RecentFile> files;

	recentFilesMenu_->clear();
	QMapIterator<QString, YaNarodDiskManager::RecentFile> it(controller_->yaNarodDiskManager()->recentFiles());
	while (it.hasNext()) {
		it.next();

		if (files.count() < 10) {
			files << it.value();
		}
	}

	qSort(files);

	foreach(const YaNarodDiskManager::RecentFile& f, files) {
		QAction* act = new QAction(YaNarodDiskManager::humanReadableName(f.fileName, f.size), recentFilesMenu_);
		act->setProperty("name", f.fileName);
		act->setProperty("url", f.url);
		act->setProperty("size", f.size);
		connect(act, SIGNAL(triggered()), SLOT(uploadRecentFile()));
		recentFilesMenu_->addAction(act);
	}

	recentFilesMenu_->setEnabled(!recentFilesMenu_->actions().isEmpty());

	recentFilesMenu_->addSeparator();
	recentFilesMenu_->addAction(clearRecentFilesAction_);
}

void YaChatDlgShared::uploadRecentFile()
{
	QAction* action = static_cast<QAction*>(sender());
	emit uploadRecentFile(action->property("name").toString(), action->property("url").toString(), action->property("size").toULongLong());
}

void YaChatDlgShared::clearRecentFiles()
{
	controller_->yaNarodDiskManager()->clearRecentFiles();
}

YaChatDlgShared::YaChatDlgShared(PsiCon* controller)
	: QObject(controller)
	, controller_(controller)
{
	recentFilesMenu_ = new QMenu(tr("Recent files"));
	clearRecentFilesAction_ = new QAction(tr("Clear"), this);
	connect(clearRecentFilesAction_, SIGNAL(triggered()), SLOT(clearRecentFiles()));
	connect(controller_->yaNarodDiskManager(), SIGNAL(recentFilesChanged()), SLOT(updateRecentFiles()));
	updateRecentFiles();

	uploadFileAction_ = new QAction(tr("Send file"), this);
	connect(uploadFileAction_, SIGNAL(triggered()), SIGNAL(uploadFile()));

	typographyAction_ = new QAction(tr("Typographica"), this);
	typographyAction_->setCheckable(true);
	connect(typographyAction_, SIGNAL(triggered(bool)), SLOT(typographyActionTriggered(bool)));

	emoticonsAction_ = new QAction(tr("Enable emoticons"), this);
	emoticonsAction_->setCheckable(true);
	connect(emoticonsAction_, SIGNAL(triggered(bool)), SLOT(emoticonsActionTriggered(bool)));

	checkSpellingAction_ = new QAction(tr("Check spelling"), this);
	checkSpellingAction_->setCheckable(true);
	connect(checkSpellingAction_, SIGNAL(triggered(bool)), SLOT(checkSpellingActionTriggered(bool)));

	sendButtonEnabledAction_ = new QAction(tr("Show 'Send' button"), this);
	sendButtonEnabledAction_->setCheckable(true);
	connect(sendButtonEnabledAction_, SIGNAL(triggered(bool)), SLOT(sendButtonEnabledActionTriggered(bool)));

	connect(PsiOptions::instance(), SIGNAL(optionChanged(const QString&)), SLOT(optionChanged(const QString&)));
	optionChanged(emoticonsEnabledOptionPath);
	optionChanged(enableTypographyOptionPath);
	optionChanged(spellCheckEnabledOptionPath);
	optionChanged(sendButtonEnabledOptionPath);
}

YaChatDlgShared::~YaChatDlgShared()
{
	delete recentFilesMenu_;
}
