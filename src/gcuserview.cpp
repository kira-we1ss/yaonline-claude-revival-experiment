/*
 * gcuserview.cpp - groupchat roster
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

#include "gcuserview.h"

#include <QPainter>
#include <QMimeData>
#include <QMenu>
#include <QAction>
#include <QHelpEvent>
#include <QHeaderView>
#include <QStyledItemDelegate>
#include <QApplication>
#include <QFontMetrics>

#include "capsmanager.h"
#include "psitooltip.h"
#include "psiaccount.h"
#include "userlist.h"
#include "psiiconset.h"
#include "psigroupchatdlg.h"
#include "common.h"
#include "psioptions.h"

static bool caseInsensitiveLessThan(const QString &s1, const QString &s2)
{
	return s1.toLower() < s2.toLower();
}

//----------------------------------------------------------------------------
// GCGroupDelegate — custom rendering for group header items
//----------------------------------------------------------------------------

class GCGroupDelegate : public QStyledItemDelegate
{
public:
	explicit GCGroupDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}

	void paint(QPainter *painter, const QStyleOptionViewItem &option,
	           const QModelIndex &index) const override
	{
		// Only custom-paint top-level (group) items
		if (index.parent().isValid()) {
			QStyledItemDelegate::paint(painter, option, index);
			return;
		}

		QStyleOptionViewItem opt = option;
		initStyleOption(&opt, index);

		painter->save();

		QFont f = opt.font;
		f.setPointSize(option.font.pointSize() - 1);
		painter->setFont(f);

		QPalette pal = opt.palette;
		painter->setPen(pal.color(QPalette::Text));

		QString text = index.data(Qt::DisplayRole).toString();
		QFontMetrics fm(f);
		int textWidth = fm.horizontalAdvance(text) + 8;

		painter->drawText(opt.rect.adjusted(4, 0, 0, 0), Qt::AlignVCenter, text);

		// Draw decorative line after text
		if (textWidth < opt.rect.width() - 8) {
			int h = opt.rect.center().y();
			painter->setPen(QPen(pal.color(QPalette::Mid)));
			painter->drawLine(textWidth, h, opt.rect.width() - 8, h);
			painter->setPen(QPen(pal.color(QPalette::Dark)));
			painter->drawLine(textWidth, h + 1, opt.rect.width() - 8, h + 1);
		}

		painter->restore();
	}
};

//----------------------------------------------------------------------------
// GCUserViewItem
//----------------------------------------------------------------------------

GCUserViewItem::GCUserViewItem(GCUserViewGroupItem *par)
	: QObject()
	, QTreeWidgetItem(par)
{
	setFlags(flags() | Qt::ItemIsDragEnabled);
}

bool GCUserViewItem::operator<(const QTreeWidgetItem &other) const
{
	return text(0).toLower() < other.text(0).toLower();
}

//----------------------------------------------------------------------------
// GCUserViewGroupItem
//----------------------------------------------------------------------------

GCUserViewGroupItem::GCUserViewGroupItem(GCUserView *par, const QString& t, int k)
	: QTreeWidgetItem(par)
	, key_(k)
{
	setText(0, t);
	setFlags(flags() & ~Qt::ItemIsDragEnabled & ~Qt::ItemIsSelectable);
}

bool GCUserViewGroupItem::operator<(const QTreeWidgetItem &other) const
{
	const GCUserViewGroupItem *g = dynamic_cast<const GCUserViewGroupItem*>(&other);
	if (g)
		return key_ < g->key_;
	return QTreeWidgetItem::operator<(other);
}

//----------------------------------------------------------------------------
// GCUserView
//----------------------------------------------------------------------------

GCUserView::GCUserView(QWidget* parent)
	: QTreeWidget(parent)
	, gcDlg_(0)
{
	setHeaderHidden(true);
	setRootIsDecorated(false);
	setIndentation(12);
	setSelectionMode(QAbstractItemView::SingleSelection);
	setDragEnabled(true);
	setDragDropMode(QAbstractItemView::DragOnly);
	setContextMenuPolicy(Qt::CustomContextMenu);
	setItemDelegate(new GCGroupDelegate(this));
	setSortingEnabled(true);
	sortItems(0, Qt::AscendingOrder);

	QTreeWidgetItem *i;
	i = new GCUserViewGroupItem(this, tr("Visitors"), 3);
	i->setExpanded(true);
	i = new GCUserViewGroupItem(this, tr("Participants"), 2);
	i->setExpanded(true);
	i = new GCUserViewGroupItem(this, tr("Moderators"), 1);
	i->setExpanded(true);

	connect(this, SIGNAL(itemDoubleClicked(QTreeWidgetItem *, int)),
	        SLOT(qlv_doubleClicked(QTreeWidgetItem *)));
	connect(this, SIGNAL(customContextMenuRequested(const QPoint &)),
	        SLOT(qlv_contextMenuRequested(const QPoint &)));
}

GCUserView::~GCUserView()
{
}

void GCUserView::setMainDlg(PsiGroupchatDlg* mainDlg)
{
	gcDlg_ = mainDlg;
}

QMimeData *GCUserView::mimeData(const QList<QTreeWidgetItem *> items) const
{
	if (!items.isEmpty()) {
		QTreeWidgetItem *it = items.first();
		if (it && it->parent()) {
			GCUserViewItem *u = (GCUserViewItem*) it;
			if (!u->s.mucItem().jid().isEmpty()) {
				QMimeData *md = new QMimeData;
				md->setText(u->s.mucItem().jid().bare());
				return md;
			}
		}
	}
	return QTreeWidget::mimeData(items);
}

void GCUserView::clear()
{
	for (int j = 0; j < topLevelItemCount(); ++j) {
		QTreeWidgetItem *group = topLevelItem(j);
		while (group->childCount() > 0)
			delete group->takeChild(0);
	}
}

void GCUserView::updateAll()
{
	for (int j = 0; j < topLevelItemCount(); ++j) {
		QTreeWidgetItem *group = topLevelItem(j);
		for (int k = 0; k < group->childCount(); ++k) {
			GCUserViewItem *i = (GCUserViewItem *)group->child(k);
			i->setIcon(0, PsiIconset::instance()->status(i->s).icon());
		}
	}
}

QStringList GCUserView::nickList() const
{
	QStringList list;
	for (int j = 0; j < topLevelItemCount(); ++j) {
		QTreeWidgetItem *group = topLevelItem(j);
		for (int k = 0; k < group->childCount(); ++k)
			list << group->child(k)->text(0);
	}
	qSort(list.begin(), list.end(), caseInsensitiveLessThan);
	return list;
}

bool GCUserView::hasJid(const Jid& jid)
{
	for (int j = 0; j < topLevelItemCount(); ++j) {
		QTreeWidgetItem *group = topLevelItem(j);
		for (int k = 0; k < group->childCount(); ++k) {
			GCUserViewItem *lvi = (GCUserViewItem *)group->child(k);
			if (!lvi->s.mucItem().jid().isEmpty() && lvi->s.mucItem().jid().compare(jid, false))
				return true;
		}
	}
	return false;
}

QTreeWidgetItem *GCUserView::findEntry(const QString &nick)
{
	for (int j = 0; j < topLevelItemCount(); ++j) {
		QTreeWidgetItem *group = topLevelItem(j);
		for (int k = 0; k < group->childCount(); ++k) {
			if (group->child(k)->text(0) == nick)
				return group->child(k);
		}
	}
	return nullptr;
}

void GCUserView::updateEntry(const QString &nick, const Status &s)
{
	GCUserViewItem *lvi = (GCUserViewItem *)findEntry(nick);
	if (lvi && lvi->s.mucItem().role() != s.mucItem().role()) {
		delete lvi;
		lvi = nullptr;
	}

	if (!lvi) {
		lvi = new GCUserViewItem(findGroup(s.mucItem().role()));
		lvi->setText(0, nick);
	}

	lvi->s = s;
	lvi->setIcon(0, PsiIconset::instance()->status(lvi->s).icon());
}

GCUserViewGroupItem* GCUserView::findGroup(MUCItem::Role a) const
{
	Role r = Visitor;
	if (a == MUCItem::Moderator)
		r = Moderator;
	else if (a == MUCItem::Participant)
		r = Participant;

	for (int j = 0; j < topLevelItemCount(); ++j) {
		GCUserViewGroupItem *g = (GCUserViewGroupItem *)topLevelItem(j);
		if (g->key() == (int)r + 1)
			return g;
	}
	return nullptr;
}

void GCUserView::removeEntry(const QString &nick)
{
	QTreeWidgetItem *lvi = findEntry(nick);
	if (lvi)
		delete lvi;
}

bool GCUserView::maybeTip(const QPoint &pos)
{
	QTreeWidgetItem *qlvi = itemAt(pos);
	if (!qlvi || !qlvi->parent())
		return false;

	GCUserViewItem *lvi = (GCUserViewItem *) qlvi;
	QRect r(visualItemRect(lvi));

	const QString &nick = lvi->text(0);
	const Status &s = lvi->s;
	UserListItem u;
	PsiGroupchatDlg* dlg = gcDlg_;
	if (!dlg) {
		qDebug("Calling maybetip on an entity without an owning dialog");
		return false;
	}
	u.setJid(dlg->jid().withResource(nick));
	u.setName(nick);

	Jid caps_jid(s.mucItem().jid().isEmpty() ? dlg->jid().withResource(nick) : s.mucItem().jid());
	QString client_name = dlg->account()->capsManager()->clientName(caps_jid);
	QString client_version = (client_name.isEmpty() ? QString() : dlg->account()->capsManager()->clientVersion(caps_jid));

	UserResource ur;
	ur.setName(nick);
	ur.setStatus(s);
	ur.setClient(client_name, client_version, "");
	u.userResourceList().append(ur);

	PsiToolTip::showText(mapToGlobal(pos), u.makeTip(), this);
	return true;
}

bool GCUserView::event(QEvent* e)
{
	if (e->type() == QEvent::ToolTip) {
		QPoint pos = ((QHelpEvent*) e)->pos();
		e->setAccepted(maybeTip(pos));
		return true;
	}
	return QTreeWidget::event(e);
}

void GCUserView::qlv_doubleClicked(QTreeWidgetItem *i)
{
	if (!i || !i->parent())
		return;

	GCUserViewItem *lvi = (GCUserViewItem *)i;
	if (option.defaultAction == 0)
		emit action(lvi->text(0), lvi->s, 0);
	else
		emit action(lvi->text(0), lvi->s, 1);
}

void GCUserView::qlv_contextMenuRequested(const QPoint &pos)
{
	QTreeWidgetItem *i = itemAt(pos);
	if (!i || !i->parent() || !gcDlg_)
		return;

	QPointer<GCUserViewItem> lvi = (GCUserViewItem *)i;
	bool self = gcDlg_->nick() == i->text(0);
	GCUserViewItem* c = (GCUserViewItem*) findEntry(gcDlg_->nick());
	if (!c) {
		qWarning(QString("groupchatdlg.cpp: Self ('%1') not found in contactlist").arg(gcDlg_->nick()).toLatin1());
		return;
	}

	QMenu *pm = new QMenu(this);
	if (PsiOptions::instance()->getOption("options.ui.message.enabled").toBool()) {
		pm->addAction(IconsetFactory::icon("psi/sendMessage").icon(), tr("Send &message"))->setData(0);
	}
	pm->addAction(IconsetFactory::icon("psi/start-chat").icon(), tr("Open &chat window"))->setData(1);
	pm->addSeparator();

	QAction *kickAct = pm->addAction(tr("&Kick"));
	kickAct->setData(10);
	kickAct->setEnabled(MUCManager::canKick(c->s.mucItem(), lvi->s.mucItem()));

	QAction *banAct = pm->addAction(tr("&Ban"));
	banAct->setData(11);
	banAct->setEnabled(MUCManager::canBan(c->s.mucItem(), lvi->s.mucItem()));

	QMenu *rm = new QMenu(tr("Change role"), pm);
	QAction *visitorAct = rm->addAction(tr("Visitor"));
	visitorAct->setData(12);
	visitorAct->setCheckable(true);
	visitorAct->setChecked(lvi->s.mucItem().role() == MUCItem::Visitor);
	visitorAct->setEnabled((!self || lvi->s.mucItem().role() == MUCItem::Visitor) && MUCManager::canSetRole(c->s.mucItem(), lvi->s.mucItem(), MUCItem::Visitor));

	QAction *participantAct = rm->addAction(tr("Participant"));
	participantAct->setData(13);
	participantAct->setCheckable(true);
	participantAct->setChecked(lvi->s.mucItem().role() == MUCItem::Participant);
	participantAct->setEnabled((!self || lvi->s.mucItem().role() == MUCItem::Participant) && MUCManager::canSetRole(c->s.mucItem(), lvi->s.mucItem(), MUCItem::Participant));

	QAction *moderatorAct = rm->addAction(tr("Moderator"));
	moderatorAct->setData(14);
	moderatorAct->setCheckable(true);
	moderatorAct->setChecked(lvi->s.mucItem().role() == MUCItem::Moderator);
	moderatorAct->setEnabled((!self || lvi->s.mucItem().role() == MUCItem::Moderator) && MUCManager::canSetRole(c->s.mucItem(), lvi->s.mucItem(), MUCItem::Moderator));

	pm->addMenu(rm);
	pm->addSeparator();

#ifndef YAPSI
	pm->addAction(tr("Check &Status"))->setData(2);
	pm->addAction(IconsetFactory::icon("psi/vCard").icon(), tr("User &Info"))->setData(3);
#endif

	QAction *chosen = pm->exec(mapToGlobal(pos));
	delete pm;

	if (!chosen || lvi.isNull())
		return;

	int x = chosen->data().toInt();
	if (!chosen->isEnabled())
		return;

	emit action(lvi->text(0), lvi->s, x);
}
