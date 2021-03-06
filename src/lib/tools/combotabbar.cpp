/* ============================================================
* QupZilla - WebKit based browser
* Copyright (C) 2013  S. Razi Alavizadeh <s.r.alavizadeh@gmail.com>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
* ============================================================ */
#include "combotabbar.h"
#include "toolbutton.h"
#include "mainapplication.h"
#include "proxystyle.h"

#include <QIcon>
#include <QHBoxLayout>
#include <QStylePainter>
#include <QStyleOptionTabV3>
#include <QStyleOptionTabBarBaseV2>
#include <QPropertyAnimation>
#include <QScrollArea>
#include <QTimer>
#include <QTabBar>
#include <QMouseEvent>
#include <QApplication>
#include <QDebug>

// taken from qtabbar_p.h
#define ANIMATION_DURATION 250

ComboTabBar::ComboTabBar(QWidget* parent)
    : QWidget(parent)
    , m_mainTabBar(0)
    , m_pinnedTabBar(0)
    , m_maxVisiblePinnedTab(0)
    , m_mainBarOverFlowed(false)
    , m_dragOffset(0)
    , m_usesScrollButtons(false)
{
    m_mainTabBar = new TabBarHelper(this);
    m_pinnedTabBar = new TabBarHelper(this);
    m_mainTabBarWidget = new TabBarScrollWidget(m_mainTabBar, this);
    m_pinnedTabBarWidget = new TabBarScrollWidget(m_pinnedTabBar, this);

    m_mainTabBar->setScrollArea(m_mainTabBarWidget->scrollArea());
    m_pinnedTabBar->setScrollArea(m_pinnedTabBarWidget->scrollArea());

    connect(m_mainTabBarWidget->scrollBar(), SIGNAL(rangeChanged(int,int)), this, SLOT(setMinimumWidths()));
    connect(m_mainTabBarWidget->scrollBar(), SIGNAL(valueChanged(int)), this, SIGNAL(scrollBarValueChanged(int)));
    connect(m_pinnedTabBarWidget->scrollBar(), SIGNAL(rangeChanged(int,int)), this, SLOT(setMinimumWidths()));
    connect(m_pinnedTabBarWidget->scrollBar(), SIGNAL(valueChanged(int)), this, SIGNAL(scrollBarValueChanged(int)));
    connect(this, SIGNAL(overFlowChanged(bool)), m_mainTabBarWidget, SLOT(overFlowChanged(bool)));

    m_mainTabBar->setActiveTabBar(true);
    m_pinnedTabBar->setTabsClosable(false);

    m_mainLayout = new QHBoxLayout;
    m_mainLayout->setSpacing(0);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->addWidget(m_pinnedTabBarWidget, 4);
    m_mainLayout->addWidget(m_mainTabBarWidget, 1);
    setLayout(m_mainLayout);

    connect(m_mainTabBar, SIGNAL(currentChanged(int)), this, SLOT(slotCurrentChanged(int)));
    connect(m_mainTabBar, SIGNAL(tabCloseRequested(int)), this, SLOT(slotTabCloseRequested(int)));
    connect(m_mainTabBar, SIGNAL(tabMoved(int,int)), this, SLOT(slotTabMoved(int,int)));

    connect(m_pinnedTabBar, SIGNAL(currentChanged(int)), this, SLOT(slotCurrentChanged(int)));
    connect(m_pinnedTabBar, SIGNAL(tabCloseRequested(int)), this, SLOT(slotTabCloseRequested(int)));
    connect(m_pinnedTabBar, SIGNAL(tabMoved(int,int)), this, SLOT(slotTabMoved(int,int)));

    setAutoFillBackground(false);
    m_mainTabBar->setAutoFillBackground(false);
    m_pinnedTabBar->setAutoFillBackground(false);

    m_mainTabBar->installEventFilter(this);
}

int ComboTabBar::addTab(const QString &text)
{
    return insertTab(-1, text);
}

int ComboTabBar::addTab(const QIcon &icon, const QString &text)
{
    return insertTab(-1, icon, text);
}

int ComboTabBar::insertTab(int index, const QString &text)
{
    return insertTab(index, QIcon(), text);
}

int ComboTabBar::insertTab(int index, const QIcon &icon, const QString &text, bool pinned)
{
    if (pinned) {
        index = m_pinnedTabBar->insertTab(index, icon, text);
    }
    else {
        index = m_mainTabBar->insertTab(index - pinnedTabsCount(), icon, text);

        if (tabsClosable()) {
            QWidget* closeButton = m_mainTabBar->tabButton(index, closeButtonPosition());
            if ((closeButton && closeButton->objectName() != QLatin1String("combotabbar_tabs_close_button")) ||
                    !closeButton) {
                // insert our close button
                insertCloseButton(index + pinnedTabsCount());
                if (closeButton) {
                    closeButton->deleteLater();
                }
            }
        }

        index += pinnedTabsCount();
    }

    updatePinnedTabBarVisibility();
    tabInserted(index);
    setMinimumWidths();

    return index;
}

void ComboTabBar::removeTab(int index)
{
    if (validIndex(index)) {
        localTabBar(index)->removeTab(toLocalIndex(index));
        updatePinnedTabBarVisibility();

        tabRemoved(index);
        setMinimumWidths();
    }
}

void ComboTabBar::moveTab(int from, int to)
{
    if (from >= pinnedTabsCount() && to >= pinnedTabsCount()) {
        m_mainTabBar->moveTab(from - pinnedTabsCount(), to - pinnedTabsCount());
    }
    else if (from < pinnedTabsCount() && to < pinnedTabsCount()) {
        m_pinnedTabBar->moveTab(from, to);
    }
}

bool ComboTabBar::isTabEnabled(int index) const
{
    return localTabBar(index)->isTabEnabled(toLocalIndex(index));
}

void ComboTabBar::setTabEnabled(int index, bool enabled)
{
    localTabBar(index)->setTabEnabled(toLocalIndex(index), enabled);
}

QColor ComboTabBar::tabTextColor(int index) const
{
    return localTabBar(index)->tabTextColor(toLocalIndex(index));
}

void ComboTabBar::setTabTextColor(int index, const QColor &color)
{
    localTabBar(index)->setTabTextColor(toLocalIndex(index), color);
}

QRect ComboTabBar::tabRect(int index) const
{
    QRect rect;
    if (index != -1) {
        bool mainTabBar = index >= pinnedTabsCount();
        rect = localTabBar(index)->tabRect(toLocalIndex(index));

        if (mainTabBar) {
            rect.moveLeft(rect.x() + mapFromGlobal(m_mainTabBar->mapToGlobal(QPoint(0, 0))).x());
            QRect widgetRect = m_mainTabBarWidget->scrollArea()->viewport()->rect();
            widgetRect.moveLeft(widgetRect.x() + mapFromGlobal(m_mainTabBarWidget->scrollArea()->viewport()->mapToGlobal(QPoint(0, 0))).x());
            rect = rect.intersected(widgetRect);
        }
        else {
            rect.moveLeft(rect.x() + mapFromGlobal(m_pinnedTabBar->mapToGlobal(QPoint(0, 0))).x());
            QRect widgetRect = m_pinnedTabBarWidget->scrollArea()->viewport()->rect();
            widgetRect.moveLeft(widgetRect.x() + mapFromGlobal(m_pinnedTabBarWidget->scrollArea()->viewport()->mapToGlobal(QPoint(0, 0))).x());
            rect = rect.intersected(widgetRect);
        }
    }

    return rect;
}

int ComboTabBar::tabAt(const QPoint &pos) const
{
    int index = m_pinnedTabBarWidget->tabAt(pos);

    if (index != -1) {
        return index;
    }

    QPoint p = pos;
    p.setX(p.x() - m_pinnedTabBarWidget->width());

    index = m_mainTabBarWidget->tabAt(p);

    if (index != -1) {
        index += pinnedTabsCount();
    }

    return index;
}

int ComboTabBar::mainTabBarCurrentIndex() const
{
    return (m_mainTabBar->currentIndex() == -1 ? -1 : pinnedTabsCount() + m_mainTabBar->currentIndex());
}

int ComboTabBar::currentIndex() const
{
    if (m_pinnedTabBar->isActiveTabBar()) {
        return m_pinnedTabBar->currentIndex();
    }
    else {
        return (m_mainTabBar->currentIndex() == -1 ? -1 : pinnedTabsCount() + m_mainTabBar->currentIndex());
    }
}

void ComboTabBar::setCurrentIndex(int index)
{
    return localTabBar(index)->setCurrentIndex(toLocalIndex(index));
}

void ComboTabBar::slotCurrentChanged(int index)
{
    if (sender() == m_pinnedTabBar) {
        if (index == -1 && m_mainTabBar->count() > 0) {
            m_mainTabBar->setActiveTabBar(true);
            m_pinnedTabBar->setActiveTabBar(false);
            emit currentChanged(pinnedTabsCount());
        }
        else {
            m_pinnedTabBar->setActiveTabBar(true);
            m_mainTabBar->setActiveTabBar(false);
            emit currentChanged(index);
        }
    }
    else {
        if (index == -1 && pinnedTabsCount() > 0) {
            m_pinnedTabBar->setActiveTabBar(true);
            m_mainTabBar->setActiveTabBar(false);
            emit currentChanged(pinnedTabsCount() - 1);
        }
        else {
            m_mainTabBar->setActiveTabBar(true);
            m_pinnedTabBar->setActiveTabBar(false);
            emit currentChanged(index + pinnedTabsCount());
        }
    }
}

void ComboTabBar::slotTabCloseRequested(int index)
{
    if (sender() == m_pinnedTabBar) {
        emit tabCloseRequested(index);
    }
    else {
        emit tabCloseRequested(index + pinnedTabsCount());
    }
}

void ComboTabBar::slotTabMoved(int from, int to)
{
    if (sender() == m_pinnedTabBar) {
        emit tabMoved(from, to);
    }
    else {
        emit tabMoved(from + pinnedTabsCount(), to + pinnedTabsCount());
    }
}

void ComboTabBar::closeTabFromButton()
{
    QWidget* button = qobject_cast<QWidget*>(sender());

    int tabToClose = -1;

    for (int i = 0; i < m_mainTabBar->count(); ++i) {
        if (m_mainTabBar->tabButton(i, closeButtonPosition()) == button) {
            tabToClose = i;
            break;
        }
    }

    if (tabToClose != -1) {
        emit tabCloseRequested(tabToClose + pinnedTabsCount());
    }
}

int ComboTabBar::count() const
{
    return pinnedTabsCount() + m_mainTabBar->count();
}

void ComboTabBar::setDrawBase(bool drawTheBase)
{
    m_mainTabBar->setDrawBase(drawTheBase);
    m_pinnedTabBar->setDrawBase(drawTheBase);
}

bool ComboTabBar::drawBase() const
{
    return m_mainTabBar->drawBase();
}

Qt::TextElideMode ComboTabBar::elideMode() const
{
    return m_mainTabBar->elideMode();
}

void ComboTabBar::setElideMode(Qt::TextElideMode elide)
{
    m_mainTabBar->setElideMode(elide);
    m_pinnedTabBar->setElideMode(elide);
}

QString ComboTabBar::tabText(int index) const
{
    return localTabBar(index)->tabText(toLocalIndex(index));
}

void ComboTabBar::setTabText(int index, const QString &text)
{
    localTabBar(index)->setTabText(toLocalIndex(index), text);
}

void ComboTabBar::setTabToolTip(int index, const QString &tip)
{
    localTabBar(index)->setTabToolTip(toLocalIndex(index), tip);
}

QString ComboTabBar::tabToolTip(int index) const
{
    return localTabBar(index)->tabToolTip(toLocalIndex(index));
}

bool ComboTabBar::tabsClosable() const
{
    return m_mainTabBar->tabsClosable();
}

void ComboTabBar::setTabsClosable(bool closable)
{
    if (closable == tabsClosable()) {
        return;
    }

    if (closable) {
        // insert our close button
        for (int i = 0; i < m_mainTabBar->count(); ++i) {
            QWidget* closeButton = m_mainTabBar->tabButton(i, closeButtonPosition());
            if (closeButton) {
                if (closeButton->objectName() == QLatin1String("combotabbar_tabs_close_button")) {
                    continue;
                }
            }

            insertCloseButton(i + pinnedTabsCount());
            if (closeButton) {
                closeButton->deleteLater();
            }
        }
    }
    m_mainTabBar->setTabsClosable(closable);
}

void ComboTabBar::setTabButton(int index, QTabBar::ButtonPosition position, QWidget* widget)
{
    localTabBar(index)->setTabButton(toLocalIndex(index), position, widget);
}

QWidget* ComboTabBar::tabButton(int index, QTabBar::ButtonPosition position) const
{
    return localTabBar(index)->tabButton(toLocalIndex(index), position);
}

QTabBar::SelectionBehavior ComboTabBar::selectionBehaviorOnRemove() const
{
    return m_mainTabBar->selectionBehaviorOnRemove();
}

void ComboTabBar::setSelectionBehaviorOnRemove(QTabBar::SelectionBehavior behavior)
{
    m_mainTabBar->setSelectionBehaviorOnRemove(behavior);
    m_pinnedTabBar->setSelectionBehaviorOnRemove(behavior);
}

bool ComboTabBar::expanding() const
{
    return m_mainTabBar->expanding();
}

void ComboTabBar::setExpanding(bool enabled)
{
    m_mainTabBar->setExpanding(enabled);
    m_pinnedTabBar->setExpanding(enabled);
}

bool ComboTabBar::isMovable() const
{
    return m_mainTabBar->isMovable();
}

void ComboTabBar::setMovable(bool movable)
{
    m_mainTabBar->setMovable(movable);
    m_pinnedTabBar->setMovable(movable);
}

bool ComboTabBar::documentMode() const
{
    return m_mainTabBar->documentMode();
}

void ComboTabBar::setDocumentMode(bool set)
{
    m_mainTabBar->setDocumentMode(set);
    m_pinnedTabBar->setDocumentMode(set);
}

int ComboTabBar::pinnedTabsCount() const
{
    return m_pinnedTabBar->count();
}

int ComboTabBar::normalTabsCount() const
{
    return m_mainTabBar->count();
}

bool ComboTabBar::isPinned(int index) const
{
    return index >= 0 && index < pinnedTabsCount();
}

void ComboTabBar::setMaxVisiblePinnedTab(int max)
{
    m_maxVisiblePinnedTab = max;
    setMinimumWidths();
}

void ComboTabBar::setObjectName(const QString &name)
{
    m_mainTabBar->setObjectName(name);
    m_pinnedTabBar->setObjectName(name);

    m_pinnedTabBarWidget->setContainersName(name);
    m_mainTabBarWidget->setContainersName(name);
}

void ComboTabBar::setMouseTracking(bool enable)
{
    m_mainTabBarWidget->scrollArea()->setMouseTracking(enable);
    m_mainTabBarWidget->setMouseTracking(enable);
    m_mainTabBar->setMouseTracking(enable);

    m_pinnedTabBarWidget->scrollArea()->setMouseTracking(enable);
    m_pinnedTabBarWidget->setMouseTracking(enable);
    m_pinnedTabBar->setMouseTracking(enable);

    QWidget::setMouseTracking(enable);
}

void ComboTabBar::setUpLayout()
{
    int height = qMax(m_mainTabBar->height(), m_pinnedTabBar->height());

    // Workaround for Oxygen theme. For some reason, QTabBar::height() returns bigger
    // height than it actually should.
    if (mApp->proxyStyle() && mApp->proxyStyle()->name() == QLatin1String("oxygen")) {
        height -= 4;
    }

    // We need to setup heights even before m_mainTabBar->height() has correct value
    // So lets just set minimum 5px height
    height = qMax(5, height);

    setFixedHeight(height);
    m_pinnedTabBar->setFixedHeight(height);
    m_mainTabBarWidget->setUpLayout();
    m_pinnedTabBarWidget->setUpLayout();

    setMinimumWidths();
}

void ComboTabBar::insertCloseButton(int index)
{
    index -= pinnedTabsCount();
    if (index < 0) {
        return;
    }

    QAbstractButton* closeButton = new CloseButton(this);
    closeButton->setToolTip(m_closeButtonsToolTip);
    connect(closeButton, SIGNAL(clicked()), this, SLOT(closeTabFromButton()));
    m_mainTabBar->setTabButton(index, closeButtonPosition(), closeButton);
}

void ComboTabBar::setCloseButtonsToolTip(const QString &tip)
{
    m_closeButtonsToolTip = tip;
}

void ComboTabBar::enableBluredBackground(bool enable)
{
    m_mainTabBar->enableBluredBackground(enable);
    m_pinnedTabBar->enableBluredBackground(enable);
    m_mainTabBarWidget->enableBluredBackground(enable);
    m_pinnedTabBarWidget->enableBluredBackground(enable);
}

int ComboTabBar::mainTabBarWidth() const
{
    return m_mainTabBar->width();
}

int ComboTabBar::pinTabBarWidth() const
{
    return m_pinnedTabBarWidget->isHidden() ? 0 : m_pinnedTabBarWidget->width();
}

void ComboTabBar::wheelEvent(QWheelEvent* event)
{
    event->accept();
    if (m_mainTabBarWidget->underMouse()) {
        if (m_mainTabBarWidget->scrollBar()->isOverFlowed()) {
            m_mainTabBarWidget->scrollByWheel(event);
        }
        else if (m_pinnedTabBarWidget->scrollBar()->isOverFlowed()) {
            m_pinnedTabBarWidget->scrollByWheel(event);
        }
    }
    else if (m_pinnedTabBarWidget->underMouse()) {
        if (m_pinnedTabBarWidget->scrollBar()->isOverFlowed()) {
            m_pinnedTabBarWidget->scrollByWheel(event);
        }
        else if (m_mainTabBarWidget->scrollBar()->isOverFlowed()) {
            m_mainTabBarWidget->scrollByWheel(event);
        }
    }

    if (!m_mainTabBarWidget->scrollBar()->isOverFlowed() && !m_pinnedTabBarWidget->scrollBar()->isOverFlowed()) {
        setCurrentNextEnabledIndex(event->delta() > 0 ? -1 : 1);
    }
}

void ComboTabBar::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);

    if (m_mainBarOverFlowed != m_mainTabBarWidget->scrollBar()->isOverFlowed()) {
        setMinimumWidths();
    }
}

bool ComboTabBar::eventFilter(QObject* obj, QEvent* ev)
{
    if (obj == m_mainTabBar && ev->type() == QEvent::Resize) {
        QResizeEvent* event = static_cast<QResizeEvent*>(ev);
        if (event->oldSize().height() != event->size().height()) {
            setUpLayout();
        }
    }

    return QWidget::eventFilter(obj, ev);
}

int ComboTabBar::comboTabBarPixelMetric(ComboTabBar::SizeType sizeType) const
{
    switch (sizeType) {
    case ExtraReservedWidth:
        return 0;

    case NormalTabMaximumWidth:
        return 150;

    case ActiveTabMinimumWidth:
    case NormalTabMinimumWidth:
    case OverflowedTabWidth:
        return 100;

    case PinnedTabWidth:
        return 30;

    default:
        break;
    }

    return -1;
}

QTabBar::ButtonPosition ComboTabBar::iconButtonPosition()
{
    return (closeButtonPosition() == QTabBar::RightSide ? QTabBar::LeftSide : QTabBar::RightSide);
}

QTabBar::ButtonPosition ComboTabBar::closeButtonPosition()
{
    return (QTabBar::ButtonPosition)style()->styleHint(QStyle::SH_TabBar_CloseButtonPosition, 0, this);
}

bool ComboTabBar::validIndex(int index) const
{
    return (index >= 0 && index < count());
}

void ComboTabBar::setCurrentNextEnabledIndex(int offset)
{
    for (int index = currentIndex() + offset; validIndex(index); index += offset) {
        if (isTabEnabled(index)) {
            setCurrentIndex(index);
            break;
        }
    }
}

bool ComboTabBar::usesScrollButtons() const
{
    return m_mainTabBarWidget->usesScrollButtons();
}

void ComboTabBar::setUsesScrollButtons(bool useButtons)
{
    m_mainTabBarWidget->setUsesScrollButtons(useButtons);
}

void ComboTabBar::addMainBarWidget(QWidget* widget, Qt::Alignment align, int stretch, Qt::Alignment layoutAlignment)
{
    if (align == Qt::AlignRight) {
        m_mainTabBarWidget->addRightWidget(widget, stretch, layoutAlignment);
    }
    else {
        m_mainTabBarWidget->addLeftWidget(widget, stretch, layoutAlignment);
    }
}

void ComboTabBar::ensureVisible(int index, int xmargin)
{
    if (index == -1) {
        index = currentIndex();
    }

    if (index < pinnedTabsCount()) {
        if (xmargin == -1) {
            xmargin = qMax(20, comboTabBarPixelMetric(PinnedTabWidth));
        }
        m_pinnedTabBarWidget->ensureVisible(index, xmargin);
    }
    else {
        if (xmargin == -1) {
            xmargin = comboTabBarPixelMetric(OverflowedTabWidth);
        }
        index -= pinnedTabsCount();
        m_mainTabBarWidget->ensureVisible(index, xmargin);
    }
}

QSize ComboTabBar::tabSizeHint(int index, bool fast) const
{
    Q_UNUSED(fast)

    return localTabBar(index)->baseClassTabSizeHint(toLocalIndex(index));
}

void ComboTabBar::tabInserted(int index)
{
    Q_UNUSED(index)
}

void ComboTabBar::tabRemoved(int index)
{
    Q_UNUSED(index)
}

TabBarHelper* ComboTabBar::localTabBar(int index) const
{
    if (index < 0 || index >= pinnedTabsCount()) {
        return m_mainTabBar;
    }
    else {
        return m_pinnedTabBar;
    }
}

int ComboTabBar::toLocalIndex(int globalIndex) const
{
    if (globalIndex < 0) {
        return -1;
    }

    if (globalIndex >= pinnedTabsCount()) {
        return globalIndex - pinnedTabsCount();
    }
    else {
        return globalIndex;
    }
}

void ComboTabBar::updatePinnedTabBarVisibility()
{
    m_pinnedTabBarWidget->setVisible(pinnedTabsCount() > 0);

    if (pinnedTabsCount() > 0) {
        m_pinnedTabBarWidget->setFixedHeight(m_mainTabBarWidget->height());
        m_pinnedTabBar->setFixedHeight(m_mainTabBar->height());
    }
}

void ComboTabBar::setMinimumWidths()
{
    if (!isVisible() || comboTabBarPixelMetric(PinnedTabWidth) < 0) {
        return;
    }

    int pinnedTabBarWidth = pinnedTabsCount() * comboTabBarPixelMetric(PinnedTabWidth);
    m_pinnedTabBar->setMinimumWidth(pinnedTabBarWidth);

    if (m_maxVisiblePinnedTab > 0) {
        pinnedTabBarWidth = qMin(pinnedTabBarWidth, m_maxVisiblePinnedTab * comboTabBarPixelMetric(PinnedTabWidth));
    }

    m_pinnedTabBarWidget->setMaximumWidth(pinnedTabBarWidth);

    int mainTabBarWidth = comboTabBarPixelMetric(NormalTabMinimumWidth) * (m_mainTabBar->count() - 1) +
                          comboTabBarPixelMetric(ActiveTabMinimumWidth) +
                          comboTabBarPixelMetric(ExtraReservedWidth);

    if (mainTabBarWidth <= m_mainTabBarWidget->width()) {
        m_mainTabBar->useFastTabSizeHint(false);
        emit overFlowChanged(false);
        m_mainTabBar->setMinimumWidth(mainTabBarWidth);
        m_mainBarOverFlowed = false;
    }
    else {
        emit overFlowChanged(true);
        // The following line is the cause of calling tabSizeHint() for all tabs that is
        // time consuming, Because of this we notify application to using a lighter
        // version of it. (this is safe because all normal tabs have the same size)
        m_mainTabBar->useFastTabSizeHint(true);
        if (m_mainTabBar->count() * comboTabBarPixelMetric(OverflowedTabWidth) != m_mainTabBar->minimumWidth()) {
            m_mainTabBar->setMinimumWidth(m_mainTabBar->count() * comboTabBarPixelMetric(OverflowedTabWidth));
        }
        m_mainBarOverFlowed = true;
    }
}

void ComboTabBar::showEvent(QShowEvent* event)
{
    if (!event->spontaneous()) {
        QTimer::singleShot(0, this, SLOT(setUpLayout()));
    }

    QWidget::showEvent(event);
}


TabBarHelper::TabBarHelper(ComboTabBar* comboTabBar)
    : QTabBar(comboTabBar)
    , m_comboTabBar(comboTabBar)
    , m_scrollArea(0)
    , m_pressedIndex(-1)
    , m_pressedGlobalX(-1)
    , m_dragInProgress(false)
    , m_activeTabBar(false)
    , m_useFastTabSizeHint(false)
    , m_bluredBackground(false)
{
}

void TabBarHelper::setTabButton(int index, QTabBar::ButtonPosition position, QWidget* widget)
{
    QTabBar::setTabButton(index, position, widget);
}

QSize TabBarHelper::tabSizeHint(int index) const
{
    if (this == m_comboTabBar->mainTabBar()) {
        index += m_comboTabBar->pinnedTabsCount();
    }
    return m_comboTabBar->tabSizeHint(index, m_useFastTabSizeHint);
}

QSize TabBarHelper::baseClassTabSizeHint(int index) const
{
    return QTabBar::tabSizeHint(index);
}

bool TabBarHelper::isActiveTabBar()
{
    return m_activeTabBar;
}

void TabBarHelper::setActiveTabBar(bool activate)
{
    if (m_activeTabBar != activate) {
        m_activeTabBar = activate;
        update();
    }
}

void TabBarHelper::setScrollArea(QScrollArea* scrollArea)
{
    m_scrollArea = scrollArea;
}

void TabBarHelper::useFastTabSizeHint(bool enabled)
{
    m_useFastTabSizeHint = enabled;
}

bool TabBarHelper::isDisplayedOnViewPort(int globalLeft, int globalRight)
{
    bool isVisible = true;

    if (m_scrollArea) {
        if (globalRight < m_scrollArea->viewport()->mapToGlobal(QPoint(0, 0)).x() ||
                globalLeft > m_scrollArea->viewport()->mapToGlobal(m_scrollArea->viewport()->rect().topRight()).x()) {
            isVisible = false;
        }
    }

    return isVisible;
}

void TabBarHelper::enableBluredBackground(bool enable)
{
    if (enable != m_bluredBackground) {
        m_bluredBackground = enable;
        update();
    }
}

void TabBarHelper::setCurrentIndex(int index)
{
    if (index == currentIndex() && !m_activeTabBar) {
        emit currentChanged(currentIndex());
    }

    QTabBar::setCurrentIndex(index);
}

bool TabBarHelper::event(QEvent* ev)
{
    switch (ev->type()) {
    case QEvent::ToolTip:
    case QEvent::Wheel:
        ev->ignore();
        return false;
        break;
    default:
        break;
    }

    QTabBar::event(ev);
    ev->ignore();
    return false;
}

// taken from qtabbar.cpp
void TabBarHelper::initStyleBaseOption(QStyleOptionTabBarBaseV2* optTabBase, QTabBar* tabbar, QSize size)
{
    QStyleOptionTab tabOverlap;
    tabOverlap.shape = tabbar->shape();
    int overlap = tabbar->style()->pixelMetric(QStyle::PM_TabBarBaseOverlap, &tabOverlap, tabbar);
    QWidget* theParent = tabbar->parentWidget();
    optTabBase->init(tabbar);
    optTabBase->shape = tabbar->shape();
    optTabBase->documentMode = tabbar->documentMode();
    if (theParent && overlap > 0) {
        QRect rect;
        switch (tabOverlap.shape) {
        case QTabBar::RoundedNorth:
        case QTabBar::TriangularNorth:
            rect.setRect(0, size.height() - overlap, size.width(), overlap);
            break;
        case QTabBar::RoundedSouth:
        case QTabBar::TriangularSouth:
            rect.setRect(0, 0, size.width(), overlap);
            break;
        case QTabBar::RoundedEast:
        case QTabBar::TriangularEast:
            rect.setRect(0, 0, overlap, size.height());
            break;
        case QTabBar::RoundedWest:
        case QTabBar::TriangularWest:
            rect.setRect(size.width() - overlap, 0, overlap, size.height());
            break;
        }
        optTabBase->rect = rect;
    }
}

// some codes were taken from qtabbar.cpp
void TabBarHelper::paintEvent(QPaintEvent* event)
{
    if (m_bluredBackground) {
        QPainter p(this);
        p.setCompositionMode(QPainter::CompositionMode_Clear);
        p.fillRect(event->rect(), QColor(0, 0, 0, 0));
    }

    // note: this code doesn't support vertical tabs
    if (!m_dragInProgress) {
        QStyleOptionTabBarBaseV2 optTabBase;
        initStyleBaseOption(&optTabBase, this, size());

        QStylePainter p(this);
        int selected = currentIndex();

        for (int i = 0; i < count(); ++i) {
            optTabBase.tabBarRect |= tabRect(i);
        }

        optTabBase.selectedTabRect = QRect();

        if (drawBase()) {
            p.drawPrimitive(QStyle::PE_FrameTabBarBase, optTabBase);
        }

        for (int i = 0; i < count(); ++i) {
            QStyleOptionTabV3 tab;
            initStyleOption(&tab, i);

            if (!(tab.state & QStyle::State_Enabled)) {
                tab.palette.setCurrentColorGroup(QPalette::Disabled);
            }

            // Don't bother drawing a tab if the entire tab is outside of the visible tab bar.
            if (!isDisplayedOnViewPort(mapToGlobal(tab.rect.topLeft()).x(), mapToGlobal(tab.rect.topRight()).x())) {
                continue;
            }

            if (i == selected) {
                continue;
            }

            // update mouse over state when scrolling
            tab.state = tab.state & ~QStyle::State_MouseOver;
            int index = tabAt(mapFromGlobal(QCursor::pos()));
            if (i == index) {
                tab.state = tab.state | QStyle::State_MouseOver;
            }

            p.drawControl(QStyle::CE_TabBarTab, tab);
        }

        // Draw the selected tab last to get it "on top"
        if (selected >= 0) {
            QStyleOptionTabV3 tab;
            initStyleOption(&tab, selected);

            if (!m_activeTabBar) {
                tab.state = tab.state & ~QStyle::State_Selected;
            }

            p.drawControl(QStyle::CE_TabBarTab, tab);
        }
    }
    else {
        QTabBar::paintEvent(event);
    }

#if 0
    if (m_scrollArea) {
        const int tearWidth = 15;
        const int maxAlpha = 200;
        const int colorId = 150;
        const bool ltr = isLeftToRight();
        QWidget* viewPort = m_scrollArea->viewport();
        QPoint globalTopLeft = ltr ? viewPort->mapToGlobal(QPoint(0, 0)) :
                               viewPort->mapToGlobal(QPoint(viewPort->width() - tearWidth, 0));
        if (m_scrollArea->horizontalScrollBar()->value() > m_scrollArea->horizontalScrollBar()->minimum()) {
            QPainter p(this);
            QPoint localTopLeft = mapFromGlobal(globalTopLeft);
            QLinearGradient fade(localTopLeft, localTopLeft + QPoint(tearWidth, 0));
            fade.setColorAt(ltr ? 0 : 1, QColor(colorId, colorId, colorId, maxAlpha));
            fade.setColorAt(ltr ? 1 : 0, QColor(colorId, colorId, colorId, 0));
            p.fillRect(QRect(localTopLeft, QSize(tearWidth, height())), fade);
        }

        QPoint globalTopRight = ltr ? viewPort->mapToGlobal(QPoint(viewPort->width() - tearWidth, 0)) :
                                viewPort->mapToGlobal(QPoint(0, 0));
        if (m_scrollArea->horizontalScrollBar()->value() < m_scrollArea->horizontalScrollBar()->maximum()) {
            QPainter p(this);
            globalTopRight = mapFromGlobal(globalTopRight);
            QLinearGradient fade(globalTopRight, globalTopRight + QPoint(tearWidth, 0));
            fade.setColorAt(ltr ? 0 : 1, QColor(colorId, colorId, colorId, 0));
            fade.setColorAt(ltr ? 1 : 0, QColor(colorId, colorId, colorId, maxAlpha));
            p.fillRect(QRect(globalTopRight, QSize(tearWidth, height())), fade);
        }
    }
#endif
}

void TabBarHelper::mousePressEvent(QMouseEvent* event)
{
    event->ignore();
    if (event->button() == Qt::LeftButton) {
        m_pressedIndex = tabAt(event->pos());
        if (m_pressedIndex != -1) {
            m_pressedGlobalX = event->globalX();
            m_dragInProgress = true;
            // virtualize selecting tab by click
            if (m_pressedIndex == currentIndex() && !m_activeTabBar) {
                emit currentChanged(currentIndex());
            }
        }
    }

    QTabBar::mousePressEvent(event);
}

void TabBarHelper::mouseReleaseEvent(QMouseEvent* event)
{
    event->ignore();
    QTabBar::mouseReleaseEvent(event);

    if (m_pressedIndex >= 0) {
        int length = qAbs(m_pressedGlobalX - event->globalX());
        int duration = qMin((length * ANIMATION_DURATION) / tabRect(m_pressedIndex).width(),
                            ANIMATION_DURATION);

        m_pressedIndex = -1;
        m_pressedGlobalX = -1;
        QTimer::singleShot(duration, this, SLOT(resetDragState()));
    }
}

void TabBarHelper::resetDragState()
{
    if (m_pressedIndex == -1) {
        m_dragInProgress = false;
        update();
    }
}

TabScrollBar::TabScrollBar(QWidget* parent)
    : QScrollBar(Qt::Horizontal, parent)
    , m_animation(0)
{
}

TabScrollBar::~TabScrollBar()
{
}

void TabScrollBar::animateToValue(int to, QEasingCurve::Type type)
{
    if (!m_animation) {
        m_animation = new QPropertyAnimation(this, "value", this);
    }
    m_animation->setEasingCurve(type);

    int current = value();
    to = qBound(minimum(), to, maximum());
    int lenght = qAbs(to - current);

    int duration = qMin(1500, 200 + lenght / 2);

    m_animation->setDuration(duration);

    if (m_animation->state() != QAbstractAnimation::Running) {
        m_animation->setStartValue(value());
    }
    m_animation->setEndValue(to);
    m_animation->start();
}

bool TabScrollBar::isOverFlowed()
{
    return maximum() != minimum();
}

void TabScrollBar::wheelEvent(QWheelEvent* event)
{
    int delta = isRightToLeft() ? -event->delta() : event->delta();
    QWheelEvent fakeEvent(event->pos(), delta, event->buttons(),
                          event->modifiers(), Qt::Vertical);
    QScrollBar::wheelEvent(&fakeEvent);
    event->accept();
}


TabBarScrollWidget::TabBarScrollWidget(QTabBar* tabBar, QWidget* parent)
    : QWidget(parent)
    , m_tabBar(tabBar)
    , m_usesScrollButtons(false)
    , m_bluredBackground(false)
    , m_scrollByButtonAnim(0)
{
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setFrameStyle(QFrame::NoFrame);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_scrollBar = new TabScrollBar(m_scrollArea);
    m_scrollArea->setHorizontalScrollBar(m_scrollBar);
    m_scrollArea->setWidget(m_tabBar);

    m_leftScrollButton = new ToolButton(this);
    m_leftScrollButton->setAutoRaise(true);
    m_leftScrollButton->setObjectName("tabbar-button-left");
    connect(m_leftScrollButton, SIGNAL(pressed()), this, SLOT(scrollStart()));
    connect(m_leftScrollButton, SIGNAL(released()), this, SLOT(scrollStop()));
    connect(m_leftScrollButton, SIGNAL(doubleClicked()), this, SLOT(scrollToLeftEdge()));
    connect(m_leftScrollButton, SIGNAL(middleMouseClicked()), this, SLOT(ensureVisible()));

    m_rightScrollButton = new ToolButton(this);
    m_rightScrollButton->setAutoRaise(true);
    m_rightScrollButton->setObjectName("tabbar-button-right");
    connect(m_rightScrollButton, SIGNAL(pressed()), this, SLOT(scrollStart()));
    connect(m_rightScrollButton, SIGNAL(released()), this, SLOT(scrollStop()));
    connect(m_rightScrollButton, SIGNAL(doubleClicked()), this, SLOT(scrollToRightEdge()));
    connect(m_rightScrollButton, SIGNAL(middleMouseClicked()), this, SLOT(ensureVisible()));

    m_leftLayout = new QHBoxLayout;
    m_leftLayout->setSpacing(0);
    m_leftLayout->setContentsMargins(0, 0, 0, 0);
    m_rightLayout = new QHBoxLayout;
    m_rightLayout->setSpacing(0);
    m_rightLayout->setContentsMargins(0, 0, 0, 0);

    QHBoxLayout* leftLayout = new QHBoxLayout;
    leftLayout->setSpacing(0);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->addLayout(m_leftLayout);
    leftLayout->addWidget(m_leftScrollButton);
    QHBoxLayout* rightLayout = new QHBoxLayout;
    rightLayout->setSpacing(0);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->addWidget(m_rightScrollButton);
    rightLayout->addLayout(m_rightLayout);

    m_leftContainer = new QWidget(this);
    m_leftContainer->setLayout(leftLayout);
    m_rightContainer = new QWidget(this);
    m_rightContainer->setLayout(rightLayout);
    m_leftContainer->installEventFilter(this);
    m_rightContainer->installEventFilter(this);

    QHBoxLayout* hLayout = new QHBoxLayout;
    hLayout->setSpacing(0);
    hLayout->setContentsMargins(0, 0, 0, 0);
    hLayout->addWidget(m_leftContainer);
    hLayout->addWidget(m_scrollArea);
    hLayout->addWidget(m_rightContainer);
    setLayout(hLayout);

    m_scrollArea->viewport()->setAutoFillBackground(false);
    connect(m_scrollBar, SIGNAL(valueChanged(int)), this, SLOT(scrollBarValueChange()));

    scrollBarValueChange();
    overFlowChanged(false);
}

void TabBarScrollWidget::addLeftWidget(QWidget* widget, int stretch, Qt::Alignment alignment)
{
    m_leftLayout->addWidget(widget, stretch, alignment);
}

void TabBarScrollWidget::addRightWidget(QWidget* widget, int stretch, Qt::Alignment alignment)
{
    m_rightLayout->addWidget(widget, stretch, alignment);
}

QTabBar* TabBarScrollWidget::tabBar()
{
    return m_tabBar;
}

QScrollArea* TabBarScrollWidget::scrollArea()
{
    return m_scrollArea;
}

TabScrollBar* TabBarScrollWidget::scrollBar()
{
    return m_scrollBar;
}

void TabBarScrollWidget::ensureVisible(int index, int xmargin)
{
    if (index == -1) {
        index = m_tabBar->currentIndex();
    }

    if (index < 0 || index >= m_tabBar->count()) {
        return;
    }

    xmargin = qMin(xmargin, m_scrollArea->viewport()->width() / 2);

    // Qt Bug? the following lines were taken from QScrollArea::ensureVisible() and
    // then were fixed. The original version caculates wrong values in RTL layouts.
    const QRect logicalTabRect = QStyle::visualRect(m_tabBar->layoutDirection(), m_tabBar->rect(), m_tabBar->tabRect(index));
    int logicalX = QStyle::visualPos(Qt::LeftToRight, m_scrollArea->viewport()->rect(), logicalTabRect.center()).x();

    if (logicalX - xmargin < m_scrollBar->value()) {
        m_scrollBar->animateToValue(qMax(0, logicalX - xmargin));
    }
    else if (logicalX > m_scrollBar->value() + m_scrollArea->viewport()->width() - xmargin) {
        m_scrollBar->animateToValue(qMin(logicalX - m_scrollArea->viewport()->width() + xmargin,
                                         m_scrollBar->maximum()));
    }
}

void TabBarScrollWidget::scrollToLeft(int n)
{
    n = qMax(1, n);
    m_scrollBar->animateToValue(m_scrollBar->value() - n * m_scrollBar->singleStep(), QEasingCurve::Linear);
}

void TabBarScrollWidget::scrollToRight(int n)
{
    n = qMax(1, n);
    m_scrollBar->animateToValue(m_scrollBar->value() + n * m_scrollBar->singleStep(), QEasingCurve::Linear);
}

void TabBarScrollWidget::scrollToLeftEdge()
{
    m_scrollBar->animateToValue(m_scrollBar->minimum());
}

void TabBarScrollWidget::scrollToRightEdge()
{
    m_scrollBar->animateToValue(m_scrollBar->maximum());
}

void TabBarScrollWidget::setUpLayout()
{
    const int height = m_tabBar->height();

    setFixedHeight(height);
    m_leftContainer->setFixedHeight(height);
    m_rightContainer->setFixedHeight(height);
}

void TabBarScrollWidget::scrollBarValueChange()
{
    m_leftScrollButton->setEnabled(m_scrollBar->value() != m_scrollBar->minimum());
    m_rightScrollButton->setEnabled(m_scrollBar->value() != m_scrollBar->maximum());
}

void TabBarScrollWidget::overFlowChanged(bool overflowed)
{
    m_leftScrollButton->setVisible(overflowed && m_usesScrollButtons);
    m_rightScrollButton->setVisible(overflowed && m_usesScrollButtons);

    // a workaround for UI issue of buttons on very fast resizing
    if (m_rightContainer->isVisible()) {
        m_rightContainer->hide();
        m_rightContainer->show();
    }
    if (m_leftContainer->isVisible()) {
        m_leftContainer->hide();
        m_leftContainer->show();
    }
}

void TabBarScrollWidget::scrollStart()
{
    if (QApplication::keyboardModifiers() & Qt::CTRL) {
        if (sender() == m_leftScrollButton) {
            scrollToLeftEdge();
        }
        else if (sender() == m_rightScrollButton) {
            scrollToRightEdge();
        }
        return;
    }

    if (!m_scrollByButtonAnim) {
        m_scrollByButtonAnim = new QPropertyAnimation(m_scrollBar, "value", this);
        m_scrollByButtonAnim->setEasingCurve(QEasingCurve::Linear);
    }
    m_scrollByButtonAnim->stop();
    int len = m_scrollBar->value();
    m_scrollByButtonAnim->setStartValue(len);
    if (sender() == m_leftScrollButton) {
        len = len - m_scrollBar->minimum();
        m_scrollByButtonAnim->setEndValue(m_scrollBar->minimum());
    }
    else if (sender() == m_rightScrollButton) {
        len = m_scrollBar->maximum() - len;
        m_scrollByButtonAnim->setEndValue(m_scrollBar->maximum());
    }
    m_scrollByButtonAnim->setDuration(len * 3);
    m_scrollByButtonAnim->start();
}

void TabBarScrollWidget::scrollStop()
{
    if (m_scrollByButtonAnim) {
        m_scrollByButtonAnim->stop();
    }
}

bool TabBarScrollWidget::eventFilter(QObject* obj, QEvent* ev)
{
    if (m_bluredBackground) {
        if (ev->type() == QEvent::Paint && (obj == m_leftContainer || obj == m_rightContainer)) {
            QPaintEvent* event = static_cast<QPaintEvent*>(ev);
            QPainter p(qobject_cast<QWidget*>(obj));
            p.setCompositionMode(QPainter::CompositionMode_Clear);
            p.fillRect(event->rect(), QColor(0, 0, 0, 0));
        }
    }

    return QWidget::eventFilter(obj, ev);
}

void TabBarScrollWidget::scrollByWheel(QWheelEvent* event)
{
    event->accept();

    // support for some finer mouse
    static int totalDeltas = 0;

    if (totalDeltas * event->delta() < 0) {
        // direction has changed from last time
        totalDeltas = 0;
    }
    totalDeltas += event->delta();

    int factor = qMax(m_scrollBar->pageStep() / 3, m_scrollBar->singleStep());
    if ((event->modifiers() & Qt::ControlModifier) || (event->modifiers() & Qt::ShiftModifier)) {
        factor = m_scrollBar->pageStep();
    }

    int offset = (totalDeltas / 120) * factor;
    if (offset != 0) {
        if (isRightToLeft()) {
            m_scrollBar->animateToValue(m_scrollBar->value() + offset);
        }
        else {
            m_scrollBar->animateToValue(m_scrollBar->value() - offset);
        }

        totalDeltas -= (offset / factor) * 120;
    }
}

bool TabBarScrollWidget::usesScrollButtons() const
{
    return m_usesScrollButtons;
}

void TabBarScrollWidget::setUsesScrollButtons(bool useButtons)
{
    if (useButtons != m_usesScrollButtons) {
        m_usesScrollButtons = useButtons;
        scrollBarValueChange();
        m_tabBar->setElideMode(m_tabBar->elideMode());
    }
}

int TabBarScrollWidget::tabAt(const QPoint &pos) const
{
    if (!m_leftScrollButton->isVisible()) {
        return m_tabBar->tabAt(pos);
    }


    QPoint p = pos;
    p.setX(p.x() - m_leftScrollButton->width() - m_scrollArea->viewport()->width());

    if (m_leftScrollButton->rect().contains(pos) || m_rightScrollButton->rect().contains(p)) {
        return -1;
    }

    return m_tabBar->tabAt(m_tabBar->mapFromGlobal(mapToGlobal(pos)));
}

void TabBarScrollWidget::setContainersName(const QString &name)
{
    m_leftContainer->setObjectName(name);
    m_rightContainer->setObjectName(name);
}

void TabBarScrollWidget::enableBluredBackground(bool enable)
{
    if (enable != m_bluredBackground) {
        m_bluredBackground = enable;
        update();
    }
}

void TabBarScrollWidget::mouseMoveEvent(QMouseEvent* event)
{
    event->ignore();
}


CloseButton::CloseButton(QWidget* parent)
    : QAbstractButton(parent)
{
    setObjectName("combotabbar_tabs_close_button");
    setFocusPolicy(Qt::NoFocus);
    setCursor(Qt::ArrowCursor);

    resize(sizeHint());
}

QSize CloseButton::sizeHint() const
{
    ensurePolished();
    static int width = style()->pixelMetric(QStyle::PM_TabCloseIndicatorWidth, 0, this);
    static int height = style()->pixelMetric(QStyle::PM_TabCloseIndicatorHeight, 0, this);
    return QSize(width, height);
}

QSize CloseButton::minimumSizeHint() const
{
    return sizeHint();
}

void CloseButton::enterEvent(QEvent* event)
{
    if (isEnabled()) {
        update();
    }

    QAbstractButton::enterEvent(event);
}

void CloseButton::leaveEvent(QEvent* event)
{
    if (isEnabled()) {
        update();
    }

    QAbstractButton::leaveEvent(event);
}

void CloseButton::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    QStyleOption opt;
    opt.init(this);
    opt.state |= QStyle::State_AutoRaise;

    // update raised state on scrolling
    bool isUnderMouse = rect().contains(mapFromGlobal(QCursor::pos()));

    if (isEnabled() && isUnderMouse && !isChecked() && !isDown()) {
        opt.state |= QStyle::State_Raised;
    }
    if (isChecked()) {
        opt.state |= QStyle::State_On;
    }
    if (isDown()) {
        opt.state |= QStyle::State_Sunken;
    }

    if (TabBarHelper* tb = qobject_cast<TabBarHelper*>(parent())) {
        int index = tb->currentIndex();
        QTabBar::ButtonPosition closeSide = (QTabBar::ButtonPosition)style()->styleHint(QStyle::SH_TabBar_CloseButtonPosition, 0, tb);
        if (tb->tabButton(index, closeSide) == this && tb->isActiveTabBar()) {
            opt.state |= QStyle::State_Selected;
        }
    }

    style()->drawPrimitive(QStyle::PE_IndicatorTabClose, &opt, &p, this);
}
