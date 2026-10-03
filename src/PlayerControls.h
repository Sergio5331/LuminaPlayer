#pragma once
#include <QWidget>
#include <QTimer>
class QLabel;
class QPushButton;
class QSlider;

// Tool window propiedad de MainWindow: DWM la compone por encima del HWND de mpv.
class PlayerControls final : public QWidget {
    Q_OBJECT
public:
    explicit PlayerControls(QWidget* owner);
    void setPosition(double seconds);
    void setDuration(double seconds);
    void setPaused(bool paused);
    void setVolume(int percent);
    void setSeekable(bool enabled);
    void setAutoHideEnabled(bool enabled);
    void setFullscreen(bool fullscreen);
    void setCompact(bool compact);
    void setSuppressed(bool suppressed);
    void showMessage(const QString& text);
    void activity();
    void suspend();
    bool controlsVisible() const { return isVisible(); }
    static QString formatTime(double seconds);
signals:
    void pauseRequested();
    void seekRequested(double seconds);
    void volumeRequested(int percent);
    void openRequested();
    void captureRequested();
    void fullscreenRequested();
    void tracksRequested();
    void pipRequested();
    void libraryRequested();
    void toolsRequested();
    void visibilityRequested(bool visible);
protected:
    void paintEvent(QPaintEvent* event) override;
private:
    void refreshTime();
    QSlider* progress_ = nullptr;
    QSlider* volume_ = nullptr;
    QLabel* time_ = nullptr;
    QLabel* volumeText_ = nullptr;
    QLabel* message_ = nullptr;
    QPushButton* play_ = nullptr;
    QPushButton* fullscreen_ = nullptr;
    QList<QWidget*> expandedWidgets_;
    QTimer hideTimer_;
    QTimer messageTimer_;
    double position_ = 0, duration_ = 0;
    bool autoHide_ = false, seekable_ = false, suppressed_ = false;
};
