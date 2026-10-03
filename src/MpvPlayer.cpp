#include "MpvPlayer.h"
#include <mpv/client.h>
#include <QDir>
#include <QFileInfo>
#include <QMetaObject>
#include <QRegularExpression>
#include <QThread>
#include <QTimer>
#include <QUuid>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
void check(int result, const char* operation)
{
    if (result < 0) throw std::runtime_error(std::string(operation) + ": " + mpv_error_string(result));
}
QString propertyString(mpv_handle* handle, const char* name)
{
    const std::unique_ptr<char, decltype(&mpv_free)> text(mpv_get_property_string(handle, name), &mpv_free);
    return text ? QString::fromUtf8(text.get()) : QString{};
}
}
void MpvPlayer::Deleter::operator()(mpv_handle* handle) const noexcept
{
    if (handle) mpv_terminate_destroy(handle);
}
MpvPlayer::MpvPlayer(WId window, QString captureDirectory, QObject* parent)
    : QObject(parent), handle_(mpv_create()),
      captureDirectory_(QDir(captureDirectory.isEmpty()
          ? QDir::current().filePath("capturas") : captureDirectory).absolutePath())
{
    if (!handle_ || !window) throw std::runtime_error("No se pudo crear mpv o su HWND anfitrión.");
    option("config", "no");
    option("load-scripts", "no");
    option("osc", "no");
    option("osd-level", "0");
    option("terminal", "no");
    option("input-default-bindings", "no");
    option("input-vo-keyboard", "no");
    option("input-cursor", "no");
    option("cursor-autohide", "no");
    option("idle", "yes");
    option("keep-open", "yes");
    option("force-window", "yes");
    // HWND: uint32_t extendido a int64_t según el contrato de wid en Windows.
    std::int64_t wid = static_cast<std::uint32_t>(window);
    check(mpv_set_option(handle_.get(), "wid", MPV_FORMAT_INT64, &wid), "wid");
    option("vo", "gpu");
    option("gpu-api", "d3d11");
    option("gpu-context", "d3d11");
    option("hwdec", "auto-safe");
    option("hwdec-codecs", "all");
    option("target-prim", "bt.709");
    option("target-trc", "srgb");
    option("tone-mapping", "auto");
    // Normalización dinámica con ventana moderada para limitar latencia.
    option("af", "lavfi=[dynaudnorm=f=150:g=9:p=0.90:m=5]");
    option("volume", "70");
    option("audio-pitch-correction", "yes");
    option("volume-max", "100");
    option("sub-ass", "yes");
    option("sub-ass-override", "no");
    option("sub-codepage", "utf-8");
    option("sub-auto", "exact");
    option("screenshot-sw", "no");
    option("screenshot-png-compression", "3");
    option("screenshot-jpeg-quality", "95");
    check(mpv_initialize(handle_.get()), "mpv_initialize");
    check(mpv_observe_property(handle_.get(), 1, "time-pos", MPV_FORMAT_DOUBLE), "time-pos");
    check(mpv_observe_property(handle_.get(), 2, "duration", MPV_FORMAT_DOUBLE), "duration");
    check(mpv_observe_property(handle_.get(), 3, "pause", MPV_FORMAT_FLAG), "pause");
    check(mpv_observe_property(handle_.get(), 4, "volume", MPV_FORMAT_DOUBLE), "volume");
    check(mpv_observe_property(handle_.get(), 5, "seekable", MPV_FORMAT_FLAG), "seekable");
    check(mpv_observe_property(handle_.get(), 6, "eof-reached", MPV_FORMAT_FLAG), "eof-reached");
    check(mpv_observe_property(handle_.get(), 7, "sub-delay", MPV_FORMAT_DOUBLE), "sub-delay");
    // Nada susceptible de lanzar una excepción después de registrar el callback.
    mpv_set_wakeup_callback(handle_.get(), &MpvPlayer::wakeup, this);
}
MpvPlayer::~MpvPlayer()
{
    mpv_set_wakeup_callback(handle_.get(), nullptr, nullptr);
    handle_.reset(); // Termina hilos antes de destruir los demás miembros y el HWND.
}
void MpvPlayer::option(const char* name, const char* value)
{
    check(mpv_set_option_string(handle_.get(), name, value), name);
}
bool MpvPlayer::command(std::initializer_list<QByteArray> args, Pending pending)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (stopped_) { emit errorOccurred(tr("El motor se ha detenido.")); return false; }
    std::vector<const char*> argv;
    argv.reserve(args.size() + 1);
    for (const auto& argument : args) argv.push_back(argument.constData());
    argv.push_back(nullptr);
    const auto id = nextRequest_++;
    const int result = mpv_command_async(handle_.get(), id, argv.data());
    if (result < 0) {
        emit errorOccurred(pending.description + ": " + QString::fromUtf8(mpv_error_string(result)));
        return false;
    }
    pending_.insert(id, std::move(pending));
    return true;
}
void MpvPlayer::loadFile(const QString& path, double start)
{
    const QFileInfo info(path);
    if (!info.isFile() || !info.isReadable()) {
        emit errorOccurred(tr("Archivo inexistente o ilegible: %1").arg(path)); return;
    }
    queuedStart_ = std::isfinite(start) ? std::max(0.0, start) : 0;
    queuedFile_ = info.absoluteFilePath(); // Retener la última carga durante una captura.
    resumeQueuedLoad();
}
void MpvPlayer::resumeQueuedLoad()
{
    if (loading_ || capturing_ || queuedFile_.isEmpty()) return;
    startLoad(std::exchange(queuedFile_, {}));
}
void MpvPlayer::startLoad(const QString& path)
{
    if (command({"loadfile", QDir::fromNativeSeparators(path).toUtf8(), "replace", "-1", "start=" + QByteArray::number(queuedStart_, 'f', 3)},
                {Operation::Load, tr("Abrir archivo"), {}})) {
        loading_ = true;
        resetPlayback();
    }
}
void MpvPlayer::resetPlayback()
{
    loaded_ = false;
    eof_ = false;
    position_ = duration_ = 0;
    seekable_ = false;
    emit playbackReset();
    emit positionChanged(0);
    emit durationChanged(0);
    emit seekableChanged(false);
}
void MpvPlayer::togglePause()
{
    if (loaded_) command({"cycle", "pause"}, {Operation::Control, tr("Pausa"), {}});
}
void MpvPlayer::setPaused(bool paused)
{
    command({"set", "pause", paused ? "yes" : "no"}, {Operation::Control, tr("Pausa"), {}});
}
void MpvPlayer::seekRelative(double seconds)
{
    if (loaded_ && seekable_ && std::isfinite(seconds))
        command({"seek", QByteArray::number(seconds, 'f', 3), "relative+exact"}, {Operation::Control, tr("Desplazamiento"), {}});
}
void MpvPlayer::seekAbsolute(double seconds)
{
    if (loaded_ && seekable_ && std::isfinite(seconds))
        command({"seek", QByteArray::number(std::clamp(seconds, 0.0, duration_), 'f', 3), "absolute+exact"},
                {Operation::Control, tr("Desplazamiento"), {}});
}
void MpvPlayer::setVolume(int percent)
{
    command({"set", "volume", QByteArray::number(std::clamp(percent, 0, 100))}, {Operation::Control, tr("Volumen"), {}});
}
void MpvPlayer::changeVolume(int delta)
{
    command({"add", "volume", QByteArray::number(delta)}, {Operation::Control, tr("Volumen"), {}});
}
void MpvPlayer::changeSubtitleDelay(double seconds)
{
    if (std::isfinite(seconds))
        command({"add", "sub-delay", QByteArray::number(seconds)}, {Operation::Control, tr("Subtítulos"), {}});
}
void MpvPlayer::captureClean(ImageFormat format) { capture("video", format); }
void MpvPlayer::captureWithSubtitles(ImageFormat format) { capture("subtitles", format); }
void MpvPlayer::capture(const char* mode, ImageFormat format)
{
    if (!loaded_ || loading_) { emit errorOccurred(tr("Abre un vídeo antes de capturar.")); return; }
    if (capturing_) { emit errorOccurred(tr("Hay una captura en curso.")); return; }
    if (!QDir().mkpath(captureDirectory_)) { emit errorOccurred(tr("No se puede crear %1").arg(captureDirectory_)); return; }
    QString base = QFileInfo(currentFile_).completeBaseName();
    base.replace(QRegularExpression(R"([<>:"/\\|?*\x00-\x1f])"), "_");
    base = base.left(70);
    if (base.isEmpty()) base = "video";
    QString prefix = QDir(captureDirectory_).filePath("Lumina_" + base + "_t");
    prefix.replace("$", "$$");
    const QString suffix = "_" + QUuid::createUuid().toString(QUuid::Id128) + (format == ImageFormat::Png ? ".png" : ".jpg");
    // El tiempo se evalúa en mpv al ejecutar el comando, conservando fracciones.
    const QString file = prefix + "${=time-pos}_" + QString::fromLatin1(mode) + suffix;
    capturing_ = command({"expand-properties", "screenshot-to-file", file.toUtf8(), mode},
        {Operation::Capture, tr("Captura"), "*" + suffix});
}
void MpvPlayer::wakeup(void* context) noexcept
{
    auto* self = static_cast<MpvPlayer*>(context);
    if (!self->wakeupQueued_.exchange(true)) {
        QMetaObject::invokeMethod(self, [self] {
            self->wakeupQueued_.store(false);
            self->drainEvents();
        }, Qt::QueuedConnection);
    }
}
void MpvPlayer::drainEvents()
{
    if (!handle_) return;
    // Lotes limitados: ceder periódicamente el hilo a la interfaz.
    for (int count = 0; count < 128; ++count) {
        const auto* event = mpv_wait_event(handle_.get(), 0);
        if (event->event_id == MPV_EVENT_NONE) return;
        switch (event->event_id) {
        case MPV_EVENT_PROPERTY_CHANGE: {
            const auto* property = static_cast<mpv_event_property*>(event->data);
            if (property->format == MPV_FORMAT_NONE || !property->data) break;
            switch (event->reply_userdata) {
            case 1: position_ = std::max(0.0, *static_cast<double*>(property->data)); emit positionChanged(position_); break;
            case 2: duration_ = std::max(0.0, *static_cast<double*>(property->data)); emit durationChanged(duration_); break;
            case 3: paused_ = *static_cast<int*>(property->data) != 0; emit pauseChanged(paused_); break;
            case 4:
                volume_ = std::clamp(static_cast<int>(std::lround(*static_cast<double*>(property->data))), 0, 100);
                emit volumeChanged(volume_); break;
            case 5: seekable_ = *static_cast<int*>(property->data) != 0; emit seekableChanged(seekable_); break;
            case 6: {
                const bool ended = *static_cast<int*>(property->data) != 0;
                if (ended && !eof_ && loaded_) {
                    eof_ = true;
                    emit playbackFinished();
                } else eof_ = ended;
                break;
            }
            case 7: subtitleDelay_ = *static_cast<double*>(property->data); break;
            default: break;
            }
            break;
        }
        case MPV_EVENT_START_FILE: resetPlayback(); break;
        case MPV_EVENT_FILE_LOADED:
            loading_ = false;
            loaded_ = true;
            currentFile_ = QDir::fromNativeSeparators(propertyString(handle_.get(), "path"));
            setPaused(false);
            emit fileLoaded(currentFile_);
            QTimer::singleShot(0, this, &MpvPlayer::resumeQueuedLoad);
            break;
        case MPV_EVENT_VIDEO_RECONFIG: emit videoReconfigured(); break;
        case MPV_EVENT_END_FILE: {
            const auto* end = static_cast<mpv_event_end_file*>(event->data);
            if (end->reason != MPV_END_FILE_REASON_STOP) {
                loading_ = false;
                resetPlayback();
                QTimer::singleShot(0, this, &MpvPlayer::resumeQueuedLoad);
            }
            if (end->reason == MPV_END_FILE_REASON_ERROR)
                emit errorOccurred(tr("No se pudo reproducir: %1").arg(QString::fromUtf8(mpv_error_string(end->error))));
            break;
        }
        case MPV_EVENT_COMMAND_REPLY: {
            const auto iterator = pending_.find(event->reply_userdata);
            if (iterator == pending_.end()) break;
            const Pending pending = iterator.value();
            pending_.erase(iterator);
            if (pending.operation == Operation::Capture) capturing_ = false;
            if (event->error < 0) {
                if (pending.operation == Operation::Load) loading_ = false;
                emit errorOccurred(pending.description + ": " + QString::fromUtf8(mpv_error_string(event->error)));
            } else if (pending.operation == Operation::Capture) {
                // screenshot-to-file no devuelve ruta: localizar sólo el UUID propio.
                const QDir directory(captureDirectory_);
                const auto files = directory.entryList({pending.pattern}, QDir::Files);
                if (files.size() == 1) emit screenshotSaved(directory.filePath(files.first()));
                else emit errorOccurred(tr("No se encontró el archivo confirmado por mpv."));
            }
            QTimer::singleShot(0, this, &MpvPlayer::resumeQueuedLoad);
            break;
        }
        case MPV_EVENT_SHUTDOWN:
            stopped_ = true;
            loading_ = capturing_ = false;
            pending_.clear();
            resetPlayback();
            emit errorOccurred(tr("El motor multimedia se ha detenido.")); break;
        case MPV_EVENT_QUEUE_OVERFLOW: emit errorOccurred(tr("Se desbordó la cola de eventos de mpv.")); break;
        default: break;
        }
    }
    wakeup(this);
}

QVector<MpvPlayer::Track> MpvPlayer::tracks() const
{
    QVector<Track> result;
    mpv_node node{};
    if (mpv_get_property(handle_.get(), "track-list", MPV_FORMAT_NODE, &node) < 0) return result;
    struct Guard { mpv_node* value; ~Guard() { mpv_free_node_contents(value); } } guard{&node};
    if (node.format != MPV_FORMAT_NODE_ARRAY) return result;
    for (int i = 0; i < node.u.list->num; ++i) {
        const auto& entry = node.u.list->values[i];
        if (entry.format != MPV_FORMAT_NODE_MAP) continue;
        Track track{};
        for (int j = 0; j < entry.u.list->num; ++j) {
            const QByteArray key(entry.u.list->keys[j]);
            const auto& value = entry.u.list->values[j];
            if (key == "id" && value.format == MPV_FORMAT_INT64) track.id = value.u.int64;
            if (key == "selected" && value.format == MPV_FORMAT_FLAG) track.selected = value.u.flag != 0;
            if (value.format == MPV_FORMAT_STRING) {
                if (key == "type") track.type = QString::fromUtf8(value.u.string);
                if (key == "title") track.title = QString::fromUtf8(value.u.string);
                if (key == "lang") track.language = QString::fromUtf8(value.u.string);
            }
        }
        if (track.type == "audio" || track.type == "sub") result.push_back(track);
    }
    return result;
}
void MpvPlayer::selectTrack(const QString& type, qint64 id)
{
    if (type != "audio" && type != "sub") return;
    command({"set", type == "audio" ? "aid" : "sid", id < 0 ? QByteArray("no") : QByteArray::number(id)},
        {Operation::Control, tr("Seleccionar pista"), {}});
}
void MpvPlayer::addSubtitles(const QString& path)
{
    if (!loaded_) { emit errorOccurred(tr("Abre un vídeo antes de cargar subtítulos.")); return; }
    command({"sub-add", QFileInfo(path).absoluteFilePath().toUtf8(), "cached"},
        {Operation::Control, tr("Cargar subtítulos"), {}});
}

void MpvPlayer::stepFrame(bool backwards)
{
    if (loaded_ && seekable_)
        command({backwards ? "frame-back-step" : "frame-step"}, {Operation::Control, tr("Avanzar fotograma"), {}});
}
void MpvPlayer::configureCaptures(const QString& directory, int jpegQuality)
{
    if (!directory.isEmpty()) captureDirectory_ = QDir(directory).absolutePath();
    command({"set", "screenshot-jpeg-quality", QByteArray::number(std::clamp(jpegQuality, 1, 100))},
        {Operation::Control, tr("Calidad de captura"), {}});
}
void MpvPlayer::configureSubtitles(double scale, int position, double delay, bool overrideStyles)
{
    if (!std::isfinite(scale) || !std::isfinite(delay)) return;
    command({"set", "sub-ass-override", overrideStyles ? "force" : "no"}, {Operation::Control, tr("Estilos de subtítulos"), {}});
    command({"set", "sub-scale", QByteArray::number(std::clamp(scale, 0.5, 3.0))}, {Operation::Control, tr("Tamaño de subtítulos"), {}});
    command({"set", "sub-pos", QByteArray::number(std::clamp(position, 0, 100))}, {Operation::Control, tr("Posición de subtítulos"), {}});
    command({"set", "sub-delay", QByteArray::number(std::clamp(delay, -600.0, 600.0))}, {Operation::Control, tr("Sincronización de subtítulos"), {}});
}

QString MpvPlayer::property(const QString& name) const { return propertyString(handle_.get(), name.toUtf8().constData()); }
void MpvPlayer::setSpeed(double value) {
    if (std::isfinite(value)) command({"set", "speed", QByteArray::number(std::clamp(value, 0.25, 3.0))}, {Operation::Control, tr("Velocidad"), {}});
}
void MpvPlayer::setAudioMode(int mode) {
    const QByteArray filter = mode == 0 ? QByteArray() : mode == 2
        ? QByteArray("lavfi=[dynaudnorm=f=150:g=9:p=0.85:m=3],lavfi=[acompressor=threshold=0.125:ratio=4:attack=20:release=250:makeup=1.5]")
        : QByteArray("lavfi=[dynaudnorm=f=150:g=9:p=0.90:m=5]");
    command({"set", "af", filter}, {Operation::Control, tr("Modo de audio"), {}});
}
void MpvPlayer::setLoop(double a, double b) {
    if (!loaded_ || !std::isfinite(a) || !std::isfinite(b) || a < 0 || b <= a || b > duration_) return;
    command({"set", "ab-loop-a", QByteArray::number(a)}, {Operation::Control, tr("Inicio A"), {}});
    command({"set", "ab-loop-b", QByteArray::number(b)}, {Operation::Control, tr("Final B"), {}});
    seekAbsolute(a);
}
void MpvPlayer::clearLoop() {
    command({"set", "ab-loop-a", "no"}, {Operation::Control, tr("Bucle A"), {}});
    command({"set", "ab-loop-b", "no"}, {Operation::Control, tr("Bucle B"), {}});
}
QVector<QPair<QString, double>> MpvPlayer::chapters() const {
    QVector<QPair<QString, double>> result;
    const int count = std::clamp(property("chapter-list/count").toInt(), 0, 10000);
    for (int i = 0; i < count; ++i) {
        const QString base = "chapter-list/" + QString::number(i) + "/";
        QString title = property(base + "title");
        if (title.isEmpty()) title = tr("Capítulo %1").arg(i + 1);
        result.append({title, property(base + "time").toDouble()});
    }
    return result;
}
