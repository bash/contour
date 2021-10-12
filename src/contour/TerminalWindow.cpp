/**
 * This file is part of the "contour" project
 *   Copyright (c) 2019-2021 Christian Parpart <christian@parpart.family>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <contour/Actions.h>
#include <contour/BackgroundBlur.h>
#include <contour/TerminalWindow.h>
#include <contour/helper.h>

#include <contour/opengl/TerminalWidget.h>

#include <qnamespace.h>
#include <terminal/Metrics.h>
#include <terminal/pty/Pty.h>
#include <terminal/pty/PtyProcess.h>

#if defined(_MSC_VER)
#include <terminal/pty/ConPty.h>
#else
#include <terminal/pty/UnixPty.h>
#endif

#include <QtCore/QDebug>
#include <QtGui/QScreen>
#include <QtGui/QGuiApplication>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QVBoxLayout>

#if defined(CONTOUR_BLUR_PLATFORM_KWIN)
#include <KWindowEffects>
#endif

#include <cstring>
#include <fstream>
#include <stdexcept>

using namespace std;
using namespace std::placeholders;

using terminal::Width;
using terminal::Height;
using terminal::ImageSize;

#if defined(_MSC_VER)
#define __PRETTY_FUNCTION__ __FUNCDNAME__
#endif

#include <QtWidgets/QStatusBar>

namespace contour {

using actions::Action;

#if defined(CONTOUR_SCROLLBAR) // {{{
ScrollableDisplay::ScrollableDisplay(QWidget* _parent, TerminalSession& _session, QWidget* _main):
    QWidget(_parent),
    session_{ _session },
    mainWidget_{ _main },
    scrollBar_{ nullptr }
{
    mainWidget_->setParent(this);

    scrollBar_ = new QScrollBar(this);
    scrollBar_->setMinimum(0);
    scrollBar_->setMaximum(0);
    scrollBar_->setValue(0);
    scrollBar_->setCursor(Qt::ArrowCursor);

    connect(scrollBar_, &QScrollBar::valueChanged,
            this, QOverload<>::of(&ScrollableDisplay::onValueChanged));

    QSize ms = mainWidget_->sizeHint();
    QSize ss = scrollBar_->sizeHint();
    ms.setWidth(width() - ss.width());
    ss.setHeight(height());
    scrollBar_->resize(ss);
    mainWidget_->resize(ms);

    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    updateGeometry();
}

QSize ScrollableDisplay::sizeHint() const
{
    QSize s = mainWidget_->sizeHint();
    s.setWidth(s.width() + scrollBar_->sizeHint().width());
    return s;
}

void ScrollableDisplay::resizeEvent(QResizeEvent* _event)
{
    QWidget::resizeEvent(_event);

    auto const sbWidth = scrollBar_->width();
    auto const mainWidth = width() - sbWidth;
    mainWidget_->resize(mainWidth, height());
    scrollBar_->resize(sbWidth, height());
    // mainWidget_->move(0, 0);
    // scrollBar_->move(mainWidth, 0);
    updatePosition();
    updateGeometry();
}

void ScrollableDisplay::showScrollBar(bool _show)
{
    if (_show)
        scrollBar_->show();
    else
        scrollBar_->hide();
}

void ScrollableDisplay::updateValues()
{
    if (!scrollBar_->isVisible())
        return;

    scrollBar_->setMaximum(session_.terminal().screen().historyLineCount().as<int>());
    if (auto const s = session_.terminal().viewport().absoluteScrollOffset(); s.has_value())
        scrollBar_->setValue(s.value().as<int>());
    else
        scrollBar_->setValue(scrollBar_->maximum());
}

void ScrollableDisplay::updatePosition()
{
    //terminalWidget_->setGeometry(calculateWidgetGeometry());
    LOGSTORE(DisplayLog)("called with {}x{} in {}", width(), height(),
                         session_.currentScreenType());

    auto const resizeMainAndScrollArea = [&]() {
        QSize ms = mainWidget_->sizeHint();
        QSize ss = scrollBar_->sizeHint();
        ms.setWidth(width() - ss.width());
        ms.setHeight(height());
        ss.setHeight(height());
        scrollBar_->resize(ss);
        mainWidget_->resize(ms);
    };

    if (session_.currentScreenType() != terminal::ScreenType::Alternate
        || !session_.profile().hideScrollbarInAltScreen)
    {
        auto const sbWidth = scrollBar_->width();
        auto const mainWidth = width() - sbWidth;
        LOGSTORE(DisplayLog)("Scrollbar Pos: {}", session_.profile().scrollbarPosition);
        switch (session_.profile().scrollbarPosition)
        {
            case config::ScrollBarPosition::Right:
                resizeMainAndScrollArea();
                scrollBar_->show();
                mainWidget_->move(0, 0);
                scrollBar_->move(mainWidth, 0);
                break;
            case config::ScrollBarPosition::Left:
                resizeMainAndScrollArea();
                scrollBar_->show();
                mainWidget_->move(sbWidth, 0);
                scrollBar_->move(0, 0);
                break;
            case config::ScrollBarPosition::Hidden:
            {
                scrollBar_->hide();
                auto const cr = contentsRect();
                mainWidget_->resize(cr.right(), cr.bottom());
                mainWidget_->move(0, 0);
                break;
            }
        }
        LOGSTORE(DisplayLog)(
            "TW {}x{}+{}x{}, SB {}, {}x{}+{}x{}, value: {}/{}",
            mainWidget_->pos().x(),
            mainWidget_->pos().y(),
            mainWidget_->width(),
            mainWidget_->height(),
            scrollBar_->isVisible() ? "visible" : "invisible",
            scrollBar_->pos().x(),
            scrollBar_->pos().y(),
            scrollBar_->width(),
            scrollBar_->height(),
            scrollBar_->value(), scrollBar_->maximum()
        );
    }
    else
    {
        LOGSTORE(DisplayLog)("Resize terminal widget over full contents.");
        scrollBar_->hide();
    }
}

void ScrollableDisplay::onValueChanged()
{
    session_.terminal().viewport().scrollToAbsolute(terminal::StaticScrollbackPosition(scrollBar_->value()));
    session_.scheduleRedraw();
}
#endif // }}}

TerminalWindow::TerminalWindow(config::Config _config, bool _liveConfig, string _profileName, string _programPath) :
    config_{ std::move(_config) },
    liveConfig_{ _liveConfig },
    profileName_{ std::move(_profileName) },
    programPath_{ std::move(_programPath) }
{
    // connect(this, SIGNAL(screenChanged(QScreen*)), this, SLOT(onScreenChanged(QScreen*)));

    // QPalette p = QApplication::palette();
    // QColor backgroundColor(0x30, 0x30, 0x30, 0x80);
    // backgroundColor.setAlphaF(0.3);
    // p.setColor(QPalette::Window, backgroundColor);
    // setPalette(p);

    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground, false);

    // {{{ fill config's maxImageSize if not yet set.
    auto const defaultMaxImageSize = [&]() -> ImageSize {
        QScreen const* screen = QGuiApplication::primaryScreen();
        auto constexpr fallbackDefaultMaxImageSize = ImageSize{ Width(800), Height(600) };
        if (!screen)
            return fallbackDefaultMaxImageSize;
        QSize const size = screen->size();
        if (size.isEmpty())
            return fallbackDefaultMaxImageSize;
        return ImageSize{
            Width::cast_from(size.width()),
            Height::cast_from(size.height())
        };
    }();
    if (config_.maxImageSize.width <= Width(0))
        config_.maxImageSize.width = defaultMaxImageSize.width;
    if (config_.maxImageSize.height <= Height(0))
        config_.maxImageSize.height = defaultMaxImageSize.height;
    // }}}

    terminalSession_ = make_unique<TerminalSession>(
        make_unique<terminal::PtyProcess>(
            profile()->shell,
            profile()->terminalSize
        ),
        config_,
        liveConfig_,
        profileName_,
        programPath_,
        unique_ptr<TerminalDisplay>{},
        [this]() {
            // NB: This is invoked whenever the newly assigned display
            //     has finished initialization.
#if defined(CONTOUR_SCROLLBAR)
            scrollableDisplay_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
#else
            (void) this;
#endif
        }
    );

    terminalSession_->setDisplay(make_unique<opengl::TerminalWidget>(
        *profile(),
        *terminalSession_,
        [this]() { centralWidget()->updateGeometry(); update(); },
        [this](bool _enable) { WindowBackgroundBlur::setEnabled(winId(), _enable); }
    ));
    terminalWidget_ = static_cast<opengl::TerminalWidget*>(terminalSession_->display());

    connect(terminalWidget_, SIGNAL(terminated()), this, SLOT(onTerminalClosed()));
    connect(terminalWidget_, SIGNAL(terminalBufferChanged(terminal::ScreenType)), this, SLOT(terminalBufferChanged(terminal::ScreenType)));

#if defined(CONTOUR_SCROLLBAR)
    scrollableDisplay_ = new ScrollableDisplay(nullptr, *terminalSession_, terminalWidget_);
    setCentralWidget(scrollableDisplay_);
    connect(terminalWidget_, SIGNAL(terminalBufferUpdated()), scrollableDisplay_, SLOT(updateValues()));
#else
    setCentralWidget(terminalWidget_);
#endif

    terminalWidget_->setFocus();

    //statusBar()->showMessage("blurb");

    terminalSession_->start();
}

void TerminalWindow::onTerminalClosed()
{
    LOGSTORE(DisplayLog)("title {}", terminalSession_->terminal().screen().windowTitle());
    close();
}

void TerminalWindow::setBackgroundBlur([[maybe_unused]] bool _enable)
{
    WindowBackgroundBlur::setEnabled(winId(), _enable);
}

void TerminalWindow::profileChanged()
{
#if defined(CONTOUR_SCROLLBAR)
    scrollableDisplay_->updatePosition();

    if (terminalSession_->terminal().screen().isPrimaryScreen())
        scrollableDisplay_->showScrollBar(profile()->scrollbarPosition != config::ScrollBarPosition::Hidden);
    else
        scrollableDisplay_->showScrollBar(!profile()->hideScrollbarInAltScreen);
#endif
}

void TerminalWindow::terminalBufferChanged(terminal::ScreenType _type)
{
#if defined(CONTOUR_SCROLLBAR)
    LOGSTORE(DisplayLog)("Screen buffer type has changed to {}.", _type);
    scrollableDisplay_->showScrollBar(
        _type == terminal::ScreenType::Main || !profile()->hideScrollbarInAltScreen
    );

    scrollableDisplay_->updatePosition();
    scrollableDisplay_->updateValues();
#endif
}

void TerminalWindow::resizeEvent(QResizeEvent* _event)
{
    LOGSTORE(DisplayLog)("TerminalWindow.resizeEvent: size {}x{} ({}x{})",
        width(), height(),
        _event->size().width(),
        _event->size().height()
    );

    QMainWindow::resizeEvent(_event);
    //centralWidget()->resize(_event->size());
    //updatePosition();
}

bool TerminalWindow::event(QEvent* _event)
{
    return QMainWindow::event(_event);
}

} // namespace contour
