#include "ThumbnailPreview.h"
#include "MpvPlayer.h"
#include "PlayerControls.h"
#include <QCoreApplication>
#include <QSlider>
#include <QLabel>
#include <QMouseEvent>
#include <QSettings>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QPainter>
#include <QScreen>
#include <algorithm>
#include <cmath>
#include <limits>
#include <windows.h>

ThumbnailPreview::ThumbnailPreview(QSlider* slider, MpvPlayer* player, QSettings* settings)
    : QObject(slider), slider_(slider), player_(player), settings_(settings),
      popup_(new QLabel(slider->window(), Qt::Tool | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus))
{
    popup_->setObjectName("thumbnailPreview");
    popup_->setAttribute(Qt::WA_ShowWithoutActivating);
    popup_->setAttribute(Qt::WA_TransparentForMouseEvents);
    popup_->setTextFormat(Qt::PlainText);
    popup_->setAlignment(Qt::AlignCenter);
    popup_->setStyleSheet("QLabel { background:#201b2c; color:#eee8fa; border:1px solid #8971ac; border-radius:8px; padding:5px; }");
    slider_->setMouseTracking(true);
    slider_->installEventFilter(this);
    process_.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) {
        args->flags |= CREATE_NO_WINDOW;
    });
    debounce_.setSingleShot(true);
    debounce_.setInterval(0);
    timeout_.setSingleShot(true);
    timeout_.setInterval(8000);
    connect(&debounce_, &QTimer::timeout, this, &ThumbnailPreview::request);
    connect(&timeout_, &QTimer::timeout, this, [this] {
        timedOut_ = true;
        process_.kill();
    });
    connect(&process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart) return; // Los otros errores llegan también a finished.
        timeout_.stop();
        directory_.reset();
        pending_.clear(); failedKey_ = wanted_;
        showStatus(tr("No se pudo iniciar el generador de miniaturas.\n%1").arg(process_.errorString()));
    });
    connect(&process_, qOverload<int,QProcess::ExitStatus>(&QProcess::finished), this,
        [this](int code, QProcess::ExitStatus status) {
        timeout_.stop();
        bool produced = false;
        if (!cancelled_ && !timedOut_ && code == 0 && status == QProcess::NormalExit && directory_) {
            const QDir dir(directory_->path());
            const auto files = dir.entryList({"*.png"}, QDir::Files);
            if (!files.isEmpty()) {
                QImage image(dir.filePath(files.first()));
                if (!image.isNull()) {
                    image = image.scaled(320,180,Qt::KeepAspectRatio,Qt::SmoothTransformation);
                    if (runningPrefetch_) overview_.insert(runningSeconds_, image);
                    cache_.insert(running_, new QImage(image));
                    produced = true;
                    // Mientras se procesa la posición nueva, conservar un fotograma real
                    // con su propio tiempo, sin etiquetarlo como si fuera el solicitado.
                    if (!wanted_.isEmpty()) {
                        if (const auto* current = cache_.object(wanted_)) display(*current, seconds_);
                        else displayNearest();
                    }
                }
            }
        }
        directory_.reset();
        if (!produced && !cancelled_) {
            failedKey_ = running_;
            if (!wanted_.isEmpty() && wanted_ == running_ && !displayNearest())
                showStatus(timedOut_ ? tr("La miniatura tardó demasiado.\nPrueba otra posición.")
                                    : tr("No se pudo generar este fotograma.\nPrueba otra posición."));
        }
        schedule();
    });
    connect(player_, &MpvPlayer::playbackReset, this, [this] {
        cancel(); debounce_.stop(); timeout_.stop(); pending_.clear(); overview_.clear(); cache_.clear();
        mediaKey_.clear(); failedKey_.clear();
        if (process_.state() != QProcess::NotRunning) { cancelled_ = true; process_.kill(); }
    });
    connect(player_, &MpvPlayer::fileLoaded, this, [this] { prepare(); });
    connect(player_, &MpvPlayer::durationChanged, this, [this] { prepare(); });
}
ThumbnailPreview::~ThumbnailPreview()
{
    process_.disconnect(this);
    process_.kill();
    process_.waitForFinished(1000);
    delete popup_;
}
void ThumbnailPreview::cancel()
{
    wanted_.clear();
    slider_->setProperty("thumbnailHover", false);
    popup_->hide();
    // Terminar la imagen en curso permite reutilizarla al volver a la barra.
    schedule();
}
QString ThumbnailPreview::keyFor(double seconds) const { return mediaKey_ + "|" + QString::number(seconds); }
void ThumbnailPreview::prepare()
{
    if (!settings_->value("thumbnails",true).toBool() || !player_->loaded() || player_->duration() <= 0 ||
        player_->property("video-codec").isEmpty() || !mediaKey_.isEmpty()) return;
    const QFileInfo file(player_->currentFile());
    mediaKey_ = file.absoluteFilePath() + "|" + QString::number(file.size()) + "|" + QString::number(file.lastModified().toMSecsSinceEpoch());
    // Cobertura progresiva: primero la zona actual y después puntos repartidos.
    pending_.append(std::floor(player_->position()));
    for (int index : {0,8,4,12,2,6,10,14,1,3,5,7,9,11,13}) {
        const double value = std::floor(std::max(0.0,player_->duration()-0.1)*index/14.0);
        if (!pending_.contains(value)) pending_.append(value);
    }
    schedule();
}
bool ThumbnailPreview::displayNearest()
{
    const QImage* nearest = nullptr;
    double time = 0, distance = std::numeric_limits<double>::max();
    for (auto it = overview_.cbegin(); it != overview_.cend(); ++it) {
        const double delta = std::abs(it.key()-seconds_);
        if (delta < distance) { distance=delta; time=it.key(); nearest=&it.value(); }
    }
    for (const auto& key : cache_.keys()) {
        const double candidate = key.section('|',-1).toDouble();
        const double delta = std::abs(candidate-seconds_);
        if (delta < distance) { distance=delta; time=candidate; nearest=cache_.object(key); }
    }
    if (!nearest) return false;
    display(*nearest,time);
    return true;
}
void ThumbnailPreview::schedule()
{
    if (!settings_->value("thumbnails",true).toBool() || mediaKey_.isEmpty() || process_.state()!=QProcess::NotRunning) return;
    const bool exact = !wanted_.isEmpty() && !cache_.object(wanted_) && wanted_ != failedKey_;
    if (exact) debounce_.start(0);
    else if (!pending_.isEmpty() && !debounce_.isActive()) debounce_.start(250);
}

bool ThumbnailPreview::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::Leave || event->type() == QEvent::Hide || event->type() == QEvent::MouseButtonPress) cancel();
    if (event->type() == QEvent::MouseMove) {
        if (!settings_->value("thumbnails",true).toBool() || !player_->loaded() || !player_->seekable() ||
            slider_->isSliderDown() || player_->property("video-codec").isEmpty()) { cancel(); return false; }
        slider_->setProperty("thumbnailHover", true);
        const auto* mouse = static_cast<QMouseEvent*>(event);
        hoverX_ = std::clamp(static_cast<int>(mouse->position().x()), 0, slider_->width());
        seconds_ = std::floor(static_cast<double>(hoverX_) / std::max(1,slider_->width()) * std::max(0.0,player_->duration()-0.1));
        prepare();
        const QString key = keyFor(seconds_);
        if (key != wanted_) {
            wanted_ = key;
            if (const auto* image = cache_.object(key)) display(*image, seconds_);
            else {
                if (!displayNearest()) showStatus(tr("Preparando vista previa…"));
                schedule();
            }
        }
        positionPopup();
    }
    return QObject::eventFilter(watched,event);
}
void ThumbnailPreview::request()
{
    if (mediaKey_.isEmpty() || !settings_->value("thumbnails",true).toBool() || process_.state()!=QProcess::NotRunning) return;
    runningPrefetch_ = wanted_.isEmpty() || cache_.object(wanted_) || wanted_ == failedKey_;
    if (runningPrefetch_) {
        while (!pending_.isEmpty() && cache_.object(keyFor(pending_.first()))) pending_.removeFirst();
        if (pending_.isEmpty()) return;
        runningSeconds_ = pending_.takeFirst(); running_ = keyFor(runningSeconds_);
    } else { running_ = wanted_; runningSeconds_ = seconds_; }
    const QString helper = QCoreApplication::applicationDirPath() + "/LuminaThumbnail.exe";
    if (!QFileInfo::exists(helper)) {
        pending_.clear(); failedKey_=wanted_; showStatus(tr("Falta LuminaThumbnail.exe\nen la carpeta del reproductor.")); return;
    }
    directory_ = std::make_unique<QTemporaryDir>();
    if (!directory_->isValid()) { showStatus(tr("No se pudo crear la carpeta temporal.")); return; }
    cancelled_ = timedOut_ = false;
    process_.setProgram(helper);
    process_.setArguments({player_->currentFile(), QString::number(runningSeconds_), directory_->path()});
    process_.start();
    timeout_.start();
}
void ThumbnailPreview::showStatus(const QString& text)
{
    if (wanted_.isEmpty() || !slider_->isVisible()) return;
    popup_->clear();
    popup_->setWordWrap(true);
    popup_->setFixedSize(332,110);
    popup_->setText(text);
    positionPopup();
    popup_->show();
    popup_->raise();
}
void ThumbnailPreview::positionPopup()
{
    QPoint point = slider_->mapToGlobal(QPoint(hoverX_-popup_->width()/2, -popup_->height()-10));
    const QRect screen = slider_->screen()->availableGeometry();
    point.setX(std::clamp(point.x(),screen.left(),std::max(screen.left(),screen.right()-popup_->width())));
    point.setY(std::max(screen.top(),point.y()));
    popup_->move(point);
}
void ThumbnailPreview::display(const QImage& image, double timestamp)
{
    if (wanted_.isEmpty() || !slider_->isVisible()) return;
    QPixmap card(320,210);
    card.fill(QColor("#201b2c"));
    QPainter painter(&card);
    painter.drawImage(QPoint((320-image.width())/2,(180-image.height())/2),image);
    painter.setPen(QColor("#eee8fa"));
    painter.drawText(QRect(0,180,320,30),Qt::AlignCenter,PlayerControls::formatTime(timestamp) + (std::abs(timestamp-seconds_) >= 1 ? tr(" · aproximación") : QString()));
    painter.end();
    popup_->setFixedSize(332,222);
    popup_->setPixmap(card);
    positionPopup();
    popup_->show();
    popup_->raise();
}
