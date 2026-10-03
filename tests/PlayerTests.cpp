#include "MainWindow.h"
#include "SingleInstance.h"
#include "MediaFiles.h"
#include <QUuid>
#include <QTextEdit>
#include "ThumbnailPreview.h"
#include "MpvPlayer.h"
#include "PlayerControls.h"
#include "WindowsComApartment.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTimer>
#include <QProcess>
#include <QKeySequenceEdit>
#include <QTabWidget>
#include <QDataStream>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QImage>
#include <QLabel>
#include <QMimeData>
#include <QScreen>
#include <QSignalSpy>
#include <QSlider>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>
#include <QWheelEvent>
#include <windows.h>

namespace {
QByteArray chunk(const char* name, const QByteArray& data)
{
    QByteArray result(name, 4);
    QDataStream stream(&result, QIODevice::Append);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream << quint32(data.size());
    result += data;
    if (data.size() % 2) result += '\0';
    return result;
}
bool writeFile(const QString& path, const QByteArray& data)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
}
QByteArray makeVideo()
{
    constexpr quint32 width = 160, height = 90, frameSize = width * height * 3, frames = 100;
    QByteArray avih;
    QDataStream a(&avih, QIODevice::WriteOnly);
    a.setByteOrder(QDataStream::LittleEndian);
    for (quint32 value : {100000u, frameSize * 10, 0u, 0x10u, frames, 0u, 1u, frameSize, width, height, 0u, 0u, 0u, 0u}) a << value;
    QByteArray strh("vidsDIB ", 8);
    QDataStream s(&strh, QIODevice::Append);
    s.setByteOrder(QDataStream::LittleEndian);
    s << quint32(0) << quint16(0) << quint16(0);
    for (quint32 value : {0u, 1u, 10u, 0u, frames, frameSize, 0xffffffffu, 0u}) s << value;
    s << qint16(0) << qint16(0) << qint16(width) << qint16(height);
    QByteArray strf;
    QDataStream f(&strf, QIODevice::WriteOnly);
    f.setByteOrder(QDataStream::LittleEndian);
    f << quint32(40) << qint32(width) << qint32(height) << quint16(1) << quint16(24)
      << quint32(0) << frameSize << qint32(0) << qint32(0) << quint32(0) << quint32(0);
    QByteArray frame;
    frame.reserve(frameSize);
    for (quint32 y = 0; y < height; ++y)
        for (quint32 x = 0; x < width; ++x) {
            frame += char(90); frame += char(30 + y); frame += char(40 + x);
        }
    QByteArray movi("movi"), index;
    QDataStream i(&index, QIODevice::WriteOnly);
    i.setByteOrder(QDataStream::LittleEndian);
    for (quint32 n = 0; n < frames; ++n) {
        i.writeRawData("00db", 4);
        i << quint32(0x10) << quint32(movi.size()) << frameSize;
        movi += chunk("00db", frame);
    }
    const QByteArray hdrl = chunk("LIST", "hdrl" + chunk("avih", avih) +
        chunk("LIST", "strl" + chunk("strh", strh) + chunk("strf", strf)));
    return chunk("RIFF", "AVI " + hdrl + chunk("LIST", movi) + chunk("idx1", index));
}
QByteArray makeAudio()
{
    QByteArray format;
    QDataStream s(&format, QIODevice::WriteOnly);
    s.setByteOrder(QDataStream::LittleEndian);
    s << quint16(1) << quint16(2) << quint32(48000) << quint32(192000) << quint16(4) << quint16(16);
    return chunk("RIFF", "WAVE" + chunk("fmt ", format) + chunk("data", QByteArray(192000 * 4, '\0')));
}
}

class PlayerTests final : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { QApplication::setStyle("Fusion"); }
    void timeFormatting()
    {
        QCOMPARE(PlayerControls::formatTime(0), "00:00");
        QCOMPARE(PlayerControls::formatTime(65.9), "01:05");
        QCOMPARE(PlayerControls::formatTime(3661), "01:01:01");
    }
    void foldersAndPlaylists() {
        QTemporaryDir temp;
        const QString a=temp.filePath("capítulo 2.mp4"),b=temp.filePath("capítulo 10.mp4");
        QVERIFY(writeFile(a,"test"));QVERIFY(writeFile(b,"test"));QVERIFY(writeFile(temp.filePath("nota.txt"),"ignore"));
        QCOMPARE(MediaFiles::expand({temp.path()}),QStringList({a,b}));
        QString error;const QString m3u=temp.filePath("lista.m3u8");
        QVERIFY2(MediaFiles::savePlaylist(m3u,{b,a},&error),qPrintable(error));
        QCOMPARE(MediaFiles::expand({m3u}),QStringList({b,a}));
        QVERIFY(writeFile(temp.filePath("cycle.m3u"),"#EXTM3U\ncycle.m3u\nlista.m3u8\n"));
        QCOMPARE(MediaFiles::expand({temp.filePath("cycle.m3u")}),QStringList({b,a}));
    }
    void singleInstanceDelivery() {
        const QString name="LuminaTests-"+QUuid::createUuid().toString(QUuid::Id128);
        SingleInstance primary(name);QVERIFY(primary.primary());
        QSignalSpy received(&primary,&SingleInstance::openRequested);
        QProcess process;
        process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args){args->flags|=CREATE_NO_WINDOW;});
        process.start(QCoreApplication::applicationFilePath(),{"--forward-instance",name,"C:/Vídeos/película.mp4"});
        QVERIFY(process.waitForStarted(3000));
        QTRY_COMPARE_WITH_TIMEOUT(received.size(),1,8000);
        QTRY_COMPARE(process.state(),QProcess::NotRunning);QCOMPARE(process.exitCode(),0);
        QCOMPARE(received.first().first().toStringList(),QStringList({"C:/Vídeos/película.mp4"}));
    }
    void remembersWindows() {
        QTemporaryDir temp;const QString ini=temp.filePath("window.ini");
        QRect normal,pip;
        {
            MainWindow window({},nullptr,ini);window.show();window.setGeometry(70,80,850,500);normal=window.geometry();
            window.togglePip();window.setGeometry(130,150,320,180);pip=window.geometry();window.togglePip();window.close();
        }
        {
            MainWindow window({},nullptr,ini);window.show();QTRY_COMPARE(window.geometry(),normal);
            window.togglePip();QTRY_COMPARE(window.geometry(),pip);window.close();
        }
    }
    void controlsAutoHide()
    {
        QWidget owner;
        owner.resize(900, 500);
        owner.show();
        PlayerControls controls(&owner);
        controls.resize(850, 118);
        QSignalSpy visibility(&controls, &PlayerControls::visibilityRequested);
        controls.setAutoHideEnabled(true);
        const auto hidden = [&visibility] {
            return std::any_of(visibility.begin(), visibility.end(),
                [](const QList<QVariant>& args) { return !args.at(0).toBool(); });
        };
        QTRY_VERIFY_WITH_TIMEOUT(hidden(), 5000);
        QVERIFY(!controls.isVisible());
        controls.activity();
        QVERIFY(controls.isVisible());
    }
    void advancedPlaybackAndThumbnails()
    {
        QTemporaryDir temp;
        const QString video=temp.filePath("advanced.avi");
        QVERIFY(writeFile(video,makeVideo()));
        MainWindow window(temp.filePath("captures"),nullptr,temp.filePath("session.ini"));window.show();window.openFile(video);
        auto* player=window.player();QSignalSpy errors(player,&MpvPlayer::errorOccurred);
        QTRY_VERIFY(player->loaded()&&player->seekable()&&player->duration()>9);
        player->setSpeed(1.5);QTRY_VERIFY(std::abs(player->property("speed").toDouble()-1.5)<0.01);
        player->setAudioMode(0);QTRY_VERIFY(!player->property("af").contains("dynaudnorm"));
        player->setAudioMode(2);QTRY_VERIFY(player->property("af").contains("acompressor"));
        player->setAudioMode(1);player->setSpeed(1);
        player->setLoop(1,1.5);player->setPaused(false);
        QTRY_VERIFY(player->position()>=1&&player->position()<1.5);
        QTest::qWait(1200);QVERIFY(player->position()<1.6);
        player->clearLoop();QTRY_COMPARE(player->property("ab-loop-a"),QString("no"));
        player->setPaused(true);QTRY_VERIFY(player->paused());
        const double before=player->position();
        QTemporaryDir thumbs;QProcess worker;
        worker.start(QCoreApplication::applicationDirPath()+"/LuminaThumbnail.exe",{video,"3",thumbs.path()});
        QVERIFY(worker.waitForStarted(3000));QTRY_COMPARE_WITH_TIMEOUT(worker.state(),QProcess::NotRunning,12000);
        QCOMPARE(worker.exitCode(),0);const auto files=QDir(thumbs.path()).entryList({"*.png"},QDir::Files);QCOMPARE(files.size(),1);
        const QImage thumbnail(QDir(thumbs.path()).filePath(files.first()));QVERIFY(!thumbnail.isNull());QCOMPARE(thumbnail.width(),320);
        QVERIFY(std::abs(player->position()-before)<0.02);
        window.controls()->activity();
        auto* progress=window.controls()->findChild<QSlider*>("progressSlider");
        auto* popup=window.findChild<QLabel*>("thumbnailPreview");QVERIFY(popup);
        auto* generator=window.findChild<ThumbnailPreview*>();QVERIFY(generator);
        QTRY_VERIFY_WITH_TIMEOUT(generator->cachedFrameCount()>0,10000);
        // Sin procesar eventos ni esperar al generador: la imagen debe estar ya disponible.
        const int firstX=progress->width()*9/10;
        QMouseEvent firstHover(QEvent::MouseMove,QPointF(firstX,8),progress->mapToGlobal(QPoint(firstX,8)),Qt::NoButton,Qt::NoButton,Qt::NoModifier);
        QApplication::sendEvent(progress,&firstHover);
        QVERIFY(popup->isVisible());QVERIFY(!popup->pixmap().isNull());
        bool sawFrameDuringMotion=false;
        for(int i=1;i<=18;++i) {
            const int x=progress->width()*(1+(i%8))/10;
            QMouseEvent hover(QEvent::MouseMove,QPointF(x,8),progress->mapToGlobal(QPoint(x,8)),Qt::NoButton,Qt::NoButton,Qt::NoModifier);
            QApplication::sendEvent(progress,&hover);
            QTest::qWait(100);
            sawFrameDuringMotion |= popup->isVisible() && !popup->pixmap().isNull();
        }
        QVERIFY(sawFrameDuringMotion);
        QTRY_VERIFY_WITH_TIMEOUT(popup->isVisible()&&!popup->pixmap().isNull(),10000);
        QTest::qWait(2800); // El autoocultado no debe cancelar la miniatura al detener el ratón.
        QVERIFY(window.controls()->isVisible());
        QVERIFY(popup->isVisible());
        popup->grab().save("artifacts/thumbnail.png");
        QVERIFY(std::abs(player->position()-before)<0.02);
        QEvent leave(QEvent::Leave);QApplication::sendEvent(progress,&leave);QVERIFY(!popup->isVisible());
        bool advanced=false;
        QTimer::singleShot(0,&window,[&]{
            auto* dialog=window.findChild<QDialog*>("advancedSettings");if(!dialog)return;
            QTimer::singleShot(4000,dialog,&QDialog::reject);
            dialog->findChild<QLineEdit*>("bookmarkName")->setText("Mi escena");
            dialog->findChild<QPushButton*>("addBookmark")->click();
            QCOMPARE(dialog->findChild<QListWidget*>("bookmarks")->count(),1);
            QVERIFY(dialog->findChild<QLabel*>("mediaInfo")->text().contains("160"));
            dialog->grab().save("artifacts/advanced-playback.png");
            auto* tabs=dialog->findChild<QTabWidget*>();tabs->setCurrentIndex(1);dialog->grab().save("artifacts/bookmarks.png");
            advanced=true;dialog->accept();
        });window.showAdvanced();QVERIFY(advanced);
        bool customized=false;
        QTimer::singleShot(0,&window,[&]{
            auto* dialog=window.findChild<QDialog*>("shortcutSettings");if(!dialog)return;
            QTimer::singleShot(4000,dialog,&QDialog::reject);
            auto* pause=dialog->findChild<QKeySequenceEdit*>("pause");
            pause->setKeySequence(QKeySequence("S"));dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();
            QVERIFY(dialog->isVisible()); // Duplicado rechazado.
            pause->setKeySequence(QKeySequence("K"));dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();customized=true;
        });window.showShortcuts();QVERIFY(customized);
        QTest::keyClick(&window,Qt::Key_Space);QVERIFY(player->paused());
        QTest::keyClick(&window,Qt::Key_K);QTRY_VERIFY(!player->paused());
        QCOMPARE(errors.size(),0);window.close();
    }
    void captureSettingsFramesAndLibrary()
    {
        QTemporaryDir temp;
        const QString first = temp.filePath("primero.avi"), second = temp.filePath("segundo.avi");
        QVERIFY(writeFile(first, makeVideo()));
        QVERIFY(writeFile(second, makeVideo()));
        const QString settingsPath = temp.filePath("session.ini");
        MainWindow window(temp.filePath("captures"), nullptr, settingsPath);
        window.show();
        window.openFiles({first, second});
        auto* player = window.player();
        QSignalSpy errors(player, &MpvPlayer::errorOccurred);
        QSignalSpy captured(player, &MpvPlayer::screenshotSaved);
        QTRY_VERIFY(player->loaded() && player->seekable() && player->duration() > 9);
        player->setPaused(true);
        QTRY_VERIFY(player->paused());
        player->seekAbsolute(2);
        QTRY_VERIFY(std::abs(player->position() - 2) < 0.05);
        QTest::keyClick(&window, Qt::Key_Period);
        QTRY_VERIFY(std::abs(player->position() - 2.1) < 0.05);
        QVERIFY(player->paused());
        QTest::keyClick(&window, Qt::Key_Comma);
        QTRY_VERIFY(std::abs(player->position() - 2) < 0.05);
        bool configured = false;
        QTimer::singleShot(0, &window, [&] {
            auto* dialog = window.findChild<QDialog*>("captureSettings");
            if (!dialog) return;
            QTimer::singleShot(3000, dialog, &QDialog::reject);
            dialog->findChild<QComboBox*>("captureFormat")->setCurrentIndex(1);
            dialog->findChild<QSpinBox*>("captureQuality")->setValue(82);
            dialog->findChild<QLineEdit*>("captureDirectory")->setText(temp.filePath("new captures"));
            dialog->grab().save("artifacts/capture-settings.png");
            dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();
            configured = true;
        });
        window.showCaptureSettings();
        QVERIFY(configured);
        QSettings settings(settingsPath, QSettings::IniFormat);
        QCOMPARE(settings.value("captures/format").toString(), "jpg");
        QCOMPARE(settings.value("captures/quality").toInt(), 82);
        window.captureFrame();
        QTRY_COMPARE_WITH_TIMEOUT(captured.size(), 1, 10000);
        const QString saved = captured.first().first().toString();
        QVERIFY(saved.endsWith(".jpg"));
        QCOMPARE(QFileInfo(saved).absolutePath(), temp.filePath("new captures"));
        QCOMPARE(QImage(saved).size(), QSize(160,90));
        bool historyShown=false;
        QTimer::singleShot(0,&window,[&]{auto* dialog=window.findChild<QDialog*>("captureHistory");if(!dialog)return;
            QCOMPARE(dialog->findChild<QListWidget*>("captureHistoryList")->count(),1);dialog->grab().save("artifacts/history.png");historyShown=true;dialog->accept();});
        window.showCaptureHistory();QVERIFY(historyShown);
        auto* preview = window.findChild<QDialog*>("capturePreview");
        QVERIFY(preview && preview->isVisible());
        QVERIFY(preview->findChild<QPushButton*>("copyCapture"));
        preview->grab().save("artifacts/capture-preview.png");
        preview->close();
        bool adjusted = false;
        QTimer::singleShot(0, &window, [&] {
            auto* dialog = window.findChild<QDialog*>("subtitleSettings");
            if (!dialog) return;
            QTimer::singleShot(3000, dialog, &QDialog::reject);
            dialog->findChild<QDoubleSpinBox*>("subtitleScale")->setValue(1.4);
            dialog->findChild<QSpinBox*>("subtitlePosition")->setValue(85);
            dialog->findChild<QDoubleSpinBox*>("subtitleDelay")->setValue(0.7);
            dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();
            dialog->grab().save("artifacts/subtitle-settings.png");
            adjusted = true;
            dialog->accept();
        });
        window.showSubtitleSettings();
        QVERIFY(adjusted);
        QTRY_VERIFY(std::abs(player->subtitleDelay() - 0.7) < 0.01);
        window.showLibrary();
        auto* library = window.findChild<QDialog*>("mediaLibrary");
        QVERIFY(library);
        QCOMPARE(library->findChild<QListWidget*>("playlist")->count(), 2);
        QCOMPARE(library->findChild<QListWidget*>("recentFiles")->count(), 1);
        library->grab().save("artifacts/library.png");
        library->close();
        player->seekAbsolute(9.7);
        QTRY_VERIFY(player->position() > 9.5);
        player->setPaused(false);
        QTRY_COMPARE_WITH_TIMEOUT(player->currentFile(), second, 10000);
        QTRY_VERIFY(std::abs(player->subtitleDelay()) < 0.01);
        QCOMPARE(window.playlist().size(), 2);
        QTest::keyClick(&window, Qt::Key_PageUp);
        QTRY_COMPARE(player->currentFile(), first);
        QCOMPARE(errors.size(), 0);
        window.close();
    }
    void playbackControlsAndCaptures()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const QString video = temp.filePath(QString::fromUtf8("vídeo_${literal}.avi"));
        QVERIFY(writeFile(video, makeVideo()));
        QVERIFY(writeFile(temp.filePath(QString::fromUtf8("vídeo_${literal}.ass")), R"([Script Info]
ScriptType: v4.00+
PlayResX: 160
PlayResY: 90
[V4+ Styles]
Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding
Style: Default,Arial,14,&H00FFFFFF,&H000000FF,&H00000000,&H00000000,0,0,0,0,100,100,0,0,1,1,0,2,2,2,4,1
[Events]
Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text
Dialogue: 0,0:00:00.00,0:01:00.00,Default,,0,0,0,,LUMINA SUB TEST
)"));
        MainWindow window(temp.filePath("capturas"), nullptr, temp.filePath("settings.ini"));
        window.show();
        // El host de pruebas puede mantener oculta la ventana nativa. mpv
        // permite validar reproducción/capturas sin forzar activación del escritorio.
        QVERIFY(window.isVisible());
        QDir().mkpath("artifacts");
        window.grab().save("artifacts/welcome.png");
        window.activateWindow();
        auto* player = window.player();
        auto* controls = window.controls();
        QSignalSpy errors(player, &MpvPlayer::errorOccurred);
        QSignalSpy saved(player, &MpvPlayer::screenshotSaved);
        window.openFile(video);
        QTRY_VERIFY_WITH_TIMEOUT(player->loaded(), 10000);
        QTRY_VERIFY_WITH_TIMEOUT(player->position() > 0.1, 5000);
        QTRY_VERIFY(player->duration() >= 9.9);
        QTRY_VERIFY(player->seekable());
        QTest::keyClick(&window, Qt::Key_Space);
        QTRY_VERIFY(player->paused());
        auto* progress = controls->findChild<QSlider*>("progressSlider");
        QVERIFY(progress && progress->isEnabled());
        QTest::mouseClick(progress, Qt::LeftButton, Qt::NoModifier, QPoint(progress->width() / 2, progress->height() / 2));
        QTRY_VERIFY(std::abs(player->position() - 5.0) < 0.4);
        QTest::keyClick(&window, Qt::Key_Left);
        QTRY_VERIFY(player->position() < 0.4);
        QTest::keyClick(&window, Qt::Key_Right);
        QTRY_VERIFY(std::abs(player->position() - 5.0) < 0.4);
        player->setVolume(40);
        QTRY_COMPARE(player->volume(), 40);
        QTest::keyClick(&window, Qt::Key_Up);
        QTRY_COMPARE(player->volume(), 45);
        QTest::keyClick(&window, Qt::Key_Down);
        QTRY_COMPARE(player->volume(), 40);
        const QPointF center = window.videoContainer()->rect().center();
        QWheelEvent wheel(center, window.videoContainer()->mapToGlobal(center.toPoint()), {}, QPoint(0, 120),
                          Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(window.videoContainer(), &wheel);
        QTRY_COMPARE(player->volume(), 45);
        auto* volume = controls->findChild<QSlider*>("volumeSlider");
        QVERIFY(volume);
        volume->setValue(30);
        QTRY_COMPARE(player->volume(), 30);
        const WId nativeId = window.videoContainer()->winId();
        QTest::mouseDClick(window.videoContainer(), Qt::LeftButton);
        QTest::mouseRelease(window.videoContainer(), Qt::LeftButton);
        QTRY_VERIFY(window.isFullScreen());
        QCOMPARE(window.videoContainer()->winId(), nativeId);
        QTest::keyClick(&window, Qt::Key_Escape);
        QTRY_VERIFY(!window.isFullScreen());
        QCOMPARE(window.videoContainer()->winId(), nativeId);
        window.togglePip();
        QVERIFY(window.pip());
        QCOMPARE(window.videoContainer()->winId(), nativeId);
        QVERIFY(GetWindowLongPtr(reinterpret_cast<HWND>(window.winId()), GWL_EXSTYLE) & WS_EX_TOPMOST);
        QTRY_COMPARE(window.videoContainer()->size(), window.size());
        QVERIFY(!window.findChild<QWidget*>("titleBar")->isVisible());
        QVERIFY(!controls->isVisible());
        controls->showMessage("Prueba de mensaje oculto");
        QVERIFY(!controls->isVisible());
        const QPoint pipPosition = window.pos();
        const QPoint start = window.videoContainer()->rect().center();
        const QPoint global = window.videoContainer()->mapToGlobal(start);
        QTest::mousePress(window.videoContainer(),Qt::LeftButton,Qt::NoModifier,start);
        const QPoint delta(60,-45);
        QMouseEvent drag(QEvent::MouseMove,QPointF(start+delta),QPointF(global+delta),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);
        QApplication::sendEvent(window.videoContainer(),&drag);
        QTest::mouseRelease(window.videoContainer(),Qt::LeftButton,Qt::NoModifier,start);
        QCOMPARE(window.pos(),pipPosition+delta);
        const QSize oldSize=window.size();
        const QPoint edge(window.videoContainer()->width()-2,window.videoContainer()->height()-2);
        const QPoint edgeGlobal=window.videoContainer()->mapToGlobal(edge);
        QTest::mousePress(window.videoContainer(),Qt::LeftButton,Qt::NoModifier,edge);
        QMouseEvent resizeDrag(QEvent::MouseMove,QPointF(edge+QPoint(100,35)),QPointF(edgeGlobal+QPoint(100,35)),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);
        QApplication::sendEvent(window.videoContainer(),&resizeDrag);
        QTest::mouseRelease(window.videoContainer(),Qt::LeftButton,Qt::NoModifier,edge);
        QVERIFY(window.width()>oldSize.width());
        QVERIFY(std::abs(static_cast<double>(window.width())/window.height()-160.0/90.0)<0.01);
        QCOMPARE(window.videoContainer()->winId(),nativeId);
        SendMessageW(reinterpret_cast<HWND>(window.winId()),WM_APPCOMMAND,0,MAKELPARAM(0,APPCOMMAND_MEDIA_PLAY_PAUSE));
        QTRY_VERIFY(!player->paused());
        SendMessageW(reinterpret_cast<HWND>(window.winId()),WM_APPCOMMAND,0,MAKELPARAM(0,APPCOMMAND_MEDIA_PLAY_PAUSE));
        QTRY_VERIFY(player->paused());
        QVERIFY(!controls->isVisible());
        QTest::keyClick(&window,Qt::Key_P,Qt::ControlModifier);
        QVERIFY(!window.pip());
        QVERIFY(!(GetWindowLongPtr(reinterpret_cast<HWND>(window.winId()), GWL_EXSTYLE) & WS_EX_TOPMOST));
        QCOMPARE(window.videoContainer()->winId(), nativeId);
        // Comprobar la ruta nativa sin consultar ventanas de otras aplicaciones.
        window.videoContainer()->routeNativeInputToQt();
        bool nativeInputDisabled = false;
        EnumChildWindows(reinterpret_cast<HWND>(nativeId), [](HWND child, LPARAM state) -> BOOL {
            wchar_t name[64]{};
            GetClassNameW(child, name, 64);
            if (wcscmp(name, L"mpv") == 0) *reinterpret_cast<bool*>(state) = !IsWindowEnabled(child);
            return TRUE;
        }, reinterpret_cast<LPARAM>(&nativeInputDisabled));
        QVERIFY(nativeInputDisabled);
        QTest::keyClick(&window, Qt::Key_S);
        QTRY_COMPARE_WITH_TIMEOUT(saved.size(), 1, 10000);
        const QImage clean(saved.at(0).at(0).toString());
        QCOMPARE(clean.size(), QSize(160, 90));
        QTest::keyClick(&window, Qt::Key_S, Qt::ShiftModifier);
        QTRY_COMPARE_WITH_TIMEOUT(saved.size(), 2, 10000);
        const QImage subtitles(saved.at(1).at(0).toString());
        QCOMPARE(subtitles.size(), clean.size());
        QVERIFY(clean != subtitles);
        player->captureClean(MpvPlayer::ImageFormat::Jpeg);
        QTRY_COMPARE_WITH_TIMEOUT(saved.size(), 3, 10000);
        QCOMPARE(QImage(saved.at(2).at(0).toString()).size(), clean.size());
        QCOMPARE(errors.size(), 0);
        QCOMPARE(QApplication::mouseButtons(), Qt::NoButton);
        // La desactivación del escritorio puede suspender el overlay: el temporizador
        // se valida por separado sin depender del foco global de Windows.
        controls->suspend();
        QMouseEvent movement(QEvent::MouseMove, QPointF(71, 63),
            window.videoContainer()->mapToGlobal(QPoint(71, 63)), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(window.videoContainer(), &movement);
        QVERIFY(controls->isVisible());
        QCOMPARE(window.videoContainer()->cursor().shape(), Qt::ArrowCursor);
        QDir().mkpath("artifacts");
        controls->grab().save("artifacts/controls.png");
        QVERIFY(writeFile("artifacts/demo.avi", makeVideo()));
        window.screen()->grabWindow(window.winId()).save("artifacts/player.png");
        const QString audio = temp.filePath("audio.wav");
        QVERIFY(writeFile(audio, makeAudio()));
        QMimeData mime;
        mime.setUrls({QUrl::fromLocalFile(audio)});
        QDragEnterEvent enter(QPoint(40, 40), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(window.videoContainer(), &enter);
        QVERIFY(enter.isAccepted());
        QDropEvent drop(QPointF(40, 40), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(window.videoContainer(), &drop);
        QVERIFY(drop.isAccepted());
        QTRY_COMPARE(player->currentFile(), audio);
        QTRY_VERIFY(!player->paused());
        QTRY_VERIFY(player->position() > 0.2);
        QCOMPARE(errors.size(), 0);
        window.openFile(video);
        QTRY_COMPARE(player->currentFile(), video);
        QTRY_VERIFY(player->loaded());
        QTRY_VERIFY(player->position() >= 4.8);
        player->setPaused(true);
        QTRY_VERIFY(player->paused());
        const auto subtitleSelected = [player] {
            const auto tracks = player->tracks();
            return std::any_of(tracks.begin(), tracks.end(), [](const MpvPlayer::Track& t) { return t.type == "sub" && t.selected; });
        };
        QTRY_VERIFY(subtitleSelected());
        player->selectTrack("sub", -1);
        QTRY_VERIFY(!subtitleSelected());
        player->addSubtitles(QFileInfo(video).path() + "/" + QFileInfo(video).completeBaseName() + ".ass");
        QTRY_VERIFY(subtitleSelected());
        QCOMPARE(errors.size(), 0);
        window.openFile(temp.filePath("missing.mkv"));
        QCOMPARE(errors.size(), 1);
        QVERIFY(QFileInfo::exists(temp.filePath("errors.log")));
        bool diagnosticsShown=false;
        QTimer::singleShot(0,&window,[&]{auto* dialog=window.findChild<QDialog*>("diagnostics");if(!dialog)return;
            QVERIFY(dialog->findChild<QTextEdit*>("diagnosticText")->toPlainText().contains("missing.mkv"));dialog->grab().save("artifacts/diagnostics.png");diagnosticsShown=true;dialog->accept();});
        window.showDiagnostics();QVERIFY(diagnosticsShown);
        window.close();
    }
};
int main(int argc, char** argv)
{
    WindowsComApartment com;
    QApplication app(argc, argv);
    if(app.arguments().size()==4 && app.arguments()[1]=="--forward-instance") {
        SingleInstance instance(app.arguments()[2]);return !instance.primary() && instance.forward({app.arguments()[3]}) ? 0 : 2;
    }
    PlayerTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "PlayerTests.moc"
