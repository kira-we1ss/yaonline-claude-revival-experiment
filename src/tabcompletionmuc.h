/*
 * tabcompletionmuc.h
 * Copyright (C) 2001-2010  Michail Pishchagin, Martin Hostettler
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

#ifndef TABCOMPLETIONMUC_H
#define TABCOMPLETIONMUC_H

#include "tabcompletion.h"
#include "groupchatdlg.h"

class TabCompletionMUC : public TabCompletion {
public:
	GCMainDlg *p_;
	TabCompletionMUC(GCMainDlg *p)
		: p_(p)
		, nickSeparator(":")
	{};

	virtual void setup(QString str, int pos, int &start, int &end) {
		// if (p_->mCmdSite.isActive()) {
		// 	mCmdList_ = p_->mCmdManager.completeCommand(str, pos, start, end);
		// } else {
			TabCompletion::setup(str, pos, start, end);
		// }
	}

	virtual QStringList possibleCompletions() {
		// if (p_->mCmdSite.isActive()) {
		// 	return mCmdList_;
		// }
		QStringList suggestedNicks;
		QStringList nicks = allNicks();

		QString postAdd = atStart_ ? nickSeparator + " " : "";

		foreach(QString nick, nicks) {
			if (nick.left(toComplete_.length()).toLower() == toComplete_.toLower()) {
				suggestedNicks << nick + postAdd;
			}
		}
		return suggestedNicks;
	};

	virtual QStringList allChoices(QString &guess) {
		// if (p_->mCmdSite.isActive()) {
		// 	guess = QString();
		// 	return mCmdList_;
		// }
		guess = p_->lastReferrer();
		if (!guess.isEmpty() && atStart_) {
			guess += nickSeparator + " ";
		}

		QStringList all = allNicks();

		if (atStart_) {
			QStringList::Iterator it = all.begin();
			for ( ; it != all.end(); ++it) {
				*it = *it + nickSeparator + " ";
			}
		}
		return all;
	};

	QStringList allNicks() {
		return p_->nickList();
	}

	QStringList mCmdList_;

	// FIXME where to move this?
	QString nickSeparator; // equals ":"
};

#endif
