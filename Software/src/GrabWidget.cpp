/*
 * GrabWidget.cpp
 *
 *	Created on: 29.01.2011
 *		Author: Mike Shatohin (brunql)
 *		Project: Lightpack
 *
 *	Lightpack is very simple implementation of the backlight for a laptop
 *
 *	Copyright (c) 2011 Mike Shatohin, mikeshatohin [at] gmail.com
 *
 *	Lightpack is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	Lightpack is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program.	If not, see <http://www.gnu.org/licenses/>.
 *
 */


#include <QtGui>
#if (QT_VERSION < QT_VERSION_CHECK(5, 10, 0))
#include <QDesktopWidget>
#endif
#include <QTextItem>
#include "GrabWidget.hpp"
#include "ui_GrabWidget.h"
#include "Settings.hpp"
#include "debug.h"

using namespace SettingsScope;

// Colors changes when middle button clicked
const QColor GrabWidget::m_colors[GrabWidget::ColorsCount][2] = {
	{ Qt::red,			Qt::black }, /* LED1 */
	{ Qt::green,		Qt::black }, /* LED2 */
	{ Qt::blue,		Qt::white }, /* LED3 */
	{ Qt::yellow,		Qt::black }, /* LED4 */
	{ Qt::darkRed,		Qt::white }, /* LED5 */
	{ Qt::darkGreen,	Qt::white }, /* LED6 */
	{ Qt::darkBlue,	Qt::white }, /* LED7 */
	{ Qt::darkYellow,	Qt::white }, /* LED8 */
	{ qRgb(0,242,123), Qt::black },
	{ Qt::magenta,		Qt::black },
	{ Qt::cyan,		Qt::black },
	{ Qt::white,		Qt::black },
};

GrabWidget::GrabWidget(int id, int features, QList<GrabWidget*> *fellows, QWidget *parent) :
	QWidget(parent),
	ui(new Ui::GrabWidget)
{
	DEBUG_LOW_LEVEL << Q_FUNC_INFO << id;

	ui->setupUi(this);

	// Button image size 24x24 px, but it makes it impossible to resize widgets to less than that
	// Setting minimumSize instead does not respect the aspect ratio, leaving at 16 for now
	ui->button_OpenConfig->setFixedSize(16, 16);

	setId(id);
	cmd = NOP;
	m_features = features;
	setFellows(fellows);
	m_backgroundColor = Qt::white;

	setOpenConfigButtonBackground(m_backgroundColor);

	if (m_features & AllowMove)
		setCursorOnAll(Qt::OpenHandCursor);

	setWindowFlags(Qt::FramelessWindowHint | Qt::ToolTip);
	setFocusPolicy(Qt::NoFocus);

	setMouseTracking(true);

	//fillBackgroundColored(); ??

	if (features & (AllowCoefConfig | AllowEnableConfig)) {
		m_configWidget = new GrabConfigWidget();
		if (features & AllowEnableConfig) {
			connect(m_configWidget, &GrabConfigWidget::isAreaEnabled_Toggled, this, &GrabWidget::onIsAreaEnabled_Toggled);
		}
		if (features & AllowCoefConfig) {
			m_configWidget->setCoefs(m_coefs.red, m_coefs.green, m_coefs.blue);
			connect(m_configWidget, &GrabConfigWidget::coefRed_ValueChanged, this, &GrabWidget::onRedCoef_ValueChanged);
			connect(m_configWidget, &GrabConfigWidget::coefGreen_ValueChanged, this, &GrabWidget::onGreenCoef_ValueChanged);
			connect(m_configWidget, &GrabConfigWidget::coefBlue_ValueChanged, this, &GrabWidget::onBlueCoef_ValueChanged);
		}
	} else {
		ui->button_OpenConfig->setVisible(false);
	}

	if (features & SyncSettings)
		settingsProfileChanged();

	if (features & (SyncSettings | AllowCoefConfig | AllowEnableConfig))
		connect(ui->button_OpenConfig, &QPushButton::clicked, this, &GrabWidget::onOpenConfigButton_Clicked);
}

GrabWidget::~GrabWidget()
{
	DEBUG_LOW_LEVEL << Q_FUNC_INFO << m_selfId;

	delete ui;
}

void GrabWidget::closeEvent(QCloseEvent *event)
{
	qWarning() << Q_FUNC_INFO << "event->type():" << event->type() << "Id:" << m_selfId;

	event->ignore();
}

void GrabWidget::saveSizeAndPosition()
{
	if (m_features & SyncSettings) {
		DEBUG_MID_LEVEL << Q_FUNC_INFO;

		Settings::setLedPosition(m_selfId, pos());
		Settings::setLedSize(m_selfId, size());
	}
}

void GrabWidget::settingsProfileChanged()
{
	if (m_features & SyncSettings) {
		DEBUG_LOW_LEVEL << Q_FUNC_INFO << m_selfId;

		m_coefs.red = Settings::getLedCoefRed(m_selfId);
		m_coefs.green = Settings::getLedCoefGreen(m_selfId);
		m_coefs.blue = Settings::getLedCoefBlue(m_selfId);

		m_configWidget->setIsAreaEnabled(Settings::isLedEnabled(m_selfId));
		m_configWidget->setCoefs(m_coefs.red, m_coefs.green, m_coefs.blue);

		move(Settings::getLedPosition(m_selfId));
		resize(Settings::getLedSize(m_selfId));

		emit resizeOrMoveCompleted(m_selfId);
	}
}

void GrabWidget::setCursorOnAll(Qt::CursorShape cursor)
{
	DEBUG_MID_LEVEL << Q_FUNC_INFO << cursor;

	ui->button_OpenConfig->setCursor(Qt::ArrowCursor);

	setCursor(cursor);
}

void GrabWidget::mousePressEvent(QMouseEvent *pe)
{
	DEBUG_MID_LEVEL << Q_FUNC_INFO << pe->pos();

	mousePressPosition = pe->pos();
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
	mousePressGlobalPosition = pe->globalPosition().toPoint();
	const QPoint eventPosition = pe->position().toPoint();
#else
	mousePressGlobalPosition = pe->globalPos();
	const QPoint eventPosition(pe->x(), pe->y());
#endif
	mousePressDiffFromBorder.setWidth(width() - eventPosition.x());
	mousePressDiffFromBorder.setHeight(height() - eventPosition.y());

	if (pe->buttons() == Qt::RightButton)
	{
		// Send signal RightButtonClicked to main window for grouping widgets
		emit mouseRightButtonClicked(m_selfId);
	}
	else if (pe->buttons() == Qt::LeftButton)
	{
		const bool resizable = m_features & AllowResize;
		// First check corners
		if (resizable && eventPosition.x() < BorderWidth && eventPosition.y() < BorderWidth)
		{
			cmd = RESIZE_LEFT_UP;
			setCursorOnAll(Qt::SizeFDiagCursor);
		}
		else if (resizable && eventPosition.x() < BorderWidth && (height() - eventPosition.y()) < BorderWidth)
		{
			cmd = RESIZE_LEFT_DOWN;
			setCursorOnAll(Qt::SizeBDiagCursor);
		}
		else if (resizable && eventPosition.y() < BorderWidth && (width() - eventPosition.x()) < BorderWidth)
		{
			cmd = RESIZE_RIGHT_UP;
			setCursorOnAll(Qt::SizeBDiagCursor);
		}
		else if (resizable && (height() - eventPosition.y()) < BorderWidth && (width() - eventPosition.x()) < BorderWidth)
		{
			cmd = RESIZE_RIGHT_DOWN;
			setCursorOnAll(Qt::SizeFDiagCursor);
		}
		// Next check sides
		else if (resizable && eventPosition.x() < BorderWidth)
		{
			cmd = RESIZE_HOR_LEFT;
			setCursorOnAll(Qt::SizeHorCursor);
		}
		else if (resizable && (width() - eventPosition.x()) < BorderWidth)
		{
			cmd = RESIZE_HOR_RIGHT;
			setCursorOnAll(Qt::SizeHorCursor);
		}
		else if (resizable && eventPosition.y() < BorderWidth)
		{
			cmd = RESIZE_VER_UP;
			setCursorOnAll(Qt::SizeVerCursor);
		}
		else if (resizable && (height() - eventPosition.y()) < BorderWidth)
		{
			cmd = RESIZE_VER_DOWN;
			setCursorOnAll(Qt::SizeVerCursor);
		}
		// Click on center, just move it
		else if (m_features & AllowMove)
		{
			cmd = MOVE;
			setCursorOnAll(Qt::ClosedHandCursor);
		}
		emit resizeOrMoveStarted(m_selfId);
	}
	else
	{
		cmd = NOP;
	}
	repaint();
}

QRect GrabWidget::resizeAccordingly(QMouseEvent *pe) {
	int newWidth = width();
	int newHeight = height();
	int newX = x();
	int newY = y();

#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
	const QPoint globalPos = pe->globalPosition().toPoint();
	const QPoint eventPosition = pe->position().toPoint();
#else
	const QPoint globalPos = pe->globalPos();
	const QPoint eventPosition(pe->x(), pe->y());
#endif

	switch (cmd)
	{
		case MOVE:
			break;

		case RESIZE_HOR_RIGHT:
			newWidth = eventPosition.x() + mousePressDiffFromBorder.width();
			newWidth = (newWidth <= MinimumWidth) ? MinimumWidth : newWidth;
			break;

		case RESIZE_VER_DOWN:
			newHeight = eventPosition.y() + mousePressDiffFromBorder.height();
			newHeight = (newHeight <= MinimumHeight) ? MinimumHeight : newHeight;
			break;

		case RESIZE_HOR_LEFT:
			newY = pos().y();
			newHeight = height();

			newWidth = mousePressGlobalPosition.x() - globalPos.x() + mousePressPosition.x() + mousePressDiffFromBorder.width();

			if (newWidth < MinimumWidth)
			{
				newWidth = MinimumWidth;
				newX = mousePressGlobalPosition.x() + mousePressDiffFromBorder.width() - MinimumWidth;
			} else {
				newX = globalPos.x() - mousePressPosition.x();
			}
			break;

		case RESIZE_VER_UP:
			newX = pos().x();
			newWidth = width();

			newHeight = mousePressGlobalPosition.y() - globalPos.y() + mousePressPosition.y() + mousePressDiffFromBorder.height();

			if (newHeight < MinimumHeight)
			{
				newHeight = MinimumHeight;
				newY = mousePressGlobalPosition.y() + mousePressDiffFromBorder.height() - MinimumHeight;
			} else {
				newY = globalPos.y() - mousePressPosition.y();
			}
			break;


		case RESIZE_RIGHT_DOWN:
			newWidth = eventPosition.x() + mousePressDiffFromBorder.width();
			newHeight = eventPosition.y() + mousePressDiffFromBorder.height();
			newWidth = (newWidth <= MinimumWidth) ? MinimumWidth : newWidth;
			newHeight = (newHeight <= MinimumHeight) ? MinimumHeight : newHeight;
			break;

		case RESIZE_RIGHT_UP:
			newWidth = eventPosition.x() + mousePressDiffFromBorder.width();
			if (newWidth < MinimumWidth) newWidth = MinimumWidth;
			newX = pos().x();

			newHeight = mousePressGlobalPosition.y() - globalPos.y() + mousePressPosition.y() + mousePressDiffFromBorder.height();

			if (newHeight < MinimumHeight)
			{
				newHeight = MinimumHeight;
				newY = mousePressGlobalPosition.y() + mousePressDiffFromBorder.height() - MinimumHeight;
			} else {
				newY = globalPos.y() - mousePressPosition.y();
			}
			break;

		case RESIZE_LEFT_DOWN:
			newHeight = eventPosition.y() + mousePressDiffFromBorder.height();
			if (newHeight < MinimumHeight) newHeight = MinimumHeight;
			newY = pos().y();

			newWidth = mousePressGlobalPosition.x() - globalPos.x() + mousePressPosition.x() + mousePressDiffFromBorder.width();

			if (newWidth < MinimumWidth)
			{
				newWidth = MinimumWidth;
				newX = mousePressGlobalPosition.x() + mousePressDiffFromBorder.width() - MinimumWidth;
			} else {
				newX = globalPos.x() - mousePressPosition.x();
			}
			break;

		case RESIZE_LEFT_UP:
			newWidth = mousePressGlobalPosition.x() - globalPos.x() + mousePressPosition.x() + mousePressDiffFromBorder.width();

			if (newWidth < MinimumWidth)
			{
				newWidth = MinimumWidth;
				newX = mousePressGlobalPosition.x() + mousePressDiffFromBorder.width() - MinimumWidth;
			} else {
				newX = globalPos.x() - mousePressPosition.x();
			}

			newHeight = mousePressGlobalPosition.y() - globalPos.y() + mousePressPosition.y() + mousePressDiffFromBorder.height();

			if (newHeight < MinimumHeight)
			{
				newHeight = MinimumHeight;
				newY = mousePressGlobalPosition.y() + mousePressDiffFromBorder.height() - MinimumHeight;
			} else {
				newY = globalPos.y() - mousePressPosition.y();
			}
			break;
		default:
			break;
	}

	return QRect(newX, newY, newWidth, newHeight);
}

bool GrabWidget::snapEdgeToScreenOrClosestFellow(
	QRect& newRect,
	const QRect& screen,
	std::function<void(QRect&,int)> setter,
	std::function<int(const QRect&)> getter,
	std::function<int(const QRect&)> oppositeGetter) {
	if (abs(getter(newRect) - getter(screen))	< StickyCloserPixels) {
		setter(newRect, getter(screen));
		return true;
	}

	if (m_fellows) {
		int candidate = INT_MIN;
		for (auto fellow : *m_fellows) {
			if (fellow != this) {
				if (abs(getter(newRect) - getter(fellow->geometry())) < StickyCloserPixels &&
					abs(getter(newRect) - getter(fellow->geometry())) < abs(candidate - getter(fellow->geometry()))) {
					candidate = getter(fellow->geometry());
					break;
				}
				if (abs(getter(newRect) - oppositeGetter(fellow->geometry())) < StickyCloserPixels &&
					abs(getter(newRect) - oppositeGetter(fellow->geometry())) < abs(candidate - oppositeGetter(fellow->geometry()))) {
					candidate = oppositeGetter(fellow->geometry());
					break;
				}
			}
		}
		if (candidate != INT_MIN) {
			setter(newRect, candidate);
			return true;
		}
	}
	return false;
}


void GrabWidget::mouseMoveEvent(QMouseEvent *pe)
{
	DEBUG_HIGH_LEVEL << Q_FUNC_INFO << "pe->pos() =" << pe->pos();

#if (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
	const QScreen* screen = QGuiApplication::screenAt(this->geometry().center());
	if (screen == nullptr) return;
	QRect screenRect = screen->geometry();
#else
	QRect screenRect = QApplication::desktop()->screenGeometry(this);
#endif

#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
	const QPoint globalPos = pe->globalPosition().toPoint();
#else
	const QPoint globalPos = pe->globalPos();
#endif

	if (cmd == NOP ){
		checkAndSetCursors(pe);
	} else if (cmd == MOVE) {
		QPoint moveHere;
		moveHere = globalPos - mousePressPosition;
		QRect newRect = QRect(moveHere, geometry().size());
		bool snapped;

		snapped = snapEdgeToScreenOrClosestFellow(
			newRect, screenRect,
			[](QRect& r, int v) { r.moveLeft(v); },
			[](const QRect& r) { return r.left(); },
			[](const QRect& r) { return r.right() + 1; });
		if (!snapped) {
			snapEdgeToScreenOrClosestFellow(
				newRect, screenRect,
				[](QRect& r, int v) { r.moveRight(v); },
				[](const QRect& r) { return r.right(); },
				[](const QRect& r) { return r.left() - 1; });
		}

		snapped = snapEdgeToScreenOrClosestFellow(
			newRect, screenRect,
			[](QRect& r, int v) { r.moveTop(v); },
			[](const QRect& r) { return r.top(); },
			[](const QRect& r) { return r.bottom() + 1; });
		if (!snapped) {
			snapEdgeToScreenOrClosestFellow(
				newRect, screenRect,
				[](QRect& r, int v) { r.moveBottom(v); },
				[](const QRect& r) { return r.bottom(); },
				[](const QRect& r) { return r.top() - 1; });
		}

#if (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
		if (QGuiApplication::screenAt(newRect.center()) == nullptr) return;
#endif
		move(newRect.topLeft());
	} else {
		QRect newRect = resizeAccordingly(pe);

		if (cmd == RESIZE_HOR_LEFT || cmd == RESIZE_LEFT_DOWN || cmd == RESIZE_LEFT_UP) {
			snapEdgeToScreenOrClosestFellow(
				newRect, screenRect,
				[](QRect& r, int v) {	r.setLeft(v); },
				[](const QRect& r) { return r.left(); },
				[](const QRect& r) { return r.right() + 1; });
		}

		if (cmd == RESIZE_VER_UP || cmd == RESIZE_RIGHT_UP || cmd == RESIZE_LEFT_UP) {
			snapEdgeToScreenOrClosestFellow(
				newRect, screenRect,
				[](QRect& r, int v) {	r.setTop(v); },
				[](const QRect& r) { return r.top(); },
				[](const QRect& r) { return r.bottom() + 1; });
		}

		if (cmd == RESIZE_HOR_RIGHT || cmd == RESIZE_RIGHT_UP || cmd == RESIZE_RIGHT_DOWN) {
			snapEdgeToScreenOrClosestFellow(
				newRect, screenRect,
				[](QRect& r, int v) {	r.setRight(v); },
				[](const QRect& r) { return r.right(); },
				[](const QRect& r) { return r.left() - 1; });
		}

		if (cmd == RESIZE_VER_DOWN || cmd == RESIZE_LEFT_DOWN || cmd == RESIZE_RIGHT_DOWN) {
			snapEdgeToScreenOrClosestFellow(
				newRect, screenRect,
				[](QRect& r, int v) {	r.setBottom(v); },
				[](const QRect& r) { return r.bottom(); },
				[](const QRect& r) { return r.top() - 1; });
		}

#if (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
		if (QGuiApplication::screenAt(newRect.center()) == nullptr) return;
#endif
		if (newRect.size() != geometry().size()) {
			resize(newRect.size());
		}
		if (newRect.topLeft() != geometry().topLeft()) {
			move(newRect.topLeft());
		}
	}
}

void GrabWidget::mouseReleaseEvent(QMouseEvent *pe)
{
	DEBUG_MID_LEVEL << Q_FUNC_INFO;

	checkAndSetCursors(pe);

	cmd = NOP;

	saveSizeAndPosition();

	emit resizeOrMoveCompleted(m_selfId);
	repaint();
}


void GrabWidget::wheelEvent(QWheelEvent *pe)
{
	DEBUG_MID_LEVEL << Q_FUNC_INFO;

	if (!(m_features & AllowColorCycle) || !isAreaEnabled())
	{
		// Do nothing if area disabled
		return;
	}

	if (pe->angleDelta().y() > 0) m_colorIndex++;
	if (pe->angleDelta().y() < 0) m_colorIndex--;

	if (m_colorIndex >= ColorsCount)
	{
		m_colorIndex = 0;
	}
	else if (m_colorIndex < 0)
	{
		m_colorIndex = ColorsCount - 1;
	}
	fillBackground(m_colorIndex);
}

void GrabWidget::resizeEvent(QResizeEvent *)
{
	DEBUG_MID_LEVEL << Q_FUNC_INFO;

	m_widthHeight = QStringLiteral("%1x%2").arg(QString::number(width()), QString::number(height()));
}

void GrabWidget::paintEvent(QPaintEvent *)
{
	DEBUG_HIGH_LEVEL << Q_FUNC_INFO;

	QPainter painter(this);
	painter.setPen(QColor(0x77, 0x77, 0x77));
	if ((m_features & DimUntilInteractedWith) && isAreaEnabled()) {
		painter.setBrush(QBrush(cmd == NOP ? m_backgroundColor.darker(150) : m_backgroundColor));
	}
	painter.drawRect(0, 0, width() - 1, height() - 1);

//	// Icon 'resize' opacity
	painter.setOpacity((m_features & AllowResize) ? 0.4 : 0.0);

	// Draw icon 12x12px with 3px padding from the bottom right corner
	if (getTextColor() == Qt::white)
		painter.drawPixmap(width() - 18, height() - 18, 12, 12, QPixmap(QStringLiteral(":/icons/res_light.png")));
	else{
		//painter.setOpacity(0.5);
		painter.drawPixmap(width() - 18, height() - 18, 12, 12, QPixmap(QStringLiteral(":/icons/res_dark.png")));
	}

	// Self ID and size text opacity
	painter.setOpacity(isAreaEnabled() ? 0.25 : 1.0);

	QFont font = painter.font();
	font.setBold(true);
	font.setPixelSize((height() / 3 < width() / 3) ? height() / 3 : width() / 3);
	painter.setFont(font);

	painter.setPen(getTextColor());
	painter.setBrush(QBrush(getTextColor()));
	painter.drawText(rect(), isAreaEnabled() ? m_selfIdString : QStringLiteral("OFF"), QTextOption(Qt::AlignCenter));

	font.setBold(false);
	font.setPointSize(10);
	painter.setFont(font);

	QRect rectWidthHeight = rect();
	rectWidthHeight.setBottom(rect().bottom() - 3);
	painter.drawText(rectWidthHeight, m_widthHeight, QTextOption(Qt::AlignHCenter | Qt::AlignBottom));
}

int GrabWidget::getId() const {
	DEBUG_HIGH_LEVEL << Q_FUNC_INFO;

	return m_selfId;
}

double GrabWidget::getCoefRed() const
{
	DEBUG_HIGH_LEVEL << Q_FUNC_INFO;

	return m_coefs.red;
}

double GrabWidget::getCoefGreen() const
{
	DEBUG_HIGH_LEVEL << Q_FUNC_INFO;

	return m_coefs.green;
}

double GrabWidget::getCoefBlue() const
{
	DEBUG_HIGH_LEVEL << Q_FUNC_INFO;

	return m_coefs.blue;
}

WBAdjustment GrabWidget::getCoefs() const
{
	return m_coefs;
}

void GrabWidget::setCoefRed(const double coef)
{
	DEBUG_HIGH_LEVEL << Q_FUNC_INFO;

	m_coefs.red = coef;
	if (m_features & AllowCoefConfig)
		m_configWidget->setCoefs(m_coefs.red, m_coefs.green, m_coefs.blue);
}

void GrabWidget::setCoefGreen(const double coef)
{
	DEBUG_HIGH_LEVEL << Q_FUNC_INFO;

	m_coefs.green = coef;
	if (m_features & AllowCoefConfig)
		m_configWidget->setCoefs(m_coefs.red, m_coefs.green, m_coefs.blue);
}

void GrabWidget::setCoefBlue(const double coef)
{
	DEBUG_HIGH_LEVEL << Q_FUNC_INFO;

	m_coefs.blue = coef;
	if (m_features & AllowCoefConfig)
		m_configWidget->setCoefs(m_coefs.red, m_coefs.green, m_coefs.blue);
}

void GrabWidget::setCoefs(const WBAdjustment& coefs)
{
	DEBUG_HIGH_LEVEL << Q_FUNC_INFO;

	m_coefs = coefs;
	if (m_features & AllowCoefConfig)
		m_configWidget->setCoefs(m_coefs.red, m_coefs.green, m_coefs.blue);
}

void GrabWidget::setId(const int id)
{
	DEBUG_HIGH_LEVEL << Q_FUNC_INFO;

	m_selfId = id;
	m_selfIdString = QString::number(m_selfId + 1);
}

void GrabWidget::setFellows(QList<GrabWidget*>* const fellows)
{
	DEBUG_HIGH_LEVEL << Q_FUNC_INFO;

	m_fellows = fellows;
}

bool GrabWidget::isAreaEnabled() const
{
	DEBUG_HIGH_LEVEL << Q_FUNC_INFO;

	if (m_configWidget)
		return m_configWidget->isAreaEnabled();
	return true;
}

void GrabWidget::setAreaEnabled(const bool enabled)
{
	DEBUG_HIGH_LEVEL << Q_FUNC_INFO;

	if (m_configWidget)
		m_configWidget->setIsAreaEnabled(enabled);
}

void GrabWidget::fillBackgroundWhite()
{
	DEBUG_MID_LEVEL << Q_FUNC_INFO << m_selfId;

	setBackgroundColor(Qt::white);
	setTextColor(Qt::black);
}

void GrabWidget::fillBackgroundColored()
{
	DEBUG_MID_LEVEL << Q_FUNC_INFO << m_selfId;

	fillBackground(m_selfId);
}

void GrabWidget::fillBackground(int index)
{
	DEBUG_MID_LEVEL << Q_FUNC_INFO << index;

	m_colorIndex = index % ColorsCount;

	setBackgroundColor(m_colors[m_colorIndex][0]);
	setTextColor(m_colors[m_colorIndex][1]);
}

void GrabWidget::checkAndSetCursors(QMouseEvent *pe)
{
	DEBUG_MID_LEVEL << Q_FUNC_INFO;

#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
	const QPoint eventPosition = pe->position().toPoint();
#else
	const QPoint eventPosition(pe->x(), pe->y());
#endif

	const bool resizable = m_features & AllowResize;
	if (resizable && eventPosition.x() < BorderWidth && eventPosition.y() < BorderWidth)
	{
		setCursorOnAll(Qt::SizeFDiagCursor);
	}
	else if (resizable && eventPosition.x() < BorderWidth && (height() - eventPosition.y()) < BorderWidth)
	{
		setCursorOnAll(Qt::SizeBDiagCursor);
	}
	else if (resizable && eventPosition.y() < BorderWidth && (width() - eventPosition.x()) < BorderWidth)
	{
		setCursorOnAll(Qt::SizeBDiagCursor);
	}
	else if (resizable && (height() - eventPosition.y()) < BorderWidth && (width() - eventPosition.x()) < BorderWidth)
	{
		setCursorOnAll(Qt::SizeFDiagCursor);
	}
	else if (resizable && eventPosition.x() < BorderWidth)
	{
		setCursorOnAll(Qt::SizeHorCursor);
	}
	else if (resizable && (width() - eventPosition.x()) < BorderWidth)
	{
		setCursorOnAll(Qt::SizeHorCursor);
	}
	else if (resizable && eventPosition.y() < BorderWidth)
	{
		setCursorOnAll(Qt::SizeVerCursor);
	}
	else if (resizable && (height() - eventPosition.y()) < BorderWidth)
	{
		setCursorOnAll(Qt::SizeVerCursor);
	}
	else if (m_features & AllowMove)
	{
		if (pe->buttons() & Qt::LeftButton)
			setCursorOnAll(Qt::ClosedHandCursor);
		else
			setCursorOnAll(Qt::OpenHandCursor);
	}
}

void GrabWidget::onIsAreaEnabled_Toggled(const bool state)
{
	DEBUG_LOW_LEVEL << Q_FUNC_INFO << state;

	if (m_features & SyncSettings) {
		Settings::setLedEnabled(m_selfId, state);
	}
	setBackgroundColor(m_backgroundColor);
	setTextColor(m_textColor);
}

void GrabWidget::onOpenConfigButton_Clicked()
{
	DEBUG_LOW_LEVEL << Q_FUNC_INFO;

	// Find y-coordinate for center of the button
	int buttonCenter = ui->button_OpenConfig->pos().y() + ui->button_OpenConfig->height() / 2;

	m_configWidget->showConfigFor(geometry(), buttonCenter, m_features & AllowEnableConfig, m_features & AllowCoefConfig);
}

void GrabWidget::onRedCoef_ValueChanged(double value)
{
	DEBUG_MID_LEVEL << Q_FUNC_INFO << value;
	if (m_features & SyncSettings)
		Settings::setLedCoefRed(m_selfId, value);
	m_coefs.red = value;
}

void GrabWidget::onGreenCoef_ValueChanged(double value)
{
	DEBUG_MID_LEVEL << Q_FUNC_INFO << value;

	if (m_features & SyncSettings)
		Settings::setLedCoefGreen(m_selfId, value);
	m_coefs.green = value;
}

void GrabWidget::onBlueCoef_ValueChanged(double value)
{
	DEBUG_MID_LEVEL << Q_FUNC_INFO << value;

	if (m_features & SyncSettings)
		Settings::setLedCoefBlue(m_selfId, value);
	m_coefs.blue = value;
}

void GrabWidget::setBackgroundColor(const QColor& color)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
	DEBUG_MID_LEVEL << Q_FUNC_INFO << Qt::hex << color.rgb();
#else
	DEBUG_MID_LEVEL << Q_FUNC_INFO << hex << color.rgb();
#endif

	m_backgroundColor = color;
	setOpenConfigButtonBackground(getBackgroundColor());

	QPalette pal = palette();
	pal.setBrush(backgroundRole(), QBrush(getBackgroundColor()));
	setPalette(pal);
}

void GrabWidget::setTextColor(const QColor& color)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
	DEBUG_MID_LEVEL << Q_FUNC_INFO << Qt::hex << color.rgb();
#else
	DEBUG_MID_LEVEL << Q_FUNC_INFO << hex << color.rgb();
#endif

	m_textColor = color;

	QPalette pal = palette();
	pal.setBrush(foregroundRole(), QBrush(getTextColor()));
	setPalette(pal);
}

QColor GrabWidget::getTextColor()
{
	return isAreaEnabled() ? m_textColor : Qt::white;
}

QColor GrabWidget::getBackgroundColor()
{
	return isAreaEnabled() ? m_backgroundColor : Qt::black;
}

void GrabWidget::setOpenConfigButtonBackground(const QColor &color)
{
	const QString image = (m_features & DimUntilInteractedWith) && color != Qt::black ? QStringLiteral("dark") : QStringLiteral("light");
	ui->button_OpenConfig->setStyleSheet(QStringLiteral(
		"QPushButton		{ border-image: url(:/buttons/settings_%1_24px.png) }\
		QPushButton:hover	{ border-image: url(:/buttons/settings_%1_24px_hover.png) }\
		QPushButton:pressed { border-image: url(:/buttons/settings_%1_24px_pressed.png) }"
	).arg(image));
}
