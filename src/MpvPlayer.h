#pragma once
#include <QObject>
#include <QByteArray>
#include <QHash>
#include <QString>
#include <QVector>
#include <QtGui/qwindowdefs.h>
#include <atomic>
#include <cstdint>
#include <initializer_list>
#include <memory>
struct mpv_handle;

// Todas las llamadas pÃºblicas pertenecen al hilo Qt. El HWND debe sobrevivir al objeto.
class MpvPlayer final : public QObject {
    Q_OBJECT
public:
    enum class ImageFormat { Png, Jpeg };
    explicit MpvPlayer(WId window, QString captureDirectory = {}, QObject* parent = nullptr);
    ~MpvPlayer() override;
    MpvPlayer(const MpvPlayer&) = delete;
    MpvPlayer& operator=(const MpvPlayer&) = delete;
    struct Track { qint64 id; QString type, title, language; bool selected; };
    QVector<Track> tracks() const;
    void selectTrack(const QString& type, qint64 id);
    void addSubtitles(const QString& path);
    void loadFile(const QString& path, double start = 0);
    QString property(const QString& name) const;
    void setSpeed(double value);
    void setAudioMode(int mode);
    void setLoop(double a, double b);
    void clearLoop();
    QVector<QPair<QString, double>> chapters() const;
    void togglePause();
    void setPaused(bool paused);
    void seekRelative(double seconds);
    void seekAbsolute(double seconds);
    void setVolume(int percent);
    void changeVolume(int delta);
    void changeSubtitleDelay(double seconds);
    void stepFrame(bool backwards = false);
    void configureCaptures(const QString& directory, int jpegQuality);
    void configureSubtitles(double scale, int position, double delay, bool overrideStyles);
    double subtitleDelay() const { return subtitleDelay_; }
    void captureClean(ImageFormat format = ImageFormat::Png);
    void captureWithSubtitles(ImageFormat format = ImageFormat::Png);
    double position() const { return position_; }
    double duration() const { return duration_; }
    int volume() const { return volume_; }
    bool paused() const { return paused_; }
    bool loaded() const { return loaded_; }
    bool seekable() const { return seekable_; }
    bool capturing() const { return capturing_; }
    QString captureDirectory() const { return captureDirectory_; }
    QString currentFile() const { return currentFile_; }
signals:
    void positionChanged(double seconds);
    void durationChanged(double seconds);
    void pauseChanged(bool paused);
    void volumeChanged(int percent);
    void seekableChanged(bool seekable);
    void fileLoaded(const QString& path);
    void playbackReset();
    void playbackFinished();
    void videoReconfigured();
    void screenshotSaved(const QString& path);
    void errorOccurred(const QString& message);
private:
    struct Deleter { void operator()(mpv_handle* handle) const noexcept; };
    enum class Operation { Control, Load, Capture };
    struct Pending { Operation operation; QString description; QString pattern; };
    void option(const char* name, const char* value);
    bool command(std::initializer_list<QByteArray> args, Pending pending);
    void capture(const char* mode, ImageFormat format);
    void startLoad(const QString& path);
    void resumeQueuedLoad();
    void resetPlayback();
    static void wakeup(void* context) noexcept;
    void drainEvents();
    std::unique_ptr<mpv_handle, Deleter> handle_;
    std::atomic_bool wakeupQueued_{false};
    QHash<quint64, Pending> pending_;
    std::uint64_t nextRequest_ = 1;
    QString captureDirectory_, currentFile_, queuedFile_;
    double subtitleDelay_ = 0;
    bool eof_ = false;
    double queuedStart_ = 0;
    double position_ = 0, duration_ = 0;
    int volume_ = 70;
    bool paused_ = false, loaded_ = false, loading_ = false;
    bool seekable_ = false, capturing_ = false, stopped_ = false;
};
