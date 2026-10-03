#include "PlayerControls.h"
#include "UiIcons.h"
#include <QApplication>
#include <QPainterPath>
#include <QIcon>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

namespace {
class SeekSlider final : public QSlider {
public:
    explicit SeekSlider(QWidget* parent) : QSlider(Qt::Horizontal, parent) {}
protected:
    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() != Qt::LeftButton) { QSlider::mousePressEvent(event); return; }
        setSliderDown(true);
        moveTo(event->position().x());
        event->accept();
    }
    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (isSliderDown()) { moveTo(event->position().x()); event->accept(); }
        else QSlider::mouseMoveEvent(event);
    }
    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton && isSliderDown()) {
            moveTo(event->position().x());
            setSliderDown(false);
            event->accept();
        } else QSlider::mouseReleaseEvent(event);
    }
private:
    void moveTo(double x)
    {
        QStyleOptionSlider option;
        initStyleOption(&option);
        const QRect groove = style()->subControlRect(QStyle::CC_Slider, &option, QStyle::SC_SliderGroove, this);
        const QRect handle = style()->subControlRect(QStyle::CC_Slider, &option, QStyle::SC_SliderHandle, this);
        const int span = std::max(1, groove.width() - handle.width());
        setSliderPosition(QStyle::sliderValueFromPosition(minimum(), maximum(),
            static_cast<int>(x) - groove.left() - handle.width() / 2, span, option.upsideDown));
    }
};
}

PlayerControls::PlayerControls(QWidget* owner)
    : QWidget(owner, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus)
{
    setObjectName("playerControls");
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setMouseTracking(true);
    setFocusPolicy(Qt::NoFocus);
    setFixedHeight(118);
    setStyleSheet(R"(
        QLabel { color: #dedee3; background: transparent; font-size: 12px; }
        QPushButton { color: #ededf2; background: transparent; border: none;
            border-radius: 8px; padding: 7px; font-size: 15px; }
        QPushButton#playButton { background: #8b6cc4; border-radius: 18px; }
        QPushButton:hover { background: #343439; }
        QPushButton:pressed { background: #494050; }
        QSlider::groove:horizontal { height: 4px; background: #49494e; border-radius: 2px; }
        QSlider::sub-page:horizontal { background: #b3a0df; border-radius: 2px; }
        QSlider::handle:horizontal { background: #eee8ff; width: 12px; margin: -5px 0;
            border-radius: 6px; }
        QSlider::sub-page:horizontal:disabled { background: #49494e; }
        QSlider::handle:horizontal:disabled { background: #66666b; }
    )");
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 10, 20, 12);
    layout->setSpacing(4);
    message_ = new QLabel(tr("LuminaPlayer · Abre un archivo o arrástralo a la ventana"), this);
    message_->setObjectName("statusMessage");
    message_->setTextFormat(Qt::PlainText);
    message_->setMinimumWidth(0);
    message_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    layout->addWidget(message_);
    progress_ = new SeekSlider(this);
    progress_->setObjectName("progressSlider");
    progress_->setAccessibleName(tr("Posición de reproducción"));
    progress_->setRange(0, 10000);
    progress_->setFixedHeight(18);
    progress_->setEnabled(false);
    layout->addWidget(progress_);
    auto* row = new QHBoxLayout;
    row->setSpacing(10);
    const auto button = [this, row](const QString& text, const QString& tip) {
        auto* result = new QPushButton(this);
        result->setIcon(luminaIcon(text));
        result->setIconSize(QSize(22,22));
        const QString id = text == "play" ? "pause" : text == "camera" ? "capture" : text == "fullscreen" ? "fullscreen" : text;
        result->setProperty("shortcutId",id);
        result->setProperty("baseTooltip",tip.section(" · ",0,0));
        result->setToolTip(tip);
        result->setAccessibleName(tip);
        result->setFocusPolicy(Qt::NoFocus);
        result->setFixedSize(36, 36);
        row->addWidget(result);
        return result;
    };
    auto* open = button("open", tr("Abrir archivo · Ctrl+O"));
    play_ = button("play", tr("Reproducir / Pausar · Espacio"));
    play_->setObjectName("playButton");
    time_ = new QLabel("00:00 / 00:00", this);
    time_->setObjectName("timeLabel");
    row->addWidget(time_);
    row->addStretch();
    auto* capture = button("camera", tr("Captura limpia · S / Con subtítulos · Shift+S"));
    auto* volumeLabel = new QLabel(tr("Vol"), this);
    row->addWidget(volumeLabel);
    volume_ = new QSlider(Qt::Horizontal, this);
    volume_->setObjectName("volumeSlider");
    volume_->setAccessibleName(tr("Volumen"));
    volume_->setRange(0, 100);
    volume_->setValue(70);
    volume_->setFixedWidth(90);
    volume_->setFocusPolicy(Qt::NoFocus);
    row->addWidget(volume_);
    volumeText_ = new QLabel("70%", this);
    volumeText_->setFixedWidth(35);
    row->addWidget(volumeText_);
    fullscreen_ = button("fullscreen", tr("Pantalla completa · Doble clic / F11"));
    auto* tracks = button("tracks", tr("Audio y subtítulos"));
    tracks->setObjectName("tracksButton");
    auto* pip = button("pip", tr("Ventana flotante · Ctrl+P"));
    pip->setObjectName("pipButton");
    connect(tracks, &QPushButton::clicked, this, &PlayerControls::tracksRequested);
    connect(pip, &QPushButton::clicked, this, &PlayerControls::pipRequested);
    auto* library = button("library", tr("Lista y recientes · Ctrl+L"));
    auto* more = button("more", tr("Más opciones"));
    library->setObjectName("libraryButton");
    more->setObjectName("moreButton");
    connect(library, &QPushButton::clicked, this, &PlayerControls::libraryRequested);
    connect(more, &QPushButton::clicked, this, &PlayerControls::toolsRequested);
    expandedWidgets_ = {open, capture, volumeLabel, volume_, volumeText_, fullscreen_, library};
    layout->addLayout(row);
    connect(open, &QPushButton::clicked, this, &PlayerControls::openRequested);
    connect(play_, &QPushButton::clicked, this, &PlayerControls::pauseRequested);
    connect(capture, &QPushButton::clicked, this, &PlayerControls::captureRequested);
    connect(fullscreen_, &QPushButton::clicked, this, &PlayerControls::fullscreenRequested);
    connect(progress_, &QSlider::sliderPressed, this, [this] { hideTimer_.stop(); });
    connect(progress_, &QSlider::valueChanged, this, [this] { if (progress_->isSliderDown()) refreshTime(); });
    connect(progress_, &QSlider::sliderReleased, this, [this] {
        if (seekable_ && duration_ > 0) emit seekRequested(duration_ * progress_->value() / 10000.0);
        activity();
    });
    connect(volume_, &QSlider::valueChanged, this, [this](int value) {
        volumeText_->setText(QString::number(value) + "%");
        emit volumeRequested(value);
        activity();
    });
    hideTimer_.setSingleShot(true);
    hideTimer_.setInterval(2500);
    connect(&hideTimer_, &QTimer::timeout, this, [this] {
        if (progress_->underMouse() || progress_->property("thumbnailHover").toBool() || QApplication::activePopupWidget() || progress_->isSliderDown() || volume_->isSliderDown() || QApplication::mouseButtons() != Qt::NoButton) {
            hideTimer_.start(); return;
        }
        hide();
        emit visibilityRequested(false);
    });
    messageTimer_.setSingleShot(true);
    messageTimer_.setInterval(6000);
    connect(&messageTimer_, &QTimer::timeout, message_, &QLabel::clear);
}

QString PlayerControls::formatTime(double seconds)
{
    const auto total = static_cast<qint64>(std::clamp(std::isfinite(seconds) ? seconds : 0.0, 0.0, 359999999.0));
    const QString tail = QString("%1:%2").arg((total / 60) % 60, 2, 10, QChar('0')).arg(total % 60, 2, 10, QChar('0'));
    return total >= 3600 ? QString("%1:%2").arg(total / 3600, 2, 10, QChar('0')).arg(tail) : tail;
}
void PlayerControls::refreshTime()
{
    const double value = progress_->isSliderDown() ? duration_ * progress_->value() / 10000.0 : position_;
    time_->setText(formatTime(value) + " / " + formatTime(duration_));
}
void PlayerControls::setPosition(double seconds)
{
    position_ = seconds;
    if (!progress_->isSliderDown()) {
        const QSignalBlocker blocker(progress_);
        progress_->setValue(duration_ > 0 ? static_cast<int>(std::clamp(seconds / duration_, 0.0, 1.0) * 10000) : 0);
    }
    refreshTime();
}
void PlayerControls::setDuration(double seconds)
{
    duration_ = seconds;
    progress_->setEnabled(seekable_ && duration_ > 0);
    setPosition(position_);
}
void PlayerControls::setPaused(bool paused) { play_->setIcon(luminaIcon(paused ? "play" : "pause")); }
void PlayerControls::setVolume(int percent)
{
    const QSignalBlocker blocker(volume_);
    if (!volume_->isSliderDown()) volume_->setValue(percent);
    volumeText_->setText(QString::number(percent) + "%");
}
void PlayerControls::setSeekable(bool enabled)
{
    seekable_ = enabled;
    progress_->setEnabled(enabled && duration_ > 0);
}
void PlayerControls::setAutoHideEnabled(bool enabled)
{
    autoHide_ = enabled;
    if (enabled) activity(); else hideTimer_.stop();
}
void PlayerControls::setFullscreen(bool fullscreen)
{
    fullscreen_->setToolTip(fullscreen ? tr("Salir de pantalla completa") : tr("Pantalla completa · F11"));
}
void PlayerControls::showMessage(const QString& text)
{
    message_->setText(text);
    message_->setToolTip(text);
    messageTimer_.start();
    activity();
}
void PlayerControls::activity()
{
    if (suppressed_) return;
    if (!parentWidget()->isVisible() || parentWidget()->isMinimized()) return;
    show();
    raise();
    emit visibilityRequested(true);
    if (autoHide_) hideTimer_.start();
}
void PlayerControls::suspend()
{
    hideTimer_.stop();
    hide();
    emit visibilityRequested(true); // Restaurar cursor al salir de la aplicación.
}
void PlayerControls::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QColor(255, 255, 255, 24));
    painter.setBrush(QColor(23, 23, 26, 232));
    painter.drawRoundedRect(QRectF(rect()).adjusted(1, 1, -1, -1), 14, 14);
}

void PlayerControls::setCompact(bool compact)
{
    for (auto* widget : expandedWidgets_) widget->setVisible(!compact);
    message_->setVisible(!compact);
    setFixedHeight(compact ? 88 : 118);
}

void PlayerControls::setSuppressed(bool suppressed)
{
    suppressed_ = suppressed;
    if (suppressed) suspend();
}
