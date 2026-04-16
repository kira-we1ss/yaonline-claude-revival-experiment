/*
 * contactview.h - contact list widget
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

#ifndef CONTACTVIEW_H
#define CONTACTVIEW_H

#include <QObject>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QList>
#include <QPoint>
#include <QSize>
#include <QStringList>

#include "xmpp_clientstream.h"

class IconAction;
class UserListItem;
class ContactView;
class ContactViewItem;
class PsiAccount;
class PsiIcon;
class QDropEvent;
class QKeyEvent;
class QPainter;
class QPalette;
class QPixmap;
class QStyleOptionViewItem;
class QTimer;
class QEvent;
namespace XMPP {
	class Status;
}

using namespace XMPP;

class ContactProfile;

// ContactProfile: this holds/manipulates a roster profile for an account
class ContactProfile : public QObject
{
	Q_OBJECT
public:
	ContactProfile(PsiAccount *, const QString &name, ContactView *, bool unique=false);
	~ContactProfile();

	void setEnabled(bool e=true);

	const QString & name() const;
	void setName(const QString &name);
	void setState(int);
	void setUsingSSL(bool);
	ContactView *contactView() const;
	ContactViewItem *self() const;

	PsiAccount *psiAccount() const;

	void updateEntry(const UserListItem &);
	void removeEntry(const Jid &);
	void updateSelf();
	void addSelf();
	void removeSelf();
	void setAlert(const Jid &, const PsiIcon *);
	void clearAlert(const Jid &);
	void animateNick(const Jid &);

	void addAllNeededContactItems();
	void removeAllUnneededContactItems();
	void resetAllContactItemNames();

	void ensureVisible(const Jid &);
	void clear();

	QString makeTip(bool trim, bool doLinkify) const;

	ContactViewItem *checkGroup(int type);
	ContactViewItem *checkGroup(const QString &name);
	ContactViewItem *firstGroupItem() const;

	void setName(const char *);


signals:
	void actionDefault(const Jid &);
	void actionRecvEvent(const Jid &);
	void actionSendMessage(const Jid &);
	void actionSendMessage(const QList<XMPP::Jid>&);
	void actionSendUrl(const Jid &);
	void actionRemove(const Jid &);
	void actionRename(const Jid &, const QString &);
	void actionGroupRename(const QString &, const QString &);
	void actionHistory(const Jid &);
	void actionOpenChat(const Jid &);
	void actionOpenChatSpecific(const Jid &);
#ifdef WHITEBOARDING
	void actionOpenWhiteboard(const Jid &);
	void actionOpenWhiteboardSpecific(const Jid &);
#endif
	void actionAgentSetStatus(const Jid &, Status &);
	void actionInfo(const Jid &);
	void actionAuth(const Jid &);
	void actionAuthRequest(const Jid &);
	void actionAuthRemove(const Jid &);
	void actionAdd(const Jid &);
	void actionGroupAdd(const Jid &, const QString &);
	void actionGroupRemove(const Jid &, const QString &);
	void actionSendFile(const Jid &);
	void actionSendFiles(const Jid &, const QStringList &);
	void actionVoice(const Jid &);
	void actionExecuteCommand(const Jid &, const QString&);
	void actionExecuteCommandSpecific(const Jid &, const QString&);
	void actionDisco(const Jid &, const QString &);
	void actionInvite(const Jid &, const QString &);
	void actionAssignKey(const Jid &);
	void actionUnassignKey(const Jid &);
	void actionSetMood();
	void actionSetAvatar();
	void actionUnsetAvatar();

private slots:
	void updateGroups();

public:
	class Entry;
	class Private;
private:
	Private *d;

	ContactViewItem *addGroup(int type);
	ContactViewItem *addGroup(const QString &name);
	ContactViewItem *ensureGroup(int type);
	ContactViewItem *ensureGroup(const QString &name);
	void checkDestroyGroup(ContactViewItem *group);
	void checkDestroyGroup(const QString &group);
	ContactViewItem *addContactItem(Entry *e, ContactViewItem *group_item);
	ContactViewItem *ensureContactItem(Entry *e, ContactViewItem *group_item);
	void removeContactItem(Entry *e, ContactViewItem *i);
	void addNeededContactItems(Entry *e);
	void removeUnneededContactItems(Entry *e);
	void clearContactItems(Entry *e);

	void removeEntry(Entry *);
	Entry *findEntry(const Jid &) const;
	Entry *findEntry(ContactViewItem *) const;

	void ensureVisible(Entry *);

	// useful functions to grab groups of users
	QList<XMPP::Jid> contactListFromCVGroup(ContactViewItem *) const;
	int contactSizeFromCVGroup(ContactViewItem *) const;
	int contactsOnlineFromCVGroup(ContactViewItem *) const;
	QList<XMPP::Jid> contactListFromGroup(const QString &groupName) const;
	int contactSizeFromGroup(const QString &groupName) const;

	void updateGroupInfo(ContactViewItem *group);
	QStringList groupList() const;

	void deferredUpdateGroups();

	friend class ContactView;
	void scActionDefault(ContactViewItem *);
	void scRecvEvent(ContactViewItem *);
	void scSendMessage(ContactViewItem *);
	void scRename(ContactViewItem *);
	void scVCard(ContactViewItem *);
	void scHistory(ContactViewItem *);
	void scOpenChat(ContactViewItem *);
#ifdef WHITEBOARDING
	void scOpenWhiteboard(ContactViewItem *);
#endif
	void scAgentSetStatus(ContactViewItem *, Status &);
	void scRemove(ContactViewItem *);
	void doItemRenamed(ContactViewItem *, const QString &);
	void doContextMenu(ContactViewItem *, const QPoint &);

	friend class ContactViewItem;
	void dragDrop(const QString &, ContactViewItem *);
	void dragDropFiles(const QStringList &, ContactViewItem *);
};

// ContactView: the actual widget
class ContactView : public QTreeWidget
{
	Q_OBJECT
public:
	ContactView(QWidget *parent=nullptr, const char *name=nullptr);
	~ContactView();

	bool isShowOffline() const { return v_showOffline; }
	ContactViewItem *itemAtPosition(const QPoint &pos) const;
	void setCurrentContactItem(ContactViewItem *item);
	void setContactItemOpen(ContactViewItem *item, bool open);
	void ensureContactItemVisible(ContactViewItem *item);
	int itemVerticalPosition(ContactViewItem *item) const;
	bool isShowAgents() const { return v_showAgents; }
	bool isShowAway() const { return v_showAway; }
	bool isShowHidden() const { return v_showHidden; }
	bool isShowSelf() const { return v_showSelf; }
	bool isShowStatusMsg() const { return v_showStatusMsg; }

	bool filterContact(ContactViewItem *item, bool refineSearch = false);
	bool filterGroup(ContactViewItem *item, bool refineSearch = false);
	void setFilter(const QString &text);
	void clearFilter();
	
	void clear();
	void resetAnim();
	QTimer *animTimer() const;
	ContactViewItem *selectedContactViewItem() const;
	ContactViewItem *firstTopLevelItem() const;

	IconAction *qa_send, *qa_chat, *qa_ren, *qa_hist, *qa_logon, *qa_recv, *qa_rem, *qa_vcard;
	IconAction *qa_assignAvatar, *qa_clearAvatar;
#ifdef WHITEBOARDING
	IconAction *qa_wb;
#endif

	QSize minimumSizeHint() const;
	QSize sizeHint() const;

protected:
	void setShortcuts();
	// reimplemented
	void keyPressEvent(QKeyEvent *);
	bool eventFilter( QObject *, QEvent * );
	void mousePressEvent(QMouseEvent *e);
	void mouseMoveEvent(QMouseEvent *e);
	void startDrag(Qt::DropActions supportedActions);
	void contextMenuEvent(QContextMenuEvent *e);
	void dragEnterEvent(QDragEnterEvent *e);
	void dragMoveEvent(QDragMoveEvent *e);
	void dropEvent(QDropEvent *e);

signals:
	void showOffline(bool);
	void showAway(bool);
	void showHidden(bool);
	void showAgents(bool);
	void showSelf(bool);
	void showStatusMsg(bool);
	void searchInput(const QString&);

public slots:
	void setShowOffline(bool);
	void setShowAgents(bool);
	void setShowAway(bool);
	void setShowHidden(bool);
	void setShowSelf(bool);
	void setShowStatusMsg(bool);
	void optionsUpdate();
	void recalculateSize();

private slots:
	void qlv_itemDoubleClicked(QTreeWidgetItem *, int);
	void leftClickTimeOut();

	void doRecvEvent();
	void doRename();
	void doEnter();
	void doContext();
	void doSendMessage();
	void doOpenChat();
#ifdef WHITEBOARDING
	void doOpenWhiteboard();
#endif
	void doHistory();
	void doVCard();
	void doLogon();
	void doRemove();

	void doAssignAvatar();
	void doClearAvatar();

	void onItemExpanded(QTreeWidgetItem *item);
	void onItemCollapsed(QTreeWidgetItem *item);

public:
	class Private;
	friend class Private;
private:
	Private *d;

	QPoint mousePressPos;
	bool v_showOffline, v_showAgents, v_showAway, v_showHidden, v_showSelf, v_showStatusMsg;
	bool lcto_active;
	QPoint lcto_pos;
	QTreeWidgetItem *lcto_item;
	QSize lastSize;
	QString filterString_;

	friend class ContactProfile;
	void link(ContactProfile *);
	void unlink(ContactProfile *);
	bool allowResize() const;

	static ContactViewItem *toContactViewItem(QTreeWidgetItem *item);

	// internal click handling
	void qlv_contextPopup(ContactViewItem *i, const QPoint &pos);
	void qlv_singleclick(Qt::MouseButton button, ContactViewItem *item, const QPoint &pos);
	void qlv_doubleclick(ContactViewItem *item);
	void qlv_itemRenamed(ContactViewItem *i, const QString &text);
};



//------------------------------------------------------------------------------
// RichListViewItem: A RichText listview item
//------------------------------------------------------------------------------

class QTextDocument;
class RichListViewItem : public QTreeWidgetItem
{
public:
	RichListViewItem( QTreeWidget * parent );
	RichListViewItem( QTreeWidgetItem * parent );
	virtual void setText(int column, const QString& text);
	virtual void setup();
	virtual ~RichListViewItem();
	int widthUsed() const;

	// Qt5 compat: replicate Q3ListViewItem pixmap API with stored pixmap
	void setPixmap(int column, const QPixmap &px);
	const QPixmap *pixmap(int column) const;

	// Custom height management (used by delegate for sizeHint)
	void setItemHeight(int h);
	int itemHeight() const;

	// Paint helper called by ContactViewDelegate
	virtual void doPaint(QPainter *p, const QPalette &pal, bool selected, bool active,
	                     int width, int height);

	QTreeWidget *contactListView() const;
	int depth() const; // walk up parent chain
	int contentLeftOffset(int column) const;
	int availableTextWidth(int column) const;

protected:
	int v_widthUsed;
	bool v_selected, v_active;
	bool v_rich;
	QTextDocument* v_rt;
	QPixmap v_pixmap;   // stored pixmap for column 0
	int v_height;       // stored item height (0 = default)
};

// ContactViewItem: an entry in the ContactView (profile, group, or contact)
class ContactViewItem : public QObject, public RichListViewItem
{
	Q_OBJECT
public:
	enum { Profile, Group, Contact };
	enum { gGeneral, gNotInList, gAgents, gPrivate, gUser };

	ContactViewItem(const QString &profileName, ContactProfile *, ContactView *parent);
	ContactViewItem(const QString &groupName, int groupType, ContactProfile *, ContactView *parent);
	ContactViewItem(const QString &groupName, int groupType, ContactProfile *, ContactViewItem *parent);
	ContactViewItem(UserListItem *, ContactProfile *, ContactViewItem *parent);
	~ContactViewItem();

	ContactProfile *contactProfile() const;
	int type() const;
	int groupType() const;
	const QString & groupName() const;
	const QString & groupInfo() const;
	UserListItem *u() const;
	int status() const;
	bool isAgent() const;
	bool isAlerting() const;
	bool isAnimatingNick() const;
	int parentGroupType() const; // use with contacts: returns grouptype of parent group
	ContactView *contactView() const;
	ContactViewItem *parentItem() const;
	ContactViewItem *firstChildItem() const;
	ContactViewItem *nextSiblingItem() const;
	bool hasChildrenItems() const;
	bool hasClosedAncestors() const;
	ContactViewItem *dropTargetGroup() const;
	QString toolTipText() const;

	void setContact(UserListItem *);
	void setProfileName(const QString &);
	void setProfileState(int);
	void setProfileSSL(bool);
	void setGroupName(const QString &);
	void setGroupInfo(const QString &);
	void setAnimateNick();
	void setAlert(const PsiIcon *);
	void clearAlert();
	void setIcon(const PsiIcon *, bool alert = false);

	void resetStatus();
	void resetName(bool forceNoStatusMsg = false);
	void resetGroupName();

	void updatePosition();
	void optionsUpdate();
	void repaintItem();

	// Paint helper called by ContactViewDelegate
	void doPaint(QPainter *p, const QPalette &pal, bool selected, bool active,
	             int width, int height) override;

	// reimplemented functions
	int rtti() const;
	void setOpen(bool o);  // saves state, calls setExpanded
	int compareTo(ContactViewItem *other) const;

	// For tree manipulation with icon updates
	void insertChildItem(ContactViewItem *child);
	void removeChildItem(ContactViewItem *child);

	bool canAcceptDrop() const;

public slots:
	void resetAnim();
	void iconUpdated();
	void animateNick();
	void stopAnimateNick();

	// Called from ContactView on expand/collapse events
	void onExpanded();
	void onCollapsed();

private:
	int type_;
	class Private;
	Private *d;

	bool canAcceptDropTarget() const;
	void cacheValues();
	int rankGroup(int groupType) const;
	void drawGroupIcon();
};

#endif
