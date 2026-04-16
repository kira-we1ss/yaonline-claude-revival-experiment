/*
 * accountmanagedlg.cpp - dialogs for manipulating PsiAccounts
 * Copyright (C) 2001, 2002  Justin Karneges
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

#include <QtCrypto>
#include <QInputDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QButtonGroup>
#include <QLineEdit>
#include <QLabel>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include "psicon.h"
#include "psiaccount.h"
#include "common.h"
#include "xmpp_tasks.h"
#include "proxy.h"
#include "miniclient.h"
#include "accountadddlg.h"
#include "accountmanagedlg.h"
#include "ui_accountremove.h"
#include "psicontactlist.h"

using namespace XMPP;


//----------------------------------------------------------------------------
// AccountRemoveDlg
//----------------------------------------------------------------------------

class AccountRemoveDlg : public QDialog, public Ui::AccountRemove
{
	Q_OBJECT
public:
	AccountRemoveDlg(ProxyManager *, const UserAccount &, QWidget *parent = nullptr);
	~AccountRemoveDlg();

protected:
	// reimplemented
	//void closeEvent(QCloseEvent *);

public slots:
	void done(int);

private slots:
	void remove();
	void bg_clicked(int);

	void client_handshaken();
	void client_error();
	void client_disconnected();
	void unreg_finished();

private:
	class Private;
	Private *d;

	MiniClient *client;
};

class AccountRemoveDlg::Private
{
public:
	Private() {}

	UserAccount acc;
	QButtonGroup *bg;
	ProxyManager *proxyman;
};

AccountRemoveDlg::AccountRemoveDlg(ProxyManager *proxyman, const UserAccount &acc, QWidget *parent)
	: QDialog(parent)
	, d(new Private)
	, client(nullptr)
{
	setupUi(this);
	setModal(false);
	d->acc = acc;
	d->proxyman = proxyman;

	setWindowTitle(CAP(caption()));

	connect(pb_close, &QAbstractButton::clicked, this, &QWidget::close);
	connect(pb_remove, &QAbstractButton::clicked, this, &AccountRemoveDlg::remove);

	d->bg = new QButtonGroup(this);
	d->bg->addButton(rb_remove, 0);
	d->bg->addButton(rb_removeAndUnreg, 1);
	connect(d->bg, static_cast<void (QButtonGroup::*)(int)>(&QButtonGroup::buttonClicked),
	        this, &AccountRemoveDlg::bg_clicked);
	rb_remove->setChecked(true);
	bg_clicked(0);

	pb_close->setFocus();

	client = new MiniClient(this);
	connect(client, &MiniClient::handshaken, this, &AccountRemoveDlg::client_handshaken);
	connect(client, &MiniClient::error, this, &AccountRemoveDlg::client_error);
	connect(client, &MiniClient::disconnected, this, &AccountRemoveDlg::client_disconnected);
}

AccountRemoveDlg::~AccountRemoveDlg()
{
	delete d;
}

/*void AccountRemoveDlg::closeEvent(QCloseEvent *e)
{
	e->ignore();
	reject();
}*/

void AccountRemoveDlg::done(int r)
{
	if(busy->isActive()) {
		int n = QMessageBox::information(this, tr("Warning"), tr("Are you sure you want to cancel the unregistration?"), tr("&Yes"), tr("&No"));
		if(n != 0)
			return;
	}
	QDialog::done(r);
}

void AccountRemoveDlg::bg_clicked(int x)
{
	if(x == 0) {
		lb_pass->setEnabled(false);
		le_pass->setEnabled(false);
	}
	else if(x == 1) {
		lb_pass->setEnabled(true);
		le_pass->setEnabled(true);
		le_pass->setFocus();
	}
}

void AccountRemoveDlg::remove()
{
	bool unreg = rb_removeAndUnreg->isChecked();

	if(unreg) {
		if(!d->acc.pass.isEmpty() && le_pass->text() != d->acc.pass) {
			QMessageBox::information(this, tr("Error"), tr("Password does not match account.  Please try again."));
			le_pass->setFocus();
			return;
		}
	}

	int n = QMessageBox::information(this, tr("Warning"), tr("Are you sure you want to remove <b>%1</b> ?").arg(d->acc.name), tr("&Yes"), tr("&No"));
	if(n != 0)
		return;

	if(!unreg) {
		accept();
		return;
	}

	busy->start();
	gb_account->setEnabled(false);
	pb_remove->setEnabled(false);

	Jid j = d->acc.jid;
	QString pass = le_pass->text();
	j.setResource(d->acc.resource);
	client->connectToServer(j, d->acc.legacy_ssl_probe, d->acc.ssl == UserAccount::SSL_Legacy, d->acc.ssl == UserAccount::SSL_Yes, d->acc.opt_host ? d->acc.host : QString(), d->acc.port, d->proxyman, d->acc.proxy_index, &pass);
}

void AccountRemoveDlg::client_handshaken()
{
	// Workaround for servers that do not send a response to the remove request
	client->setErrorOnDisconnect(false);

	// try to unregister an account
	auto *reg = new JT_Register(client->client()->rootTask());
	connect(reg, &JT_Register::finished, this, &AccountRemoveDlg::unreg_finished);
	reg->unreg();
	reg->go(true);
}

void AccountRemoveDlg::client_error()
{
	busy->stop();
	gb_account->setEnabled(true);
	pb_remove->setEnabled(true);
}

void AccountRemoveDlg::unreg_finished()
{
	auto *reg = qobject_cast<JT_Register *>(sender());

	if (!reg)
		return;

	client->close();
	busy->stop();

	if(reg->success()) {
		QMessageBox::information(this, tr("Success"), tr("The account was unregistered successfully."));
		accept();
		return;
	}
	else if(reg->statusCode() != Task::ErrDisc) {
		gb_account->setEnabled(true);
		pb_remove->setEnabled(true);
		QMessageBox::critical(this, tr("Error"), QString(tr("There was an error unregistering the account.\nReason: %1")).arg(reg->statusString()));
	}
}

void AccountRemoveDlg::client_disconnected()
{
	// Workaround for servers that do not send a response to the remove request
	busy->stop();
	QMessageBox::information(this, tr("Success"), tr("The account was unregistered successfully."));
	accept();
}

//----------------------------------------------------------------------------
// AccountManageDlg
//----------------------------------------------------------------------------
class AccountManageItem : public QTreeWidgetItem
{
public:
	AccountManageItem(QTreeWidget *par, PsiAccount *_pa)
	: QTreeWidgetItem(par)
	{
		pa = _pa;
		setFlags(flags() | Qt::ItemIsUserCheckable);
		updateInfo();
	}

	void updateInfo()
	{
		const UserAccount &acc = pa->userAccount();
		setText(0, pa->name());
		Jid j = acc.jid;
		setText(1, acc.opt_host && acc.host.length() ? acc.host : j.host());
		setText(2, pa->isActive() ? AccountManageDlg::tr("Active") : AccountManageDlg::tr("Not active"));
		setCheckState(0, pa->enabled() ? Qt::Checked : Qt::Unchecked);
	}

	PsiAccount *pa;
};

AccountManageDlg::AccountManageDlg(PsiCon *_psi)
	: QDialog(nullptr)
	, psi(_psi)
{
	setupUi(this);
	setModal(false);
	psi->dialogRegister(this);

	setWindowTitle(CAP(caption()));

	// setup signals
	connect(pb_add, &QAbstractButton::clicked, this, &AccountManageDlg::add);
	connect(pb_modify, &QAbstractButton::clicked, this, qOverload<>(&AccountManageDlg::modify));
	connect(pb_remove, &QAbstractButton::clicked, this, &AccountManageDlg::remove);
	connect(pb_close, &QAbstractButton::clicked, this, &QWidget::close);

	connect(lv_accs, &QTreeWidget::itemDoubleClicked, this,
	        [this](QTreeWidgetItem *item, int) { modify(item); });
	connect(lv_accs, &QTreeWidget::itemSelectionChanged,
	        this, &AccountManageDlg::qlv_selectionChanged);
	connect(lv_accs, &QTreeWidget::itemChanged,
	        this, &AccountManageDlg::qlv_itemChanged);
	connect(psi, &PsiCon::accountAdded, this, &AccountManageDlg::accountAdded);
	connect(psi, &PsiCon::accountUpdated, this, &AccountManageDlg::accountUpdated);
	connect(psi, &PsiCon::accountRemoved, this, &AccountManageDlg::accountRemoved);

	lv_accs->setAllColumnsShowFocus(true);
	lv_accs->header()->setStretchLastSection(true);

	const auto accounts = psi->contactList()->accounts();
	for (PsiAccount *pa : accounts)
		new AccountManageItem(lv_accs, pa);

	if(lv_accs->topLevelItemCount() > 0)
		lv_accs->setCurrentItem(lv_accs->topLevelItem(0));
	else
		qlv_selectionChanged();
}

AccountManageDlg::~AccountManageDlg()
{
	psi->dialogUnregister(this);
}

void AccountManageDlg::qlv_selectionChanged()
{
	auto *i = dynamic_cast<AccountManageItem *>(lv_accs->currentItem());
	const bool ok = (i != nullptr);

	pb_modify->setEnabled(ok);
	pb_remove->setEnabled(ok);
}

void AccountManageDlg::qlv_itemChanged(QTreeWidgetItem *lvi, int column)
{
	if (column != 0)
		return;
	auto *i = dynamic_cast<AccountManageItem *>(lvi);
	if (!i)
		return;
	const bool s = (i->checkState(0) == Qt::Checked);
	if (i->pa->enabled() != s)
		i->pa->setEnabled(s);
	i->updateInfo();
}

void AccountManageDlg::add()
{
	auto *w = new AccountAddDlg(psi, nullptr);
	w->show();
}

void AccountManageDlg::modify()
{
	modify(lv_accs->currentItem());
}

void AccountManageDlg::modify(QTreeWidgetItem *lvi)
{
	auto *i = dynamic_cast<AccountManageItem *>(lvi);
	if(!i)
		return;

	i->pa->modify();
}

void AccountManageDlg::remove()
{
	auto *i = dynamic_cast<AccountManageItem *>(lv_accs->currentItem());
	if(!i)
		return;

	if(i->pa->isActive()) {
		QMessageBox::information(this, tr("Error"), tr("Unable to remove the account, as it is currently active."));
		return;
	}

	AccountRemoveDlg w(psi->proxy(), i->pa->userAccount());
	const int n = w.exec();
	if(n != QDialog::Accepted) {
		return;
	}
	psi->removeAccount(i->pa);
}

void AccountManageDlg::accountAdded(PsiAccount *pa)
{
	new AccountManageItem(lv_accs, pa);
}

void AccountManageDlg::accountUpdated(PsiAccount *pa)
{
	for (int n = 0; n < lv_accs->topLevelItemCount(); ++n) {
		auto *i = dynamic_cast<AccountManageItem *>(lv_accs->topLevelItem(n));
		if (i && i->pa == pa) {
			i->updateInfo();
			return;
		}
	}
}

void AccountManageDlg::accountRemoved(PsiAccount *pa)
{
	for (int n = 0; n < lv_accs->topLevelItemCount(); ++n) {
		auto *i = dynamic_cast<AccountManageItem *>(lv_accs->topLevelItem(n));
		if (i && i->pa == pa) {
			delete i;
			qlv_selectionChanged();
			return;
		}
	}
}

#include "accountmanagedlg.moc"

