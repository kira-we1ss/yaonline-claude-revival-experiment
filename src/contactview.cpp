/*
 * contactview.cpp - contact list widget
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

#include "contactview.h"

#include <QFileDialog>
#include <QApplication>
#include <QList>
#include <QTimer>
#include <QPainter>
#include <QMenu>
#include <QMessageBox>
#include <QInputDialog>
#include <QIcon>
#include <QLayout>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QEvent>
#include <QHelpEvent>
#include <QMimeData>
#include <QDrag>
#include <QDropEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QPixmap>
#include <QUrl>
#include <QDesktopWidget>
#include <QTreeWidgetItemIterator>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QHeaderView>
#include <algorithm>
#include <stdlib.h>
#include "common.h"
#include "userlist.h"
#include "psiaccount.h"
#include "psicon.h"
#include "jidutil.h"
#include "psioptions.h"
#include "iconaction.h"
#include "alerticon.h"
#include "avatars.h"
#include "psiiconset.h"
#include "serverinfomanager.h"
#include "pepmanager.h"
#include "psitooltip.h"
#include "capsmanager.h"
#include "resourcemenu.h"
#include "shortcutmanager.h"
#include "xmpp_message.h"
#include "textutil.h"
#ifdef HAVE_PGPUTIL
#include "pgputil.h"
#endif


static inline int rankStatus(int status) 
{
	switch (status) {
		case STATUS_CHAT : return 0;
		case STATUS_ONLINE : return 1;
		case STATUS_AWAY : return 2;
		case STATUS_XA : return 3;
		case STATUS_DND : return 4;
		case STATUS_INVISIBLE: return 5;
		default:
			return 6;
	}
	return 0;
}

static bool caseInsensitiveLessThan(const QString &s1, const QString &s2)
{
	return s1.toLower() < s2.toLower();
}

static QStringList decodeLocalDropFiles(const QMimeData *mimeData)
{
	QStringList files;
	if (!mimeData || !mimeData->hasUrls())
		return files;

	const QList<QUrl> urls = mimeData->urls();
	for (QList<QUrl>::ConstIterator it = urls.begin(); it != urls.end(); ++it) {
		if (!it->isLocalFile())
			continue;

		const QString localFile = it->toLocalFile();
		if (!localFile.isEmpty())
			files.append(localFile);
	}

	return files;
}

static bool decodeDropText(const QMimeData *mimeData, QString *text)
{
	if (!mimeData || !mimeData->hasText())
		return false;

	const QString dropText = mimeData->text().trimmed();
	if (dropText.isEmpty())
		return false;

	if (text)
		*text = dropText;
	return true;
}

static QDrag *createContactTextDrag(const QString &text, QWidget *source, const QPixmap &pixmap)
{
	QMimeData *mimeData = new QMimeData;
	mimeData->setText(text);

	QDrag *drag = new QDrag(source);
	drag->setMimeData(mimeData);
	if (!pixmap.isNull()) {
		drag->setPixmap(pixmap);
		drag->setHotSpot(QPoint(8, 8));
	}
	return drag;
}

static int fontMetricsTextWidth(const QFontMetrics &fontMetrics, const QString &text)
{
#if QT_VERSION >= 0x050b00
	return fontMetrics.horizontalAdvance(text);
#else
	return fontMetrics.width(text);
#endif
}

//----------------------------------------------------------------------------
// ContactProfile
//----------------------------------------------------------------------------
class ContactProfile::Entry
{
public:
	Entry()
	{
		alerting = false;
	}
	~Entry()
	{
	}

	UserListItem u;
	QList<ContactViewItem*> cvi;
	bool alerting;
	PsiIcon anim;
};

class ContactProfile::Private : public QObject
{
	Q_OBJECT
public:
	Private() {}

	QString name;
	ContactView *cv;
	ContactViewItem *cvi;
	ContactViewItem *self;
	UserListItem su;
	QList<Entry*> roster;
	QList<ContactViewItem*> groups;
	int oldstate;
	QTimer *t;
	PsiAccount *pa;
	bool v_enabled;

public slots:
	/*
	 * \brief This slot is toggled when number of active accounts is changed
	 *
	 * At the moment, it tries to recalculate the roster size.
	 */
	void numAccountsChanged()
	{
		cv->recalculateSize();
	}
};

ContactProfile::ContactProfile(PsiAccount *pa, const QString &name, ContactView *cv, bool unique)
{
	d = new Private;
	d->pa = pa;
	d->v_enabled = d->pa->enabled();
	d->name = name;
	d->cv = cv;
	d->cv->link(this);
	d->t = new QTimer;
	d->t->setSingleShot(true);
	connect(d->t, &QTimer::timeout, this, &ContactProfile::updateGroups);
	connect(pa->psi(), &PsiCon::accountCountChanged, d, &Private::numAccountsChanged);

	d->self = 0;

	if(!unique)
		d->cvi = new ContactViewItem(name, this, d->cv);
	else
		d->cvi = 0;

	d->oldstate = -2;

	deferredUpdateGroups();
}

ContactProfile::~ContactProfile()
{
	// delete the roster
	clear();

	// clean up
	delete d->self;
	delete d->cvi;

	delete d->t;
	d->cv->unlink(this);

	delete d;
}

void ContactProfile::setEnabled(bool e)
{
	d->v_enabled = e;
	if(d->v_enabled){
		if(!d->cvi)
			d->cvi = new ContactViewItem(d->name, this, d->cv);
		addAllNeededContactItems();
	}
	else{
		if(d->self)
			removeSelf();

		removeAllUnneededContactItems();
		if(d->cvi)
			delete d->cvi;
		d->cvi = 0;
		d->self = 0;
	}
}

ContactView *ContactProfile::contactView() const
{
	return d->cv;
}

ContactViewItem *ContactProfile::self() const
{
	return d->self;
}

PsiAccount *ContactProfile::psiAccount() const
{
	return d->pa;
}

const QString & ContactProfile::name() const
{
	return d->name;
}

void ContactProfile::setName(const QString &name)
{
	d->name = name;
	if(d->cvi)
		d->cvi->setProfileName(name);
}

void ContactProfile::setName(const char *s)
{
	setObjectName(QString::fromLatin1(s));
}

void ContactProfile::setState(int state)
{
	if(state == d->oldstate)
		return;
	d->oldstate = state;

	if(d->cvi) {
		d->cv->resetAnim();
		d->cvi->setProfileState(state);
	}
}

void ContactProfile::setUsingSSL(bool on)
{
	if(d->cvi)
		d->cvi->setProfileSSL(on);
}

ContactViewItem *ContactProfile::addGroup(int type)
{
	ContactViewItem *item;

	QString gname;
	if(type == ContactViewItem::gGeneral)
		gname = tr("General");
	else if(type == ContactViewItem::gNotInList)
		gname = tr("Not in list");
	else if(type == ContactViewItem::gAgents)
		gname = tr("Agents/Transports");
	else if(type == ContactViewItem::gPrivate)
		gname = tr("Private Messages");

	if(d->cvi)
		item = new ContactViewItem(gname, type, this, d->cvi);
	else
		item = new ContactViewItem(gname, type, this, d->cv);

	d->groups.append(item);

	return item;
}

ContactViewItem *ContactProfile::addGroup(const QString &name)
{
	ContactViewItem *item;
	if(d->cvi)
		item = new ContactViewItem(name, ContactViewItem::gUser, this, d->cvi);
	else
		item = new ContactViewItem(name, ContactViewItem::gUser, this, d->cv);

	d->groups.append(item);

	return item;
}

// check for special group
ContactViewItem *ContactProfile::checkGroup(int type)
{
	for(ContactViewItem *item = firstGroupItem(); item; item = item->nextSiblingItem()) {
		if(item->type() == ContactViewItem::Group && item->groupType() == type)
			return item;
	}

	return 0;
}

ContactViewItem *ContactProfile::firstGroupItem() const
{
	if(d->cvi)
		return d->cvi->firstChildItem();

	return d->cv->firstTopLevelItem();
}

// make a tooltip with account information
QString ContactProfile::makeTip(bool trim, bool doLinkify) const
{
	if (d->cvi)
		return "<qt> <center> <b>" + d->cvi->text(0) + " " + d->cvi->groupInfo() + "</b> </center> " + d->su.makeBareTip(trim,doLinkify) + "</qt>";
	else
		return d->su.makeTip(trim,doLinkify);
}

// check for user group
ContactViewItem *ContactProfile::checkGroup(const QString &name)
{
	for(ContactViewItem *item = firstGroupItem(); item; item = item->nextSiblingItem()) {
		if(item->type() == ContactViewItem::Group && item->groupType() == ContactViewItem::gUser && item->groupName() == name)
				return item;
	}

	return 0;
}

ContactViewItem *ContactProfile::ensureGroup(int type)
{
	ContactViewItem *group_item = checkGroup(type);
	if(!group_item)
		group_item = addGroup(type);

	return group_item;
}

ContactViewItem *ContactProfile::ensureGroup(const QString &name)
{
	ContactViewItem *group_item = checkGroup(name);
	if(!group_item)
		group_item = addGroup(name);

	return group_item;
}

void ContactProfile::checkDestroyGroup(const QString &group)
{
	ContactViewItem *group_item = checkGroup(group);
	if(group_item)
		checkDestroyGroup(group_item);
}

void ContactProfile::checkDestroyGroup(ContactViewItem *group)
{
	if(group->childCount() == 0) {
		d->groups.removeOne(group);
		delete group;
	}
}

void ContactProfile::updateEntry(const UserListItem &u)
{
	if (u.isSelf()) {
		// Update the self item
		d->su = u;

		// Show and/or update item if necessary
		if (d->cv->isShowSelf() || d->su.userResourceList().count() > 1) {
			if (d->self) {
				updateSelf();
			}
			else {
				addSelf();
			}
		}
		else {
			removeSelf();
		}
	}
	else {
		Entry *e = findEntry(u.jid());
		if(!e) {
			e = new Entry;
			d->roster.append(e);
			e->u = u;
		}
		else {
			e->u = u;
			removeUnneededContactItems(e);

			// update remaining items
			for (int _ci = 0; _ci < e->cvi.size(); ++_ci) {
				ContactViewItem *i = e->cvi[_ci];
				i->setContact(&e->u);
				if(!u.isAvailable())
					i->stopAnimateNick();
			}
		}

		deferredUpdateGroups();
		addNeededContactItems(e);
	}
}

void ContactProfile::updateSelf()
{
	if (d->self) {
		d->self->setContact(&d->su);
		if(!d->su.isAvailable())
			d->self->stopAnimateNick();
	}
}

void ContactProfile::addSelf()
{
	if(!d->self) {
		if(!d->cvi)
			return;
		d->self = new ContactViewItem(&d->su, this, d->cvi);
	}
}

void ContactProfile::removeSelf()
{
	if (d->self) {
		delete d->self;
		d->self = 0;
	}
}

ContactViewItem *ContactProfile::addContactItem(Entry *e, ContactViewItem *group_item)
{
	ContactViewItem *i = new ContactViewItem(&e->u, this, group_item);
	e->cvi.append(i);
	if(e->alerting)
		i->setAlert(&e->anim);
	deferredUpdateGroups();
	//printf("ContactProfile: adding [%s] to group [%s]\n", e->u.jid().full().toLatin1().constData(), group_item->groupName().toLatin1().constData());
	return i;
}

/*
 * \brief Ensures that specified Entry is present in contactlist
 *
 * \param e - Entry with the necessary data about item
 * \param group_item - ContactViewItem that will be the group for this item
 */
ContactViewItem *ContactProfile::ensureContactItem(Entry *e, ContactViewItem *group_item)
{
	d->cv->recalculateSize();

	for (int _ci = 0; _ci < e->cvi.size(); ++_ci) {
		ContactViewItem *i = e->cvi[_ci];
		ContactViewItem *g = i->parentItem();
		if(g == group_item)
			return i;
	}
	return addContactItem(e, group_item);
}

/*
 * \brief Removes specified item from ContactView
 *
 * \param e - Entry with item's data
 * \param i - ContactViewItem corresponding to the e
 */
void ContactProfile::removeContactItem(Entry *e, ContactViewItem *i)
{
	d->cv->recalculateSize();

	ContactViewItem *group_item = i->parentItem();
	//printf("ContactProfile: removing [%s] from group [%s]\n", e->u.jid().full().toLatin1().constData(), group_item->groupName().toLatin1().constData());
	e->cvi.removeAll(i);
	delete i;
	deferredUpdateGroups();
	checkDestroyGroup(group_item);
}

void ContactProfile::addNeededContactItems(Entry *e)
{
	if(!d->v_enabled)
		return;

	const UserListItem &u = e->u;

	if(u.inList()) {
		// don't add if we're not supposed to see it
		if(u.isTransport()) {
			if(!d->cv->isShowAgents() && !e->alerting) {
				return;
			}
		}
		else {
			if(!e->alerting) {
				if((!d->cv->isShowOffline() && !u.isAvailable()) || (!d->cv->isShowAway() && u.isAway()) || (!d->cv->isShowHidden() && u.isHidden()))
					return;
			}
		}
	}

	if(u.isPrivate())
		ensureContactItem(e, ensureGroup(ContactViewItem::gPrivate));
	else if(!u.inList())
		ensureContactItem(e, ensureGroup(ContactViewItem::gNotInList));
	else if(u.isTransport()) {
		ensureContactItem(e, ensureGroup(ContactViewItem::gAgents));
	}
	else if(u.groups().isEmpty())
		ensureContactItem(e, ensureGroup(ContactViewItem::gGeneral));
	else {
		const QStringList &groups = u.groups();
		for(QStringList::ConstIterator git = groups.begin(); git != groups.end(); ++git)
			ensureContactItem(e, ensureGroup(*git));
	}
}

void ContactProfile::removeUnneededContactItems(Entry *e)
{
	const UserListItem &u = e->u;
	
	if(u.inList()) {
		bool delAll = !d->v_enabled;
		if(u.isTransport()) {
			if(!d->cv->isShowAgents() && !e->alerting) {
				delAll = true;
			}
		}
		else {
			if(!e->alerting) {
				if((!d->cv->isShowOffline() && !u.isAvailable()) || (!d->cv->isShowAway() && u.isAway()) || (!d->cv->isShowHidden() && u.isHidden()))
					delAll = true;
			}
		}
		if(delAll) {
			clearContactItems(e);
			return;
		}
	}

	for (int _ci = e->cvi.size() - 1; _ci >= 0; --_ci) {
		ContactViewItem *i = e->cvi[_ci];
		bool del = false;
		ContactViewItem *g = i->parentItem();

		if(g->groupType() == ContactViewItem::gNotInList && u.inList())
			del = true;
		else if(g->groupType() != ContactViewItem::gNotInList && g->groupType() != ContactViewItem::gPrivate && !u.inList())
			del = true;
		else if(g->groupType() == ContactViewItem::gAgents && !u.isTransport())
			del = true;
		else if(g->groupType() != ContactViewItem::gAgents && u.isTransport())
			del = true;
		else if(g->groupType() == ContactViewItem::gGeneral && !u.groups().isEmpty())
			del = true;
		else if(g->groupType() != ContactViewItem::gPrivate && g->groupType() != ContactViewItem::gGeneral && u.groups().isEmpty() && !u.isTransport() && u.inList())
			del = true;
		else if(g->groupType() == ContactViewItem::gUser) {
			const QStringList &groups = u.groups();
			if(!groups.isEmpty()) {
				bool found = false;
				for(QStringList::ConstIterator git = groups.begin(); git != groups.end(); ++git) {
					if(g->groupName() == *git) {
						found = true;
						break;
					}
				}
				if(!found)
					del = true;
			}
		}
		else if(PsiOptions::instance()->getOption("options.ui.contactlist.auto-delete-unlisted").toBool() && !e->alerting && (g->groupType() == ContactViewItem::gPrivate || g->groupType() == ContactViewItem::gNotInList)) {
			del = true;
		}

		if(del) {
			removeContactItem(e, i);
		}
	}
}

void ContactProfile::clearContactItems(Entry *e)
{
	for (int _ci = e->cvi.size() - 1; _ci >= 0; --_ci)
		removeContactItem(e, e->cvi[_ci]);
}

void ContactProfile::addAllNeededContactItems()
{
	for (int _ri = 0; _ri < d->roster.size(); ++_ri)
		addNeededContactItems(d->roster[_ri]);
}

void ContactProfile::removeAllUnneededContactItems()
{
	for (int _ri = 0; _ri < d->roster.size(); ++_ri)
		removeUnneededContactItems(d->roster[_ri]);
}

void ContactProfile::resetAllContactItemNames()
{
	for (int _ri = 0; _ri < d->roster.size(); ++_ri) {
		Entry *e = d->roster[_ri];
		for (int _ci = 0; _ci < e->cvi.size(); ++_ci) {
			ContactViewItem *i = e->cvi[_ci];
			i->resetName();
			contactView()->filterContact(i);
		}
	}
}

void ContactProfile::removeEntry(const Jid &j)
{
	Entry *e = findEntry(j);
	if(e)
		removeEntry(e);
}

void ContactProfile::removeEntry(Entry *e)
{
	e->alerting = false;
	clearContactItems(e);
	d->roster.removeAll(e);
	delete e;
}

void ContactProfile::setAlert(const Jid &j, const PsiIcon *anim)
{
	if(d->su.jid().compare(j)) {
		if(d->self)
			d->self->setAlert(anim);
	}
	else {
		Entry *e = findEntry(j);
		if(!e)
			return;

		e->alerting = true;
		e->anim = *anim;
		addNeededContactItems(e);
		for (int _ci = 0; _ci < e->cvi.size(); ++_ci)
			e->cvi[_ci]->setAlert(anim);

		if(option.scrollTo)
			ensureVisible(e);
	}
}

void ContactProfile::clearAlert(const Jid &j)
{
	if(d->su.jid().compare(j)) {
		if(d->self)
			d->self->clearAlert();
	}
	else {
		Entry *e = findEntry(j);
		if(!e)
			return;

		e->alerting = false;
		for (int _ci = 0; _ci < e->cvi.size(); ++_ci)
			e->cvi[_ci]->clearAlert();
		removeUnneededContactItems(e);
	}
}

void ContactProfile::clear()
{
	for (int _ri = d->roster.size() - 1; _ri >= 0; --_ri)
		removeEntry(d->roster[_ri]);
}

ContactProfile::Entry *ContactProfile::findEntry(const Jid &jid) const
{
	for (int _ri = 0; _ri < d->roster.size(); ++_ri) {
		Entry *e = d->roster[_ri];
		if(e->u.jid().compare(jid))
			return e;
	}
	return 0;
}

ContactProfile::Entry *ContactProfile::findEntry(ContactViewItem *i) const
{
	for (int _ri = 0; _ri < d->roster.size(); ++_ri) {
		Entry *e = d->roster[_ri];
		for (int _ci = 0; _ci < e->cvi.size(); ++_ci) {
			ContactViewItem *cvi = e->cvi[_ci];
			if(cvi == i)
				return e;
		}
	}
	return 0;
}

// return a list of contacts from a CVI group
QList<XMPP::Jid> ContactProfile::contactListFromCVGroup(ContactViewItem *group) const
{
	QList<XMPP::Jid> list;

	for(ContactViewItem *item = group->firstChildItem(); item ; item = item->nextSiblingItem()) {
		if(item->type() != ContactViewItem::Contact)
			continue;

		list.append(item->u()->jid());
	}

	return list;
}

// return the number of contacts from a CVI group
int ContactProfile::contactSizeFromCVGroup(ContactViewItem *group) const
{
	int total = 0;

	for(ContactViewItem *item = group->firstChildItem(); item ; item = item->nextSiblingItem()) {
		if(item->type() != ContactViewItem::Contact)
			continue;

		++total;
	}

	return total;
}

// return the number of contacts from a CVI group
int ContactProfile::contactsOnlineFromCVGroup(ContactViewItem *group) const
{
	int total = 0;

	for(ContactViewItem *item = group->firstChildItem(); item ; item = item->nextSiblingItem()) {
		if(item->type() == ContactViewItem::Contact && item->u()->isAvailable())
			++total;
	}

	return total;
}

// return a list of contacts associated with "groupName"
QList<XMPP::Jid> ContactProfile::contactListFromGroup(const QString &groupName) const
{
	QList<XMPP::Jid> list;

	for (int _ri = 0; _ri < d->roster.size(); ++_ri) {
		const Entry *e = d->roster[_ri];
		const UserListItem &u = e->u;
		if(u.isTransport())
			continue;
		const QStringList &g = u.groups();
		if(g.isEmpty()) {
			if(groupName.isEmpty())
				list.append(u.jid());
		}
		else {
			for(QStringList::ConstIterator git = g.begin(); git != g.end(); ++git) {
				if(*git == groupName) {
					list.append(u.jid());
					break;
				}
			}
		}
	}

	return list;
}

// return the number of contacts associated with "groupName"
int ContactProfile::contactSizeFromGroup(const QString &groupName) const
{
	int total = 0;

	for (int _ri = 0; _ri < d->roster.size(); ++_ri) {
		const Entry *e = d->roster[_ri];
		const UserListItem &u = e->u;
		if(u.isTransport())
			continue;
		const QStringList &g = u.groups();
		if(g.isEmpty()) {
			if(groupName.isEmpty())
				++total;
		}
		else {
			for(QStringList::ConstIterator git = g.begin(); git != g.end(); ++git) {
				if(*git == groupName) {
					++total;
					break;
				}
			}
		}
	}

	return total;
}

void ContactProfile::updateGroupInfo(ContactViewItem *group)
{
	int type = group->groupType();
	if(type == ContactViewItem::gGeneral || type == ContactViewItem::gAgents || type == ContactViewItem::gPrivate || type == ContactViewItem::gUser) {
		int online = contactsOnlineFromCVGroup(group);
		int total;
		if(type == ContactViewItem::gGeneral || type == ContactViewItem::gUser) {
			QString gname;
			if(type == ContactViewItem::gUser)
				gname = group->groupName();
			else
				gname = "";
			total = contactSizeFromGroup(gname);
		}
		else {
			total = group->childCount();
		}
		if (option.showGroupCounts)
			group->setGroupInfo(QString("(%1/%2)").arg(online).arg(total));
	}
	else if (option.showGroupCounts) {
		int inGroup = contactSizeFromCVGroup(group);
		group->setGroupInfo(QString("(%1)").arg(inGroup));
	}
}

QStringList ContactProfile::groupList() const
{
	QStringList groupList;

	for (int _ri = 0; _ri < d->roster.size(); ++_ri) {
		Entry *e = d->roster[_ri];
		foreach(QString group, e->u.groups()) {
			if (!groupList.contains(group))
				groupList.append(group);
		}
	}

	groupList.sort();
	return groupList;
}

void ContactProfile::animateNick(const Jid &j)
{
	if(d->su.jid().compare(j)) {
		if(d->self)
			d->self->setAnimateNick();
	}

	Entry *e = findEntry(j);
	if(!e)
		return;
	for (int _ci = 0; _ci < e->cvi.size(); ++_ci)
		e->cvi[_ci]->setAnimateNick();
}

void ContactProfile::deferredUpdateGroups()
{
	d->t->start(250);
}

void ContactProfile::updateGroups()
{
	int totalOnline = 0;
	{
		for (int _ri = 0; _ri < d->roster.size(); ++_ri) {
			if(d->roster[_ri]->u.isAvailable())
				++totalOnline;
		}
		if(d->cvi && option.showGroupCounts)
			d->cvi->setGroupInfo(QString("(%1/%2)").arg(totalOnline).arg(d->roster.count()));
	}

	{
		for (int _gi = 0; _gi < d->groups.size(); ++_gi) {
			ContactViewItem *g = d->groups[_gi];
			updateGroupInfo(g);
			contactView()->filterGroup(g);
		}
	}
}

void ContactProfile::ensureVisible(const Jid &j)
{
	Entry *e = findEntry(j);
	if(!e)
		return;
	ensureVisible(e);
}

void ContactProfile::ensureVisible(Entry *e)
{
	if(!e->alerting) {
		if(!d->cv->isShowAgents() && e->u.isTransport())
			d->cv->setShowAgents(true);
		if(!d->cv->isShowOffline() && !e->u.isAvailable())
			d->cv->setShowOffline(true);
		if(!d->cv->isShowAway() && e->u.isAway())
			d->cv->setShowAway(true);
		if(!d->cv->isShowHidden() && e->u.isHidden())
			d->cv->setShowHidden(true);
	}

	ContactViewItem *i = e->cvi.isEmpty() ? 0 : e->cvi.first();
	if(!i)
		return;
	d->cv->ensureContactItemVisible(i);
}

void ContactProfile::doContextMenu(ContactViewItem *i, const QPoint &pos)
{
	bool online = d->pa->loggedIn();

	if(i->type() == ContactViewItem::Profile) {
		QMenu pm;

		QMenu *am = new QMenu(&pm);
		QAction *act_admin_disco   = am->addAction(IconsetFactory::icon("psi/disco").icon(), tr("Online Users"));
		QAction *act_server_msg    = am->addAction(IconsetFactory::icon("psi/sendMessage").icon(), tr("Send server message"));
		am->addSeparator();
		QAction *act_set_motd      = am->addAction(tr("Set MOTD"));
		QAction *act_update_motd   = am->addAction(tr("Update MOTD"));
		QAction *act_delete_motd   = am->addAction(IconsetFactory::icon("psi/remove").icon(), tr("Delete MOTD"));

		QMenu *sm = new QMenu(&pm);
		// status actions indexed by STATUS_* constant
		QMap<int,QAction*> statusActs;
		statusActs[STATUS_ONLINE]  = sm->addAction(PsiIconset::instance()->status(STATUS_ONLINE).icon(), status2txt(STATUS_ONLINE));
		if (PsiOptions::instance()->getOption("options.ui.menu.status.chat").toBool())
			statusActs[STATUS_CHAT] = sm->addAction(PsiIconset::instance()->status(STATUS_CHAT).icon(), status2txt(STATUS_CHAT));
		sm->addSeparator();
		statusActs[STATUS_AWAY] = sm->addAction(PsiIconset::instance()->status(STATUS_AWAY).icon(), status2txt(STATUS_AWAY));
		if (PsiOptions::instance()->getOption("options.ui.menu.status.xa").toBool())
			statusActs[STATUS_XA] = sm->addAction(PsiIconset::instance()->status(STATUS_XA).icon(), status2txt(STATUS_XA));
		statusActs[STATUS_DND] = sm->addAction(PsiIconset::instance()->status(STATUS_DND).icon(), status2txt(STATUS_DND));
		if (PsiOptions::instance()->getOption("options.ui.menu.status.invisible").toBool()) {
			sm->addSeparator();
			statusActs[STATUS_INVISIBLE] = sm->addAction(PsiIconset::instance()->status(STATUS_INVISIBLE).icon(), status2txt(STATUS_INVISIBLE));
		}
		sm->addSeparator();
		statusActs[STATUS_OFFLINE] = sm->addAction(PsiIconset::instance()->status(STATUS_OFFLINE).icon(), status2txt(STATUS_OFFLINE));
		pm.addMenu(sm)->setText(tr("&Status"));

#ifdef USE_PEP
		bool hasPEP = d->pa->serverInfoManager()->hasPEP();
		QAction *act_mood = pm.addAction(tr("Mood"));
		act_mood->setEnabled(hasPEP);

		QMenu *avatarm = new QMenu(&pm);
		QAction *act_set_avatar   = avatarm->addAction(tr("Set Avatar"));
		QAction *act_unset_avatar = avatarm->addAction(tr("Unset Avatar"));
		QAction *act_avatar_menu = pm.addMenu(avatarm);
		act_avatar_menu->setText(tr("Avatar"));
		act_avatar_menu->setEnabled(hasPEP);
#endif

		pm.addSeparator();
		QAction *act_add_contact  = pm.addAction(IconsetFactory::icon("psi/addContact").icon(), tr("&Add a contact"));
		QAction *act_disco        = pm.addAction(IconsetFactory::icon("psi/disco").icon(), tr("Service &Discovery"));
		QAction *act_blank_msg    = nullptr;
		if (PsiOptions::instance()->getOption("options.ui.message.enabled").toBool())
			act_blank_msg = pm.addAction(IconsetFactory::icon("psi/sendMessage").icon(), tr("New &blank message"));
		pm.addSeparator();
		QAction *act_xml          = pm.addAction(IconsetFactory::icon("psi/xml").icon(), tr("&XML Console"));
		pm.addSeparator();
		QAction *act_modify       = pm.addAction(IconsetFactory::icon("psi/account").icon(), tr("&Modify Account..."));

		QAction *act_admin_menu = nullptr;
		if (PsiOptions::instance()->getOption("options.ui.menu.account.admin").toBool()) {
			pm.addSeparator();
			act_admin_menu = pm.addMenu(am);
			act_admin_menu->setText(tr("&Admin"));
			act_admin_menu->setEnabled(online);
		}

		QAction *result = pm.exec(pos);
		if(!result)
			return;

		if(result == act_modify)
			d->pa->modify();
		else if(result == act_server_msg) {
			Jid j = d->pa->jid().host() + '/' + "announce/online";
			actionSendMessage(j);
		}
		else if(result == act_set_motd) {
			Jid j = d->pa->jid().host() + '/' + "announce/motd";
			actionSendMessage(j);
		}
		else if(result == act_update_motd) {
			Jid j = d->pa->jid().host() + '/' + "announce/motd/update";
			actionSendMessage(j);
		}
		else if(result == act_delete_motd) {
			Jid j = d->pa->jid().host() + '/' + "announce/motd/delete";
			Message m;
			m.setTo(j);
			d->pa->dj_sendMessage(m, false);
		}
		else if(result == act_admin_disco) {
			Jid j = d->pa->jid().host() + '/' + "admin";
			actionDisco(j, "");
		}
		else if(act_blank_msg && result == act_blank_msg) {
			actionSendMessage("");
		}
		else if(result == act_add_contact) {
			d->pa->openAddUserDlg();
		}
		else if(result == act_disco) {
			Jid j = d->pa->jid().host();
			actionDisco(j, "");
		}
		else if(result == act_xml) {
			d->pa->showXmlConsole();
		}
#ifdef USE_PEP
		else if(result == act_mood && hasPEP) {
			emit actionSetMood();
		}
		else if(result == act_set_avatar && hasPEP) {
			emit actionSetAvatar();
		}
		else if(result == act_unset_avatar && hasPEP) {
			emit actionUnsetAvatar();
		}
#endif
		else {
			// Check status submenu
			for (auto it = statusActs.begin(); it != statusActs.end(); ++it) {
				if (result == it.value()) {
					d->pa->changeStatus(it.key());
					break;
				}
			}
		}
	}
	else if(i->type() == ContactViewItem::Group) {
		QString gname = i->groupName();
		QMenu pm;

		QAction *act_send_group = nullptr;
		if (PsiOptions::instance()->getOption("options.ui.message.enabled").toBool())
			act_send_group = pm.addAction(IconsetFactory::icon("psi/sendMessage").icon(), tr("Send message to group"));

		QAction *act_remove_group = nullptr;
		QAction *act_remove_group_contacts = nullptr;
		if(!option.lockdown.roster) {
			bool canRename = online && i->groupType() == ContactViewItem::gUser && gname != ContactView::tr("Hidden");
			d->cv->qa_ren->setEnabled(canRename);
			pm.addAction(d->cv->qa_ren);
			pm.addSeparator();
			act_remove_group          = pm.addAction(IconsetFactory::icon("psi/remove").icon(), tr("Remove group"));
			act_remove_group_contacts = pm.addAction(IconsetFactory::icon("psi/remove").icon(), tr("Remove group and contacts"));
			if (!canRename) {
				act_remove_group->setEnabled(false);
				act_remove_group_contacts->setEnabled(false);
			}
		}

		QAction *act_hide_agents = nullptr;
		if(i->groupType() == ContactViewItem::gAgents) {
			pm.addSeparator();
			act_hide_agents = pm.addAction(tr("Hide"));
		}

		QAction *result = pm.exec(pos);

		// restore actions
		if(option.lockdown.roster)
			d->cv->qa_ren->setEnabled(false);
		else
			d->cv->qa_ren->setEnabled(true);

		if(!result)
			return;

		if(act_send_group && result == act_send_group) {
			QList<XMPP::Jid> list = contactListFromCVGroup(i);
			actionSendMessage(list);
		}
		else if(act_remove_group && result == act_remove_group && online) {
			int n = QMessageBox::information(d->cv, tr("Remove Group"),tr(
			"This will cause all contacts in this group to be disassociated with it.\n"
			"\n"
			"Proceed?"), tr("&Yes"), tr("&No"));
			if(n == 0) {
				QList<XMPP::Jid> list = contactListFromGroup(i->groupName());
				for(QList<Jid>::Iterator it = list.begin(); it != list.end(); ++it)
					actionGroupRemove(*it, gname);
			}
		}
		else if(act_remove_group_contacts && result == act_remove_group_contacts && online) {
			int n = QMessageBox::information(d->cv, tr("Remove Group and Contacts"),tr(
			"WARNING!  This will remove all contacts associated with this group!\n"
			"\n"
			"Proceed?"), tr("&Yes"), tr("&No"));
			if(n == 0) {
				QList<XMPP::Jid> list = contactListFromGroup(i->groupName());
				for(QList<Jid>::Iterator it = list.begin(); it != list.end(); ++it) {
					removeEntry(*it);
					actionRemove(*it);
				}
			}
		}
		else if(act_hide_agents && result == act_hide_agents) {
			if(i->groupType() == ContactViewItem::gAgents)
				d->cv->setShowAgents(false);
		}
	}
	else if(i->type() == ContactViewItem::Contact) {
		bool self = false;
		UserListItem *u;
		Entry *e = 0;
		if(i == d->self) {
			self = true;
			u = &d->su;
		}
		else {
			e = findEntry(i);
			if(!e)
				return;
			u = &e->u;
		}

		QStringList gl = groupList();
		std::sort(gl.begin(), gl.end(), caseInsensitiveLessThan);

		bool inList = e ? e->u.inList() : false;
		bool isPrivate = e ? e->u.isPrivate(): false;
		bool isAgent = e ? e->u.isTransport() : false;
		bool avail = e ? e->u.isAvailable() : false;
		QString groupNameCache = i->parentItem()->groupName();

		QMenu pm;

		// Pending resource action (set in ResourceMenu signals during exec)
		int   pendingResourceType = -1; // 0=send, 1=chat, 2=wb, 3=cmd, 4=activechat
		QString pendingResource;

		QAction *act_add_authorize = nullptr;
		if(!self && !inList && !isPrivate && !option.lockdown.roster) {
			act_add_authorize = pm.addAction(IconsetFactory::icon("psi/addContact").icon(), tr("Add/Authorize to contact list"));
			act_add_authorize->setEnabled(online);
			pm.addSeparator();
		}

		if ( (self && i->isAlerting()) || (!self && e->alerting) ) {
			pm.addAction(d->cv->qa_recv);
			pm.addSeparator();
		}

		if (PsiOptions::instance()->getOption("options.ui.message.enabled").toBool())
			pm.addAction(d->cv->qa_send);

		const UserResourceList &rl = u->userResourceList();

		ResourceMenu *s2m  = new ResourceMenu(&pm);
		ResourceMenu *c2m  = new ResourceMenu(&pm);
#ifdef WHITEBOARDING
		ResourceMenu *wb2m = new ResourceMenu(&pm);
#endif
		ResourceMenu *rc2m = new ResourceMenu(&pm);

		if(!rl.isEmpty()) {
			for(UserResourceList::ConstIterator it = rl.begin(); it != rl.end(); ++it) {
				s2m->addResource(*it);
				c2m->addResource(*it);
#ifdef WHITEBOARDING
				wb2m->addResource(*it);
#endif
				rc2m->addResource(*it);
			}
		}

		connect(s2m, &ResourceMenu::resourceActivated, [&](const QString &r) {
			pendingResourceType = 0; pendingResource = r; });
		connect(c2m, &ResourceMenu::resourceActivated, [&](const QString &r) {
			pendingResourceType = 1; pendingResource = r; });
#ifdef WHITEBOARDING
		connect(wb2m, &ResourceMenu::resourceActivated, [&](const QString &r) {
			pendingResourceType = 2; pendingResource = r; });
#endif
		connect(rc2m, &ResourceMenu::resourceActivated, [&](const QString &r) {
			pendingResourceType = 3; pendingResource = r; });

		QAction *act_s2m_menu = nullptr;
		if(!isPrivate && PsiOptions::instance()->getOption("options.ui.message.enabled").toBool()) {
			act_s2m_menu = pm.addMenu(s2m);
			act_s2m_menu->setText(tr("Send message to"));
			act_s2m_menu->setEnabled(!rl.isEmpty());
		}

		d->cv->qa_chat->setIcon(QIcon(IconsetFactory::iconPixmap("psi/start-chat")));
		pm.addAction(d->cv->qa_chat);

		QAction *act_c2m_menu = nullptr;
		if(!isPrivate) {
			act_c2m_menu = pm.addMenu(c2m);
			act_c2m_menu->setText(tr("Open chat to"));
			act_c2m_menu->setEnabled(!rl.isEmpty());
		}

#ifdef WHITEBOARDING
		d->cv->qa_wb->setIcon(QIcon(IconsetFactory::iconPixmap("psi/whiteboard")));
		pm.addAction(d->cv->qa_wb);
		QAction *act_wb2m_menu = nullptr;
		if(!isPrivate) {
			act_wb2m_menu = pm.addMenu(wb2m);
			act_wb2m_menu->setText(tr("Open a whiteboard to"));
			act_wb2m_menu->setEnabled(!rl.isEmpty());
		}
#endif

		QAction *act_rc2m_menu = nullptr;
		if(!isPrivate) {
			act_rc2m_menu = pm.addMenu(rc2m);
			act_rc2m_menu->setText(tr("E&xecute command"));
			act_rc2m_menu->setEnabled(!rl.isEmpty());
		}

		QAction *act_active_chats = nullptr;
		if(!isPrivate && PsiOptions::instance()->getOption("options.ui.menu.contact.active-chats").toBool()) {
			QStringList hc = d->pa->hiddenChats(u->jid());
			ResourceMenu *cm = new ResourceMenu(&pm);
			for(QStringList::ConstIterator it = hc.begin(); it != hc.end(); ++it) {
				int status;
				const UserResourceList &rl2 = u->userResourceList();
				UserResourceList::ConstIterator uit = rl2.find(*it);
				if(uit != rl2.end() || (uit = rl2.priority()) != rl2.end())
					status = makeSTATUS((*uit).status());
				else
					status = STATUS_OFFLINE;
				cm->addResource(status, *it);
			}
			connect(cm, &ResourceMenu::resourceActivated, [&](const QString &r) {
				pendingResourceType = 4; pendingResource = r; });
			act_active_chats = pm.addMenu(cm);
			act_active_chats->setText(tr("Active chats"));
			act_active_chats->setEnabled(!hc.isEmpty());
		}

		QAction *act_voice = nullptr;
		if(d->pa->voiceCaller() && !isAgent) {
			act_voice = pm.addAction(IconsetFactory::icon("psi/voice").icon(), tr("Voice Call"));
			if(!online)
				act_voice->setEnabled(false);
			else {
				bool hasVoice = false;
				const UserResourceList &rli = u->userResourceList();
				for (UserResourceList::ConstIterator it = rli.begin(); it != rli.end() && !hasVoice; ++it)
					hasVoice = psiAccount()->capsManager()->features(u->jid().withResource((*it).name())).canVoice();
				act_voice->setEnabled(!psiAccount()->capsManager()->isEnabled() || hasVoice);
			}
		}

		QAction *act_send_file = nullptr;
		if(!isAgent) {
			pm.addSeparator();
			act_send_file = pm.addAction(IconsetFactory::icon("psi/upload").icon(), tr("Send &file"));
			act_send_file->setEnabled(online);
		}

		// Invite to groupchat
		QMap<QAction*,QString> inviteActionMap;
		if(!isPrivate && !isAgent) {
			QMenu *gm = new QMenu(&pm);
			QStringList groupchats = d->pa->groupchats();
			for(const QString &gc : groupchats) {
				QAction *a = gm->addAction(gc);
				a->setEnabled(online);
				inviteActionMap[a] = gc;
			}
			QAction *act_invite = pm.addMenu(gm);
			act_invite->setText(tr("Invite to"));
			act_invite->setEnabled(!groupchats.isEmpty());
		}

		if(inList || !isAgent)
			pm.addSeparator();

		if(!self) {
			if(inList && !option.lockdown.roster) {
				d->cv->qa_ren->setEnabled(online);
				pm.addAction(d->cv->qa_ren);
			}

			QAction *act_group_menu = nullptr;
			QAction *act_group_none = nullptr;
			QAction *act_group_new = nullptr;
			QMap<QAction*,QString> groupActionMap;

			if(!isAgent) {
				if(inList && !option.lockdown.roster) {
					QMenu *gm = new QMenu(&pm);
					act_group_none = gm->addAction(tr("&None"));
					act_group_none->setCheckable(true);
					gm->addSeparator();

					QString g;
					if(e->u.groups().isEmpty())
						act_group_none->setChecked(true);
					else
						g = groupNameCache;

					gl.removeAll(ContactView::tr("Hidden"));
					int n = 0;
					for(const QString &grp : gl) {
						QString str;
						if(n < 9) str = "&";
						str += QString("%1. %2").arg(n+1).arg(grp);
						QAction *ga = gm->addAction(str);
						ga->setCheckable(true);
						ga->setChecked(grp == g);
						groupActionMap[ga] = grp;
						++n;
					}
					if(n > 0)
						gm->addSeparator();

					QAction *ga_hidden = gm->addAction(ContactView::tr("Hidden"));
					ga_hidden->setCheckable(true);
					ga_hidden->setChecked(g == ContactView::tr("Hidden"));
					groupActionMap[ga_hidden] = ContactView::tr("Hidden");
					gm->addSeparator();
					act_group_new = gm->addAction(tr("&Create new..."));

					act_group_menu = pm.addMenu(gm);
					act_group_menu->setText(tr("&Group"));
					act_group_menu->setEnabled(online);
				}
			}
			else {
				pm.addSeparator();
				d->cv->qa_logon->setEnabled(!avail && online);
				d->cv->qa_logon->setIcon(PsiIconset::instance()->status(e->u.jid(), STATUS_ONLINE).icon());
				pm.addAction(d->cv->qa_logon);

				QAction *act_logoff = pm.addAction(PsiIconset::instance()->status(e->u.jid(), STATUS_OFFLINE).icon(), tr("Log off"));
				act_logoff->setEnabled(avail && online);
				pm.addSeparator();
			}

			QAction *act_auth_resend = nullptr, *act_auth_rerequest = nullptr, *act_auth_remove = nullptr;
			if(inList && !option.lockdown.roster) {
				QMenu *authm = new QMenu(&pm);
				act_auth_resend    = authm->addAction(tr("Resend authorization to"));
				act_auth_rerequest = authm->addAction(tr("Rerequest authorization from"));
				act_auth_remove    = authm->addAction(tr("Remove authorization from"));
				QAction *act_authm = pm.addMenu(authm);
				act_authm->setText(tr("Authorization"));
				act_authm->setEnabled(online);
			}

			if(!option.lockdown.roster) {
				d->cv->qa_rem->setEnabled(online || !inList);
				pm.addAction(d->cv->qa_rem);
			}
			pm.addSeparator();

			// Avatars
			if (PsiOptions::instance()->getOption("options.ui.menu.contact.custom-picture").toBool()) {
				QMenu *avpm = new QMenu(&pm);
				avpm->addAction(d->cv->qa_assignAvatar);
				d->cv->qa_clearAvatar->setEnabled(d->pa->avatarFactory()->hasManualAvatar(u->jid()));
				avpm->addAction(d->cv->qa_clearAvatar);
				pm.addMenu(avpm)->setText(tr("&Picture"));
			}

#ifdef HAVE_PGPUTIL
			QAction *act_assign_key = nullptr, *act_unassign_key = nullptr;
			if(PGPUtil::instance().pgpAvailable() && PsiOptions::instance()->getOption("options.ui.menu.contact.custom-pgp-key").toBool()) {
				if(u->publicKeyID().isEmpty())
					act_assign_key   = pm.addAction(IconsetFactory::icon("psi/gpg-yes").icon(), tr("Assign Open&PGP key"));
				else
					act_unassign_key = pm.addAction(IconsetFactory::icon("psi/gpg-no").icon(), tr("Unassign Open&PGP key"));
			}
#endif

			pm.addAction(d->cv->qa_vcard);
			if(!isPrivate)
				pm.addAction(d->cv->qa_hist);

			QString name = u->jid().full();
			QString show = JIDUtil::nickOrJid(u->name(), u->jid().full());
			if(name != show)
				name += QString(" (%1)").arg(u->name());

			QAction *result = pm.exec(pos);

			// restore actions
			if(option.lockdown.roster) {
				d->cv->qa_ren->setEnabled(false);
				d->cv->qa_rem->setEnabled(false);
			} else {
				d->cv->qa_ren->setEnabled(true);
				d->cv->qa_rem->setEnabled(true);
			}

			if(!result && pendingResourceType < 0)
				return;

			// Handle resource submenu results (set via signals during exec)
			if(pendingResourceType >= 0) {
				Jid j = pendingResource.isEmpty() ? u->jid() : Jid(u->jid().userHost() + '/' + pendingResource);
				if(pendingResourceType == 0)      actionSendMessage(j);
				else if(pendingResourceType == 1) actionOpenChatSpecific(j);
#ifdef WHITEBOARDING
				else if(pendingResourceType == 2) actionOpenWhiteboardSpecific(j);
#endif
				else if(pendingResourceType == 3) actionExecuteCommandSpecific(j, "");
				else if(pendingResourceType == 4) actionOpenChatSpecific(j);
				return;
			}

			if(act_add_authorize && result == act_add_authorize) {
				if(online) { actionAdd(u->jid()); actionAuth(u->jid());
					QMessageBox::information(d->cv, tr("Add"), tr("Added/Authorized <b>%1</b> to the contact list.").arg(name)); }
			}
			else if(act_auth_resend && result == act_auth_resend) {
				if(online) { actionAuth(u->jid());
					QMessageBox::information(d->cv, tr("Authorize"), tr("Sent authorization to <b>%1</b>.").arg(name)); }
			}
			else if(act_auth_rerequest && result == act_auth_rerequest) {
				if(online) { actionAuthRequest(u->jid());
					QMessageBox::information(d->cv, tr("Authorize"), tr("Rerequested authorization from <b>%1</b>.").arg(name)); }
			}
			else if(act_auth_remove && result == act_auth_remove) {
				if(online) {
					int n = QMessageBox::information(d->cv, tr("Remove"), tr("Are you sure you want to remove authorization from <b>%1</b>?").arg(name), tr("&Yes"), tr("&No"));
					if(n == 0) actionAuthRemove(u->jid());
				}
			}
			else if(act_group_none && result == act_group_none) {
				if(online && !u->groups().isEmpty()) actionGroupRemove(u->jid(), groupNameCache);
			}
			else if(act_group_new && result == act_group_new) {
				if(online) {
					while(1) {
						bool ok = false;
						QString newgroup = QInputDialog::getText(d->cv, tr("Create New Group"), tr("Enter the new Group name:"), QLineEdit::Normal, QString(), &ok);
						if(!ok) break;
						if(newgroup.isEmpty()) continue;
						bool found = false;
						const QStringList &groups = u->groups();
						for(const QString &grp : groups) { if(grp == newgroup) { found = true; break; } }
						if(!found) { actionGroupRemove(u->jid(), groupNameCache); actionGroupAdd(u->jid(), newgroup); break; }
					}
				}
			}
			else if(groupActionMap.contains(result)) {
				if(online) {
					QString newgroup = groupActionMap[result];
					if(!u->groups().isEmpty()) actionGroupRemove(u->jid(), groupNameCache);
					actionGroupAdd(u->jid(), newgroup);
				}
			}
			else if(inviteActionMap.contains(result)) {
				if(online) { actionInvite(u->jid(), inviteActionMap[result]);
					QMessageBox::information(d->cv, tr("Invitation"), tr("Sent groupchat invitation to <b>%1</b>.").arg(name)); }
			}
			else if(act_voice && result == act_voice) { if(online) actionVoice(u->jid()); }
			else if(act_send_file && result == act_send_file) { if(online) actionSendFile(u->jid()); }
#ifdef HAVE_PGPUTIL
			else if(act_assign_key && result == act_assign_key) { actionAssignKey(u->jid()); }
			else if(act_unassign_key && result == act_unassign_key) { actionUnassignKey(u->jid()); }
#endif
			// Agent logoff
			else if(result && result->text() == tr("Log off") && isAgent && online) {
				Status s = makeStatus(STATUS_OFFLINE, "");
				actionAgentSetStatus(u->jid(), s);
			}
		}
		else {
			// self item - minimal menu
			if (PsiOptions::instance()->getOption("options.ui.message.enabled").toBool())
				pm.addAction(d->cv->qa_send);
			pm.addAction(d->cv->qa_vcard);
			pm.exec(pos);
		}
	}
}

void ContactProfile::scActionDefault(ContactViewItem *i)
{
	if(i->type() == ContactViewItem::Contact)
		actionDefault(i->u()->jid());
}

void ContactProfile::scRecvEvent(ContactViewItem *i)
{
	if(i->type() == ContactViewItem::Contact)
		actionRecvEvent(i->u()->jid());
}

void ContactProfile::scSendMessage(ContactViewItem *i)
{
	if(i->type() == ContactViewItem::Contact)
		actionSendMessage(i->u()->jid());
}

void ContactProfile::scRename(ContactViewItem *i)
{
	if(!d->pa->loggedIn())
		return;

	if((i->type() == ContactViewItem::Contact && i->u()->inList()) ||
		(i->type() == ContactViewItem::Group && i->groupType() == ContactViewItem::gUser && i->groupName() != ContactView::tr("Hidden"))) {
		i->resetName(true);
		QString currentName;
		if (i->type() == ContactViewItem::Contact)
			currentName = JIDUtil::nickOrJid(i->u()->name(), i->u()->jid().full());
		else
			currentName = i->groupName();
		bool ok;
		QString newName = QInputDialog::getText(d->cv,
			ContactView::tr("Rename"),
			ContactView::tr("Enter new name:"),
			QLineEdit::Normal, currentName, &ok);
		if (ok)
			doItemRenamed(i, newName);
		else
			i->resetName();
	}
}

void ContactProfile::scVCard(ContactViewItem *i)
{
	if(i->type() == ContactViewItem::Contact)
		actionInfo(i->u()->jid());
}

void ContactProfile::scHistory(ContactViewItem *i)
{
	if(i->type() == ContactViewItem::Contact)
		actionHistory(i->u()->jid());
}

void ContactProfile::scOpenChat(ContactViewItem *i)
{
	if(i->type() == ContactViewItem::Contact)
		actionOpenChat(i->u()->jid());
}

#ifdef WHITEBOARDING
void ContactProfile::scOpenWhiteboard(ContactViewItem *i)
{
	if(i->type() == ContactViewItem::Contact)
		actionOpenWhiteboard(i->u()->jid());
}
#endif

void ContactProfile::scAgentSetStatus(ContactViewItem *i, Status &s)
{
	if(i->type() != ContactViewItem::Contact)
		return;
	if(!i->isAgent())
		return;

	if(i->u()->isAvailable() || !d->pa->loggedIn())
		return;

	actionAgentSetStatus(i->u()->jid(), s);
}

void ContactProfile::scRemove(ContactViewItem *i)
{
	if(i->type() != ContactViewItem::Contact)
		return;

	Entry *e = findEntry(i);
	if(!e)
		return;

	bool ok = true;
	if(!d->pa->loggedIn())
		ok = false;
	if(!i->u()->inList())
		ok = true;

	if(ok) {
		QString name = e->u.jid().full();
		QString show = JIDUtil::nickOrJid(e->u.name(), e->u.jid().full());
		if(name != show)
			name += QString(" (%1)").arg(e->u.name());

		int n = 0;
		int gt = i->parentGroupType();
		if(gt != ContactViewItem::gNotInList && gt != ContactViewItem::gPrivate) {
			n = QMessageBox::information(d->cv, tr("Remove"),
			tr("Are you sure you want to remove <b>%1</b> from your contact list?").arg(name),
			tr("&Yes"), tr("&No"));
		}
		else
			n = 0;

		if(n == 0) {
			Jid j = e->u.jid();
			removeEntry(e);
			actionRemove(j);
		}
	}
}

void ContactProfile::doItemRenamed(ContactViewItem *i, const QString &text)
{
	if(i->type() == ContactViewItem::Contact) {
		Entry *e = findEntry(i);
		if(!e)
			return;

		// no change?
		//if(text == i->text(0))
		//	return;
		if(text.isEmpty()) {
			i->resetName();
			QMessageBox::information(d->cv, tr("Error"), tr("You can't set a blank name."));
			return;
		}

		//e->u.setName(text);
		//i->setContact(&e->u);
		actionRename(e->u.jid(), text);
		i->resetName(); // to put the status message in if needed
	}
	else {
		// no change?
		if(text == i->groupName()) {
			i->resetGroupName();
			return;
		}
		if(text.isEmpty()) {
			i->resetGroupName();
			QMessageBox::information(d->cv, tr("Error"), tr("You can't set a blank group name."));
			return;
		}

		// make sure we don't have it already
		QStringList g = groupList();
		bool found = false;
		for(QStringList::ConstIterator it = g.begin(); it != g.end(); ++it) {
			if(*it == text) {
				found = true;
				break;
			}
		}
		if(found) {
			i->resetGroupName();
			QMessageBox::information(d->cv, tr("Error"), tr("You already have a group with that name."));
			return;
		}

		QString oldName = i->groupName();

		// set group name
		i->setGroupName(text);

		// send signal
		actionGroupRename(oldName, text);
	}
}

void ContactProfile::dragDrop(const QString &text, ContactViewItem *i)
{
	if(!d->pa->loggedIn() || !i)
		return;

	ContactViewItem *gr = i->dropTargetGroup();
	if(!gr)
		return;

	Jid j(text);
	if(!j.isValid())
		return;
	Entry *e = findEntry(j);
	if(!e)
		return;
	const UserListItem &u = e->u;
	QStringList gl = u.groups();

	// already in the general group
	if(gr->groupType() == ContactViewItem::gGeneral && gl.isEmpty())
		return;
	// already in this user group
	if(gr->groupType() == ContactViewItem::gUser && u.inGroup(gr->groupName()))
		return;

	//printf("putting [%s] into group [%s]\n", u.jid().full().toLatin1().constData(), gr->groupName().toLatin1().constData());

	// remove all other groups from this contact
	for(QStringList::ConstIterator it = gl.begin(); it != gl.end(); ++it) {
		actionGroupRemove(u.jid(), *it);
	}
	if(gr->groupType() == ContactViewItem::gUser) {
		// add the new group
		actionGroupAdd(u.jid(), gr->groupName());
	}
}

void ContactProfile::dragDropFiles(const QStringList &files, ContactViewItem *i)
{
	if(files.isEmpty() || !d->pa->loggedIn() || i->type() != ContactViewItem::Contact)
		return;

	Entry *e = findEntry(i);
	if(!e)
		return;

	actionSendFiles(e->u.jid(),files);
}

//----------------------------------------------------------------------------
// ContactViewDelegate: custom painter for ContactViewItem
//----------------------------------------------------------------------------
class ContactViewDelegate : public QStyledItemDelegate
{
public:
	explicit ContactViewDelegate(ContactView *view) : QStyledItemDelegate(view), view_(view) {}

	void paint(QPainter *painter, const QStyleOptionViewItem &opt, const QModelIndex &index) const override
	{
		// itemFromIndex is protected in QTreeWidget; use model index row lookup instead
		QTreeWidgetItem *twItem = nullptr;
		{
			// Walk the tree by path from the model index to find the item
			QVector<int> path;
			QModelIndex idx = index;
			while (idx.isValid()) {
				path.prepend(idx.row());
				idx = idx.parent();
			}
			if (!path.isEmpty()) {
				twItem = view_->topLevelItem(path[0]);
				for (int i = 1; i < path.size() && twItem; ++i)
					twItem = twItem->child(path[i]);
			}
		}
		ContactViewItem *cvi = dynamic_cast<ContactViewItem *>(twItem);
		if (!cvi) {
			QStyledItemDelegate::paint(painter, opt, index);
			return;
		}
		painter->save();
		painter->translate(opt.rect.topLeft());
		bool selected = opt.state & QStyle::State_Selected;
		bool active   = opt.state & QStyle::State_Active;
		cvi->doPaint(painter, opt.palette, selected, active, opt.rect.width(), opt.rect.height());
		painter->restore();
	}

	QSize sizeHint(const QStyleOptionViewItem &opt, const QModelIndex &index) const override
	{
		// Same protected workaround as paint()
		QTreeWidgetItem *twItem = nullptr;
		{
			QVector<int> path;
			QModelIndex idx = index;
			while (idx.isValid()) { path.prepend(idx.row()); idx = idx.parent(); }
			if (!path.isEmpty()) {
				twItem = view_->topLevelItem(path[0]);
				for (int i = 1; i < path.size() && twItem; ++i) twItem = twItem->child(path[i]);
			}
		}
		if (RichListViewItem *rli = dynamic_cast<RichListViewItem *>(twItem)) {
			int h = rli->itemHeight();
			if (h > 0)
				return QSize(opt.rect.width(), h);
		}
		return QStyledItemDelegate::sizeHint(opt, index);
	}

private:
	ContactView *view_;
};

//----------------------------------------------------------------------------
// ContactView
//----------------------------------------------------------------------------
class ContactView::Private : public QObject
{
	Q_OBJECT
public:
	Private(ContactView *_cv)
		: QObject(_cv)
	{
		cv = _cv;
		autoRosterResizeInProgress = false;

	}

	ContactView *cv;
	QTimer *animTimer, *recalculateSizeTimer;
	QList<ContactProfile*> profiles;
	QSize lastSize;
	bool autoRosterResizeInProgress;



public slots:
	/*
	 * \brief Recalculates the size of ContactView and resizes it accordingly
	 */
	void recalculateSize()
	{
		// save some CPU
		if ( !cv->allowResize() )
			return;

		if ( !cv->updatesEnabled() )
			return;

		QSize oldSize = cv->size();
		QSize newSize = cv->sizeHint();

		if ( newSize.height() != oldSize.height() ) {
			lastSize = newSize;
			QWidget *topParent = cv->window();

			if ( cv->allowResize() ) {
				topParent->layout()->setEnabled( false ); // try to reduce some flicker

				int dh = newSize.height() - oldSize.height();
				topParent->resize( topParent->width(),
						   topParent->height() + dh );

				autoRosterResizeInProgress = true;

				QRect desktop = qApp->desktop()->availableGeometry( (QWidget *)topParent );
				if ( option.autoRosterSizeGrowTop ) {
					int newy = topParent->y() - dh;

					// check, if we need to move roster lower
					if ( dh > 0 && ( topParent->frameGeometry().top() <= desktop.top() ) ) {
						newy = desktop.top();
					}

					topParent->move( topParent->x(), newy );
				}

				// check, if we need to move roster upper
				if ( dh > 0 && ( topParent->frameGeometry().bottom() >= desktop.bottom() ) ) {
					int newy = desktop.bottom() - topParent->frameGeometry().height();
					topParent->move( topParent->x(), newy );
				}

				QTimer::singleShot(0, this, [this]() { resetAutoRosterResize(); });

				topParent->layout()->setEnabled( true );
			}

			// issue a layout update
			cv->parentWidget()->layout()->update();
		}
	}

	/*
	 * \brief Determine in which direction to grow Psi roster window when autoRosterSize is enabled
	 */
	void determineAutoRosterSizeGrowSide()
	{
		if ( autoRosterResizeInProgress )
			return;

		QWidget *topParent = cv->window();
		QRect desktop = qApp->desktop()->availableGeometry( (QWidget *)topParent );

		int top_offs    = abs( desktop.top()    - topParent->frameGeometry().top() );
		int bottom_offs = abs( desktop.bottom() - topParent->frameGeometry().bottom() );

		option.autoRosterSizeGrowTop = bottom_offs < top_offs;
		//qWarning("growTop = %d", option.autoRosterSizeGrowTop);
	}
	
	/*
	 * \brief Display tool tip for item under cursor.
	 */
	bool doToolTip(const QPoint &pos)
	{
		ContactViewItem *i = cv->itemAtPosition(pos);
		if(!i)
			return false;

		const QString tip = i->toolTipText();
		if(tip.isEmpty())
			return false;

		PsiToolTip::showText(cv->mapToGlobal(pos), tip, cv);
		return true;
	}

private slots:
	void resetAutoRosterResize()
	{
		//qWarning("resetAutoRosterResize");
		autoRosterResizeInProgress = false;
	}

};

ContactView::ContactView(QWidget *parent, const char *name)
	: QTreeWidget(parent)
{
	if (name) setObjectName(QString::fromLatin1(name));
	d = new Private(this);

	setAttribute(Qt::WA_NoSystemBackground);
	setAttribute(Qt::WA_StaticContents);

	// setup QTreeWidget
	setAllColumnsShowFocus(true);
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setMinimumSize(96,32);
	setIndentation(4);
	setSortingEnabled(false);

	window()->installEventFilter(this);
	viewport()->installEventFilter(this);

	// Single column, no visible header
	setColumnCount(1);
	setHeaderHidden(true);
	header()->setStretchLastSection(true);

	// Enable drag & drop
	setDragEnabled(true);
	setAcceptDrops(true);
	viewport()->setAcceptDrops(true);
	setDragDropMode(QAbstractItemView::DragDrop);

	// Install custom delegate for item painting
	setItemDelegate(new ContactViewDelegate(this));

	// catch signals
	lcto_active = false;
	connect(this, &QTreeWidget::itemDoubleClicked, this, &ContactView::qlv_itemDoubleClicked);
	connect(this, &QTreeWidget::itemExpanded,  this, &ContactView::onItemExpanded);
	connect(this, &QTreeWidget::itemCollapsed, this, &ContactView::onItemCollapsed);

	v_showOffline = true;
	v_showAway = true;
	v_showHidden = true;
	v_showAgents = true;
	v_showSelf = true;
	v_showStatusMsg = PsiOptions::instance()->getOption("options.ui.contactlist.status-messages.show").toBool();

	d->lastSize = QSize( 0, 0 );

	// animation timer
	d->animTimer = new QTimer(this);
	d->animTimer->start(120 * 5);

	d->recalculateSizeTimer = new QTimer(this);
	d->recalculateSizeTimer->setSingleShot(true);
	connect(d->recalculateSizeTimer, &QTimer::timeout, d, &Private::recalculateSize);

	// actions
	qa_send = new IconAction("", "psi/sendMessage", tr("Send &message"), 0, this);
	connect(qa_send, &QAction::triggered, this, &ContactView::doSendMessage);
	qa_ren = new IconAction("", /*"psi/edit/clear",*/ tr("Re&name"), 0, this);
	connect(qa_ren, &QAction::triggered, this, &ContactView::doRename);
	qa_assignAvatar = new IconAction("", tr("&Assign Custom Picture"), 0, this);
	connect(qa_assignAvatar, &QAction::triggered, this, &ContactView::doAssignAvatar);
	qa_clearAvatar = new IconAction("", tr("&Clear Custom Picture"), 0, this);
	connect(qa_clearAvatar, &QAction::triggered, this, &ContactView::doClearAvatar);
	qa_chat = new IconAction("", "psi/start-chat", tr("Open &chat window"), 0, this);
	connect(qa_chat, &QAction::triggered, this, &ContactView::doOpenChat);
#ifdef WHITEBOARDING
	qa_wb = new IconAction("", "psi/whiteboard", tr("Open a &whiteboard"), Qt::CTRL+Qt::Key_W, this);
	connect(qa_wb, &QAction::triggered, this, &ContactView::doOpenWhiteboard);
#endif
	qa_hist = new IconAction("", "psi/history", tr("&History"), 0, this);
	connect(qa_hist, &QAction::triggered, this, &ContactView::doHistory);
	qa_logon = new IconAction("", tr("&Log on"), 0, this);
	connect(qa_logon, &QAction::triggered, this, &ContactView::doLogon);
	qa_recv = new IconAction("", tr("&Receive incoming event"), 0, this);
	connect(qa_recv, &QAction::triggered, this, &ContactView::doRecvEvent);
	qa_rem = new IconAction("", "psi/remove", tr("Rem&ove"), 0, this);
	connect(qa_rem, &QAction::triggered, this, &ContactView::doRemove);
	qa_vcard = new IconAction("", "psi/vCard", tr("User &Info"), 0, this);
	connect(qa_vcard, &QAction::triggered, this, &ContactView::doVCard);

	if(option.lockdown.roster) {
		qa_ren->setEnabled(false);
		qa_rem->setEnabled(false);
	}

	optionsUpdate();
	filterString_ = QString();
}

ContactView::~ContactView()
{
	clear();
	delete d;
}

ContactViewItem *ContactView::toContactViewItem(QTreeWidgetItem *item)
{
	return static_cast<ContactViewItem *>(item);
}

ContactViewItem *ContactView::itemAtPosition(const QPoint &pos) const
{
	return toContactViewItem(itemAt(pos));
}

void ContactView::setCurrentContactItem(ContactViewItem *item)
{
	if(item)
		setCurrentItem(item);
}

void ContactView::setContactItemOpen(ContactViewItem *item, bool open)
{
	if(item)
		item->setOpen(open);
}

void ContactView::ensureContactItemVisible(ContactViewItem *item)
{
	if(item)
		scrollToItem(item);
}

int ContactView::itemVerticalPosition(ContactViewItem *item) const
{
	return item ? visualItemRect(item).y() : 0;
}

bool ContactView::filterContact(ContactViewItem *item, bool refineSearch)
{
	if (!item) {
		return false;
	}
	if (item->type() != ContactViewItem::Contact) {
		return false;
	}
	if (filterString_.isNull()) {
		return true;	
	}
	//if refineSearch, only search still visible items
	if (refineSearch && item->isHidden()) {
		return false;		
	}
	if (TextUtil::rich2plain(item->text(0)).contains(filterString_,Qt::CaseInsensitive))
	{
		ensureContactItemVisible(item);
		item->setHidden(false);
		item->optionsUpdate();			
	}
	else
	{
		item->setHidden(true);
	}
	item->repaintItem();
	return !item->isHidden();
}

bool ContactView::filterGroup(ContactViewItem *group, bool refineSearch)
{
	if (!group) {
		return false;	
	} else if (group->type() != ContactViewItem::Group) {
		return false;
	} else if (filterString_.isNull()) {
		return true;
	}
	//if refine_search, only search still visible items
	if (refineSearch && group->isHidden()) {
		return false;	
	}
	group->setHidden(false); //if not refined search
	
	// iterate over children
	bool groupContainsAFinding = false;
	ContactViewItem *item = group->firstChildItem();
	while(item) {
		if (filterContact(item,refineSearch))
			groupContainsAFinding = true;
        item = item->nextSiblingItem();
	}
	if (groupContainsAFinding == false) {
		group->setHidden(true);		
	}
	group->repaintItem();
	return groupContainsAFinding;
}

void ContactView::setFilter(const QString &text)
{
	bool refineSearch = text.startsWith(filterString_);
	filterString_ = text;
	
	for (ContactViewItem *item = firstTopLevelItem(); item; item = item->nextSiblingItem()) {
		if (item->type() == ContactViewItem::Group)
			filterGroup(item, refineSearch);
	}
}

void ContactView::clearFilter()
{
	filterString_=QString();
	for (ContactViewItem *item = firstTopLevelItem(); item; item = item->nextSiblingItem()) {
		if (item->type() != ContactViewItem::Contact && item->type() != ContactViewItem::Group)
			continue;
		item->setHidden(false);
		item->optionsUpdate();
		item->repaintItem();
	}
}


QTimer *ContactView::animTimer() const
{
	return d->animTimer;
}

ContactViewItem *ContactView::selectedContactViewItem() const
{
	return static_cast<ContactViewItem *>(currentItem());
}

ContactViewItem *ContactView::firstTopLevelItem() const
{
	return static_cast<ContactViewItem *>(topLevelItem(0));
}

void ContactView::clear()
{
	qDeleteAll(d->profiles);
	d->profiles.clear();
}

void ContactView::link(ContactProfile *cp)
{
	d->profiles.append(cp);
}

void ContactView::unlink(ContactProfile *cp)
{
	d->profiles.removeAll(cp);
}

void ContactView::keyPressEvent(QKeyEvent *e)
{
	int key = e->key();
	if(key == Qt::Key_Enter || key == Qt::Key_Return) {
		doEnter();
	} else if(key == Qt::Key_Space /*&& d->typeAhead.isEmpty()*/) {
		doContext();
	} else if (key == Qt::Key_Home   || key == Qt::Key_End      ||
		 key == Qt::Key_PageUp || key == Qt::Key_PageDown ||
		 key == Qt::Key_Up     || key == Qt::Key_Down     ||
		 key == Qt::Key_Left   || key == Qt::Key_Right) {

		//d->typeAhead = QString::null;
		QTreeWidget::keyPressEvent(e);
	} else {
		QString text = e->text().toLower();
		if ( text.isEmpty() ) {
			QTreeWidget::keyPressEvent(e);
		} else if (key == Qt::Key_Escape) {
			e->ignore();
		} else {
			emit searchInput(text);
		}

	}
}

void ContactView::setShowOffline(bool x)
{
	bool oldstate = v_showOffline;
	v_showOffline = x;

	if(v_showOffline != oldstate) {
		showOffline(v_showOffline);

		for (int _pi = 0; _pi < d->profiles.size(); ++_pi) {
			ContactProfile *cp = d->profiles[_pi];
			if(!v_showOffline)
				cp->removeAllUnneededContactItems();
			else
				cp->addAllNeededContactItems();
		}
	}
}

void ContactView::setShowAway(bool x)
{
	bool oldstate = v_showAway;
	v_showAway = x;

	if(v_showAway != oldstate) {
		showAway(v_showAway);

		for (int _pi = 0; _pi < d->profiles.size(); ++_pi) {
			ContactProfile *cp = d->profiles[_pi];
			if(!v_showAway)
				cp->removeAllUnneededContactItems();
			else
				cp->addAllNeededContactItems();
		}
	}
}

void ContactView::setShowHidden(bool x)
{
	bool oldstate = v_showHidden;
	v_showHidden = x;

	if(v_showHidden != oldstate) {
		showHidden(v_showHidden);

		for (int _pi = 0; _pi < d->profiles.size(); ++_pi) {
			ContactProfile *cp = d->profiles[_pi];
			if(!v_showHidden)
				cp->removeAllUnneededContactItems();
			else
				cp->addAllNeededContactItems();
		}
	}
}

void ContactView::setShowStatusMsg(bool x)
{
	if (v_showStatusMsg != x) {
		v_showStatusMsg = x;
		PsiOptions::instance()->setOption("options.ui.contactlist.status-messages.show",x);
		emit showStatusMsg(v_showStatusMsg);

		for (int _pi = 0; _pi < d->profiles.size(); ++_pi) {
			ContactProfile *cp = d->profiles[_pi];
			cp->resetAllContactItemNames();
		}
		
		recalculateSize();
	}
}

/*
 * \brief Shows/hides the self contact in roster
 *
 * \param x - boolean variable specifies whether to show self-contact or not
 */
void ContactView::setShowSelf(bool x)
{
	if (v_showSelf != x) {
		v_showSelf = x;
		showSelf(v_showSelf);

		for (int _pi = 0; _pi < d->profiles.size(); ++_pi) {
			ContactProfile *cp = d->profiles[_pi];
			if (v_showSelf && ! cp->self()) {
				cp->addSelf();
			}
			else if (!v_showSelf && cp->self() && cp->self()->u()->userResourceList().count() <= 1) {
				cp->removeSelf();
			}
		}

		recalculateSize();
	}
}

/*
 * \brief Event filter. Nuff said.
 */
bool ContactView::eventFilter(QObject *obj, QEvent *event)
{
	if (event->type() == QEvent::Move)
		d->determineAutoRosterSizeGrowSide();
	else if (event->type() == QEvent::ToolTip && obj == viewport()) {
		const QHelpEvent *helpEvent = static_cast<const QHelpEvent *>(event);
		if (d->doToolTip(helpEvent->pos())) {
			event->accept();
			return true;
		}
	}

	return QTreeWidget::eventFilter(obj, event);
}


void ContactView::setShowAgents(bool x)
{
	bool oldstate = v_showAgents;
	v_showAgents = x;

	if(v_showAgents != oldstate) {
		showAgents(v_showAgents);

		for (int _pi = 0; _pi < d->profiles.size(); ++_pi) {
			ContactProfile *cp = d->profiles[_pi];
			if(!v_showAgents)
				cp->removeAllUnneededContactItems();
			else
				cp->addAllNeededContactItems();
		}
	}
}

// right-click context menu (called from contextMenuEvent and doContext)
void ContactView::qlv_contextPopup(ContactViewItem *i, const QPoint &pos)
{
	if(!i)
		return;
	i->contactProfile()->doContextMenu(i, pos);
}

void ContactView::contextMenuEvent(QContextMenuEvent *e)
{
	if(option.useleft)
		return;
	ContactViewItem *i = toContactViewItem(itemAt(e->pos()));
	if(i)
		qlv_contextPopup(i, e->globalPos());
}

void ContactView::mousePressEvent(QMouseEvent *e)
{
	mousePressPos = e->pos();
	QTreeWidget::mousePressEvent(e);

	ContactViewItem *item = toContactViewItem(itemAt(e->pos()));
	if (!item)
		return;

	qlv_singleclick(e->button(), item, e->globalPos());
}

void ContactView::mouseMoveEvent(QMouseEvent *e)
{
	QTreeWidget::mouseMoveEvent(e);
}

void ContactView::qlv_singleclick(Qt::MouseButton button, ContactViewItem *item, const QPoint &globalPos)
{
	lcto_active = false;

	if(!item)
		return;

	setCurrentContactItem(item);

	if(button == Qt::MidButton) {
		if(item->type() == ContactViewItem::Contact)
			item->contactProfile()->scActionDefault(item);
	}
	else {
		if(option.useleft) {
			if(button == Qt::LeftButton) {
				if(option.singleclick) {
					qlv_contextPopup(item, globalPos);
				}
				else {
					lcto_active = true;
					lcto_pos = globalPos;
					lcto_item = item;
					QTimer::singleShot(QApplication::doubleClickInterval() / 2, this, [this]() {
						leftClickTimeOut();
					});
				}
			}
			else if(option.singleclick && button == Qt::RightButton) {
				if(item->type() == ContactViewItem::Contact)
					item->contactProfile()->scActionDefault(item);
			}
		}
		else {
			if(button == Qt::LeftButton && option.singleclick) {
				if(item->type() == ContactViewItem::Contact)
					item->contactProfile()->scActionDefault(item);
			}
		}
	}
}

void ContactView::qlv_itemDoubleClicked(QTreeWidgetItem *i, int)
{
	lcto_active = false;

	if(!i)
		return;

	if(option.singleclick)
		return;

	ContactViewItem *item = toContactViewItem(i);
	item->contactProfile()->scActionDefault(item);
}

void ContactView::qlv_itemRenamed(ContactViewItem *i, const QString &text)
{
	if(i)
		i->contactProfile()->doItemRenamed(i, text);
}

void ContactView::onItemExpanded(QTreeWidgetItem *item)
{
	if (ContactViewItem *cvi = toContactViewItem(item))
		cvi->onExpanded();
}

void ContactView::onItemCollapsed(QTreeWidgetItem *item)
{
	if (ContactViewItem *cvi = toContactViewItem(item))
		cvi->onCollapsed();
}

void ContactView::leftClickTimeOut()
{
	if(lcto_active) {
		ContactViewItem *cvi = toContactViewItem(lcto_item);
		if(cvi)
			qlv_contextPopup(cvi, lcto_pos);
		lcto_active = false;
	}
}

void ContactView::optionsUpdate()
{
	// set the font
	QFont f;
	f.fromString(option.font[fRoster]);
	setFont(f);

	// set the text and background colors
	QPalette mypal = palette();
	mypal.setColor(QPalette::Text, option.color[cOnline]);
	mypal.setColor(QPalette::Base, option.color[cListBack]);
	setPalette(mypal);

	// reload the icons
	for (ContactViewItem *item = firstTopLevelItem(); item; item = item->nextSiblingItem())
		item->optionsUpdate();

	// shortcuts
	setShortcuts();

	// resize if necessary
	if (option.autoRosterSize)
		recalculateSize();

	update();
}

void ContactView::setShortcuts()
{
	qa_send->setShortcuts(ShortcutManager::instance()->shortcuts("contactlist.message"));
	qa_ren->setShortcuts(ShortcutManager::instance()->shortcuts("contactlist.rename"));
	qa_assignAvatar->setShortcuts(ShortcutManager::instance()->shortcuts("contactlist.assign-custom-avatar"));
	qa_clearAvatar->setShortcuts(ShortcutManager::instance()->shortcuts("contactlist.clear-custom-avatar"));
	qa_chat->setShortcuts(ShortcutManager::instance()->shortcuts("contactlist.chat"));
	qa_hist->setShortcuts(ShortcutManager::instance()->shortcuts("common.history"));
	qa_logon->setShortcuts(ShortcutManager::instance()->shortcuts("contactlist.login-transport"));
	qa_recv->setShortcuts(ShortcutManager::instance()->shortcuts("contactlist.event"));
	qa_rem->setShortcuts(ShortcutManager::instance()->shortcuts("contactlist.delete"));
	qa_vcard->setShortcuts(ShortcutManager::instance()->shortcuts("common.user-info"));
}

void ContactView::resetAnim()
{
	for (ContactViewItem *item = firstTopLevelItem(); item; item = item->nextSiblingItem()) {
		if(item->isAlerting())
			item->resetAnim();
	}
}

void ContactView::doRecvEvent()
{
	ContactViewItem *i = selectedContactViewItem();
	if(!i)
		return;
	i->contactProfile()->scRecvEvent(i);
}

void ContactView::doRename()
{
	ContactViewItem *i = selectedContactViewItem();
	if(!i)
		return;
	i->contactProfile()->scRename(i);
}

void ContactView::doAssignAvatar()
{
	// FIXME: Should check the supported filetypes dynamically
	QString file = QFileDialog::getOpenFileName(this, tr("Choose an image"), "", tr("All files (*.png *.jpg *.gif)"));
	if (!file.isNull()) {
		ContactViewItem *i = selectedContactViewItem();
		i->contactProfile()->psiAccount()->avatarFactory()->importManualAvatar(i->u()->jid(),file);
	}
}

void ContactView::doClearAvatar()
{
	ContactViewItem *i = selectedContactViewItem();
	i->contactProfile()->psiAccount()->avatarFactory()->removeManualAvatar(i->u()->jid());
}

void ContactView::doEnter()
{
	ContactViewItem *i = selectedContactViewItem();
	if(!i)
		return;
	i->contactProfile()->scActionDefault(i);
}

void ContactView::doContext()
{
	ContactViewItem *i = selectedContactViewItem();
	if(!i)
		return;
	ensureContactItemVisible(i);

	if(i->type() == ContactViewItem::Group)
		setContactItemOpen(i, !i->isExpanded());
	else
		qlv_contextPopup(i, viewport()->mapToGlobal(QPoint(32, itemVerticalPosition(i))));
}

void ContactView::doSendMessage()
{
	ContactViewItem *i = selectedContactViewItem();
	if(!i)
		return;
	i->contactProfile()->scSendMessage(i);
}

void ContactView::doOpenChat()
{
	ContactViewItem *i = selectedContactViewItem();
	if(!i)
		return;
	i->contactProfile()->scOpenChat(i);
}

#ifdef WHITEBOARDING
void ContactView::doOpenWhiteboard()
{
	ContactViewItem *i = selectedContactViewItem();
	if(!i)
		return;
	i->contactProfile()->scOpenWhiteboard(i);
}
#endif

void ContactView::doHistory()
{
	ContactViewItem *i = selectedContactViewItem();
	if(!i)
		return;
	i->contactProfile()->scHistory(i);
}

void ContactView::doVCard()
{
	ContactViewItem *i = selectedContactViewItem();
	if(!i)
		return;
	i->contactProfile()->scVCard(i);
}

void ContactView::doLogon()
{
	ContactViewItem *i = selectedContactViewItem();
	if(!i)
		return;
	Status s=i->contactProfile()->psiAccount()->status();
	i->contactProfile()->scAgentSetStatus(i, s);
}

void ContactView::doRemove()
{
	ContactViewItem *i = selectedContactViewItem();
	if(!i)
		return;
	i->contactProfile()->scRemove(i);
}

void ContactView::startDrag(Qt::DropActions supportedActions)
{
	ContactViewItem *i = selectedContactViewItem();
	if(!i || i->type() != ContactViewItem::Contact)
		return;

	QDrag *drag = createContactTextDrag(i->u()->jid().full(), this,
		IconsetFactory::iconPixmap("status/online"));
	drag->exec(supportedActions);
}

void ContactView::dragEnterEvent(QDragEnterEvent *e)
{
	if(e->mimeData()->hasText() || e->mimeData()->hasUrls())
		e->acceptProposedAction();
	else
		e->ignore();
}

void ContactView::dragMoveEvent(QDragMoveEvent *e)
{
	ContactViewItem *item = toContactViewItem(itemAt(e->pos()));
	if(item && item->canAcceptDrop())
		e->acceptProposedAction();
	else
		e->ignore();
}

void ContactView::dropEvent(QDropEvent *e)
{
	ContactViewItem *item = toContactViewItem(itemAt(e->pos()));
	if(!item || !item->canAcceptDrop()) {
		e->ignore();
		return;
	}

	const QMimeData *mimeData = e->mimeData();

	// Files
	const QStringList localFiles = decodeLocalDropFiles(mimeData);
	if (!localFiles.isEmpty()) {
		item->contactProfile()->dragDropFiles(localFiles, item);
		e->acceptProposedAction();
		return;
	}

	// Text (JID)
	QString text;
	if (decodeDropText(mimeData, &text)) {
		item->contactProfile()->dragDrop(text, item);
		e->acceptProposedAction();
		return;
	}

	e->ignore();
}

bool ContactView::allowResize() const
{
	if ( !option.autoRosterSize )
		return false;

	if ( window()->isMaximized() )
		return false;

	return true;
}

QSize ContactView::minimumSizeHint() const
{
	return QSize( minimumWidth(), minimumHeight() );
}

QSize ContactView::sizeHint() const
{
	// save some CPU
	if ( !allowResize() )
		return minimumSizeHint();

	QSize s( QTreeWidget::sizeHint().width(), 0 );
	int border = 5;
	int h = border;

	QTreeWidgetItemIterator it(const_cast<ContactView*>(this));
	while (*it) {
		ContactViewItem *current = toContactViewItem(*it);
		if (current && !current->isHidden() && !current->hasClosedAncestors())
			h += sizeHintForRow(indexFromItem(current).row());
		++it;
	}

	QWidget *topParent = window();
	QRect desktop = qApp->desktop()->availableGeometry( (QWidget *)topParent );
	int dh = h - d->lastSize.height();

	// check that our dialog's height doesn't exceed the desktop's
	if ( allowResize() && dh > 0 && (topParent->frameGeometry().height() + dh) >= desktop.height() ) {
		h = desktop.height() - ( topParent->frameGeometry().height() - d->lastSize.height() );
	}

	int minH = minimumSizeHint().height();
	if ( h < minH )
		h = minH + border;
	s.setHeight( h );
	return s;
}

/*
 * \brief Adds the request to recalculate the ContactView size to the event queue
 */
void ContactView::recalculateSize()
{
	d->recalculateSizeTimer->start(0);
}

//------------------------------------------------------------------------------
// RichListViewItem: A RichText listview item
//------------------------------------------------------------------------------

#include <QAbstractTextDocumentLayout>
#include <QPainter>
#include <QTextDocument>

static const int icon_vpadding = 2;

RichListViewItem::RichListViewItem( QTreeWidget * parent ) : QTreeWidgetItem(parent)
{
	v_rt = 0;
	v_height = 0;
	v_active = v_selected = false;
	v_rich = !PsiOptions::instance()->getOption("options.ui.contactlist.status-messages.single-line").toBool();
}

RichListViewItem::RichListViewItem( QTreeWidgetItem * parent ) : QTreeWidgetItem(parent)
{
	v_rt = 0;
	v_height = 0;
	v_active = v_selected = false;
	v_rich = !PsiOptions::instance()->getOption("options.ui.contactlist.status-messages.single-line").toBool();
}

RichListViewItem::~RichListViewItem()
{
	delete v_rt;
}

void RichListViewItem::setPixmap(int column, const QPixmap &px)
{
	if (column == 0) v_pixmap = px;
	setIcon(column, px.isNull() ? QIcon() : QIcon(px));
}

const QPixmap *RichListViewItem::pixmap(int column) const
{
	if (column == 0 && !v_pixmap.isNull())
		return &v_pixmap;
	return nullptr;
}

void RichListViewItem::setItemHeight(int h)
{
	v_height = h;
	setSizeHint(0, QSize(sizeHint(0).width(), h));
}

int RichListViewItem::itemHeight() const
{
	return v_height > 0 ? v_height : 20; // fallback
}

QTreeWidget *RichListViewItem::contactListView() const
{
	return treeWidget();
}

int RichListViewItem::depth() const
{
	int d = 0;
	const QTreeWidgetItem *item = this;
	while (item->parent()) { ++d; item = item->parent(); }
	return d;
}

static const int kItemMargin = 2;

int RichListViewItem::contentLeftOffset(int column) const
{
	const QPixmap *px = pixmap(column);
	return kItemMargin + (px ? px->width() + kItemMargin : 0);
}

int RichListViewItem::availableTextWidth(int column) const
{
	QTreeWidget *lv = contactListView();
	if(!lv)
		return 0;
	return lv->columnWidth(column) - contentLeftOffset(column) - depth() * lv->indentation();
}

void RichListViewItem::setText(int column, const QString& text)
{
	QTreeWidgetItem::setText(column, text);
	setup();
}

void RichListViewItem::setup()
{
	if (v_rich) {
		int h = itemHeight();
		QString txt = text(0);
		if( txt.isEmpty() ){
			delete v_rt;
			v_rt = 0;
			return;
		}

		QTreeWidget *lv = contactListView();
		if(!lv)
			return;

		const int left = contentLeftOffset(0);

		v_active = lv->isActiveWindow();
		v_selected = isSelected();

		if ( v_selected  ) {
			txt = QString("<font color=\"%1\">").arg(lv->palette().color( QPalette::HighlightedText ).name()) + txt + "</font>";
		}
		
		if(v_rt)
			delete v_rt;
		v_rt = new QTextDocument;
		v_rt->setDefaultFont(lv->font());
		v_rt->setDocumentMargin(0);
		v_rt->setHtml(txt);
		v_rt->setTextWidth(availableTextWidth(0));

		v_widthUsed = int(v_rt->idealWidth()) + left;

		h = qMax(h, int(v_rt->size().height()));

		if ( h % 2 > 0 )
			h++;

		setItemHeight( h );
	}
}

void RichListViewItem::doPaint(QPainter *p, const QPalette &cg, bool selected, bool active,
                               int width, int height)
{
	if(!v_rt){
		// Default paint: background + icon + text
		if (selected)
			p->fillRect(0, 0, width, height, cg.brush(QPalette::Highlight));
		else
			p->fillRect(0, 0, width, height, cg.brush(QPalette::Base));
		const QPixmap *px = pixmap(0);
		int x = kItemMargin;
		if (px) {
			int y = (height - px->height()) / 2;
			p->drawPixmap(x, y, *px);
			x += px->width() + kItemMargin;
		}
		if (selected)
			p->setPen(cg.color(QPalette::HighlightedText));
		else
			p->setPen(cg.color(QPalette::Text));
		QFontMetrics fm(p->font());
		int y = (height + fm.ascent() - fm.descent()) / 2;
		p->drawText(x, y, text(0));
		return;
	}

	p->save();

	if ( selected != v_selected || active != v_active)
		setup();

	int r = kItemMargin;

	QBrush paper;
	if ( selected ) {
		paper = cg.brush( QPalette::Highlight );
	}
	else {
		paper = cg.brush(QPalette::Base);
		paper.setStyle(Qt::NoBrush);
	}

	const QPixmap *px = pixmap( 0 );
	QRect pxrect;
	int pxw = 0;
	int pxh = 0;
	if(px) {
		pxw = px->width();
		pxh = px->height();
		pxrect = QRect(r, icon_vpadding, pxw, pxh);
		r += pxw + kItemMargin;
	}

	// start drawing
	QRect rtrect(r, (height - int(v_rt->size().height()))/2, v_widthUsed, int(v_rt->size().height()));
	p->fillRect(0, 0, width, height, paper);
	p->translate(rtrect.left(), rtrect.top());
	QAbstractTextDocumentLayout::PaintContext ctx;
	ctx.clip = QRectF(0, 0, rtrect.width(), rtrect.height());
	v_rt->documentLayout()->draw(p, ctx);
	p->translate(-rtrect.left(), -rtrect.top());

	QRegion clip(0, 0, width, height);
	clip -= rtrect;
	p->setClipRegion(clip);

	if(px)
		p->drawPixmap(pxrect, *px);

	p->restore();
}

int RichListViewItem::widthUsed() const
{
	return v_widthUsed;
}

//----------------------------------------------------------------------------
// ContactViewItem
//----------------------------------------------------------------------------
class ContactViewItem::Private
{
private:
	ContactViewItem *cvi;

public:
	Private(ContactViewItem *parent, ContactProfile *_cp) {
		cvi = parent;
		cp = _cp;
		u = 0;
		animateNickColor = false;

		icon = lastIcon = 0;
	}

	~Private() {
	}

	void initGroupState() {
		UserAccount::GroupData gd = groupData();
		cvi->setOpen(gd.open);
	}

	QString getGroupName() {
		QString group;
		if ( cvi->type() == Profile )
			group = "/\\/" + profileName + "\\/\\";
		else
			group = groupName;

		return group;
	}

	QMap<QString, UserAccount::GroupData> *groupState() {
		return (QMap<QString, UserAccount::GroupData> *)&cp->psiAccount()->userAccount().groupState;
	}

	UserAccount::GroupData groupData() {
		QMap<QString, UserAccount::GroupData> groupState = (QMap<QString, UserAccount::GroupData>)cp->psiAccount()->userAccount().groupState;
		QMap<QString, UserAccount::GroupData>::Iterator it = groupState.find(getGroupName());

		UserAccount::GroupData gd;
		gd.open = true;
		gd.rank = 0;

		if ( it != groupState.end() )
			gd = it.value();

		return gd;
	}

	ContactProfile *cp;
	int status;

	// profiles
	QString profileName;
	bool ssl;

	// groups
	int groupType;
	QString groupName;
	QString groupInfo;

	// contact
	UserListItem *u;
	bool isAgent;
	bool alerting;
	bool animatingNick;
	bool status_single;

	PsiIcon *icon, *lastIcon;
	int animateNickX, animateNickColor; // nick animation
};

ContactViewItem::ContactViewItem(const QString &profileName, ContactProfile *cp, ContactView *parent)
:RichListViewItem(parent)
{
	type_ = Profile;
	d = new Private(this, cp);
	d->profileName = profileName;
	d->alerting = false;
	d->ssl = false;
	d->status_single = !PsiOptions::instance()->getOption("options.ui.contactlist.status-messages.single-line").toBool();

	setProfileState(STATUS_OFFLINE);
	if (!PsiOptions::instance()->getOption("options.ui.account.single").toBool())
		setText(0, profileName);

	d->initGroupState();
}

ContactViewItem::ContactViewItem(const QString &groupName, int groupType, ContactProfile *cp, ContactView *parent)
:RichListViewItem(parent)
{
	type_ = Group;
	d = new Private(this, cp);
	d->groupName = groupName;
	d->groupType = groupType;
	d->alerting = false;
	d->status_single = !PsiOptions::instance()->getOption("options.ui.contactlist.status-messages.single-line").toBool();

	drawGroupIcon();
	resetGroupName();
	setFlags(flags() | Qt::ItemIsDropEnabled);

	d->initGroupState();
}

ContactViewItem::ContactViewItem(const QString &groupName, int groupType, ContactProfile *cp, ContactViewItem *parent)
:RichListViewItem(parent)
{
	type_ = Group;
	d = new Private(this, cp);
	d->groupName = groupName;
	d->groupType = groupType;
	d->alerting = false;
	d->status_single = !PsiOptions::instance()->getOption("options.ui.contactlist.status-messages.single-line").toBool();

	drawGroupIcon();
	resetGroupName();
	setFlags(flags() | Qt::ItemIsDropEnabled);

	if(parent->isHidden())
		setHidden(true);

	d->initGroupState();
}

ContactViewItem::ContactViewItem(UserListItem *u, ContactProfile *cp, ContactViewItem *parent)
:RichListViewItem(parent)
{
	type_ = Contact;
	d = new Private(this, cp);
	d->cp = cp;
	d->u = u;
	d->alerting = false;
	d->animatingNick = false;
	d->status_single = !PsiOptions::instance()->getOption("options.ui.contactlist.status-messages.single-line").toBool();

	cacheValues();

	resetStatus();
	resetName();

	setFlags(flags() | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);

	if(parent->isHidden())
		setHidden(true);
	else
		setup();
}

ContactViewItem::~ContactViewItem()
{
	setIcon( 0 );
	delete d;
}

void ContactViewItem::cacheValues()
{
	if ( d->u ) {
		if( !d->u->isAvailable() )
			d->status = STATUS_OFFLINE;
		else
			d->status = makeSTATUS((*d->u->priority()).status());
		d->isAgent = d->u->isTransport();
	}
}

ContactProfile *ContactViewItem::contactProfile() const
{
	return d->cp;
}

int ContactViewItem::type() const
{
	return type_;
}

const QString & ContactViewItem::groupName() const
{
	return d->groupName;
}

const QString & ContactViewItem::groupInfo() const
{
	return d->groupInfo;
}

int ContactViewItem::groupType() const
{
	return d->groupType;
}

UserListItem *ContactViewItem::u() const
{
	return d->u;
}

int ContactViewItem::status() const
{
	return d->status;
}

bool ContactViewItem::isAgent() const
{
	return d->isAgent;
}

bool ContactViewItem::isAlerting() const
{
	return d->alerting;
}

bool ContactViewItem::isAnimatingNick() const
{
	return d->animatingNick;
}

int ContactViewItem::parentGroupType() const
{
	ContactViewItem *item = parentItem();
	return item->groupType();
}

ContactView *ContactViewItem::contactView() const
{
	return static_cast<ContactView *>(treeWidget());
}

ContactViewItem *ContactViewItem::parentItem() const
{
	return static_cast<ContactViewItem *>(QTreeWidgetItem::parent());
}

ContactViewItem *ContactViewItem::firstChildItem() const
{
	return static_cast<ContactViewItem *>(child(0));
}

ContactViewItem *ContactViewItem::nextSiblingItem() const
{
	QTreeWidgetItem *par = QTreeWidgetItem::parent();
	if (par) {
		int idx = par->indexOfChild(const_cast<ContactViewItem*>(this));
		return static_cast<ContactViewItem *>(par->child(idx + 1));
	} else {
		QTreeWidget *tw = treeWidget();
		if (!tw) return nullptr;
		int idx = tw->indexOfTopLevelItem(const_cast<ContactViewItem*>(this));
		return static_cast<ContactViewItem *>(tw->topLevelItem(idx + 1));
	}
}

bool ContactViewItem::hasChildrenItems() const
{
	return childCount() > 0;
}

bool ContactViewItem::hasClosedAncestors() const
{
	for (ContactViewItem *item = parentItem(); item; item = item->parentItem()) {
		if (!item->isExpanded())
			return true;
	}

	return false;
}

ContactViewItem *ContactViewItem::dropTargetGroup() const
{
	if (type_ == Group)
		return const_cast<ContactViewItem *>(this);
	if (type_ == Contact)
		return parentItem();
	return 0;
}

QString ContactViewItem::toolTipText() const
{
	if (type_ == Contact && d->u)
		return d->u->makeTip(true, false);
	if (type_ == Profile)
		return d->cp->makeTip(true, false);
	if (type_ == Group)
		return d->groupInfo.isEmpty() ? d->groupName : d->groupName + " " + d->groupInfo;
	return QString();
}

void ContactViewItem::drawGroupIcon()
{
	if ( type_ == Group ) {
		if ( !hasChildrenItems() )
			setIcon(IconsetFactory::iconPtr("psi/groupEmpty"));
		else if ( isExpanded() )
			setIcon(IconsetFactory::iconPtr("psi/groupOpen"));
		else
			setIcon(IconsetFactory::iconPtr("psi/groupClosed"));
	}
	else if ( type_ == Profile ) {
		if ( !d->alerting )
			setProfileState(d->status);
	}
}

void ContactViewItem::doPaint(QPainter *p, const QPalette &cg, bool selected, bool active,
                              int width, int height)
{
	if ( type_ == Contact ) {
		QPalette xcg = cg;

		if(d->status == STATUS_AWAY || d->status == STATUS_XA)
			xcg.setColor(QPalette::Text, option.color[cAway]);
		else if(d->status == STATUS_DND)
			xcg.setColor(QPalette::Text, option.color[cDND]);
		else if(d->status == STATUS_OFFLINE)
			xcg.setColor(QPalette::Text, option.color[cOffline]);

		if(d->animatingNick) {
			xcg.setColor(QPalette::Text, d->animateNickColor ? option.color[cAnimFront] : option.color[cAnimBack]);
			xcg.setColor(QPalette::HighlightedText, d->animateNickColor ? option.color[cAnimFront] : option.color[cAnimBack]);
		}

		RichListViewItem::doPaint(p, xcg, selected, active, width, height);

		int x;
		if (d->status_single)
			x = widthUsed();
		else {
			QFontMetrics fm(p->font());
			const QPixmap *pix = pixmap(0);
			x = fontMetricsTextWidth(fm, text(0)) + (pix ? pix->width() : 0) + 8;
		}

		if ( d->u ) {
			UserResourceList::ConstIterator it = d->u->priority();
			if(it != d->u->userResourceList().end()) {
				if(d->u->isSecure((*it).name())) {
					const QPixmap &pix = IconsetFactory::iconPixmap("psi/cryptoYes");
					int y = (height - pix.height()) / 2;
					p->drawPixmap(x, y, pix);
					x += 24;
				}
			}
		}
	}
	else if ( type_ == Group || type_ == Profile ) {
		QPalette xcg = cg;

		if(type_ == Profile) {
			xcg.setColor(QPalette::Text, option.color[cProfileFore]);
			xcg.setColor(QPalette::Base, option.color[cProfileBack]);
		}
		else if(type_ == Group) {
			QFont f = p->font();
			f.setPointSize(option.smallFontSize);
			p->setFont(f);
			xcg.setColor(QPalette::Text, option.color[cGroupFore]);
			if (!option.clNewHeadings)
				xcg.setColor(QPalette::Base, option.color[cGroupBack]);
		}

		// Draw base background + icon + text using parent doPaint
		RichListViewItem::doPaint(p, xcg, selected, active, width, height);

		QFontMetrics fm(p->font());
		const QPixmap *pix = pixmap(0);
		int x = fontMetricsTextWidth(fm, text(0)) + (pix ? pix->width() : 0) + 8;

		if(type_ == Profile) {
			const QPixmap &px = d->ssl ? IconsetFactory::iconPixmap("psi/cryptoYes") : IconsetFactory::iconPixmap("psi/cryptoNo");
			int y = (height - px.height()) / 2;
			p->drawPixmap(x, y, px);
			x += 24;
		}

		if(selected)
			p->setPen(xcg.color(QPalette::HighlightedText));
		else
			p->setPen(xcg.color(QPalette::Text));

		QFont f_info = p->font();
		f_info.setPointSize(option.smallFontSize);
		p->setFont(f_info);
		QFontMetrics fm_info(p->font());
		int info_x = x;
		int info_y = ((height - fm_info.height()) / 2) + fm_info.ascent();
		p->drawText(info_x, info_y, d->groupInfo);

		if(type_ == Group && option.clNewHeadings && !selected) {
			x += fontMetricsTextWidth(fm, d->groupInfo) + 8;
			if(x < width - 8) {
				int h = (height / 2) - 1;
				p->setPen(QPen(option.color[cGroupBack]));
				p->drawLine(x, h, width - 8, h);
				h++;
				p->setPen(QPen(option.color[cGroupFore]));
				p->drawLine(x, h, width - 8, h);
			}
		}
		else {
			if (option.outlineHeadings) {
				p->setPen(QPen(option.color[cGroupFore]));
				p->drawRect(0, 0, width, height);
			}
		}
	}
}

/*
 * \brief "Opens" or "closes the ContactViewItem
 *
 * When the item is in "open" state, all it's children items are visible.
 *
 * \param o - if true, the item will be "open"
 */
void ContactViewItem::setOpen(bool o)
{
	// setExpanded fires itemExpanded/itemCollapsed → onExpanded/onCollapsed
	setExpanded(o);
}

void ContactViewItem::onExpanded()
{
	if (ContactView *cv = contactView())
		cv->recalculateSize();
	drawGroupIcon();
	UserAccount::GroupData gd = d->groupData();
	gd.open = true;
	d->groupState()->insert(d->getGroupName(), gd);
}

void ContactViewItem::onCollapsed()
{
	if (ContactView *cv = contactView())
		cv->recalculateSize();
	drawGroupIcon();
	UserAccount::GroupData gd = d->groupData();
	gd.open = false;
	d->groupState()->insert(d->getGroupName(), gd);
}

void ContactViewItem::insertChildItem(ContactViewItem *child)
{
	addChild(child);
	drawGroupIcon();
}

void ContactViewItem::removeChildItem(ContactViewItem *child)
{
	int idx = indexOfChild(child);
	if (idx >= 0) {
		takeChild(idx);
		drawGroupIcon();
	}
}

int ContactViewItem::rankGroup(int groupType) const
{
	static const int rankgroups[] = {
		gGeneral,
		gUser,
		gPrivate,
		gAgents,
		gNotInList,
	};
	static const int rankgroupsCount = sizeof(rankgroups) / sizeof(rankgroups[0]);

	for (int n = 0; n < rankgroupsCount; ++n) {
		if (rankgroups[n] == groupType)
			return n;
	}

	return rankgroupsCount - 1;
}

int ContactViewItem::compareTo(ContactViewItem *i) const
{
	int ret = 0;

	if(type_ == Contact) {
		// contacts always go before groups
		if(i->type() == Group)
			ret = -1;
		else {
			if ( option.rosterContactSortStyle == Options::ContactSortStyle_Status ) {
				ret = rankStatus(d->status) - rankStatus(i->status());
				if(ret == 0)
					ret = text(0).toLower().localeAwareCompare(i->text(0).toLower());
			}
			else { // ContactSortStyle_Alpha
				ret = text(0).toLower().localeAwareCompare(i->text(0).toLower());
			}
		}
	}
	else if(type_ == Group || type_ == Profile) {
		// contacts always go before groups
		if(i->type() == Contact)
			ret = 1;
		else if(i->type() == Group) {
			if ( option.rosterGroupSortStyle == Options::GroupSortStyle_Rank ) {
				int ourRank   = d->groupData().rank;
				int theirRank = i->d->groupData().rank;

				ret = ourRank - theirRank;
			}
			else { // GroupSortStyle_Alpha
				ret = rankGroup(d->groupType) - rankGroup(i->groupType());
				if(ret == 0)
					ret = text(0).toLower().localeAwareCompare(i->text(0).toLower());
			}
		}
		else if(i->type() == Profile) {
			if ( option.rosterAccountSortStyle == Options::AccountSortStyle_Rank ) {
				int ourRank = d->groupData().rank;
				int theirRank = i->d->groupData().rank;

				ret = ourRank - theirRank;
			}
			else // AccountSortStyle_Alpha
				ret = text(0).toLower().localeAwareCompare(i->text(0).toLower());
		}
	}

	return ret;
}

void ContactViewItem::setProfileName(const QString &name)
{
	d->profileName = name;
	if (!PsiOptions::instance()->getOption("options.ui.account.single").toBool())
		setText(0, d->profileName);
	else
		setText(0, "");
}

void ContactViewItem::setProfileState(int status)
{
	if ( status == -1 ) {
		setAlert( IconsetFactory::iconPtr("psi/connect") );
	}
	else {
		d->status = status;

		clearAlert();
		setIcon(PsiIconset::instance()->statusPtr(status));
	}
}

void ContactViewItem::repaintItem()
{
	if (QTreeWidget *tw = treeWidget()) {
		// indexFromItem is protected; use viewport update instead
		tw->viewport()->update();
	}
}

void ContactViewItem::setProfileSSL(bool on)
{
	d->ssl = on;
	repaintItem();
}

void ContactViewItem::setGroupName(const QString &name)
{
	d->groupName = name;
	resetGroupName();

	updatePosition();
}

void ContactViewItem::setGroupInfo(const QString &info)
{
	d->groupInfo = info;
	repaintItem();
}

void ContactViewItem::resetStatus()
{
	if ( !d->alerting && d->u ) {
		setIcon(PsiIconset::instance()->statusPtr(d->u));
	}

	// If the status is shown, update the text of the item too
	if (contactView()->isShowStatusMsg())
		resetName();
}

void ContactViewItem::resetName(bool forceNoStatusMsg)
{
	if ( d->u ) {
		QString s = JIDUtil::nickOrJid(d->u->name(), d->u->jid().full());
			
		if (d->status_single && !forceNoStatusMsg) {
			s = "<nobr>" + s + "</nobr>";
		}

		// Add the status message if wanted 
		if (!forceNoStatusMsg && contactView()->isShowStatusMsg()) {
			QString statusMsg;
			if (d->u->priority() != d->u->userResourceList().end()) 
				statusMsg = (*d->u->priority()).status().status();
			else 
				statusMsg = d->u->lastUnavailableStatus().status();

			if (d->status_single) {
				statusMsg.replace("<","&lt;");
				statusMsg.replace(">","&gt;");
				statusMsg = statusMsg.simplified();
				if (!statusMsg.isEmpty())
					s += "<br><font size=-1 color='" + option.color[cStatus].name() + "'><nobr>" + statusMsg + "</nobr></font>";
			}
			else {
				statusMsg.replace('\n'," ");
				if (!statusMsg.isEmpty())
					s += " (" + statusMsg + ")";
			}
		}

		if ( s != text(0) ) {
			setText(0, s);
		}
	}
}

void ContactViewItem::resetGroupName()
{
	if ( d->groupName != text(0) )
		setText(0, d->groupName);
}

void ContactViewItem::resetAnim()
{
	if ( d->alerting ) {
		// TODO: think of how to reset animation frame
	}
}

void ContactViewItem::setAlert(const PsiIcon *icon)
{
	bool reset = false;

	if ( !d->alerting ) {
		d->alerting = true;
		reset = true;
	}
	else {
		if ( d->lastIcon != icon )
			reset = true;
	}

	if ( reset )
		setIcon(icon, true);
}

void ContactViewItem::clearAlert()
{
	if ( d->alerting ) {
		d->alerting = false;
		resetStatus();
	}
}

void ContactViewItem::setIcon(const PsiIcon *icon, bool alert)
{
	if ( d->lastIcon == icon ) {
		return; // cause less flicker. but still have to run calltree valgring skin on psi while online (mblsha).
	}
	else
		d->lastIcon = (PsiIcon *)icon;

	if ( d->icon ) {
		disconnect(d->icon, &PsiIcon::pixmapChanged, this, &ContactViewItem::iconUpdated);
		d->icon->stop();

		delete d->icon;
		d->icon = 0;
	}

	QPixmap pix;
	if ( icon ) {
		if ( !alert )
			d->icon = new PsiIcon(*icon);
		else
			d->icon = new AlertIcon(icon);

		if (!PsiOptions::instance()->getOption("options.ui.contactlist.temp-no-roster-animation").toBool()) {
			connect(d->icon, &PsiIcon::pixmapChanged, this, &ContactViewItem::iconUpdated);
		}
		d->icon->activated();

		pix = d->icon->pixmap();
	}

	setPixmap(0, pix);
}

void ContactViewItem::iconUpdated()
{
	setPixmap(0, d->icon ? d->icon->pixmap() : QPixmap());
}

void ContactViewItem::animateNick()
{
	d->animateNickColor = !d->animateNickColor;
	repaintItem();

	if(++d->animateNickX >= 16)
		stopAnimateNick();
}

void ContactViewItem::stopAnimateNick()
{
	if ( !d->animatingNick )
		return;

	disconnect(contactView()->animTimer(), &QTimer::timeout, this, &ContactViewItem::animateNick);

	d->animatingNick = false;
	repaintItem();
}

void ContactViewItem::setAnimateNick()
{
	stopAnimateNick();

	connect(contactView()->animTimer(), &QTimer::timeout, this, &ContactViewItem::animateNick);

	d->animatingNick = true;
	d->animateNickX = 0;
	animateNick();
}

// Helper: move this item to come right after 'after' in the same parent
static void moveItemAfter(QTreeWidgetItem *item, QTreeWidgetItem *after)
{
	QTreeWidgetItem *par = item->parent();
	if (par) {
		int myIdx = par->indexOfChild(item);
		par->takeChild(myIdx);
		int afterIdx = par->indexOfChild(after);
		par->insertChild(afterIdx + 1, item);
	} else {
		QTreeWidget *tw = item->treeWidget();
		if (!tw) return;
		int myIdx = tw->indexOfTopLevelItem(item);
		tw->takeTopLevelItem(myIdx);
		int afterIdx = tw->indexOfTopLevelItem(after);
		tw->insertTopLevelItem(afterIdx + 1, item);
	}
}

void ContactViewItem::updatePosition()
{
	ContactViewItem *par = parentItem();
	if(!par)
		return;

	ContactViewItem *after = 0;
	for (ContactViewItem *item = par->firstChildItem(); item; item = item->nextSiblingItem()) {
		if(item == this)
			continue;
		int x = compareTo(item);
		if(x == 0)
			continue;
		if(x < 0)
			break;
		after = item;
	}

	if(after) {
		moveItemAfter(this, after);
	} else if (ContactViewItem *first = par->firstChildItem()) {
		if (first != this) {
			moveItemAfter(this, first);
			moveItemAfter(first, this);
		}
	}
}

void ContactViewItem::optionsUpdate()
{
	if(type_ == Group || type_ == Profile) {
		drawGroupIcon();
	}
	else if(type_ == Contact) {
		if(!d->alerting)
			resetStatus();
		else
			resetAnim();
	}
}

void ContactViewItem::setContact(UserListItem *u)
{
	int oldStatus = d->status;
	QString oldName = text(0);
	bool wasAgent = d->isAgent;

	//QString newName = JIDUtil::nickOrJid(u->name(),u->jid().full());

	d->u = u;
	cacheValues();

	bool needUpdate = false;
	if(d->status != oldStatus || d->isAgent != wasAgent || !u->presenceError().isEmpty()) {
		resetStatus();
		needUpdate = true;
	}

	// Hack, but that's the safest way.
	resetName();
	QString newName = text(0);
	if(newName != oldName) {
		needUpdate = true;
	}

	if(needUpdate)
		updatePosition();

	repaintItem();
	setup();
}

bool ContactViewItem::canAcceptDrop() const
{
	return canAcceptDropTarget();
}

bool ContactViewItem::canAcceptDropTarget() const
{
	if (type_ == Profile)
		return false;

	ContactViewItem *group = dropTargetGroup();
	if(!group)
		return false;

	if(type_ == Contact && d->u && d->u->isSelf())
		return false;

	return group->groupType() == gGeneral || group->groupType() == gUser;
}

// dragEntered/dragLeft/dropped removed (now handled in ContactView::dropEvent)
// cancelRename removed (scRename uses QInputDialog now)

int ContactViewItem::rtti() const
{
	return 5103;
}

#include "contactview.moc"
