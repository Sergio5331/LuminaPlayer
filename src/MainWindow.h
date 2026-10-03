#pragma once
#include <QMainWindow>
#include <QPoint>
#include <QStringList>
#include <QPointer>
#include <memory>
class MpvPlayer;
class ThumbnailPreview;
class PlayerControls;
class QLabel;
class QDialog;
class QListWidget;
class QSettings;
class QStackedWidget;
class QVBoxLayout;
class QDragEnterEvent;
class QDropEvent;
class QMouseEvent;
class QWheelEvent;

class QVideoContainerWidget final : public QWidget {
    Q_OBJECT
public:
    explicit QVideoContainerWidget(QWidget* parent = nullptr);
    void routeNativeInputToQt();
signals:
    void filesDropped(const QStringList& paths);
    void doubleClicked();
protected:
    bool nativeEvent(const QByteArray& type, void* message, qintptr* result) override;
    void paintEvent(QPaintEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
};

class MainWindow final : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QString captureDirectory = {}, QWidget* parent = nullptr, QString settingsFile = {});
    ~MainWindow() override;
    void openFile(const QString& path);
    void openFiles(const QStringList& paths);
    void showLibrary();
    void showCaptureHistory();
    void showDiagnostics();
    void showAdvanced();
    void showShortcuts();
    void showCaptureSettings();
    void showSubtitleSettings();
    void nextFile(int direction = 1);
    QStringList playlist() const { return playlist_; }
    void captureFrame(bool subtitles = false);
    void toggleFullscreen();
    void togglePip();
    bool pip() const { return pip_; }
    MpvPlayer* player() const { return player_.get(); }
    PlayerControls* controls() const { return controls_; }
    QVideoContainerWidget* videoContainer() const { return video_; }
protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    bool nativeEvent(const QByteArray& type, void* message, qintptr* result) override;
    void resizeEvent(QResizeEvent* event) override;
    void moveEvent(QMoveEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void changeEvent(QEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
private:
    void chooseFile();
    void showPipMenu(const QPoint& position);
    bool pipDragging_ = false;
    Qt::Edges pipResizeEdges_;
    QRect pipResizeStart_;
    double pipAspect_ = 16.0/9.0;
    QRect constrainPip(const QRect& proposed, Qt::Edges edges) const;
    void saveWindowState();
    void recordError(const QString& message);
    QStringList errors_;
    QString diagnosticText() const;
    QPoint pipDragStart_, pipWindowStart_;
    void updateShortcutHints();
    ThumbnailPreview* thumbnails_ = nullptr;
    double loopA_ = -1;
    void runAction(const QString& id);
    QString shortcut(const QString& id, const QString& fallback) const;
    void showCapturePreview(const QString& path);
    void refreshLibrary();
    QStringList playlist_;
    int playlistIndex_ = -1;
    QPointer<QDialog> preview_;
    QPointer<QDialog> library_;
    QListWidget* queueList_ = nullptr;
    QListWidget* recentList_ = nullptr;
    void showTracks();
    void showTools();
    void saveSession();
    QString sessionKey(const QString& path) const;
    std::unique_ptr<QSettings> settings_;
    QStackedWidget* stack_ = nullptr;
    bool pip_ = false, maximizedBeforePip_ = false;
    QRect geometryBeforePip_;
    void positionControls();
    void activity();
    bool handleKey(int key, Qt::KeyboardModifiers modifiers, bool repeated);
    QVideoContainerWidget* video_ = nullptr;
    PlayerControls* controls_ = nullptr;
    QWidget* titleBar_ = nullptr;
    QLabel* title_ = nullptr;
    QVBoxLayout* layout_ = nullptr;
    std::unique_ptr<MpvPlayer> player_;
    QPoint lastMousePosition_{-100000, -100000};
    bool maximizedBeforeFullscreen_ = false;
};
