/*
 * yatabbarbase.cpp - description of the file
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

#include "yatabbarbase.h"

#include <QDebug>
#include <QPainter>
#include <QStyleOptionTab>
#include <QTimeLine>
#include <QEvent>
#include <QMouseEvent>

#include "yatabwidget.h"

#ifndef WIDGET_PLUGIN
#include "yavisualutil.h"
#include "tabbablewidget.h"
#include "yachatsendbutton.h"
#endif
#include "psitooltip.h"

// Bumped from 3 to 7 on Qt5/macOS — with the original 3 the tab bar
// renders at 22 px tall and Arial 12 px text visibly clips at the top
// edge (the Qt5 macOS text renderer places glyphs differently than
// Qt4 did). 7 gives a 30 px bar with a comfortable 7 px top/bottom
// padding, matching the reference Yandex design.
static int margin = 7;

QPixmap YaTabBarBase::tabShadow(bool isCurrent)
{
	static QPixmap shadow_inactive;
	static QPixmap shadow_active;

	if (isCurrent) {
		if (shadow_active.isNull()) {
			shadow_active = QPixmap(":images/chat/tab_active.png");
		}
		return shadow_active;
	}

	if (shadow_inactive.isNull()) {
		shadow_inactive = QPixmap(":images/chat/tab_inactive.png");
	}
	return shadow_inactive;
}

YaTabBarBase::YaTabBarBase(QWidget* parent)
	: QTabBar(parent)
	, layoutUpdatesEnabled_(true)
	, timeLine_(new QTimeLine(1500, this))
	, drawTabNumbers_(false)
	, cachedTextHeight_(0)
{
	Q_ASSERT(dynamic_cast<YaTabWidget*>(parentWidget()));

	timeLine_->setFrameRange(0, 100);
	timeLine_->setUpdateInterval(100);
	timeLine_->setLoopCount(0);
	timeLine_->setCurveShape(QTimeLine::SineCurve);
	connect(timeLine_, SIGNAL(frameChanged(int)), this, SLOT(update()));

	connect(this, SIGNAL(currentChanged(int)), SLOT(currentIndexChanged()));
}

YaTabBarBase::~YaTabBarBase()
{
}

QSize YaTabBarBase::preferredTabSize(int index) const
{
	return preferredTabSize(tabText(index), index == currentIndex());
}

int YaTabBarBase::maximumWidth() const
{
	return static_cast<YaTabWidget*>(parentWidget())->tabRect().width();
}

bool YaTabBarBase::tabHovered(int index) const
{
	QRect tabRect = this->tabRect(index);
	if (!pressedPosition().isNull()) {
		if (!tabRect.contains(pressedPosition())) {
			return false;
		}
	}
	return tabRect.contains(mapFromGlobal(QCursor::pos())) && draggedTabIndex() == -1;
}

QRect YaTabBarBase::tabIconRect(int index) const
{
	// Same caveat as tabTextRect: use effectiveTabRect instead of calling
	// QTabBar::tabRect through a non-virtual dispatch.
	QRect iconRect(effectiveTabRect(index));
	iconRect.setLeft(iconRect.left() + margin());
	iconRect.setWidth(cachedIconSize_.width());
	return iconRect;
}

QRect YaTabBarBase::effectiveTabRect(int index) const
{
	// Default: use Qt's native tabRect. Subclasses (YaMultiLineTabBar)
	// override to return their custom tabRect_[].
	return QTabBar::tabRect(index);
}

QRect YaTabBarBase::tabTextRect(int index) const
{
	// IMPORTANT: QTabBar::tabRect is NOT virtual, so calling
	// this->tabRect(index) from this base class dispatches to Qt's default
	// implementation (width=bar width, height=0 for a bar whose tabs have
	// Qt sizeHint=0). That gave textTop=0 and clipped text at the top of
	// the tab. Use the effectiveTabRect virtual helper instead — subclasses
	// override it to return their real tabRect_[].
	QRect tabRect = effectiveTabRect(index);
	QRect iconRect = this->tabIconRect(index);
	iconRect.moveTop(tabRect.top());
	iconRect.setHeight(tabRect.height());
	QRect closeRect = closeButtonRect(index, tabRect);

	// Build textRect explicitly from axis values. The previous approach
	// used QRect::moveCenter() + setLeft()/setRight() which on Qt5 / macOS
	// produced textRect y=-6 (negative!) for a tabRect of (0,0,181,30),
	// apparently because setLeft/setRight retain opposite edges and can
	// unexpectedly mutate height when negative-width intermediate states
	// occur. Computing left/right/top/height directly avoids any reliance
	// on Qt's undocumented coordinate mutation behaviour.
	const int textLeft  = iconRect.right() + margin();
	const int textRight = (closeRect.isNull()
	                       ? tabRect.right() - margin()
	                       : closeRect.left() - margin());
	const int textWidth = qMax(0, textRight - textLeft + 1);
	const int textTop   = tabRect.top()
	                      + qMax(0, (tabRect.height() - cachedTextHeight_) / 2);
	QRect textRect(textLeft, textTop, textWidth, cachedTextHeight_);

	return textRect;
}

void YaTabBarBase::drawTab(QPainter* painter, int index, const QRect& tabRect)
{
	QStyleOptionTab tab = getStyleOption(index);
	if (!(tab.state & QStyle::State_Enabled)) {
		tab.palette.setCurrentColorGroup(QPalette::Disabled);
	}

#ifndef WIDGET_PLUGIN
	// Force the painter to use the exact same font we measured textRect
	// with. Without this the QPainter inherits the widget's current font
	// (macOS system 13pt default) while our layout math used Arial 12px,
	// and the mismatch pushed rendered text above the tab top edge.
	painter->setFont(Ya::VisualUtil::normalFont());
#endif


	// Don't bother drawing a tab if the entire tab is outside of the visible tab bar.
	if (tabRect.right() < 0 ||
	    tabRect.left() > width() ||
	    tabRect.bottom() < 0 ||
	    tabRect.top() > height() ||
	    tabRect.width() < 3)
	{
		return;
	}

	bool isCurrent = index == currentIndex();
	bool isHovered = tabHovered(index);
	bool isHighlighted = tabData(index).toBool();

	QColor backgroundColor = this->tabBackgroundColor();
	if (isCurrent) {
#ifndef WIDGET_PLUGIN
		backgroundColor = Ya::VisualUtil::editAreaColor();
#else
		backgroundColor = Qt::white;
#endif
	}
	else if (isHovered) {
#ifndef WIDGET_PLUGIN
		backgroundColor = Ya::VisualUtil::tabHighlightColor();
#else
		backgroundColor = Qt::gray;
#endif
	}
	else if (isHighlighted) {
		backgroundColor = highlightColor();
	}

	if (backgroundColor.isValid()) {
		painter->fillRect(tabRect, backgroundColor);
	}

	painter->save();
#ifndef WIDGET_PLUGIN
	painter->setPen(Ya::VisualUtil::rosterTabBorderColor());
#endif

	bool drawLeftLine  = tabRect.left() != rect().left();
	bool drawRightLine = true; // tabRect.right() != rect().right() || rect().width() < maximumWidth();

	switch (shape()) {
	case YaTabBarBase::RoundedSouth:
	case YaTabBarBase::TriangularSouth:
		if (isMultiLine())
			drawRightLine = tabRect.right() + 1 < maximumWidth();

		if (!isCurrent || isMultiLine())
			painter->drawLine(tabRect.topLeft(), tabRect.topRight());

		if (isCurrent) {
			if (drawLeftLine && !isMultiLine())
				painter->drawLine(tabRect.topLeft(), tabRect.bottomLeft());
			if (drawRightLine)
				painter->drawLine(tabRect.topRight(), tabRect.bottomRight());
		}
		else {
			if (isHovered || isMultiLine()) {
				if (currentIndex() != (index - 1) && !isMultiLine())
					if (drawLeftLine)
						painter->drawLine(tabRect.topLeft(), tabRect.bottomLeft());
				if (currentIndex() != (index + 1) || isMultiLine())
					if (drawRightLine)
						painter->drawLine(tabRect.topRight(), tabRect.bottomRight());
			}
		}
		break;
	default:
		Q_ASSERT(false);
		break;
	}

	painter->restore();

	tabIcon(index).paint(painter, tabIconRect(index));

	// Guard: ensure caches are populated before first paint
	if (cachedTextHeight_ == 0)
		clearCaches();

	QRect textRect = tabTextRect(index);
	QString text = tabText(index);

	if (drawTabNumbers_ && index < 10) {
		int numberToDraw = index + 1;
		if (numberToDraw > 9) {
			numberToDraw = 0;
		}

		painter->save();
		painter->setPen(Qt::gray);
		QString numberToDrawText = QString::number(numberToDraw) + " ";
		painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, numberToDrawText);
		textRect.adjust(fontMetrics().horizontalAdvance(numberToDrawText), 0, 0, 0);
		painter->restore();
	}

	painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, text);
#ifndef WIDGET_PLUGIN
	if (textWidth(text) > textRect.width() && backgroundColor.isValid()) {
		Ya::VisualUtil::drawTextFadeOut(painter, textRect.adjusted(1, 0, 1, 0), backgroundColor, 15);
	}
#endif

	if (isMultiLine()) {
		QPixmap shadow(tabShadow(isCurrent));
		QRect r(tabRect);
		r.setHeight(shadow.height());
		painter->drawTiledPixmap(r, shadow);
	}
}

QStyleOptionTab YaTabBarBase::getStyleOption(int tab) const
{
	QStyleOptionTab opt;
	opt.init(this);
	opt.state &= ~(QStyle::State_HasFocus | QStyle::State_MouseOver);
	opt.rect = tabRect(tab);
	bool isCurrent = tab == currentIndex();
	opt.row = 0;
//    if (tab == pressedIndex)
//        opt.state |= QStyle::State_Sunken;
	if (isCurrent)
		opt.state |= QStyle::State_Selected;
	if (isCurrent && hasFocus())
		opt.state |= QStyle::State_HasFocus;
	if (isTabEnabled(tab))
		opt.state &= ~QStyle::State_Enabled;
	if (isActiveWindow())
		opt.state |= QStyle::State_Active;
//    if (opt.rect == hoverRect)
//        opt.state |= QStyle::State_MouseOver;
	opt.shape = shape();
	opt.text = tabText(tab);

	if (tabTextColor(tab).isValid())
		opt.palette.setColor(foregroundRole(), tabTextColor(tab));

	opt.icon = tabIcon(tab);
	opt.iconSize = opt.icon.actualSize(QSize(32, 32));  // Will get the default value then.

	int totalTabs = count();

	if (tab > 0 && tab - 1 == currentIndex())
		opt.selectedPosition = QStyleOptionTab::PreviousIsSelected;
	else if (tab < totalTabs - 1 && tab + 1 == currentIndex())
		opt.selectedPosition = QStyleOptionTab::NextIsSelected;
	else
		opt.selectedPosition = QStyleOptionTab::NotAdjacent;

	if (tab == 0) {
		if (totalTabs > 1)
			opt.position = QStyleOptionTab::Beginning;
		else
			opt.position = QStyleOptionTab::OnlyOneTab;
	}
	else if (tab == totalTabs - 1) {
		opt.position = QStyleOptionTab::End;
	}
	else {
		opt.position = QStyleOptionTab::Middle;
	}
	if (const QTabWidget *tw = qobject_cast<const QTabWidget *>(parentWidget())) {
		if (tw->cornerWidget(Qt::TopLeftCorner) || tw->cornerWidget(Qt::BottomLeftCorner))
			opt.cornerWidgets |= QStyleOptionTab::LeftCornerWidget;
		if (tw->cornerWidget(Qt::TopRightCorner) || tw->cornerWidget(Qt::BottomRightCorner))
			opt.cornerWidgets |= QStyleOptionTab::RightCornerWidget;
	}
	return opt;
}

int YaTabBarBase::draggedTabIndex() const
{
	return -1;
}

const YaWindowTheme& YaTabBarBase::theme() const
{
#ifndef WIDGET_PLUGIN
	TabbableWidget* dlg = currentTab();
	if (dlg) {
		return dlg->theme();
	}
#endif

	static YaWindowTheme dummy;
	return dummy;
}

QString YaTabBarBase::tabBackgroundName() const
{
#ifndef WIDGET_PLUGIN
	return theme().name();
#endif

	return QString();
}

QColor YaTabBarBase::tabBackgroundColor() const
{
#ifdef WIDGET_PLUGIN
	return QColor("#6CF");
#else
	return theme().theme().tabBackgroundColor();
#endif
}

QColor YaTabBarBase::tabBlinkColor() const
{
#ifdef WIDGET_PLUGIN
	return Qt::gray;
#else
	return theme().theme().tabBlinkColor();
#endif
}

#ifndef WIDGET_PLUGIN
TabbableWidget* YaTabBarBase::currentTab() const
{
	QTabWidget* tabWidget = dynamic_cast<QTabWidget*>(parentWidget());
	Q_ASSERT(tabWidget);
	TabbableWidget* dlg = tabWidget ? dynamic_cast<TabbableWidget*>(tabWidget->currentWidget()) : 0;
	return dlg;
}

int YaTabBarBase::indexOf(TabbableWidget* tab) const
{
	QTabWidget* tabWidget = dynamic_cast<QTabWidget*>(parentWidget());
	Q_ASSERT(tabWidget);
	return tabWidget ? tabWidget->indexOf(tab) : -1;
}
#endif

bool YaTabBarBase::layoutUpdatesEnabled()
{
	return layoutUpdatesEnabled_;
}

void YaTabBarBase::setLayoutUpdatesEnabled(bool enabled)
{
	if (enabled != layoutUpdatesEnabled_) {
		layoutUpdatesEnabled_ = enabled;

		if (enabled) {
			tabLayoutChange();
		}
	}
}

int YaTabBarBase::margin()
{
	return ::margin;
}

QColor YaTabBarBase::highlightColor() const
{
#ifndef WIDGET_PLUGIN
	QColor highlightColor = this->tabBlinkColor();
#else
	QColor highlightColor = Qt::yellow;
#endif
	if (tabBackgroundName() == "orange.png") {
		highlightColor = QColor(0xFF, 0xFF, 0xFF);
	}

	QColor result;
	if (timeLine_->state() == QTimeLine::Running) {
		result = getCurrentColorGrade(
		             tabBackgroundColor(),
		             highlightColor,
		             timeLine_->endFrame() - timeLine_->startFrame(),
		             timeLine_->currentFrame()
		         );
	}
	return result;
}

QTimeLine* YaTabBarBase::timeLine() const
{
	return timeLine_;
}

void YaTabBarBase::startFading()
{
	switch (timeLine()->state()) {
	case QTimeLine::NotRunning:
		timeLine()->start();
		break;
	case QTimeLine::Paused:
		timeLine()->resume();
		break;
	case QTimeLine::Running:
	default:
		break;
	}
}

void YaTabBarBase::stopFading()
{
	timeLine()->stop();
}

void YaTabBarBase::updateFading()
{
	updateHiddenTabActions();

	for (int i = 0; i < count(); i++) {
		if (tabData(i).toBool()) {
			startFading();
			return;
		}
	}
	stopFading();
}

// TODO: move to Ya::VisualUtil
QColor YaTabBarBase::getCurrentColorGrade(const QColor& c1, const QColor& c2, const int sliceCount, const int currentStep) const
{
	double colorStep = sliceCount > 1 ? 1.0 * (currentStep % sliceCount) / (sliceCount - 1) : 0;
	QColor result(
		(int)(c1.red() < c2.red() ? c1.red() + (c2.red() - c1.red()) * colorStep : c1.red() - (c1.red() - c2.red()) * colorStep),
		(int)(c1.green() < c2.green() ? c1.green() + (c2.green() - c1.green()) * colorStep : c1.green() - (c1.green() - c2.green()) * colorStep),
		(int)(c1.blue() < c2.blue() ? c1.blue() + (c2.blue() - c1.blue()) * colorStep : c1.blue() - (c1.blue() - c2.blue()) * colorStep)
	);
	return result;
}

void YaTabBarBase::updateHiddenTabActions()
{
}

void YaTabBarBase::updateHiddenTabs()
{
}

/**
 * Qt needs to be patched in order to contain virtual method:
 * virtual void QTabBar::setCurrentIndex(int), otherwise
 * all our magic is powerless.
 */
void YaTabBarBase::setCurrentIndex(int index)
{
	bool updatesEnabled = window()->updatesEnabled();
	window()->setUpdatesEnabled(false);

	QTabBar::setCurrentIndex(index);

	// we need to do it after the tab was made current in order for it
	// to get sensible size, so it's layout could be updated immediately
	emit aboutToShow(index);

	window()->setUpdatesEnabled(updatesEnabled);
}

void YaTabBarBase::currentIndexChanged()
{
	updateLayout();
}

void YaTabBarBase::updateLayout()
{
	// force re-layout
	QEvent e(QEvent::ActivationChange);
	changeEvent(&e);

	updateSendButton();
}

void YaTabBarBase::updateSendButton()
{
#ifndef WIDGET_PLUGIN
	// hack in order to get YaChatSendButtonExtra into correct position
	if (currentTab()) {
		YaChatSendButton* sendButton = currentTab()->findChild<YaChatSendButton*>();
		if (sendButton) {
			sendButton->updatePosition();
		}
	}
#endif
}

void YaTabBarBase::clearCaches()
{
	cachedIconSize_ = QTabBar::iconSize();
	cachedTextWidth_.clear();
	// cachedTextWidth_.reserve(count());
	// Use the SAME font we actually paint tabs with (Ya::VisualUtil::normalFont,
	// Arial 12px) rather than the widget's current font — on macOS Qt5 the
	// widget font defaults to the 13pt system font, and mixing the two made
	// textRect too short for the Arial 12 glyphs' actual ascent+descent,
	// pushing the top of the text outside the tab rectangle.
#ifndef WIDGET_PLUGIN
	cachedTextHeight_ = QFontMetrics(Ya::VisualUtil::normalFont()).height();
#else
	cachedTextHeight_ = fontMetrics().height();
#endif
}

int YaTabBarBase::textWidth(const QString& text) const
{
	int result = cachedTextWidth_[text];
	if (!result) {
#ifndef WIDGET_PLUGIN
		// Match the metrics used when we actually paint (Ya::VisualUtil::
		// normalFont); mismatch between layout font and paint font used to
		// clip the right side of tab labels.
		result = QFontMetrics(Ya::VisualUtil::normalFont()).horizontalAdvance(text);
#else
		result = fontMetrics().horizontalAdvance(text);
#endif
		cachedTextWidth_[text] = result;
	}
	return result;
}

int YaTabBarBase::cachedTextHeight() const
{
	return cachedTextHeight_;
}

QSize YaTabBarBase::cachedIconSize() const
{
	return cachedIconSize_;
}

void YaTabBarBase::mousePressEvent(QMouseEvent* event)
{
	pressedPosition_ = event->pos();
	QTabBar::mousePressEvent(event);
}

void YaTabBarBase::clearPressedPosition()
{
	pressedPosition_ = QPoint();
}

void YaTabBarBase::mouseReleaseEvent(QMouseEvent* event)
{
	clearPressedPosition();
	QTabBar::mouseReleaseEvent(event);
}

QPoint YaTabBarBase::pressedPosition() const
{
	return pressedPosition_;
}

void YaTabBarBase::setPressedPosition(const QPoint& p)
{
	pressedPosition_ = p;
}

bool YaTabBarBase::isMultiLine() const
{
	return false;
}

bool YaTabBarBase::tabTextFits(int index) const
{
	// return tabSizeHint(index).width() >= YaTabBarBase::preferredTabSize(index).width();
	return textWidth(tabText(index)) <= tabTextRect(index).width();
}

bool YaTabBarBase::event(QEvent* event)
{
	if (event->type() == QEvent::ToolTip) {
		QPoint pos = dynamic_cast<QHelpEvent *>(event)->globalPos();
		QPoint localPos = dynamic_cast<QHelpEvent *>(event)->pos();
		QString toolTip;

		int tab = tabAt(localPos);
		if (closeButtonRect(tab, tabRect(tab)).contains(localPos)) {
			toolTip = tr("Close this tab");
		}
		else if (!tabTextFits(tab)) {
			toolTip = tabText(tab);
		}

		PsiToolTip::showText(pos, toolTip, this);
		return true;
	}

	return QTabBar::event(event);
}

bool YaTabBarBase::drawTabNumbers() const
{
	return drawTabNumbers_;
}

void YaTabBarBase::setDrawTabNumbers(bool drawTabNumbers)
{
	if (drawTabNumbers_ != drawTabNumbers) {
		drawTabNumbers_ = drawTabNumbers;
		update();
	}
}
