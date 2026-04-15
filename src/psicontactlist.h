/*
 * psicontactlist.h - general abstraction of the psi-specific contact list
 * Copyright (C) 2006  Michail Pishchagin
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

#ifndef PSICONTACTLIST_H
#define PSICONTACTLIST_H

#include <QList>
#include <QPointer>

#include "profiles.h"

using namespace XMPP;

class PsiCon;
class PsiAccount;
namespace XMPP { class Jid; }
class PsiContact;
#ifdef YAPSI
class YaPddManager;
class YaContactListModel;
#endif

class PsiContactList : public QObject
{
	Q_OBJECT
public:
	PsiContactList(PsiCon* psi);
	~PsiContactList();

	PsiCon* psi() const;
#ifdef YAPSI
	YaPddManager* yaPddManager() const;
#endif

	bool showAgents() const;
	bool showHidden() const;
	bool showSelf() const;
	bool showOffline() const;
	bool showGroups() const;

	bool accountsLoaded() const;

	PsiAccount* getAccount(const QString& id) const;
	PsiAccount* getAccountByJid(const XMPP::Jid& jid) const;

	const QList<PsiAccount*>& accounts() const;
	const QList<PsiAccount*>& enabledAccounts() const;
#ifdef YAPSI
	QList<PsiAccount*> sortedEnabledAccounts() const;
#endif
	bool haveActiveAccounts() const;
	bool haveAvailableAccounts() const;
	bool haveEnabledAccounts() const;
	bool haveConnectingAccounts() const;

	PsiAccount *defaultAccount() const;
#ifdef YAPSI_ACTIVEX_SERVER
	PsiAccount* onlineAccount() const;
#endif
#ifdef YAPSI
	PsiAccount* yaServerHistoryAccount() const;
	PsiAccount* yandexTeamAccount() const;
#endif

	UserAccountList getUserAccountList() const;

	PsiAccount* createAccount(const QString& name, const Jid& j = "", const QString& pass = "", bool opt_host = false, const QString& host = "", int port = 5222, bool legacy_ssl_probe = true, UserAccount::SSLFlag ssl = UserAccount::SSL_Auto, int proxy = 0, bool modify = true);
	void createAccount(const UserAccount&);
	void removeAccount(PsiAccount*);
	void setAccountEnabled(PsiAccount*, bool enabled = true);

	int queueCount() const;
	PsiAccount *queueLowestEventId();

	void loadAccounts(const UserAccountList &);
	void link(PsiAccount*);
	void unlink(PsiAccount*);

	const QList<PsiContact*>& contacts() const;

#ifdef YAPSI
	QString firstErrorMessage() const;
	PsiContact* addFakeTempContact(const XMPP::Jid& jid, PsiAccount* account);
	void setContactListModel(YaContactListModel* model);
#endif

	QStringList connectingAccounts() const;

public slots:
	void setShowAgents(bool);
	void setShowHidden(bool);
	void setShowSelf(bool);
	void setShowOffline(bool);
	void setShowGroups(bool);

signals:
	void showAgentsChanged(bool);
	void showHiddenChanged(bool);
	void showSelfChanged(bool);
	bool showOfflineChanged(bool);
	bool showGroupsChanged(bool);

signals:
	void addedContact(PsiContact*);
	void removedContact(PsiContact*);

	void beginBulkContactUpdate();
	void endBulkContactUpdate();
	void rosterRequestFinished();

	void connectingAccountsChanged();

#ifdef YAPSI
	void firstErrorMessageChanged(const QString& firstErrorMessage);
#endif

	/**
	 * This signal is emitted when account is loaded from disk or created
	 * anew.
	 */
	void accountAdded(PsiAccount*);
	/**
	 * This signal is emitted when account is completely removed from the
	 * program, i.e. deleted.
	 */
	void accountRemoved(PsiAccount*);
	/**
	 * This signal is emitted when number of accounts managed by
	 * PsiContactList changes.
	 */
	void accountCountChanged();
	/**
	 * This signal is emitted when one of the accounts emits
	 * updatedActivity() signal.
	 */
	void accountActivityChanged();
	/**
	 * Emitted when account count changes, account's state (or properties) changes.
	 */
	void accountStateChanged();
	/**
	 * This signal is emitted when the features of one of the accounts change.
	 */
	void accountFeaturesChanged();
	/**
	 * This signal is emitted when either new account was added, or 
	 * existing one was removed altogether.
	 */
	void saveAccounts();
	/**
	 * This signal is emitted when event queue of one account was changed
	 * by adding or removing an event.
	 */
	void queueChanged();
	/**
	 * This signal is emitted at the end of loadAccounts() call, when all
	 * the accounts are initialized.
	 */
	void loadedAccounts();
	/**
	 * This signal is emitted when PsiContactList enters its final steps before
	 * total destruction, and all accounts are still accessible for last
	 * minute processing.
	 */
	void destroying();

private slots:
	void accountEnabledChanged();
	void accountAddedContact(PsiContact*);
	void accountRemovedContact(PsiContact*);
#ifdef YAPSI
	void updateErrorMessage();
#endif

	void private_accountStateChanged();

private:
	PsiAccount *loadAccount(const UserAccount &);
	PsiAccount *tryQueueLowestEventId(bool includeDND);

	PsiCon *psi_;
#ifdef YAPSI
	YaPddManager* yaPddManager_;
#endif
#ifdef YAPSI_ACTIVEX_SERVER
	PsiAccount* onlineAccount_;
#endif
	QList<PsiAccount *> accounts_, enabledAccounts_;
	QList<PsiContact *> contacts_;

	bool showAgents_;
	bool showHidden_;
	bool showSelf_;
	bool showOffline_;
	bool showGroups_;
	bool accountsLoaded_;
#ifdef YAPSI
	QString firstErrorMessage_;
	QTimer* updateErrorMessageTimer_;
	QPointer<YaContactListModel> contactListModel_;
#endif
	QStringList lastConnectingAccounts_;

	void addEnabledAccount(PsiAccount* account);
	void removeEnabledAccount(PsiAccount* account);
};

#endif
