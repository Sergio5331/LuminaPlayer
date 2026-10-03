#include "MainWindow.h"
#include "MediaFiles.h"
#include <QFile>
#include <QTextStream>
#include <cmath>
#include "MpvPlayer.h"
#include "PlayerControls.h"
#include "UiIcons.h"
#include "ThumbnailPreview.h"
#include <QSlider>
#include "ShortcutDefinitions.h"
#include <QKeySequence>
#include <QDialog>
#include <QApplication>
#include <QCursor>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QCryptographicHash>
#include <QStackedWidget>
#include <QMenu>
#include <QActionGroup>
#include <QScreen>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QWindow>
#include <algorithm>
#include <windows.h>
#include <windowsx.h>

namespace {
QStringList droppedFiles(const QMimeData* mime)
{
    QStringList files;
    for (const auto& url : mime->urls())
        if (url.isLocalFile() && QFileInfo(url.toLocalFile()).exists()) files.append(url.toLocalFile());
    return files;
}
QString droppedFile(const QMimeData* mime)
{
    for (const auto& url : mime->urls()) {
        if (url.isLocalFile() && QFileInfo(url.toLocalFile()).exists()) return url.toLocalFile();
    }
    return {};
}
}
QVideoContainerWidget::QVideoContainerWidget(QWidget* parent) : QWidget(parent)
{
    setObjectName("videoContainer");
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAcceptDrops(true);
}
void QVideoContainerWidget::routeNativeInputToQt()
{
    // El HWND de mpv vive en su propio hilo. Deshabilitar sólo su entrada hace
    // que Windows entregue ratón/foco al widget anfitrión; no detiene D3D11.
    EnumChildWindows(reinterpret_cast<HWND>(winId()), [](HWND child, LPARAM) -> BOOL {
        wchar_t name[64]{};
        GetClassNameW(child, name, 64);
        if (wcscmp(name, L"mpv") == 0) EnableWindow(child, FALSE);
        return TRUE;
    }, 0);
}
bool QVideoContainerWidget::nativeEvent(const QByteArray& type, void* message, qintptr* result)
{
    const auto* msg = static_cast<MSG*>(message);
    if (msg->message == WM_PARENTNOTIFY && LOWORD(msg->wParam) == WM_CREATE)
        QTimer::singleShot(0, this, &QVideoContainerWidget::routeNativeInputToQt);
    return QWidget::nativeEvent(type, message, result);
}
void QVideoContainerWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.fillRect(rect(), QColor("#121212"));
}
void QVideoContainerWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) { emit doubleClicked(); event->accept(); }
    else QWidget::mouseDoubleClickEvent(event);
}
void QVideoContainerWidget::dragEnterEvent(QDragEnterEvent* event)
{
    if (!droppedFile(event->mimeData()).isEmpty()) event->acceptProposedAction();
}
void QVideoContainerWidget::dropEvent(QDropEvent* event)
{
    const QString file = droppedFile(event->mimeData());
    if (!file.isEmpty()) { emit filesDropped(droppedFiles(event->mimeData())); event->acceptProposedAction(); }
}

MainWindow::MainWindow(QString captureDirectory, QWidget* parent, QString settingsFile)
    : QMainWindow(parent, Qt::Window | Qt::FramelessWindowHint)
{
    Q_INIT_RESOURCE(branding);
    setWindowIcon(QIcon(":/assets/lumina-logo.png"));
    if (settingsFile.isEmpty()) {
        const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
        QDir().mkpath(directory);
        settingsFile = directory + "/session.ini";
    }
    settings_ = std::make_unique<QSettings>(settingsFile, QSettings::IniFormat);
    setObjectName("mainWindow");
    setWindowTitle("LuminaPlayer");
    resize(1100, 680);
    setMinimumSize(640, 360);
    setMouseTracking(true);
    setAcceptDrops(true);
    setStyleSheet(R"(
        QMainWindow, QWidget#surface, QWidget#titleBar { background: #121212; color: #ececf0; }
        QLabel#windowTitle { color: #bfbfc8; font-size: 12px; }
        QPushButton { border: none; color: #dcdce3; padding: 6px 12px; border-radius: 5px; background: transparent; }
        QPushButton:hover { background: #303035; }
        QPushButton#closeButton:hover { background: #a83248; }
    )");
    auto* surface = new QWidget(this);
    surface->setObjectName("surface");
    setCentralWidget(surface);
    layout_ = new QVBoxLayout(surface);
    layout_->setContentsMargins(6, 6, 6, 6);
    layout_->setSpacing(0);
    titleBar_ = new QWidget(surface);
    titleBar_->setObjectName("titleBar");
    titleBar_->setFixedHeight(42);
    titleBar_->setCursor(Qt::ArrowCursor);
    auto* titleLayout = new QHBoxLayout(titleBar_);
    titleLayout->setContentsMargins(10, 0, 0, 0);
    auto* brand = new QLabel(titleBar_);
    brand->setPixmap(windowIcon().pixmap(30, 30));
    titleLayout->addWidget(brand);
    title_ = new QLabel("LuminaPlayer", titleBar_);
    title_->setObjectName("windowTitle");
    title_->setTextFormat(Qt::PlainText);
    title_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    titleLayout->addWidget(title_, 1);
    const auto titleButton = [this, titleLayout](const QString& text, const QString& tip) {
        auto* button = new QPushButton(titleBar_);
        if (text == "—") button->setIcon(luminaIcon("minimize"));
        else if (text == "□") button->setIcon(luminaIcon("maximize"));
        else if (text == "✕") button->setIcon(luminaIcon("close"));
        else { button->setText(text); button->setIcon(luminaIcon(text == tr("Biblioteca") ? "library" : "open")); }
        button->setIconSize(QSize(18,18));
        if (text == tr("Abrir") || text == tr("Biblioteca")) {
            button->setProperty("shortcutId",text == tr("Abrir") ? "open" : "library");
            button->setProperty("baseTooltip",tip.section(" · ",0,0));
        }
        button->setToolTip(tip);
        button->setAccessibleName(tip);
        button->setFocusPolicy(Qt::NoFocus);
        titleLayout->addWidget(button);
        return button;
    };
    connect(titleButton(tr("Abrir"), tr("Abrir archivo · Ctrl+O")), &QPushButton::clicked, this, &MainWindow::chooseFile);
    connect(titleButton(tr("Biblioteca"), tr("Lista y recientes · %1").arg(shortcut("library","Ctrl+L"))), &QPushButton::clicked, this, &MainWindow::showLibrary);
    connect(titleButton("—", tr("Minimizar")), &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(titleButton("□", tr("Maximizar / Restaurar")), &QPushButton::clicked, this, [this] {
        isMaximized() ? showNormal() : showMaximized();
    });
    auto* closeButton = titleButton("✕", tr("Cerrar"));
    closeButton->setObjectName("closeButton");
    connect(closeButton, &QPushButton::clicked, this, &QWidget::close);
    layout_->addWidget(titleBar_);
    video_ = new QVideoContainerWidget(surface);
    stack_ = new QStackedWidget(surface);
    stack_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    layout_->addWidget(stack_, 1);
    stack_->addWidget(video_);
    auto* welcome = new QWidget(stack_);
    welcome->setObjectName("welcome");
    welcome->setStyleSheet(R"(
        QWidget#welcome { background: #121218; border-radius: 18px; }
        QLabel { color: #a6a4b6; background: transparent; }
        QLabel#hero { color: #f3efff; font-size: 38px; font-weight: 600; }
        QPushButton#openHero { background: #a991ee; color: #171122; padding: 14px 34px;
            border-radius: 12px; font-size: 15px; font-weight: 600; }
        QPushButton#openHero:hover { background: #c3afff; }
    )");
    auto* home = new QVBoxLayout(welcome);
    home->setSpacing(18);
    home->addStretch();
    auto* logo = new QLabel(welcome);
    logo->setPixmap(windowIcon().pixmap(144, 144));
    home->addWidget(logo, 0, Qt::AlignHCenter);
    auto* heading = new QLabel("LuminaPlayer", welcome);
    heading->setObjectName("hero");
    home->addWidget(heading, 0, Qt::AlignHCenter);
    home->addWidget(new QLabel(tr("Tu vídeo. Tu música. Tu momento."), welcome), 0, Qt::AlignHCenter);
    auto* openHero = new QPushButton(tr("Abrir archivo"), welcome);
    openHero->setObjectName("openHero");
    home->addWidget(openHero, 0, Qt::AlignHCenter);
    connect(openHero, &QPushButton::clicked, this, &MainWindow::chooseFile);
    home->addWidget(new QLabel(tr("o arrastra un archivo a esta ventana"), welcome), 0, Qt::AlignHCenter);
    home->addStretch();
    auto* hints = new QLabel(tr("ESPACIO  Pausa     ·     S  Captura     ·     F11  Pantalla completa"), welcome);
    hints->setObjectName("keyboardHints");
    home->addWidget(hints, 0, Qt::AlignHCenter);
    home->addSpacing(24);
    stack_->addWidget(welcome);
    stack_->setCurrentWidget(welcome);
    player_ = std::make_unique<MpvPlayer>(video_->winId(), captureDirectory);
    player_->configureCaptures(captureDirectory.isEmpty() ? settings_->value("captures/directory", player_->captureDirectory()).toString() : captureDirectory,
        settings_->value("captures/quality", 95).toInt());
    controls_ = new PlayerControls(this);
    thumbnails_ = new ThumbnailPreview(controls_->findChild<QSlider*>("progressSlider"),player_.get(),settings_.get());
    connect(player_.get(), &MpvPlayer::positionChanged, controls_, &PlayerControls::setPosition);
    connect(player_.get(), &MpvPlayer::durationChanged, controls_, &PlayerControls::setDuration);
    connect(player_.get(), &MpvPlayer::pauseChanged, controls_, &PlayerControls::setPaused);
    connect(player_.get(), &MpvPlayer::volumeChanged, controls_, &PlayerControls::setVolume);
    connect(player_.get(), &MpvPlayer::seekableChanged, controls_, &PlayerControls::setSeekable);
    connect(player_.get(), &MpvPlayer::videoReconfigured, this, [this] {
        if(!pip_)return;
        const int w=player_->property("dwidth").toInt(), h=player_->property("dheight").toInt();
        if(w>0&&h>0){pipAspect_=static_cast<double>(w)/h;setGeometry(constrainPip(geometry(),Qt::RightEdge));}
    });
    connect(player_.get(), &MpvPlayer::videoReconfigured, video_, &QVideoContainerWidget::routeNativeInputToQt);
    connect(player_.get(), &MpvPlayer::fileLoaded, this, [this](const QString& file) {
        player_->clearLoop(); loopA_ = -1;
        QStringList recent = settings_->value("recentFiles").toStringList();
        recent.removeAll(file); recent.prepend(file);
        while (recent.size() > 30) recent.removeLast();
        settings_->setValue("recentFiles", recent);
        refreshLibrary();
        player_->configureSubtitles(settings_->value("subtitles/scale", 1).toDouble(),
            settings_->value("subtitles/position", 100).toInt(), 0, settings_->value("subtitles/override", false).toBool());
        stack_->setCurrentWidget(video_);
        const QString name = QFileInfo(file).fileName();
        title_->setText(name);
        setWindowTitle(name + " — LuminaPlayer");
        video_->routeNativeInputToQt();
        controls_->setAutoHideEnabled(true);
        controls_->showMessage(name);
    });
    connect(player_.get(), &MpvPlayer::playbackReset, this, [this] {
        controls_->setAutoHideEnabled(false);
    });
    connect(player_.get(), &MpvPlayer::errorOccurred, this, &MainWindow::recordError);
    connect(player_.get(), &MpvPlayer::errorOccurred, controls_, &PlayerControls::showMessage);
    connect(player_.get(), &MpvPlayer::screenshotSaved, this, [this](const QString& path) {
        QStringList history=settings_->value("captureHistory").toStringList();
        history.removeAll(path);history.prepend(path);while(history.size()>200)history.removeLast();
        settings_->setValue("captureHistory",history);
        controls_->showMessage(tr("Captura guardada: %1").arg(QFileInfo(path).fileName()));
        showCapturePreview(path);
    });
    connect(controls_, &PlayerControls::pauseRequested, player_.get(), &MpvPlayer::togglePause);
    connect(controls_, &PlayerControls::seekRequested, player_.get(), &MpvPlayer::seekAbsolute);
    connect(controls_, &PlayerControls::volumeRequested, player_.get(), &MpvPlayer::setVolume);
    connect(controls_, &PlayerControls::openRequested, this, &MainWindow::chooseFile);
    connect(controls_, &PlayerControls::captureRequested, this, [this] { captureFrame(); });
    connect(controls_, &PlayerControls::fullscreenRequested, this, &MainWindow::toggleFullscreen);
    connect(controls_, &PlayerControls::visibilityRequested, this, [this](bool visible) {
        video_->setCursor(visible ? Qt::ArrowCursor : Qt::BlankCursor);
    });
    connect(video_, &QVideoContainerWidget::filesDropped, this, &MainWindow::openFiles);
    connect(video_, &QVideoContainerWidget::doubleClicked, this, &MainWindow::toggleFullscreen);
    connect(controls_, &PlayerControls::libraryRequested, this, &MainWindow::showLibrary);
    connect(controls_, &PlayerControls::toolsRequested, this, &MainWindow::showTools);
    connect(player_.get(), &MpvPlayer::playbackFinished, this, [this] {
        if (settings_->value("playlist/autoNext", true).toBool())
            QTimer::singleShot(0, this, [this] { nextFile(); });
    });
    connect(controls_, &PlayerControls::tracksRequested, this, &MainWindow::showTracks);
    connect(controls_, &PlayerControls::pipRequested, this, &MainWindow::togglePip);
    auto* checkpoint = new QTimer(this);
    checkpoint->setInterval(5000);
    connect(checkpoint, &QTimer::timeout, this, &MainWindow::saveSession);
    checkpoint->start();
    player_->setSpeed(settings_->value("playback/speed",1).toDouble());
    player_->setAudioMode(settings_->value("playback/audio",1).toInt());
    player_->setVolume(settings_->value("volume", 70).toInt());
    const QRect saved = settings_->value("window/normal").toRect();
    if(saved.isValid()) {
        QRect usable = saved;
        bool onScreen=false;
        for(auto* screen:QApplication::screens()) if(screen->availableGeometry().intersects(saved)) {onScreen=true;break;}
        if(!onScreen)usable.moveCenter(screen()->availableGeometry().center());
        usable.setSize(usable.size().boundedTo(screen()->availableGeometry().size()).expandedTo(minimumSize()));
        setGeometry(usable);
    }
    if(settings_->value("window/maximized",false).toBool())setWindowState(Qt::WindowMaximized);
    updateShortcutHints();
    qApp->installEventFilter(this);
    const int mediaKeys[]={VK_MEDIA_PLAY_PAUSE,VK_MEDIA_NEXT_TRACK,VK_MEDIA_PREV_TRACK,VK_MEDIA_STOP};
    for(int i=0;i<4;++i)RegisterHotKey(reinterpret_cast<HWND>(winId()),700+i,MOD_NOREPEAT,mediaKeys[i]);
}
MainWindow::~MainWindow()
{
    saveWindowState();
    saveSession();
    settings_->sync();
    for(int i=0;i<4;++i)UnregisterHotKey(reinterpret_cast<HWND>(winId()),700+i);
    qApp->removeEventFilter(this);
    controls_->suspend();
    delete thumbnails_; thumbnails_ = nullptr;
    player_.reset(); // Antes de destruir el contenedor de vídeo.
}
void MainWindow::openFile(const QString& path)
{
    const QString extension = QFileInfo(path).suffix().toLower();
    if(QFileInfo(path).isDir() || extension=="m3u" || extension=="m3u8") {openFiles({path});return;}
    if (QStringList{"ass", "ssa", "srt", "vtt", "sub"}.contains(extension)) {
        player_->addSubtitles(path); return;
    }
    if (QFileInfo(path).isFile()) {
        const QString absolute = QFileInfo(path).absoluteFilePath();
        if (playlistIndex_ < 0 || playlistIndex_ >= playlist_.size() || playlist_[playlistIndex_] != absolute) {
            const int index = playlist_.indexOf(absolute);
            if (index >= 0) playlistIndex_ = index;
            else { playlist_ = {absolute}; playlistIndex_ = 0; }
        }
    }
    saveSession();
    player_->loadFile(path, settings_->value(sessionKey(path), 0).toDouble());
    activity();
}
void MainWindow::chooseFile()
{
    controls_->suspend();
    const QStringList paths = QFileDialog::getOpenFileNames(this, tr("Abrir multimedia"), {}, tr("Todos los archivos (*)"));
    if (!paths.isEmpty()) openFiles(paths);
    activity();
}
void MainWindow::positionControls()
{
    if (!controls_) return;
    controls_->setCompact(pip_ || video_->width() < 760);
    const int width = std::min(1100, video_->width() - 32);
    const QPoint location = video_->mapToGlobal(QPoint((video_->width() - width) / 2,
        std::max(0, video_->height() - controls_->height() - 16)));
    controls_->setGeometry(QRect(location, QSize(width, controls_->height())));
}
void MainWindow::activity()
{
    if (pip_) return;
    if (stack_->currentWidget() != video_) { controls_->suspend(); return; }
    positionControls();
    controls_->activity();
}
void MainWindow::toggleFullscreen()
{
    if (pip_) togglePip();
    controls_->suspend();
    if (isFullScreen()) {
        titleBar_->show();
        layout_->setContentsMargins(6, 6, 6, 6);
        maximizedBeforeFullscreen_ ? showMaximized() : showNormal();
    } else {
        maximizedBeforeFullscreen_ = isMaximized();
        titleBar_->hide();
        layout_->setContentsMargins(0, 0, 0, 0);
        showFullScreen();
    }
    controls_->setFullscreen(isFullScreen());
    updateShortcutHints();
    QTimer::singleShot(0, this, &MainWindow::activity);
}
bool MainWindow::handleKey(int key, Qt::KeyboardModifiers modifiers, bool repeated)
{
    const QString pressed = QKeySequence(QKeyCombination(modifiers, static_cast<Qt::Key>(key))).toString(QKeySequence::PortableText);
    for (const auto& def : shortcutDefinitions()) {
        if (shortcut(def.id,def.key) == pressed) {
            if (!repeated || def.repeat) runAction(def.id);
            return true;
        }
    }
    return false;
}
bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    auto* widget = qobject_cast<QWidget*>(watched);
    if (!controls_ || !widget || (widget != this && !isAncestorOf(widget)))
        return QMainWindow::eventFilter(watched, event);
    if (widget->window() != this && widget->window() != controls_) return QMainWindow::eventFilter(watched, event);
    if (QApplication::activeModalWidget() || QApplication::activePopupWidget()) return QMainWindow::eventFilter(watched, event);
    if (pip_ && widget == video_) {
        if (event->type() == QEvent::MouseButtonPress) {
            const auto* mouse = static_cast<QMouseEvent*>(event);
            if (mouse->button() == Qt::LeftButton) {
                const QPoint local=mouse->position().toPoint();
                const int grip=9;
                pipResizeEdges_={};
                if(local.x()<grip)pipResizeEdges_|=Qt::LeftEdge;
                else if(local.x()>=video_->width()-grip)pipResizeEdges_|=Qt::RightEdge;
                if(local.y()<grip)pipResizeEdges_|=Qt::TopEdge;
                else if(local.y()>=video_->height()-grip)pipResizeEdges_|=Qt::BottomEdge;
                pipResizeStart_=geometry();
                pipDragging_ = true;
                pipDragStart_ = mouse->globalPosition().toPoint();
                pipWindowStart_ = pos();
                video_->grabMouse();
                return true;
            }
            if (mouse->button() == Qt::RightButton) {
                showPipMenu(mouse->globalPosition().toPoint());
                return true;
            }
        } else if (event->type() == QEvent::MouseMove && pipDragging_) {
            const auto* mouse = static_cast<QMouseEvent*>(event);
            if (mouse->buttons().testFlag(Qt::LeftButton)) {
                const QPoint delta=mouse->globalPosition().toPoint()-pipDragStart_;
                if(pipResizeEdges_) {
                    QRect proposed=pipResizeStart_;
                    if(pipResizeEdges_.testFlag(Qt::LeftEdge))proposed.setLeft(proposed.left()+delta.x());
                    if(pipResizeEdges_.testFlag(Qt::RightEdge))proposed.setRight(proposed.right()+delta.x());
                    if(pipResizeEdges_.testFlag(Qt::TopEdge))proposed.setTop(proposed.top()+delta.y());
                    if(pipResizeEdges_.testFlag(Qt::BottomEdge))proposed.setBottom(proposed.bottom()+delta.y());
                    setGeometry(constrainPip(proposed,pipResizeEdges_));
                } else move(pipWindowStart_+delta);
            }
            else { pipDragging_ = false; video_->releaseMouse(); }
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease && pipDragging_) {
            pipDragging_ = false;
            video_->releaseMouse();
            return true;
        }
    }
    if (event->type() == QEvent::MouseMove) {
        if(pip_ && widget==video_) {
            const QPoint p=static_cast<QMouseEvent*>(event)->position().toPoint();
            const bool left=p.x()<9,right=p.x()>=video_->width()-9,top=p.y()<9,bottom=p.y()>=video_->height()-9;
            video_->setCursor((left&&top)||(right&&bottom)?Qt::SizeFDiagCursor:(right&&top)||(left&&bottom)?Qt::SizeBDiagCursor:left||right?Qt::SizeHorCursor:top||bottom?Qt::SizeVerCursor:Qt::ArrowCursor);
        }
        const QPoint point = static_cast<QMouseEvent*>(event)->globalPosition().toPoint();
        if (point != lastMousePosition_) { lastMousePosition_ = point; activity(); }
    } else if (event->type() == QEvent::MouseButtonPress) {
        activity();
        const auto* mouse = static_cast<QMouseEvent*>(event);
        if ((widget == titleBar_ || widget == title_) && mouse->button() == Qt::LeftButton && !isFullScreen()) {
            windowHandle()->startSystemMove(); return true;
        }
    } else if (event->type() == QEvent::MouseButtonDblClick && (widget == titleBar_ || widget == title_)) {
        isMaximized() ? showNormal() : showMaximized(); return true;
    } else if (event->type() == QEvent::Wheel) {
        const int delta = static_cast<QWheelEvent*>(event)->angleDelta().y();
        if (delta) player_->changeVolume(delta > 0 ? 5 : -5);
        activity();
        event->accept(); return true;
    } else if (event->type() == QEvent::KeyPress) {
        const auto* key = static_cast<QKeyEvent*>(event);
        if (handleKey(key->key(), key->modifiers(), key->isAutoRepeat())) { activity(); event->accept(); return true; }
    }
    return QMainWindow::eventFilter(watched, event);
}
bool MainWindow::nativeEvent(const QByteArray& type, void* message, qintptr* result)
{
    const auto* msg = static_cast<MSG*>(message);
    if(msg->message==WM_HOTKEY && msg->wParam>=700 && msg->wParam<=703) {
        switch(msg->wParam){case 700:player_->togglePause();break;case 701:nextFile();break;case 702:nextFile(-1);break;case 703:player_->setPaused(true);player_->seekAbsolute(0);break;}
        *result=0;return true;
    }
    if(msg->message==WM_APPCOMMAND) {
        switch(GET_APPCOMMAND_LPARAM(msg->lParam)) {
        case APPCOMMAND_MEDIA_PLAY_PAUSE:player_->togglePause();break;
        case APPCOMMAND_MEDIA_PLAY:player_->setPaused(false);break;
        case APPCOMMAND_MEDIA_PAUSE:player_->setPaused(true);break;
        case APPCOMMAND_MEDIA_NEXTTRACK:nextFile();break;
        case APPCOMMAND_MEDIA_PREVIOUSTRACK:nextFile(-1);break;
        case APPCOMMAND_MEDIA_STOP:player_->setPaused(true);player_->seekAbsolute(0);break;
        default:return QMainWindow::nativeEvent(type,message,result);
        }
        *result=TRUE;return true;
    }
    if(msg->message==WM_SIZING && pip_) {
        auto* native=reinterpret_cast<RECT*>(msg->lParam);
        Qt::Edges edges;
        const int side=static_cast<int>(msg->wParam);
        if(side==WMSZ_LEFT||side==WMSZ_TOPLEFT||side==WMSZ_BOTTOMLEFT)edges|=Qt::LeftEdge;
        if(side==WMSZ_RIGHT||side==WMSZ_TOPRIGHT||side==WMSZ_BOTTOMRIGHT)edges|=Qt::RightEdge;
        if(side==WMSZ_TOP||side==WMSZ_TOPLEFT||side==WMSZ_TOPRIGHT)edges|=Qt::TopEdge;
        if(side==WMSZ_BOTTOM||side==WMSZ_BOTTOMLEFT||side==WMSZ_BOTTOMRIGHT)edges|=Qt::BottomEdge;
        const QRect fit=constrainPip(QRect(native->left,native->top,native->right-native->left,native->bottom-native->top),edges);
        native->left=fit.x();native->top=fit.y();native->right=fit.x()+fit.width();native->bottom=fit.y()+fit.height();
        *result=TRUE;return true;
    }
    if (msg->message == WM_NCHITTEST && !isFullScreen() && !isMaximized()) {
        RECT rect{};
        GetWindowRect(reinterpret_cast<HWND>(winId()), &rect);
        const int x = GET_X_LPARAM(msg->lParam), y = GET_Y_LPARAM(msg->lParam);
        const int edge = static_cast<int>(6 * devicePixelRatioF());
        const bool left = x < rect.left + edge, right = x >= rect.right - edge;
        const bool top = y < rect.top + edge, bottom = y >= rect.bottom - edge;
        if (left || right || top || bottom) {
            *result = top ? (left ? HTTOPLEFT : right ? HTTOPRIGHT : HTTOP)
                : bottom ? (left ? HTBOTTOMLEFT : right ? HTBOTTOMRIGHT : HTBOTTOM)
                : left ? HTLEFT : HTRIGHT;
            return true;
        }
    }
    return QMainWindow::nativeEvent(type, message, result);
}
void MainWindow::resizeEvent(QResizeEvent* event) { QMainWindow::resizeEvent(event); positionControls(); }
void MainWindow::moveEvent(QMoveEvent* event) { QMainWindow::moveEvent(event); positionControls(); }
void MainWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);
    QTimer::singleShot(0, this, &MainWindow::activity);
}
void MainWindow::hideEvent(QHideEvent* event)
{
    if (controls_) controls_->suspend();
    QMainWindow::hideEvent(event);
}
void MainWindow::changeEvent(QEvent* event)
{
    QMainWindow::changeEvent(event);
    if (!controls_) return;
    if (event->type() == QEvent::WindowStateChange) {
        if (isMinimized()) controls_->suspend(); else QTimer::singleShot(0, this, &MainWindow::activity);
    } else if (event->type() == QEvent::ActivationChange) {
        if (isActiveWindow()) activity(); else controls_->suspend();
    }
}
void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (!droppedFile(event->mimeData()).isEmpty()) event->acceptProposedAction();
}
void MainWindow::dropEvent(QDropEvent* event)
{
    const QString file = droppedFile(event->mimeData());
    if (!file.isEmpty()) { openFiles(droppedFiles(event->mimeData())); event->acceptProposedAction(); }
}

QString MainWindow::sessionKey(const QString& path) const
{
    const QFileInfo file(path);
    const QByteArray identity = (file.canonicalFilePath().toLower() + "|" + QString::number(file.size())
        + "|" + QString::number(file.lastModified().toMSecsSinceEpoch())).toUtf8();
    return "resume/" + QString::fromLatin1(QCryptographicHash::hash(identity, QCryptographicHash::Sha256).toHex());
}
void MainWindow::saveSession()
{
    if (!player_) return;
    settings_->setValue("volume", player_->volume());
    if (!player_->loaded() || !player_->seekable() || player_->duration() <= 0) return;
    const QString key = sessionKey(player_->currentFile());
    const double position = player_->position();
    settings_->setValue(key, position >= 3 && player_->duration() - position > 3 ? position : 0);
    QStringList recent = settings_->value("recentKeys").toStringList();
    recent.removeAll(key);
    recent.prepend(key);
    while (recent.size() > 100) settings_->remove(recent.takeLast());
    settings_->setValue("recentKeys", recent);
    settings_->sync();
}
void MainWindow::showTracks()
{
    QMenu menu(this);
    menu.setStyleSheet("QMenu { background:#23212c; color:#eeeaf9; border:1px solid #484052; padding:8px; }"
        "QMenu::item { padding:8px 24px; } QMenu::item:selected { background:#514064; border-radius:5px; }");
    const auto tracks = player_->tracks();
    for (const QString& type : {QString("audio"), QString("sub")}) {
        auto* sub = menu.addMenu(type == "audio" ? tr("Audio") : tr("Subtítulos"));
        auto* group = new QActionGroup(sub);
        group->setExclusive(true);
        bool selected = false;
        for (const auto& track : tracks) {
            if (track.type != type) continue;
            auto* action = sub->addAction(tr("Pista %1  %2  %3").arg(track.id).arg(track.language, track.title));
            action->setCheckable(true);
            action->setChecked(track.selected);
            selected |= track.selected;
            group->addAction(action);
            connect(action, &QAction::triggered, this, [this, type, id = track.id] { player_->selectTrack(type, id); });
        }
        sub->addSeparator();
        auto* off = sub->addAction(tr("Desactivar"));
        off->setCheckable(true);
        off->setChecked(!selected);
        group->addAction(off);
        connect(off, &QAction::triggered, this, [this, type] { player_->selectTrack(type, -1); });
    }
    menu.addSeparator();
    auto* load = menu.addAction(tr("Cargar subtítulos…"));
    connect(load, &QAction::triggered, this, [this] {
        const QString file = QFileDialog::getOpenFileName(this, tr("Cargar subtítulos"), {}, "Subtítulos (*.ass *.ssa *.srt *.vtt *.sub);;Todos (*)");
        if (!file.isEmpty()) player_->addSubtitles(file);
    });
    auto* adjust = menu.addAction(tr("Tamaño, posición y sincronización…"));
    connect(adjust, &QAction::triggered, this, &MainWindow::showSubtitleSettings);
    menu.exec(QCursor::pos());
    activity();
}
void MainWindow::togglePip()
{
    if (!pip_) {
        if (isFullScreen()) toggleFullscreen();
        maximizedBeforePip_ = isMaximized();
        geometryBeforePip_ = normalGeometry();
        showNormal();
        pip_ = true;
        controls_->setSuppressed(true);
        thumbnails_->cancel();
        if (preview_) preview_->hide();
        if (library_) library_->hide();
        titleBar_->hide();
        layout_->setContentsMargins(0,0,0,0);
        setMinimumSize(160,90);
        const int videoWidth = player_->property("dwidth").toInt();
        const int videoHeight = player_->property("dheight").toInt();
        pipAspect_=videoWidth>0&&videoHeight>0?static_cast<double>(videoWidth)/videoHeight:16.0/9.0;
        resize((videoWidth > 0 && videoHeight > 0 ? QSize(videoWidth,videoHeight) : QSize(16,9))
            .scaled(QSize(480,360),Qt::KeepAspectRatio).expandedTo(minimumSize()));
        const QRect available = screen()->availableGeometry();
        const QRect saved=settings_->value("window/pip").toRect();
        if(saved.isValid()) {
            QRect fit=constrainPip(saved,Qt::RightEdge);
            bool visible=false;for(auto* s:QApplication::screens())if(s->availableGeometry().intersects(fit)){visible=true;break;}
            if(!visible)fit.moveBottomRight(available.bottomRight()-QPoint(24,24));
            setGeometry(fit);
        } else move(available.right() - width() - 24, available.bottom() - height() - 24);
    } else {
        settings_->setValue("window/pip",geometry());
        pip_ = false;
        if (pipDragging_) { pipDragging_ = false; video_->releaseMouse(); }
        titleBar_->show();
        layout_->setContentsMargins(6,6,6,6);
        controls_->setSuppressed(false);
        setMinimumSize(640, 360);
        setGeometry(geometryBeforePip_);
        if (maximizedBeforePip_) showMaximized();
    }
    // Cambiar z-order sin recrear el HWND al que está asociado libmpv.
    SetWindowPos(reinterpret_cast<HWND>(winId()), pip_ ? HWND_TOPMOST : HWND_NOTOPMOST,
        0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    controls_->setCompact(pip_);
    activity();
}

void MainWindow::showTools()
{
    QMenu menu(this);
    menu.setStyleSheet("QMenu { background:#24212d; color:#eee9fa; padding:8px; border:1px solid #493e59; }"
        "QMenu::item { padding:9px 24px; } QMenu::item:selected { background:#514069; border-radius:5px; }");
    menu.addAction(tr("Velocidad, audio, A–B, marcadores y capítulos…"), this, &MainWindow::showAdvanced);
    menu.addAction(tr("Personalizar atajos…"), this, &MainWindow::showShortcuts);
    menu.addAction(tr("Historial de capturas…"), this, &MainWindow::showCaptureHistory);
    menu.addAction(tr("Diagnóstico de errores…"), this, &MainWindow::showDiagnostics);
    menu.addSeparator();
    menu.addAction(luminaIcon("camera"), tr("Captura limpia · %1").arg(shortcut("capture","S")), this, [this] { captureFrame(); });
    menu.addAction(luminaIcon("tracks"), tr("Captura con subtítulos · %1").arg(shortcut("captureSub","Shift+S")), this, [this] { captureFrame(true); });
    menu.addAction(tr("Ajustes de captura…"), this, &MainWindow::showCaptureSettings);
    menu.addSeparator();
    menu.addAction(luminaIcon("frameBack"), tr("Fotograma anterior · %1").arg(shortcut("frameBack",",")), this, [this] { player_->stepFrame(true); });
    menu.addAction(luminaIcon("frameNext"), tr("Fotograma siguiente · %1").arg(shortcut("frameNext",".")), this, [this] { player_->stepFrame(); });
    menu.addSeparator();
    menu.addAction(luminaIcon("previous"), tr("Archivo anterior · %1").arg(shortcut("previous","PgUp")), this, [this] { nextFile(-1); });
    menu.addAction(luminaIcon("next"), tr("Archivo siguiente · %1").arg(shortcut("next","PgDown")), this, [this] { nextFile(); });
    menu.addAction(luminaIcon("library"), tr("Lista y recientes · %1").arg(shortcut("library","Ctrl+L")), this, &MainWindow::showLibrary);
    menu.addAction(luminaIcon("tracks"), tr("Ajustar subtítulos…"), this, &MainWindow::showSubtitleSettings);
    menu.addAction(luminaIcon("fullscreen"), tr("Pantalla completa · %1").arg(shortcut("fullscreen","F11")), this, &MainWindow::toggleFullscreen);
    menu.exec(QCursor::pos());
    activity();
}

void MainWindow::showPipMenu(const QPoint& position)
{
    QMenu menu(this);
    menu.setObjectName("pipMenu");
    menu.setStyleSheet("QMenu { background:#24212d; color:#eee9fa; padding:8px; border:1px solid #493e59; }"
        "QMenu::item { padding:9px 24px; } QMenu::item:selected { background:#514069; }");
    menu.addAction(player_->paused() ? tr("Reproducir") : tr("Pausar"), player_.get(), &MpvPlayer::togglePause);
    menu.addAction(tr("Volver a la ventana normal"), this, &MainWindow::togglePip);
    menu.addAction(tr("Pantalla completa"), this, &MainWindow::toggleFullscreen);
    menu.addSeparator();
    menu.addAction(tr("Cerrar reproductor"), this, &QWidget::close);
    menu.exec(position);
}

QRect MainWindow::constrainPip(const QRect& proposed,Qt::Edges edges) const {
    const double ratio=std::clamp(pipAspect_,0.05,20.0);
    const int minimumWidth=std::max(160,static_cast<int>(std::ceil(90*ratio)));
    int width=edges.testFlag(Qt::LeftEdge)||edges.testFlag(Qt::RightEdge)?proposed.width():static_cast<int>(std::lround(proposed.height()*ratio));
    width=std::max(minimumWidth,width);
    const int height=static_cast<int>(std::lround(width/ratio));
    return QRect(edges.testFlag(Qt::LeftEdge)?proposed.right()-width+1:proposed.left(),
        edges.testFlag(Qt::TopEdge)?proposed.bottom()-height+1:proposed.top(),width,height);
}
void MainWindow::saveWindowState() {
    if(pip_) {
        settings_->setValue("window/pip",geometry());settings_->setValue("window/normal",geometryBeforePip_);
        settings_->setValue("window/maximized",maximizedBeforePip_);
    } else {
        settings_->setValue("window/normal",isMaximized()||isFullScreen()?normalGeometry():geometry());
        settings_->setValue("window/maximized",isFullScreen()?maximizedBeforeFullscreen_:isMaximized());
    }
}
void MainWindow::recordError(const QString& message) {
    const QString entry=QDateTime::currentDateTime().toString(Qt::ISODate)+"  "+message.left(4000);
    errors_.append(entry);while(errors_.size()>100)errors_.removeFirst();
    const QString directory=QFileInfo(settings_->fileName()).absolutePath();QDir().mkpath(directory);
    const QString path=directory+"/errors.log";
    if(QFileInfo(path).size()>256*1024){QFile::remove(directory+"/errors.previous.log");QFile::rename(path,directory+"/errors.previous.log");}
    QFile file(path);if(file.open(QIODevice::Append|QIODevice::Text))file.write(entry.toUtf8()+'\n');
}
QString MainWindow::diagnosticText() const {
    return tr("LuminaPlayer\nQt: %1\nmpv: %2\nArchivo: %3\nVídeo: %4\nGPU activa: %5\n\nErrores de esta sesión:\n%6")
        .arg(QString::fromLatin1(qVersion()),player_->property("mpv-version"),player_->currentFile(),player_->property("video-codec"),player_->property("hwdec-current"),errors_.isEmpty()?tr("Sin errores registrados."):errors_.join('\n'));
}
