#include "tabmanager.h"

#include <QtAlgorithms>

#include "tabdlg.h"
#include "tabbablewidget.h"
#ifdef GROUPCHAT
#include "groupchatdlg.h"
#endif
#include "chatdlg.h"

TabManager::TabManager(PsiCon* psiCon, QObject *parent)
	: QObject(parent)
	, psiCon_(psiCon)
{
}

TabManager::~TabManager()
{
	deleteAll();
}

PsiCon* TabManager::psiCon() const
{
	return psiCon_;
}

TabDlg* TabManager::getTabs()
{
	if (!tabs_.isEmpty()) {
		return tabs_.first();
	}
	else {
		return newTabs();
	}
}

bool TabManager::shouldBeTabbed(QWidget *widget)
{
	if (!option.useTabs) {
		return false;
	}
	if (qobject_cast<ChatDlg*> (widget)) {
		return true;
	}
#ifdef GROUPCHAT
	if (qobject_cast<GCMainDlg*> (widget)) {
		return true;
	}
#endif
	qDebug("Checking if widget should be tabbed: Unknown type");
	return false;
}

TabDlg* TabManager::newTabs()
{
	TabDlg *tab = new TabDlg(this);
#ifdef YAPSI
	tab->init();
#endif
	tabs_.append(tab);
	connect(tab, SIGNAL(destroyed(QObject*)), SLOT(tabDestroyed(QObject*)));
	connect(tab, SIGNAL(openedChatsChanged()), SIGNAL(openedChatsChanged()));
	connect(psiCon_, SIGNAL(emitOptionsUpdate()), tab, SLOT(optionsUpdate()));
	return tab;
}

void TabManager::tabDestroyed(QObject* obj)
{
	// Qt5 regression: qobject_cast fails during QObject::~QObject() because
	// the vtable has already been reset from TabDlg* to QObject* by the time
	// destroyed() is emitted. qobject_cast returns null, Q_ASSERT is a no-op
	// in release, removeAll(nullptr) is a no-op → TabDlg stays in the list as
	// a dangling pointer → crash when managesTab() dereferences freed memory.
	// Fix: static_cast is safe because this slot is only connected to TabDlg
	// objects (see newTabs() — the only place this connection is made).
	TabDlg *tabDlg = static_cast<TabDlg*>(obj);
	tabs_.removeAll(tabDlg);
}

bool TabManager::isChatTabbed(const TabbableWidget* chat) const
{
	foreach(TabDlg* tabDlg, tabs_) {
		if (tabDlg->managesTab(chat)) {
			return true;
		}
	}
	return false;
}

TabDlg* TabManager::getManagingTabs(const TabbableWidget* chat) const
{
	//FIXME: this looks like it could be broken to me (KIS)
	//Does this mean that opening two chats to the same jid will go wrong?
	foreach(TabDlg* tabDlg, tabs_) {
		if (tabDlg->managesTab(chat)) {
			return tabDlg;
		}
	}
	return nullptr;
}

const QList<TabDlg*>& TabManager::tabSets()
{
	return tabs_;
}

void TabManager::deleteAll()
{
	qDeleteAll(tabs_);
	tabs_.clear();
}
