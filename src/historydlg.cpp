/*
 * historydlg.cpp - a dialog to show event history
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

#include "historydlg.h"

#include <QMenu>
#include <QHeaderView>
#include <QLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QFrame>
#include <QApplication>
#include <QClipboard>
#include <QTextStream>
#include <QCloseEvent>
#include <QKeyEvent>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QResizeEvent>
#include "psiaccount.h"
#include "psievent.h"
#include "busywidget.h"
#include "applicationinfo.h"
#include "common.h"
#include "eventdb.h"
#include "psiiconset.h"
#include "textutil.h"
#include "jidutil.h"
#include "userlist.h"

static QString getNext(QString *str)
{
	int n = 0;
	// skip leading spaces (but *do* return them later!)
	while(n < (int)str->length() && str->at(n).isSpace()) {
		++n;
	}
	if(n == (int)str->length()) {
		return QString();
	}
	// find end or next space
	while(n < (int)str->length() && !str->at(n).isSpace()) {
		++n;
	}
	QString result = str->mid(0, n);
	*str = str->mid(n);
	return result;
}

// wraps a string against a fixed width
static QStringList wrapString(const QString &str, int wid)
{
	QStringList lines;
	QString cur;
	QString tmp = str;
	//printf("parsing: [%s]\n", tmp.toLatin1().constData());
	while(1) {
		QString word = getNext(&tmp);
		if(word == QString()) {
			lines += cur;
			break;
		}
		//printf("word:[%s]\n", word.toLatin1().constData());
		if(!cur.isEmpty()) {
			if((int)cur.length() + (int)word.length() > wid) {
				lines += cur;
				cur = "";
			}
		}
		if(cur.isEmpty()) {
			// trim the whitespace in front
			for(int n = 0; n < (int)word.length(); ++n) {
				if(!word.at(n).isSpace()) {
					if(n > 0) {
						word = word.mid(n);
					}
					break;
				}
			}
		}
		cur += word;
	}
	return lines;
}

//----------------------------------------------------------------------------
// HistoryView
//----------------------------------------------------------------------------
class HistoryDlg::Private
{
public:
	Private() {}

	Jid jid;
	PsiAccount *pa;
	HistoryView *lv;
	//BusyWidget *busy;
	QPushButton *pb_prev, *pb_next, *pb_refresh, *pb_find;
	QLineEdit *le_find;

	QString id_prev, id_begin, id_end, id_next;
	int reqtype;
	QString findStr;

	EDBHandle *h, *exp;
};

HistoryDlg::HistoryDlg(const Jid &jid, PsiAccount *pa)
	: QWidget(0, 0)
{
	setAttribute(Qt::WA_DeleteOnClose);
  	if ( option.brushedMetal )
		setAttribute(Qt::WA_MacMetalStyle);
	d = new Private;
	d->pa = pa;
	d->jid = jid;
	d->pa->dialogRegister(this, d->jid);
	d->exp = 0;

	setWindowTitle(d->jid.full());
#ifndef Q_OS_MAC
	setWindowIcon(IconsetFactory::icon("psi/history").icon());
#endif

	d->h = new EDBHandle(d->pa->edb());
	connect(d->h, SIGNAL(finished()), SLOT(edb_finished()));

	QVBoxLayout *vb1 = new QVBoxLayout(this, 8);
	d->lv = new HistoryView(this);
	d->lv->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	connect(d->lv, SIGNAL(aOpenEvent(PsiEvent *)), SLOT(actionOpenEvent(PsiEvent *)));
	QSizePolicy sp = d->lv->sizePolicy();
	sp.setVerStretch(1);
	d->lv->setSizePolicy(sp);
	vb1->addWidget(d->lv);

	QHBoxLayout *hb1 = new QHBoxLayout(vb1);

	QVBoxLayout *vb2 = new QVBoxLayout(hb1);
	QHBoxLayout *hb2 = new QHBoxLayout(vb2);

	//d->busy = new BusyWidget(this);
	//hb1->addWidget(d->busy);

	d->pb_refresh = new QPushButton(tr("&Latest"), this);
	d->pb_refresh->setMinimumWidth(80);
	connect(d->pb_refresh, SIGNAL(clicked()), SLOT(doLatest()));
	hb2->addWidget(d->pb_refresh);

	d->pb_prev = new QPushButton(tr("&Previous"), this);
	d->pb_prev->setMinimumWidth(80);
	connect(d->pb_prev, SIGNAL(clicked()), SLOT(doPrev()));
	hb2->addWidget(d->pb_prev);

	d->pb_next = new QPushButton(tr("&Next"), this);
	d->pb_next->setMinimumWidth(80);
	connect(d->pb_next, SIGNAL(clicked()), SLOT(doNext()));
	hb2->addWidget(d->pb_next);

	QHBoxLayout *hb3 = new QHBoxLayout(vb2);

	d->le_find = new QLineEdit(this);
	connect(d->le_find, SIGNAL(textChanged(const QString &)), SLOT(le_textChanged(const QString &)));
	connect(d->le_find, SIGNAL(returnPressed()), SLOT(doFind()));
	hb3->addWidget(d->le_find);
	d->pb_find = new QPushButton(tr("Find"), this);
	connect(d->pb_find, SIGNAL(clicked()), SLOT(doFind()));
	d->pb_find->setEnabled(false);
	hb3->addWidget(d->pb_find);

	QFrame *sep;
	sep = new QFrame(this);
	sep->setFrameShape(QFrame::VLine);
	hb1->addWidget(sep);

	QVBoxLayout *vb3 = new QVBoxLayout(hb1);
	QPushButton *pb_save = new QPushButton(tr("&Export..."), this);
	connect(pb_save, SIGNAL(clicked()), SLOT(doSave()));
	vb3->addWidget(pb_save);
	QPushButton *pb_erase = new QPushButton(tr("Er&ase All"), this);
	connect(pb_erase, SIGNAL(clicked()), SLOT(doErase()));
	vb3->addWidget(pb_erase);

	sep = new QFrame(this);
	sep->setFrameShape(QFrame::VLine);
	hb1->addWidget(sep);

	hb1->addStretch(1);

	QVBoxLayout *vb4 = new QVBoxLayout(hb1);
	vb4->addStretch(1);

	QPushButton *pb_close = new QPushButton(tr("&Close"), this);
	pb_close->setMinimumWidth(80);
	connect(pb_close, SIGNAL(clicked()), SLOT(close()));
	vb4->addWidget(pb_close);

	resize(520,320);

	X11WM_CLASS("history");

	d->le_find->setFocus();

	setButtons();
	doLatest();
}

HistoryDlg::~HistoryDlg()
{
	delete d->exp;
	d->pa->dialogUnregister(this);
	delete d;
}

void HistoryDlg::keyPressEvent(QKeyEvent *e)
{
	if(e->key() == Qt::Key_Escape)
		close();
	else {
		e->ignore();
	}
}

void HistoryDlg::closeEvent(QCloseEvent *e)
{
	if(d->exp)
		return;

	e->accept();
}

void HistoryDlg::show()
{
	QWidget::show();
	d->lv->doResize();
}

void HistoryDlg::le_textChanged(const QString &s)
{
	if(s.isEmpty() && d->pb_find->isEnabled())
		d->pb_find->setEnabled(false);
	else if(!s.isEmpty() && !d->pb_find->isEnabled())
		d->pb_find->setEnabled(true);
}

void HistoryDlg::setButtons()
{
	d->pb_prev->setEnabled(!d->id_prev.isEmpty());
	d->pb_next->setEnabled(!d->id_next.isEmpty());
}

void HistoryDlg::doLatest()
{
	loadPage(0);
}

void HistoryDlg::doPrev()
{
	loadPage(1);
}

void HistoryDlg::doNext()
{
	loadPage(2);
}

void HistoryDlg::doSave()
{
	if(option.lastSavePath.isEmpty()) {
		option.lastSavePath = QDir::homeDirPath();
	}

	UserListItem *u = d->pa->findFirstRelevant(d->jid);
	QString them = JIDUtil::nickOrJid(u->name(), u->jid().full());
	QString s = JIDUtil::encode(them).toLower();

	QString str = option.lastSavePath + "/" + s + ".txt";
	str = QFileDialog::getSaveFileName(this, tr("Export message history"), str, tr("Text files (*.txt);;All files (*.*)"));
	if(!str.isEmpty()) {
		QFileInfo fi(str);
		option.lastSavePath = fi.dirPath();
		exportHistory(str);
	}
}

void HistoryDlg::doErase()
{
	int x = QMessageBox::information(this, tr("Confirm erase all"), tr("This will erase all message history for this contact!\nAre you sure you want to do this?"), tr("&Yes"), tr("&No"), QString(), 1);
	if (x == 0) {
		d->h->erase(d->pa, d->jid);
	}
}

void HistoryDlg::loadPage(int type)
{
	d->reqtype = type;
	if(type == 0) {
		d->pb_refresh->setEnabled(false);
		d->h->getLatest(d->pa, d->jid, 50);
		//printf("EDB: requesting latest 50 events\n");
	}
	else if(type == 1) {
		d->pb_prev->setEnabled(false);
		d->h->get(d->pa, d->jid, d->id_prev, EDB::Backward, 50);
		//printf("EDB: requesting 50 events backward, starting at %s\n", d->id_prev.toLatin1().constData());
	}
	else if(type == 2) {
		d->pb_next->setEnabled(false);
		d->h->get(d->pa, d->jid, d->id_next, EDB::Forward, 50);
		//printf("EDB: requesting 50 events forward, starting at %s\n", d->id_next.toLatin1().constData());
	}

	//d->busy->start();
}

void HistoryDlg::displayResult(const EDBResult *r, int direction, int max)
{
	//d->lv->setUpdatesEnabled(false);
	d->lv->clear();
	int at = 0;
	int startIdx = (direction == EDB::Forward) ? r->count() - 1 : 0;
	int step = (direction == EDB::Forward) ? -1 : 1;
	for(int idx = startIdx; idx >= 0 && idx < r->count() && (max == -1 ? true : at < max); idx += step) {
		const EDBItem *i = &r->at(idx);
		PsiEvent *e = i->event();
		d->lv->addEvent(e, i->prevId());
		++at;
	}
	//d->lv->setUpdatesEnabled(true);
	//d->lv->repaint(true);
}

void HistoryDlg::edb_finished()
{
	const EDBResult *r = d->h->result();
	if(d->h->lastRequestType() == EDBHandle::Read && r) {
		//printf("EDB: retrieved %d events:\n", r->count());
		if(r->count() > 0) {
			if(d->reqtype == 0 || d->reqtype == 1) {
				// events are in backward order
				// first entry is the end event
				d->id_end = r->first().id();
				d->id_next = r->first().nextId();
				// last entry is the begin event
				d->id_begin = r->last().id();
				d->id_prev = r->last().prevId();
				displayResult(r, EDB::Backward);
				//printf("[%s],[%s],[%s],[%s]\n", d->id_prev.toLatin1().constData(), d->id_begin.toLatin1().constData(), d->id_end.toLatin1().constData(), d->id_next.toLatin1().constData());
			}
			else if(d->reqtype == 2) {
				// events are in forward order
				// last entry is the end event
				d->id_end = r->last().id();
				d->id_next = r->last().nextId();
				// first entry is the begin event
				d->id_begin = r->first().id();
				d->id_prev = r->first().prevId();
				displayResult(r, EDB::Forward);
			}
			else if(d->reqtype == 3) {
				// should only be one entry
				const EDBItem &ei = r->first();
				d->reqtype = 1;
				d->h->get(d->pa, d->jid, ei.id(), EDB::Backward, 50);
				//printf("EDB: requesting 50 events backward, starting at %s\n", d->id_prev.toLatin1().constData());
				return;
			}
		}
		else {
			if(d->reqtype == 3) {
				QMessageBox::information(this, tr("Find"), tr("Search string '%1' not found.").arg(d->findStr));
				return;
			}
		}
	}
	else if (d->h->lastRequestType() == EDBHandle::Erase) {
		if (d->h->writeSuccess()) {
			d->lv->clear();
			d->id_prev = "";
			d->id_begin = "";
			d->id_end = "";
			d->id_next = "";
		}
		else {
			QMessageBox::critical(this, tr("Error"), tr("Unable to delete history file."));
		}
	}
	else {
		//printf("EDB: error\n");
	}

	if(d->lv->topLevelItemCount() > 0)
		d->lv->setCurrentItem(d->lv->topLevelItem(0));

	//d->busy->stop();
	d->pb_refresh->setEnabled(true);
	setButtons();
}

void HistoryDlg::actionOpenEvent(PsiEvent *e)
{
	openEvent(e);
}

void HistoryDlg::doFind()
{
	QString str = d->le_find->text();
	if(str.isEmpty())
		return;

	if(d->lv->topLevelItemCount() < 1)
		return;

	HistoryViewItem *i = (HistoryViewItem *)d->lv->currentItem();
	if(!i)
		i = (HistoryViewItem *)d->lv->topLevelItem(0);
	QString id = i->eventId;
	if(id.isEmpty()) {
		QMessageBox::information(this, tr("Find"), tr("Already at beginning of message history."));
		return;
	}

	//printf("searching for: [%s], starting at id=[%s]\n", str.toLatin1().constData(), id.toLatin1().constData());
	d->reqtype = 3;
	d->findStr = str;
	d->h->find(d->pa, str, d->jid, id, EDB::Backward);
}

void HistoryDlg::exportHistory(const QString &fname)
{
	QFile f(fname);
	if(!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
		QMessageBox::information(this, tr("Error"), tr("Error writing to file."));
		return;
	}
	QTextStream stream(&f);

	QString us = d->pa->nick();
	UserListItem *u = d->pa->findFirstRelevant(d->jid);
	QString them = JIDUtil::nickOrJid(u->name(), u->jid().full());

	d->exp = new EDBHandle(d->pa->edb());
	QString id;
	while(1) {
		if(id.isEmpty()) {
			d->exp->getOldest(d->pa, d->jid, 1000);
		} else {
			d->exp->get(d->pa, d->jid, id, EDB::Forward, 1000);
		}
		while(d->exp->busy()) {
			qApp->processEvents();
		}

		const EDBResult *r = d->exp->result();
		if(!r) {
			break;
		}
		if(r->count() <= 0) {
			break;
		}

		// events are in forward order
		for (int idx = 0; idx < r->count(); ++idx) {
			const EDBItem *i = &r->at(idx);
			id = i->nextId();
			PsiEvent *e = i->event();
			QString txt;

			QDateTime dt = e->timeStamp();
			QString ts;
			//ts.sprintf("%04d/%02d/%02d %02d:%02d:%02d", dt.date().year(), dt.date().month(), dt.date().day(), dt.time().hour(), dt.time().minute(), dt.time().second());
			ts = dt.toString(Qt::LocalDate);

			QString nick;
			if(e->originLocal()) {
				nick = us;
			} else {
				nick = them;
			}

			QString heading = QString("(%1) ").arg(ts) + nick + ": ";
			if(e->type() == PsiEvent::Message) {
				MessageEvent *me = (MessageEvent *)e;
				stream << heading << endl;

				QStringList lines = me->message().body().split('\n', QString::KeepEmptyParts);
				for(QStringList::ConstIterator lit = lines.begin(); lit != lines.end(); ++lit) {
					QStringList sub = wrapString(*lit, 72);
					for(QStringList::ConstIterator lit2 = sub.begin(); lit2 != sub.end(); ++lit2) {
						txt += QString("    ") + *lit2 + '\n';
					}
				}
			}
			else {
				continue;
			}

			stream << txt << endl;
		}

		// done!
		if(id.isEmpty()) {
			break;
		}
	}
	delete d->exp;
	d->exp = 0;
	f.close();
}

//----------------------------------------------------------------------------
// HistoryView
//----------------------------------------------------------------------------
HistoryView::HistoryView(QWidget *parent)
: QTreeWidget(parent)
{
	at_id = 0;
	connect(this, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)), SLOT(qlv_doubleclick(QTreeWidgetItem*,int)));
	setContextMenuPolicy(Qt::CustomContextMenu);
	connect(this, SIGNAL(customContextMenuRequested(const QPoint&)), SLOT(qlv_contextPopup(const QPoint&)));

	setAllColumnsShowFocus(true);
	setColumnCount(4);
	setHeaderLabels(QStringList() << tr("Type") << tr("Origin") << tr("Date") << tr("Text"));
	setSortingEnabled(true);
	sortByColumn(2, Qt::AscendingOrder);
	header()->setStretchLastSection(true);
	header()->setSectionsClickable(false);
	header()->setSectionsMovable(false);
	header()->setSectionResizeMode(QHeaderView::Interactive);
	header()->setSectionResizeMode(3, QHeaderView::Stretch);
}

void HistoryView::resizeEvent(QResizeEvent *e)
{
	QTreeWidget::resizeEvent(e);
}

void HistoryView::keyPressEvent(QKeyEvent *e)
{
	if(e->key() == Qt::Key_Enter || e->key() == Qt::Key_Return)
		doOpenEvent();
	else
		QTreeWidget::keyPressEvent(e);
}

void HistoryView::doResize()
{
	// no-op: rich text resize not needed with QTreeWidget
}

void HistoryView::addEvent(PsiEvent *e, const QString &eid)
{
	new HistoryViewItem(e, eid, at_id++, this);
}

void HistoryView::doOpenEvent()
{
	HistoryViewItem *i = (HistoryViewItem *)currentItem();
	if(!i)
		return;
	aOpenEvent(i->e);
}

void HistoryView::qlv_doubleclick(QTreeWidgetItem *xi, int)
{
	setCurrentItem(xi);
	doOpenEvent();
}

void HistoryView::qlv_contextPopup(const QPoint &pos)
{
	HistoryViewItem *i = (HistoryViewItem *)currentItem();
	if(!i)
		return;

	QMenu popup;
	QAction *openAct = popup.addAction(tr("Open"));
	popup.addSeparator();
	QAction *copyAct = popup.addAction(tr("Copy"));
	copyAct->setEnabled(i->e->type() == PsiEvent::Message);

	QAction *chosen = popup.exec(viewport()->mapToGlobal(pos));

	if(chosen == openAct)
		doOpenEvent();
	else if(chosen == copyAct) {
		HistoryViewItem *ci = (HistoryViewItem *)currentItem();
		if(!ci)
			return;

		MessageEvent *me = (MessageEvent *)ci->e;
		QApplication::clipboard()->setText(me->message().body(), QClipboard::Clipboard);
		if(QApplication::clipboard()->supportsSelection())
			QApplication::clipboard()->setText(me->message().body(), QClipboard::Selection);
	}
}


//----------------------------------------------------------------------------
// HistoryViewItem
//----------------------------------------------------------------------------
HistoryViewItem::HistoryViewItem(PsiEvent *_e, const QString &eid, int xid, QTreeWidget *parent)
: QTreeWidgetItem(parent)
{
	id = xid;
	eventId = eid;

	if(_e->type() == PsiEvent::Message) {
		MessageEvent *me = (MessageEvent *)_e;
		e = new MessageEvent(*me);
	}
	else if(_e->type() == PsiEvent::Auth) {
		AuthEvent *ae = (AuthEvent *)_e;
		e = new AuthEvent(*ae);
	}

	PsiIcon *a = PsiIconset::instance()->event2icon(e);
	if(e->type() == PsiEvent::Message) {
		MessageEvent *me = (MessageEvent *)e;
		const Message &m = me->message();
		text = m.body();

		if(!m.urlList().isEmpty())
			setIcon(0, QIcon(IconsetFactory::icon("psi/www").impix()));
		else if(e->originLocal())
			setIcon(0, QIcon(IconsetFactory::icon("psi/sendMessage").impix()));
		else if(a)
			setIcon(0, QIcon(a->impix()));
	}
	else if(e->type() == PsiEvent::Auth) {
		AuthEvent *ae = (AuthEvent *)e;
		text = ae->authType();
		if (a)
			setIcon(0, QIcon(a->impix()));
	}

	if(e->originLocal())
		setText(1, HistoryView::tr("To"));
	else
		setText(1, HistoryView::tr("From"));

	QString date = e->timeStamp().toString(Qt::LocalDate);
	setText(2, date);
	setText(3, text);
}

HistoryViewItem::~HistoryViewItem()
{
	delete e;
}


