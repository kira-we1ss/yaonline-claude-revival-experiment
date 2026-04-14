/*
 * yaonlinemainwin.cpp
 * Copyright (C) 2008  Yandex LLC (Michail Pishchagin)
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

#include "yaonlinemainwin.h"

#include <QPainter>
#include <QPushButton>
#include <QBitmap>
#include <QDesktopWidget>
#include <QApplication>

#ifdef YAPSI_ACTIVEX_SERVER
#include "yaonline.h"
#include "JsonToVariant.h"
#endif

#include "yawindowtheme.h"
#include "psicon.h"
#include "yavisualutil.h"
#include "psioptions.h"
#include "psicontactlist.h"
#include "borderrenderer.h"
#include "psiaccount.h"

#ifdef YAPSI_ACTIVEX_SERVER
#if QT_VERSION >= 0x040500
#define CUSTOM_SHADOW
#endif
#endif

//----------------------------------------------------------------------------
// YaWin7Window
//----------------------------------------------------------------------------

YaWin7Window::YaWin7Window(QWidget* parent, Qt::WindowFlags f)
	: YaWindow(parent, f)
{
#ifdef Q_WS_WIN
	taskBarCreatedMessage_ = RegisterWindowMessage(L"TaskbarButtonCreated");

	// HRESULT hr = CoCreateInstance(CLSID_TaskbarList,
	//                               NULL,
	//                               CLSCTX_INPROC_SERVER,
	//                               IID_ITaskbarList,
	//                               reinterpret_cast<void**>(&taskBar_));
	HRESULT hr = CoCreateInstance(CLSID_TaskbarList,
	                              NULL,
	                              CLSCTX_INPROC_SERVER,
	                              IID_PPV_ARGS(&taskBar_));
	if (hr == S_OK) {
		hr = taskBar_->HrInit();
	}
	else {
		taskBar_ = 0;
	}
#endif
}

YaWin7Window::~YaWin7Window()
{
#ifdef Q_WS_WIN
	if (taskBar_) {
		taskBar_->Release();
	}
#endif
}

#ifdef Q_WS_WIN
bool YaWin7Window::winEvent(MSG* message, long* result)
{
	if (message->message == taskBarCreatedMessage_) {
		taskBarCreated();
	}

	return YaWindow::winEvent(message, result);
}

void YaWin7Window::taskBarCreated()
{
	setOverlayIcon(iconHandle_, accessibilityHint_);
}

ITaskbarList3* YaWin7Window::taskBar() const
{
	return taskBar_;
}

void YaWin7Window::setOverlayIcon(HICON iconHandle, const QString& accessibilityHint)
{
	iconHandle_ = iconHandle;
	accessibilityHint_ = accessibilityHint;

	if (taskBar()) {
		taskBar()->SetOverlayIcon(winId(), iconHandle_, accessibilityHint_.toStdWString().c_str());
	}
}

void YaWin7Window::setW7ToolTip(const QString& tooltip)
{
	if (taskBar()) {
		taskBar()->SetThumbnailTooltip(winId(), tooltip.toStdWString().c_str());
	}
}
#endif

//----------------------------------------------------------------------------
// YaOnlineExpansionButton
//----------------------------------------------------------------------------

class YaOnlineExpansionButton : public QPushButton
{
public:
	YaOnlineExpansionButton(YaWindow* parent)
		: QPushButton(parent)
		, parent_(parent)
	{
		aaCorners_ = QPixmap(":images/window/online_expansion_aa.png");
		shadow_ = QPixmap(":images/window/online_expansion_shadow.png");
		mask_ = QBitmap(":images/window/online_expansion_mask.bmp");
		setCursor(Qt::PointingHandCursor);
	}

	// reimplemented
	QSize sizeHint() const
	{
		return shadow_.size();
	}

	const QBitmap& getMask() const
	{
		return mask_;
	}

	// reimplemented
	void paintEvent(QPaintEvent*)
	{
		QPainter p(this);
		p.drawPixmap(0, 0, shadow_);

		const QPixmap& pix = parent_->theme().theme().onlineExpansion();
		QPoint pixPoint(rect().width() - pix.width(), (rect().height() - pix.height()) / 2 + 1);
		p.drawPixmap(pixPoint, pix);
		if (underMouse()) {
			p.save();
			p.setCompositionMode(QPainter::CompositionMode_Multiply);
			p.drawPixmap(pixPoint, pix);
			p.restore();
		}

		p.drawPixmap(0, 0, aaCorners_);
	}

private:
	YaWindow* parent_;
	QPixmap aaCorners_;
	QPixmap shadow_;
	QBitmap mask_;
};

YaOnlineMainWin::YaOnlineMainWin(PsiCon* controller, QWidget* parent, Qt::WindowFlags f)
	: YaWin7Window(parent, f)
	, controller_(controller)
	, theme_(YaWindowTheme::Roster)
#ifdef YAPSI_ACTIVEX_SERVER
	, onlineExpansion_(0)
	, onlineExpansionVisible_(false)
	, temporarilyHiddenOnline_(false)
	, delayingVisibility_(false)
	, initialShow_(true)
	, showingSidebarWithoutRaise_(false)
	, onlineWinId_(-1)
	, showRosterWithoutActivation_(false)
#endif
	, lastStatusType_(XMPP::Status::Offline)
	, lastHaveAvailableAccounts_(false)
{
	Q_ASSERT(controller_);

	setProperty("is-appwindow", true);

#ifdef YAPSI_ACTIVEX_SERVER
	setProperty("show-offscreen", true);

	isOnlineActive_.setDelay(200 + 150);
	connect(&isOnlineActive_, SIGNAL(valueChanged()), SLOT(activationChangeUpdate()));
	isOnlineActive_.setValueImmediately(false);

	onlineExpansion_ = new YaOnlineExpansionButton(this);
	onlineExpansion_->hide();
	connect(onlineExpansion_, SIGNAL(clicked()), SLOT(onlineExpansionClicked()));

	updateInteractiveOperationTimer_ = new QTimer(this);
	connect(updateInteractiveOperationTimer_, SIGNAL(timeout()), SLOT(updateInteractiveOperation()));
	updateInteractiveOperationTimer_->setInterval(50);
	updateInteractiveOperationTimer_->setSingleShot(false);

	afterWindowMovedTimer_ = new QTimer(this);
	connect(afterWindowMovedTimer_, SIGNAL(timeout()), SLOT(afterWindowMoved()));
	afterWindowMovedTimer_->setInterval(1000);
	afterWindowMovedTimer_->setSingleShot(true);

	showOnlineWithoutAnimationTimer_ = new QTimer(this);
	connect(showOnlineWithoutAnimationTimer_, SIGNAL(timeout()), SLOT(showOnlineWithoutAnimation()));
	showOnlineWithoutAnimationTimer_->setInterval(0);
	showOnlineWithoutAnimationTimer_->setSingleShot(true);

	showOnlineAfterDesktopResizeTimer_ = new QTimer(this);
	connect(showOnlineAfterDesktopResizeTimer_, SIGNAL(timeout()), SLOT(showOnlineAfterDesktopResize()));
	showOnlineAfterDesktopResizeTimer_->setInterval(100);
	showOnlineAfterDesktopResizeTimer_->setSingleShot(true);

	activationChangeUpdateTimer()->setInterval(isOnlineActive_.delay());

	connect(qApp->desktop(), SIGNAL(resized(int)), this, SLOT(desktopResized(int)));

	Q_ASSERT(controller_->yaOnline());
	connect(controller_->yaOnline(), SIGNAL(hideRoster()), SLOT(onlineHideRoster()));
	connect(controller_->yaOnline(), SIGNAL(showRoster()), SLOT(onlineShowRoster()));
	connect(controller_->yaOnline(), SIGNAL(showRosterWithoutActivation()), SLOT(showRosterWithoutActivation()));
	connect(controller_->yaOnline(), SIGNAL(showRosterMinimized()), SLOT(showRosterMinimized()));
	connect(controller_->yaOnline(), SIGNAL(forceStatus(XMPP::Status::Type)), SLOT(statusSelected(XMPP::Status::Type)));
	connect(controller_->yaOnline(), SIGNAL(forceManualStatus(XMPP::Status::Type)), SLOT(statusSelectedManuallyHelper(XMPP::Status::Type)));
	connect(controller_->yaOnline(), SIGNAL(clearMoods()), SLOT(clearMoods()));
	connect(controller_->yaOnline(), SIGNAL(showYapsiPreferences()), SLOT(togglePreferences()));

	connect(controller_->yaOnline(), SIGNAL(doOnlineHiding()), SLOT(onlineHiding()));
	connect(controller_->yaOnline(), SIGNAL(doOnlineVisible()), SLOT(onlineVisible()));
	connect(controller_->yaOnline(), SIGNAL(doOnlineCreated(int)), SLOT(onlineCreated(int)));
	connect(controller_->yaOnline(), SIGNAL(doActivateRoster()), SLOT(activateRoster()));
	connect(controller_->yaOnline(), SIGNAL(doOnlineDeactivated()), SLOT(onlineDeactivated()));
	connect(controller_->yaOnline(), SIGNAL(showRelativeToOnline(const QRect&)), SLOT(showRelativeToOnline(const QRect&)));

	controller_->yaOnline()->setMainWin(this);
	controller_->yaOnline()->rosterHandle((int)window()->winId());
#endif

	connect(controller_->contactList(), SIGNAL(accountStateChanged()), this, SLOT(accountStateChanged()));
	connect(controller_->contactList(), SIGNAL(connectingAccountsChanged()), this, SLOT(connectingAccountsChanged()));
	accountStateChanged();
	connectingAccountsChanged();
}

YaOnlineMainWin::~YaOnlineMainWin()
{
}

PsiCon* YaOnlineMainWin::controller() const
{
	return controller_;
}

void YaOnlineMainWin::doBringToFront()
{
#ifdef YAPSI_ACTIVEX_SERVER
	bool doShowOnline = (isMinimized() || !isVisible()) && controller()->yaOnline()->onlineShouldBeVisible();
	if (doShowOnline) {
		showingSidebarWithoutRaise_ = true;
		setWindowVisible(true);
		showOnline(false, false);
	}
#endif
	::bringToFront(this);
}

#ifdef Q_WS_WIN
extern bool ForceForegroundWindow(HWND hwnd);
#endif

void YaOnlineMainWin::setWindowVisible(bool visible)
{
#ifdef YAPSI_ACTIVEX_SERVER
	HWND foregroundWindow = GetForegroundWindow();
	if (showRosterWithoutActivation_) {
		// setAttribute(Qt::WA_ShowWithoutActivating, true);
	}

	controller_->yaOnline()->rosterHandle((int)window()->winId());
	if (!visible) {
		if (onlineWinId_ != -1) {
			ShowWindow((HWND)onlineWinId_, SW_HIDE);
		}
	}
#endif

	QList<QWidget*> widgets;
	widgets << this;
	// if (option.useTabs && !psi_->getTabSets()->isEmpty())
	// 	widgets << psi_->getTabs();

	bool hideAll = !visible;
	foreach(QWidget* w, widgets) {
		if (hideAll) {
			w->hide();
		}
		else {
#ifdef YAPSI_ACTIVEX_SERVER
			//if (!showRosterWithoutActivation_)
				bringToFront(w, false);
			//else
			//	w->show();
#else
			w->show();
#endif
		}
	}

	if (visible) {
		raiseSidebar();
		QTimer::singleShot(100, this, SLOT(invalidateGeometry()));
	}

#ifdef YAPSI_ACTIVEX_SERVER
	// setAttribute(Qt::WA_ShowWithoutActivating, false);
	if (showRosterWithoutActivation_) {
		ForceForegroundWindow(foregroundWindow);
		showRosterWithoutActivation_ = false;
	}
#endif
}

void YaOnlineMainWin::statusSelectedManually(XMPP::Status::Type statusType)
{
#ifdef YAPSI_ACTIVEX_SERVER
	if (statusType == XMPP::Status::Online ||
	    statusType == XMPP::Status::DND)
	{
		controller()->yaOnline()->setDND(statusType == XMPP::Status::DND);
	}

	if (statusType != XMPP::Status::Offline) {
		controller()->yaOnline()->setStatus(statusType);
	}
#else
	Q_UNUSED(statusType);
#endif
}

void YaOnlineMainWin::clearMoods()
{
}

void YaOnlineMainWin::togglePreferences()
{
}

QRegion YaOnlineMainWin::getMask() const
{
#ifdef YAPSI_ACTIVEX_SERVER
	QSize size = this->size();
	size.setWidth(size.width() - additionalLeftMargin() - additionalRightMargin());
	size.setHeight(size.height() - additionalTopMargin() - additionalBottomMargin());
	QRegion mask = Ya::VisualUtil::roundedMask(size, cornerRadius(), Ya::VisualUtil::TopBorders);
	mask.translate(additionalLeftMargin(), additionalTopMargin());

	if (onlineExpansionVisible_) {
		// QRegion expansionMask = Ya::VisualUtil::roundedMask(onlineExpansion_->sizeHint(),
		//                         cornerRadius(),
		//                         Ya::VisualUtil::LeftBorders);
		QRegion expansionMask(onlineExpansion_->getMask());
		expansionMask.translate(onlineExpansionRect().topLeft());
		mask += expansionMask;
	}

	return mask;
#else
	return YaWin7Window::getMask();
#endif
}

int YaOnlineMainWin::additionalTopMargin() const
{
	return YaWin7Window::additionalTopMargin();
}

int YaOnlineMainWin::additionalBottomMargin() const
{
	return YaWin7Window::additionalBottomMargin();
}

int YaOnlineMainWin::additionalLeftMargin() const
{
#if defined(CUSTOM_SHADOW) || defined(YAPSI_ACTIVEX_SERVER)
# ifdef YAPSI_ACTIVEX_SERVER
	return onlineExpansion_->sizeHint().width();
# else
	return YaWin7Window::additionalLeftMargin();
# endif
#endif
	return 0;
}

int YaOnlineMainWin::additionalRightMargin() const
{
	return YaWin7Window::additionalRightMargin();
}

void YaOnlineMainWin::paint(QPainter* p)
{
	Q_UNUSED(p);
// #ifdef YAPSI_ACTIVEX_SERVER
// 	if (onlineExpansionVisible_) {
// 		p->drawPixmap(onlineExpansionRect().topLeft(), onlineExpansionPixmap_);
// 	}
// #endif
}

void YaOnlineMainWin::paintEvent(QPaintEvent* e)
{
	Q_UNUSED(e);
	// YaWin7Window::paintEvent(e);

	QPainter p(this);

#ifdef YAPSI_ACTIVEX_SERVER
	if (onlineExpansionVisible_) {
		QRegion mask(rect());
		mask -= QRegion(onlineExpansionRect());
		p.setClipRegion(mask);
	}
#endif

	Ya::VisualUtil::drawWindowTheme(&p, theme(), rect(), yaContentsRect(), showAsActiveWindow());
	p.setClipping(false);

	paint(&p);
	Ya::VisualUtil::drawAACorners(&p, theme(), rect(), yaContentsRect());
}

#ifdef YAPSI_ACTIVEX_SERVER
void YaOnlineMainWin::hideOnline()
{
	if (onlineWinId_ != -1) {
		ShowWindow((HWND)onlineWinId_, SW_HIDE);
	}
	if (controller()->contactList()->accountsLoaded()) {
		controller()->yaOnline()->hideOnline();
	}

	showOnlineWithoutAnimationTimer_->stop();
	showOnlineAfterDesktopResizeTimer_->stop();
}

void YaOnlineMainWin::showOnline(bool animate, bool raiseWindow)
{
	QVariantMap map;
	map["parent_hwnd"] = (int)winId();
	map["raise_window"] = raiseWindow;
	controller()->yaOnline()->showOnline(frameGeometryToContentsGeometry(frameGeometry()), animate, map);
}

void YaOnlineMainWin::showOnlineWithoutAnimation()
{
	showOnline(true);
}

void YaOnlineMainWin::showOnlineAfterDesktopResize()
{
	showingSidebarWithoutRaise_ = true;
	showOnline(false, false);
}

QRect YaOnlineMainWin::onlineExpansionRect() const
{
	return QRect(QPoint(0, (height() - onlineExpansion_->sizeHint().height()) / 2),
	             onlineExpansion_->sizeHint());
}

void YaOnlineMainWin::invalidateMask()
{
	onlineExpansion_->setVisible(onlineExpansionVisible_);
	onlineExpansion_->setGeometry(onlineExpansionRect());
	YaWin7Window::invalidateMask();
}

bool YaOnlineMainWin::showAsActiveWindow() const
{
	return YaWin7Window::showAsActiveWindow() || isOnlineActive();
}

void YaOnlineMainWin::updateOnlineExpansion()
{
	invalidateMask();
}

void YaOnlineMainWin::onlineHiding()
{
	onlineWinId_ = -1;
	onlineExpansionVisible_ = true;
	isOnlineActive_.setValue(false);
	updateOnlineExpansion();
}

void YaOnlineMainWin::onlineVisible()
{
	onlineExpansionVisible_ = false;
	updateOnlineExpansion();

	if ((delayingVisibility_ || isVisible())) {
		HWND hwndInsertAfter = !showingSidebarWithoutRaise_ ?
		                        HWND_TOP :
		                        window()->winId();

		if (!initialShow_ && onlineWinId_ != -1) {
			SetWindowPos((HWND)onlineWinId_, hwndInsertAfter, 0, 0, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE);
		}

		if (!showingSidebarWithoutRaise_) {
			setWindowVisible(true);
		}
	}
	delayingVisibility_ = false;

	showingSidebarWithoutRaise_ = false;
	if (isMoveOperationActive() && isInInteractiveMode()) {
		initCurrentOperation(mousePressGlobalPosition());
	}
}

void YaOnlineMainWin::afterShowWidgetOffscreen()
{
	YaWin7Window::afterShowWidgetOffscreen();

	if (initialShow_ && onlineWinId_ != -1) {
		SetWindowPos((HWND)onlineWinId_, HWND_TOP, 0, 0, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE);
		initialShow_ = false;
		setProperty("show-offscreen", false);
	}
}

void YaOnlineMainWin::onlineCreated(int onlineWinId)
{
	onlineWinId_ = onlineWinId;
}

bool YaOnlineMainWin::isOnlineVisible() const
{
	return !onlineExpansionVisible_ && (onlineWinId_ != -1);
}

bool YaOnlineMainWin::isOnlineActive() const
{
	return isOnlineActive_.value();
}

void YaOnlineMainWin::onlineDeactivated()
{
	isOnlineActive_.setValue(false);

	QEvent e(QEvent::ActivationChange);
	changeEvent(&e);
}

void YaOnlineMainWin::activateRoster()
{
	if (!isVisible())
		return;
	isOnlineActive_.setValueImmediately(true);
	forceRaise();
	raiseSidebar();
	activationChangeUpdate();
	QTimer::singleShot(100, this, SLOT(invalidateGeometry()));
}

void YaOnlineMainWin::showRelativeToOnline(const QRect& onlineRect)
{
	onlineExpansionVisible_ = !controller()->yaOnline()->onlineShouldBeVisible();
	updateOnlineExpansion();

	int height = qMax(this->height(), onlineRect.height() + 20);
	int x = onlineRect.right();
	int y = (onlineRect.top() + onlineRect.height() / 2) - height / 2;
	QRect r(x, y, width(), height);
	r = frameGeometryToContentsGeometry(r);
	r.translate(-additionalLeftMargin() - 1, -additionalTopMargin());
	r = contentsGeometryToFrameGeometry(r);
	r = ensureGeometryVisible(r);
	setGeometry(r);
	if (!onlineExpansionVisible_) {
		onlineShowRoster();
	}
	else {
		setWindowVisible(true);
	}
}

void YaOnlineMainWin::interactiveOperationStarted()
{
	YaWin7Window::interactiveOperationStarted();
	if (isMoveOperationActive())
		return;

	updateInteractiveOperationTimer_->start();

	if (isOnlineVisible() && !temporarilyHiddenOnline_) {
		temporarilyHiddenOnline_ = true;
		hideOnline();
		onlineHiding();
	}
}

void YaOnlineMainWin::interactiveOperationFinished()
{
	YaWin7Window::interactiveOperationFinished();
	if (isMoveOperationActive()) {
		controller()->yaOnline()->onlineMoved();
		return;
	}

	interactiveOperationFinishedHelper();
}

bool YaOnlineMainWin::interactiveOperationEnabled() const
{
	if (isMoveOperationActive() && isInInteractiveMode()) {
		return isOnlineVisible() || !controller()->yaOnline()->onlineShouldBeVisible();
	}
	return true;
}

void YaOnlineMainWin::interactiveOperationFinishedHelper()
{
	updateInteractiveOperationTimer_->stop();

	if (temporarilyHiddenOnline_) {
		temporarilyHiddenOnline_ = false;
		showOnline(true);
		onlineVisible();
	}
}

#define HIGH_BIT_MASK_SHORT 0x8000

// work-around for us not getting MouseButtonRelease on QSizeGrips on Windows (Qt 4.4.3)
// the hack is pretty hard-core, but gets the job done when we want to know
// when we've stopped resizing the window
void YaOnlineMainWin::updateInteractiveOperation()
{
	int key = GetSystemMetrics(SM_SWAPBUTTON) ? VK_RBUTTON : VK_LBUTTON;
	bool leftButton = GetAsyncKeyState(key) & HIGH_BIT_MASK_SHORT;
	if (!leftButton) {
		interactiveOperationFinishedHelper();
	}
}

void YaOnlineMainWin::activationChangeUpdate()
{
	YaWin7Window::activationChangeUpdate();

	if (!controller()->contactList()->accountsLoaded() || !controller()->yaOnline())
		return;
	if (isActiveWindow() && !onlineExpansionVisible_ && !QApplication::activePopupWidget()) {
		controller()->yaOnline()->activateOnline();
	}
	else if (!isActiveWindow() && !QApplication::activePopupWidget()) {
		controller()->yaOnline()->deactivateOnline();
	}
}

void YaOnlineMainWin::changeEvent(QEvent* e)
{
	YaWin7Window::changeEvent(e);
}

void YaOnlineMainWin::onlineHideRoster()
{
#ifdef Q_WS_WIN
	if (forceMinimizeOnClose()) {
		doMinimize();
		return;
	}
#endif
	setWindowVisible(false);
}

void YaOnlineMainWin::onlineShowRoster()
{
	onlineExpansionVisible_ = !controller()->yaOnline()->onlineShouldBeVisible();
	updateOnlineExpansion();

	if (controller()->yaOnline()->onlineShouldBeVisible()) {
		showOnline(false);
		delayingVisibility_ = true;
	}
	else {
		setWindowVisible(true);
	}
}

void YaOnlineMainWin::showRosterWithoutActivation()
{
	showRosterWithoutActivation_ = true;
	onlineShowRoster();
}

void YaOnlineMainWin::showRosterMinimized()
{
	QWidget::showMinimized();
}

void YaOnlineMainWin::onlineExpansionClicked()
{
	controller()->yaOnline()->setVisible();
	showOnline(true);
}

bool YaOnlineMainWin::event(QEvent* e)
{
	if (e->type() == QEvent::WindowStateChange || e->type() == QEvent::Hide) {
		// QWindowStateChangeEvent* windowStateChangeEvent = static_cast<QWindowStateChangeEvent*>(e);
		// bool minimizeOperation = (windowState() & Qt::WindowMinimized) ||
		//                         (windowStateChangeEvent->oldState() & Qt::WindowMinimized);
		bool invisible = isMinimized() || !isVisible();
		if (invisible && controller()->yaOnline()->chatIsMain()) {
			temporarilyHiddenOnline_ = false;
			hideOnline();
		}
	}

	return YaWin7Window::event(e);
}

void YaOnlineMainWin::setYaMaximized(bool maximized)
{
	if (controller()->yaOnline()->onlineShouldBeVisible()) {
		onlineExpansionVisible_ = true;
		hideOnline();
	}
	YaWin7Window::setYaMaximized(maximized);
	if (controller()->yaOnline()->onlineShouldBeVisible()) {
		showOnlineWithoutAnimationTimer_->start();
	}
}

bool YaOnlineMainWin::winEvent(MSG* msg, long* result)
{
	if (msg->message == WM_SYSCOMMAND && msg->wParam == SC_RESTORE) {
		if (controller()->yaOnline()->onlineShouldBeVisible()) {
			showOnline(false);
			delayingVisibility_ = true;

			*result = 0;
			return true;
		}
	}

	return YaWin7Window::winEvent(msg, result);
}

void YaOnlineMainWin::initCurrentOperation(const QPoint& mousePos)
{
	YaWin7Window::initCurrentOperation(mousePos);
	if (onlineWinId_ != -1) {
		RECT rect;
		if (GetWindowRect((HWND)onlineWinId_, &rect)) {
			onlineOldGeometry_ = QRect(rect.left, rect.top,
			                           rect.right - rect.left, rect.bottom - rect.top);
		}
	}
}

void YaOnlineMainWin::deinitCurrentOperation()
{
	onlineOldGeometry_ = QRect();
	YaWin7Window::deinitCurrentOperation();
}

void YaOnlineMainWin::moveOperation(const QPoint& delta)
{
	if (!onlineOldGeometry_.isEmpty() && !onlineOldGeometry_.isNull()) {
		MoveWindow((HWND)onlineWinId_,
		           onlineOldGeometry_.left() + delta.x(),
		           onlineOldGeometry_.top() + delta.y(),
		           onlineOldGeometry_.width(),
		           onlineOldGeometry_.height(),
		           true);
		UpdateWindow((HWND)onlineWinId_);
	}
	YaWin7Window::moveOperation(delta);
}

void YaOnlineMainWin::setMainToOnline()
{
	controller()->yaOnline()->setMainToOnline();
}

void YaOnlineMainWin::desktopResized(int desktop)
{
	if (desktop != qApp->desktop()->screenNumber(this))
		return;

	if (!isMinimized() && isVisible() && controller()->yaOnline()->onlineShouldBeVisible()) {
		hideOnline();
		showOnlineAfterDesktopResizeTimer_->start();
	}
}

void YaOnlineMainWin::moveEvent(QMoveEvent* e)
{
	YaWin7Window::moveEvent(e);
	afterWindowMovedTimer_->start();
}

void YaOnlineMainWin::resizeEvent(QResizeEvent* e)
{
	YaWin7Window::resizeEvent(e);
	afterWindowMovedTimer_->start();
}

void YaOnlineMainWin::afterWindowMoved()
{
	if (isMoveOperationActive() && isInInteractiveMode()) {
		afterWindowMovedTimer_->start();
		return;
	}

	if (!isMinimized() && isVisible() && isOnlineVisible() && !temporarilyHiddenOnline_) {
		QRect r = frameGeometryToContentsGeometry(geometry()).adjusted(-1, 0, 0, 0);
		QRect onlineR = controller()->yaOnline()->onlineSidebarGeometry();
		if (r.left() != onlineR.right() || onlineR.top() < r.top() || onlineR.bottom() > r.bottom() ) {
			hideOnline();
			showOnlineAfterDesktopResizeTimer_->start();
		}
	}
}

bool YaOnlineMainWin::enableTopLeftBorderResize() const
{
	return false;
}
#endif

#ifdef Q_WS_WIN
bool YaOnlineMainWin::forceMinimizeOnClose() const
{
	return (QSysInfo::WindowsVersion >= QSysInfo::WV_WINDOWS7)
#ifdef YAPSI_ACTIVEX_SERVER
	        && controller_->yaOnline()->forceMinimizeOnClose()
	        && controller()->yaOnline()->chatIsMain() // roster should close when switching to sidebar-only view
#endif
	        ;
}

void YaOnlineMainWin::doMinimize()
{
#ifdef Q_WS_WIN
	// ONLINE-3509
	HWND hwnd = GetNextWindow(winId(), GW_HWNDNEXT);
	if (hwnd) {
		ForceForegroundWindow(hwnd);
	}
#endif
	showMinimized();
}
#endif

void YaOnlineMainWin::raiseSidebar()
{
#ifdef YAPSI_ACTIVEX_SERVER
	if (onlineWinId_ != -1) {
		SetWindowPos((HWND)onlineWinId_, HWND_TOP, 0, 0, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE | SWP_NOMOVE);
	}
#endif
}

const YaWindowTheme& YaOnlineMainWin::theme() const
{
	return theme_;
}

void YaOnlineMainWin::decorateButton(int)
{
}

#ifdef YAPSI_ACTIVEX_SERVER
void YaOnlineMainWin::setOverlayIcon(const QString& json)
{
	QVariant variant;
	try {
		variant = JsonQt::JsonToVariant::parse(json);
	}
	catch(...) {
	}
	QVariantMap map = variant.toMap();

	int iconHandle = map["icon"].toInt();
	QString accessibilityHint = map["accessibility_hint"].toString();

	YaWin7Window::setOverlayIcon((HICON)iconHandle, accessibilityHint);
}
#endif

void YaOnlineMainWin::accountStateChanged()
{
#ifdef YAPSI_ACTIVEX_SERVER
	if (!controller() || !controller()->contactList() || !controller()->contactList()->accountsLoaded())
		return;

	XMPP::Status::Type statusType = controller()->currentStatusType();
	if (lastStatusType_ != statusType) {
		lastStatusType_ = statusType;
		controller()->yaOnline()->setCurrentlyVisibleStatus(lastStatusType_);
	}

	bool haveAvailableAccounts = controller()->contactList()->haveAvailableAccounts();
	if (lastHaveAvailableAccounts_ != haveAvailableAccounts) {
		lastHaveAvailableAccounts_ = haveAvailableAccounts;
		controller()->yaOnline()->setHaveConnectedAccounts(lastHaveAvailableAccounts_);
	}
#endif
}

void YaOnlineMainWin::connectingAccountsChanged()
{
#ifdef YAPSI_ACTIVEX_SERVER
	if (!controller() || !controller()->contactList() || !controller()->contactList()->accountsLoaded())
		return;

	QStringList connectingAccounts = controller()->contactList()->connectingAccounts();
	controller()->yaOnline()->setHaveConnectingAccounts(connectingAccounts);

	QStringList processedAccounts;
	foreach(QString accountJid, connectingAccounts) {
		if (accountJid.endsWith("@chat.facebook.com"))
			accountJid = "Facebook";
		processedAccounts << accountJid;

	}

	QString text;
	if(!processedAccounts.empty()) {
		text = tr("Error connecting accounts: %1", 
		          "Windows7 Contact List Tooltip", 
		          processedAccounts.length())
		       .arg(processedAccounts.join("\n"));
	}
	setW7ToolTip(text);
#endif
}
