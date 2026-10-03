#pragma once
#include <QObject>
#include <QCache>
#include <QImage>
#include <QMap>
#include <QList>
#include <QProcess>
#include <QTimer>
#include <QTemporaryDir>
#include <memory>
class QSlider;
class QLabel;
class MpvPlayer;
class QSettings;
class ThumbnailPreview final : public QObject {
    Q_OBJECT
public:
    ThumbnailPreview(QSlider* slider, MpvPlayer* player, QSettings* settings);
    ~ThumbnailPreview() override;
    void cancel();
    int cachedFrameCount() const { return cache_.size(); }
protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
private:
    void request();
    void display(const QImage& image, double timestamp);
    void showStatus(const QString& text);
    void positionPopup();
    void schedule();
    void prepare();
    bool displayNearest();
    QString keyFor(double seconds) const;
    QMap<double,QImage> overview_;
    QList<double> pending_;
    QString mediaKey_, failedKey_;
    bool runningPrefetch_ = false;
    QSlider* slider_;
    MpvPlayer* player_;
    QSettings* settings_;
    QLabel* popup_;
    QTimer debounce_, timeout_;
    QProcess process_;
    std::unique_ptr<QTemporaryDir> directory_;
    QCache<QString,QImage> cache_{40};
    QString wanted_, running_;
    double seconds_ = 0, runningSeconds_ = 0;
    int hoverX_ = 0;
    bool cancelled_ = false, timedOut_ = false;
};
