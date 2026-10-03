#include "MainWindow.h"
#include "MediaFiles.h"
#include <QTextEdit>
#include <QSaveFile>
#include "MpvPlayer.h"
#include "PlayerControls.h"
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QImage>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QPalette>
#include <QSettings>
#include <QSpinBox>
#include <QTabWidget>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace {
void stylePanel(QWidget* panel)
{
    QPalette palette = panel->palette();
    palette.setColor(QPalette::Window, QColor("#17171f"));
    palette.setColor(QPalette::Base, QColor("#20202b"));
    palette.setColor(QPalette::Button, QColor("#302a40"));
    for (auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
        palette.setColor(role, QColor("#eee9fa"));
    palette.setColor(QPalette::Highlight, QColor("#70518f"));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    panel->setPalette(palette);
    panel->setStyleSheet(R"(
        QDialog { background:#17171f; color:#edeaf5; }
        QLabel, QCheckBox { color:#dcd8e9; background:transparent; }
        QLabel#panelHeading { font-size:22px; font-weight:600; color:#f5f1ff; }
        QPushButton { background:#302a40; color:#eee9fc; border:1px solid #494058;
            border-radius:8px; padding:9px 16px; }
        QPushButton:hover { background:#514065; border-color:#ab8eee; }
        QPushButton:disabled { color:#77717e; }
        QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox, QListWidget {
            background:#20202b; color:#eeeaf8; border:1px solid #40394f; border-radius:7px; padding:8px; }
        QListWidget::item { padding:10px; border-radius:5px; }
        QListWidget::item:selected { background:#514069; }
        QCheckBox::indicator { width:16px; height:16px; border:1px solid #92839f;
            border-radius:4px; background:#272330; }
        QCheckBox::indicator:checked { background:#b094e4; border-color:#d1baff; }
        QSpinBox::up-button, QDoubleSpinBox::up-button,
        QSpinBox::down-button, QDoubleSpinBox::down-button {
            background:#ae99c6; border:1px solid #24212b; width:20px; }
        QTabWidget::pane { border:0; }
        QTabBar::tab { background:#23212e; color:#c7bfdc; padding:10px 22px; }
        QTabBar::tab:selected { background:#514069; color:white; }
    )");
}
QLabel* heading(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setObjectName("panelHeading");
    return label;
}
}

void MainWindow::captureFrame(bool subtitles)
{
    const auto format = settings_->value("captures/format", "png").toString() == "jpg"
        ? MpvPlayer::ImageFormat::Jpeg : MpvPlayer::ImageFormat::Png;
    subtitles ? player_->captureWithSubtitles(format) : player_->captureClean(format);
}

void MainWindow::showCaptureSettings()
{
    QDialog dialog(this);
    dialog.setObjectName("captureSettings");
    dialog.setWindowTitle(tr("Capturas"));
    dialog.setMinimumWidth(520);
    stylePanel(&dialog);
    auto* layout = new QVBoxLayout(&dialog);
    layout->setSpacing(18);
    layout->addWidget(heading(tr("Cada detalle, a tu manera"), &dialog));
    auto* form = new QFormLayout;
    auto* format = new QComboBox(&dialog);
    format->setObjectName("captureFormat");
    format->addItem(tr("PNG · sin pérdida"), "png");
    format->addItem(tr("JPG · archivo más pequeño"), "jpg");
    format->setCurrentIndex(settings_->value("captures/format", "png").toString() == "jpg" ? 1 : 0);
    form->addRow(tr("Formato"), format);
    auto* quality = new QSpinBox(&dialog);
    quality->setObjectName("captureQuality");
    quality->setButtonSymbols(QAbstractSpinBox::NoButtons);
    quality->setRange(1, 100);
    quality->setSuffix(" %");
    quality->setValue(settings_->value("captures/quality", 95).toInt());
    quality->setEnabled(format->currentIndex() == 1);
    connect(format, &QComboBox::currentIndexChanged, &dialog, [quality](int index) { quality->setEnabled(index == 1); });
    form->addRow(tr("Calidad JPG"), quality);
    auto* directory = new QLineEdit(player_->captureDirectory(), &dialog);
    directory->setObjectName("captureDirectory");
    directory->setCursorPosition(0);
    auto* browse = new QPushButton(tr("Elegir…"), &dialog);
    auto* folderRow = new QHBoxLayout;
    folderRow->addWidget(directory, 1);
    folderRow->addWidget(browse);
    connect(browse, &QPushButton::clicked, &dialog, [directory, &dialog] {
        const QString path = QFileDialog::getExistingDirectory(&dialog, tr("Carpeta de capturas"), directory->text());
        if (!path.isEmpty()) directory->setText(path);
    });
    form->addRow(tr("Guardar en"), folderRow);
    layout->addLayout(form);
    auto* preview = new QCheckBox(tr("Mostrar vista previa después de capturar"), &dialog);
    preview->setObjectName("capturePreviewEnabled");
    preview->setChecked(settings_->value("captures/preview", true).toBool());
    layout->addWidget(preview);
    auto* note = new QLabel(tr("S: captura limpia  ·  Shift+S: con subtítulos\nPNG conserva toda la calidad. El texto grabado en la imagen del vídeo permanece."), &dialog);
    note->setWordWrap(true);
    layout->addWidget(note);
    auto* error = new QLabel(&dialog);
    error->setWordWrap(true);
    layout->addWidget(error);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Save)->setText(tr("Guardar"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancelar"));
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&, this] {
        const QString folder = directory->text().trimmed();
        if (folder.isEmpty() || !QDir().mkpath(folder) || !QFileInfo(folder).isWritable()) {
            error->setText(tr("Elige una carpeta en la que puedas guardar archivos.")); return;
        }
        settings_->setValue("captures/format", format->currentData());
        settings_->setValue("captures/quality", quality->value());
        settings_->setValue("captures/directory", QDir(folder).absolutePath());
        settings_->setValue("captures/preview", preview->isChecked());
        player_->configureCaptures(folder, quality->value());
        settings_->sync();
        dialog.accept();
    });
    layout->addWidget(buttons);
    dialog.exec();
    activity();
}

void MainWindow::showCapturePreview(const QString& path)
{
    if (pip_ || !settings_->value("captures/preview", true).toBool()) return;
    if (preview_) delete preview_.data();
    auto* dialog = new QDialog(this, Qt::Tool);
    preview_ = dialog;
    dialog->setObjectName("capturePreview");
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setAttribute(Qt::WA_ShowWithoutActivating);
    dialog->setWindowTitle(tr("Captura guardada"));
    dialog->resize(540, 390);
    stylePanel(dialog);
    auto* layout = new QVBoxLayout(dialog);
    layout->addWidget(heading(tr("Fotograma guardado"), dialog));
    QImageReader reader(path);
    const QSize originalSize = reader.size();
    if (originalSize.isValid()) reader.setScaledSize(originalSize.scaled(720, 400, Qt::KeepAspectRatio));
    const QImage thumbnail = reader.read();
    auto* image = new QLabel(dialog);
    image->setAlignment(Qt::AlignCenter);
    image->setPixmap(QPixmap::fromImage(thumbnail).scaled(500, 250, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    layout->addWidget(image, 1);
    auto* info = new QLabel(tr("%1 × %2 · %3").arg(originalSize.width()).arg(originalSize.height()).arg(QFileInfo(path).fileName()), dialog);
    info->setTextFormat(Qt::PlainText);
    info->setWordWrap(true);
    info->setToolTip(path);
    layout->addWidget(info);
    auto* row = new QHBoxLayout;
    auto* open = new QPushButton(tr("Abrir imagen"), dialog);
    auto* copy = new QPushButton(tr("Copiar"), dialog);
    copy->setObjectName("copyCapture");
    auto* folder = new QPushButton(tr("Abrir carpeta"), dialog);
    row->addWidget(open); row->addWidget(copy); row->addWidget(folder);
    connect(open, &QPushButton::clicked, dialog, [path] { QDesktopServices::openUrl(QUrl::fromLocalFile(path)); });
    connect(folder, &QPushButton::clicked, dialog, [path] { QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath())); });
    connect(copy, &QPushButton::clicked, dialog, [path, info] {
        const QImage original(path);
        if (original.isNull()) { info->setText(tr("No se pudo leer la captura.")); return; }
        QApplication::clipboard()->setImage(original);
        info->setText(tr("Imagen copiada a resolución original."));
    });
    layout->addLayout(row);
    dialog->show();
}

void MainWindow::showSubtitleSettings()
{
    QDialog dialog(this);
    dialog.setObjectName("subtitleSettings");
    dialog.setWindowTitle(tr("Ajustar subtítulos"));
    dialog.setMinimumWidth(480);
    stylePanel(&dialog);
    auto* layout = new QVBoxLayout(&dialog);
    layout->setSpacing(18);
    layout->addWidget(heading(tr("Subtítulos a tu ritmo"), &dialog));
    auto* form = new QFormLayout;
    auto* scale = new QDoubleSpinBox(&dialog);
    scale->setObjectName("subtitleScale");
    scale->setButtonSymbols(QAbstractSpinBox::NoButtons);
    scale->setRange(0.5, 3.0); scale->setSingleStep(0.1); scale->setSuffix(" ×");
    scale->setValue(settings_->value("subtitles/scale", 1.0).toDouble());
    auto* position = new QSpinBox(&dialog);
    position->setObjectName("subtitlePosition");
    position->setButtonSymbols(QAbstractSpinBox::NoButtons);
    position->setRange(0, 100); position->setSuffix(" %");
    position->setValue(settings_->value("subtitles/position", 100).toInt());
    auto* delay = new QDoubleSpinBox(&dialog);
    delay->setObjectName("subtitleDelay");
    delay->setButtonSymbols(QAbstractSpinBox::NoButtons);
    delay->setRange(-600, 600); delay->setDecimals(2); delay->setSingleStep(0.1); delay->setSuffix(" s");
    delay->setValue(player_->subtitleDelay());
    form->addRow(tr("Tamaño"), scale);
    form->addRow(tr("Posición vertical"), position);
    form->addRow(tr("Desfase (+ retrasa)"), delay);
    layout->addLayout(form);
    auto* overrideStyles = new QCheckBox(tr("Aplicar ajustes también a estilos ASS/SSA"), &dialog);
    overrideStyles->setChecked(settings_->value("subtitles/override", false).toBool());
    layout->addWidget(overrideStyles);
    auto* note = new QLabel(tr("0 %: arriba · 100 %: abajo. Los estilos ASS/SSA originales se respetan por defecto. Personalizarlos puede modificar carteles y efectos. El desfase se reinicia al abrir otro archivo."), &dialog);
    note->setWordWrap(true); layout->addWidget(note);
    auto* reset = new QPushButton(tr("Restablecer valores"), &dialog);
    connect(reset, &QPushButton::clicked, &dialog, [=] { scale->setValue(1); position->setValue(100); delay->setValue(0); overrideStyles->setChecked(false); });
    layout->addWidget(reset);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Apply | QDialogButtonBox::Close, &dialog);
    buttons->button(QDialogButtonBox::Apply)->setText(tr("Aplicar"));
    buttons->button(QDialogButtonBox::Close)->setText(tr("Cerrar"));
    connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked, &dialog, [&, this] {
        settings_->setValue("subtitles/scale", scale->value());
        settings_->setValue("subtitles/position", position->value());
        settings_->setValue("subtitles/override", overrideStyles->isChecked());
        player_->configureSubtitles(scale->value(), position->value(), delay->value(), overrideStyles->isChecked());
        settings_->sync();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    dialog.exec();
    activity();
}

void MainWindow::openFiles(const QStringList& paths)
{
    QStringList playable;
    QStringList warnings;
    const QStringList expanded=MediaFiles::expand(paths,&warnings);
    for(const auto& warning:warnings)recordError(warning);
    if(!warnings.isEmpty())controls_->showMessage(warnings.first());
    for (const auto& path : expanded) {
        const QFileInfo file(path);
        if (!file.isFile() || !file.isReadable()) continue;
        if (QStringList{"ass", "ssa", "srt", "vtt", "sub"}.contains(file.suffix().toLower())) {
            player_->addSubtitles(path); continue;
        }
        playable.append(file.absoluteFilePath());
    }
    if (playable.isEmpty()) return;
    playlist_ = playable;
    playlistIndex_ = 0;
    openFile(playlist_.first());
    refreshLibrary();
}
void MainWindow::nextFile(int direction)
{
    const int next = playlistIndex_ + direction;
    if (next < 0 || next >= playlist_.size()) return;
    playlistIndex_ = next;
    openFile(playlist_.at(next));
}
void MainWindow::refreshLibrary()
{
    if (!library_) return;
    queueList_->clear();
    for (int i = 0; i < playlist_.size(); ++i) {
        auto* item = new QListWidgetItem(QString(i == playlistIndex_ ? "▶  " : "") + QFileInfo(playlist_[i]).fileName(), queueList_);
        item->setData(Qt::UserRole, playlist_[i]); item->setToolTip(playlist_[i]);
    }
    queueList_->setCurrentRow(playlistIndex_);
    recentList_->clear();
    for (const auto& path : settings_->value("recentFiles").toStringList()) {
        auto* item = new QListWidgetItem(QFileInfo(path).fileName(), recentList_);
        item->setData(Qt::UserRole, path); item->setToolTip(path);
    }
}
void MainWindow::showLibrary()
{
    if (library_) { refreshLibrary(); library_->show(); library_->raise(); library_->activateWindow(); return; }
    auto* dialog = new QDialog(this, Qt::Tool);
    library_ = dialog;
    dialog->setObjectName("mediaLibrary");
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(tr("Tu biblioteca"));
    dialog->resize(620, 470);
    stylePanel(dialog);
    auto* layout = new QVBoxLayout(dialog);
    layout->addWidget(heading(tr("Sigue disfrutando"), dialog));
    auto* tabs = new QTabWidget(dialog);
    queueList_ = new QListWidget(tabs);
    queueList_->setObjectName("playlist");
    recentList_ = new QListWidget(tabs);
    recentList_->setObjectName("recentFiles");
    tabs->addTab(queueList_, tr("Lista de reproducción"));
    tabs->addTab(recentList_, tr("Recientes"));
    layout->addWidget(tabs, 1);
    auto* row = new QHBoxLayout;
    const auto button = [dialog, row](const QString& text) { auto* b = new QPushButton(text, dialog); row->addWidget(b); return b; };
    connect(button(tr("Añadir")), &QPushButton::clicked, dialog, [this, dialog] {
        const auto paths = QFileDialog::getOpenFileNames(dialog, tr("Añadir a la lista"));
        if (paths.isEmpty()) return;
        QStringList warnings;
        playlist_.append(MediaFiles::expand(paths,&warnings));
        for(const auto& warning:warnings)recordError(warning);
        if(playlist_.isEmpty())return;
        if (playlistIndex_ < 0) { playlistIndex_ = 0; openFile(playlist_.first()); }
        refreshLibrary();
    });
    connect(button(tr("Reproducir")), &QPushButton::clicked, dialog, [this, tabs] {
        auto* list = tabs->currentIndex() == 0 ? queueList_ : recentList_;
        if (auto* item = list->currentItem()) {
            if (tabs->currentIndex() == 0) playlistIndex_ = list->currentRow();
            openFile(item->data(Qt::UserRole).toString());
        }
    });
    connect(button(tr("Quitar")), &QPushButton::clicked, dialog, [this, tabs] {
        if (tabs->currentIndex() == 0) {
            const int rowIndex = queueList_->currentRow();
            if (rowIndex < 0) return;
            playlist_.removeAt(rowIndex);
            if (rowIndex <= playlistIndex_) --playlistIndex_;
        } else if (auto* item = recentList_->currentItem()) {
            QStringList recent = settings_->value("recentFiles").toStringList();
            recent.removeAll(item->data(Qt::UserRole).toString()); settings_->setValue("recentFiles", recent);
        }
        refreshLibrary();
    });
    connect(button(tr("Vaciar")), &QPushButton::clicked, dialog, [this, tabs] {
        if (tabs->currentIndex() == 0) { playlist_.clear(); playlistIndex_ = -1; }
        else settings_->remove("recentFiles");
        refreshLibrary();
    });
    layout->addLayout(row);
    auto* fileRow=new QHBoxLayout;
    auto* folder=new QPushButton(tr("Añadir carpeta"),dialog);
    auto* loadList=new QPushButton(tr("Abrir lista M3U"),dialog);
    auto* saveList=new QPushButton(tr("Guardar lista"),dialog);
    fileRow->addWidget(folder);fileRow->addWidget(loadList);fileRow->addWidget(saveList);layout->addLayout(fileRow);
    connect(folder,&QPushButton::clicked,dialog,[this,dialog]{
        const QString path=QFileDialog::getExistingDirectory(dialog,tr("Añadir carpeta de multimedia"));if(path.isEmpty())return;
        QStringList warnings;playlist_.append(MediaFiles::expand({path},&warnings));for(const auto& warning:warnings)recordError(warning);
        if(playlistIndex_<0&&!playlist_.isEmpty()){playlistIndex_=0;openFile(playlist_.first());}refreshLibrary();
    });
    connect(loadList,&QPushButton::clicked,dialog,[this,dialog]{
        const QString path=QFileDialog::getOpenFileName(dialog,tr("Abrir lista"),{},"Listas (*.m3u *.m3u8)");if(!path.isEmpty())openFiles({path});
    });
    connect(saveList,&QPushButton::clicked,dialog,[this,dialog]{
        QString path=QFileDialog::getSaveFileName(dialog,tr("Guardar lista"),"Mi lista.m3u8","Listas UTF-8 (*.m3u8 *.m3u)");if(path.isEmpty())return;
        if(QFileInfo(path).suffix().isEmpty())path+=".m3u8";
        QString error;if(!MediaFiles::savePlaylist(path,playlist_,&error)){recordError(error);controls_->showMessage(error);}
        else controls_->showMessage(tr("Lista guardada"));
    });
    auto* autoNext = new QCheckBox(tr("Reproducir el siguiente archivo automáticamente"), dialog);
    autoNext->setChecked(settings_->value("playlist/autoNext", true).toBool());
    connect(autoNext, &QCheckBox::toggled, dialog, [this](bool enabled) { settings_->setValue("playlist/autoNext", enabled); });
    layout->addWidget(autoNext);
    connect(queueList_, &QListWidget::itemDoubleClicked, dialog, [this](QListWidgetItem* item) {
        playlistIndex_ = queueList_->row(item); openFile(item->data(Qt::UserRole).toString());
    });
    connect(recentList_, &QListWidget::itemDoubleClicked, dialog, [this](QListWidgetItem* item) { openFile(item->data(Qt::UserRole).toString()); });
    refreshLibrary();
    dialog->show();
}

#include "ShortcutDefinitions.h"
#include <QKeySequenceEdit>
#include <QScrollArea>
#include <QSet>

QString MainWindow::shortcut(const QString& id, const QString& fallback) const {
    return settings_->value("shortcuts/" + id, fallback).toString();
}
void MainWindow::runAction(const QString& id) {
    if (id == "open") chooseFile();
    else if (id == "pause") player_->togglePause();
    else if (id == "left") player_->seekRelative(-5);
    else if (id == "right") player_->seekRelative(5);
    else if (id == "volumeUp") player_->changeVolume(5);
    else if (id == "volumeDown") player_->changeVolume(-5);
    else if (id == "capture") captureFrame();
    else if (id == "captureSub") captureFrame(true);
    else if (id == "frameNext") player_->stepFrame();
    else if (id == "frameBack") player_->stepFrame(true);
    else if (id == "next") nextFile();
    else if (id == "previous") nextFile(-1);
    else if (id == "subEarlier") player_->changeSubtitleDelay(-0.1);
    else if (id == "subLater") player_->changeSubtitleDelay(0.1);
    else if (id == "fullscreen") toggleFullscreen();
    else if (id == "escape") { if (isFullScreen()) toggleFullscreen(); }
    else if (id == "pip") togglePip();
    else if (id == "library") showLibrary();
    else if (id == "captureSettings") showCaptureSettings();
    else if (id == "advanced") showAdvanced();
    else if (id == "bookmark" && player_->loaded()) {
        const QString key = "bookmarks/" + sessionKey(player_->currentFile()).section('/', -1);
        QVariantList marks = settings_->value(key).toList();
        if (marks.size() >= 200) { controls_->showMessage(tr("Máximo de 200 marcadores por archivo.")); return; }
        marks.append(QVariantMap{{"name", tr("Escena %1").arg(marks.size()+1)}, {"time", player_->position()}});
        settings_->setValue(key, marks); settings_->sync();
        controls_->showMessage(tr("Marcador guardado en %1").arg(PlayerControls::formatTime(player_->position())));
    } else if (id == "loop" && player_->loaded() && player_->seekable()) {
        if (player_->property("ab-loop-b") != "no" && !player_->property("ab-loop-b").isEmpty()) {
            player_->clearLoop(); loopA_ = -1; controls_->showMessage(tr("Repetición desactivada"));
        } else if (loopA_ < 0) { loopA_ = player_->position(); controls_->showMessage(tr("Punto A guardado")); }
        else if (player_->position() > loopA_) { player_->setLoop(loopA_, player_->position()); controls_->showMessage(tr("Repetición A–B activada")); }
        else controls_->showMessage(tr("El punto B debe estar después de A."));
    }
}
void MainWindow::showShortcuts() {
    QDialog dialog(this); dialog.setWindowTitle(tr("Atajos de teclado")); dialog.setObjectName("shortcutSettings");
    dialog.resize(570,620); stylePanel(&dialog);
    auto* layout = new QVBoxLayout(&dialog);
    layout->addWidget(heading(tr("Tus teclas, tu forma de ver"), &dialog));
    auto* area = new QScrollArea(&dialog); area->setWidgetResizable(true);
    auto* content = new QWidget(area); auto* form = new QFormLayout(content);
    QHash<QString, QKeySequenceEdit*> editors;
    for (const auto& def : shortcutDefinitions()) {
        auto* edit = new QKeySequenceEdit(QKeySequence::fromString(shortcut(def.id,def.key), QKeySequence::PortableText), content);
        edit->setMaximumSequenceLength(1); edit->setClearButtonEnabled(true); edit->setObjectName(def.id);
        editors.insert(def.id, edit); form->addRow(def.label, edit);
    }
    area->setWidget(content); layout->addWidget(area,1);
    auto* status = new QLabel(tr("Una combinación por acción. Deja el campo vacío para desactivar un atajo."), &dialog);
    status->setWordWrap(true); layout->addWidget(status);
    auto* defaults = new QPushButton(tr("Restablecer teclas"), &dialog);
    connect(defaults,&QPushButton::clicked,&dialog,[editors] {
        for (const auto& def : shortcutDefinitions()) editors[def.id]->setKeySequence(QKeySequence::fromString(def.key,QKeySequence::PortableText));
    }); layout->addWidget(defaults);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel,&dialog);
    buttons->button(QDialogButtonBox::Save)->setText(tr("Guardar")); buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancelar"));
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,[&,this] {
        QSet<QString> used;
        for (auto* edit : editors) {
            const QString key = edit->keySequence().toString(QKeySequence::PortableText);
            if (!key.isEmpty() && used.contains(key)) { status->setText(tr("Hay teclas repetidas. Cada combinación debe pertenecer a una sola acción.")); return; }
            if (!key.isEmpty()) used.insert(key);
        }
        for (auto it=editors.begin();it!=editors.end();++it) settings_->setValue("shortcuts/"+it.key(),it.value()->keySequence().toString(QKeySequence::PortableText));
        settings_->sync(); updateShortcutHints(); dialog.accept();
    }); layout->addWidget(buttons); dialog.exec(); activity();
}
void MainWindow::showAdvanced() {
    QDialog dialog(this); dialog.setWindowTitle(tr("Herramientas de reproducción")); dialog.setObjectName("advancedSettings");
    dialog.resize(660,580); stylePanel(&dialog);
    connect(player_.get(), &MpvPlayer::fileLoaded, &dialog, &QDialog::reject);
    auto* layout = new QVBoxLayout(&dialog);
    layout->addWidget(heading(tr("Controla cada momento"),&dialog));
    auto* tabs = new QTabWidget(&dialog); layout->addWidget(tabs,1);
    auto* playback = new QWidget(tabs); auto* form = new QFormLayout(playback);
    auto* speed = new QDoubleSpinBox(playback); speed->setObjectName("speed"); speed->setButtonSymbols(QAbstractSpinBox::NoButtons); speed->setRange(0.25,3); speed->setSingleStep(0.25); speed->setSuffix(" ×");
    speed->setValue(player_->property("speed").toDouble()); form->addRow(tr("Velocidad (conserva el tono)"),speed);
    connect(speed,qOverload<double>(&QDoubleSpinBox::valueChanged),&dialog,[this](double value) { player_->setSpeed(value); settings_->setValue("playback/speed",value); });
    auto* audio = new QComboBox(playback); audio->setObjectName("audioMode"); audio->addItems({tr("Original"),tr("Normalizado"),tr("Nocturno")});
    audio->setCurrentIndex(settings_->value("playback/audio",1).toInt()); form->addRow(tr("Sonido"),audio);
    connect(audio,&QComboBox::currentIndexChanged,&dialog,[this](int index) { player_->setAudioMode(index); settings_->setValue("playback/audio",index); });
    auto* thumbs = new QCheckBox(tr("Miniaturas al pasar por la barra"),playback); thumbs->setChecked(settings_->value("thumbnails",true).toBool()); form->addRow(thumbs);
    connect(thumbs,&QCheckBox::toggled,&dialog,[this](bool enabled) { settings_->setValue("thumbnails",enabled); });
    auto* a = new QDoubleSpinBox(playback); auto* b = new QDoubleSpinBox(playback);
    for (auto* value : {a,b}) { value->setButtonSymbols(QAbstractSpinBox::NoButtons); value->setRange(0,player_->duration()); value->setDecimals(3); value->setSuffix(" s"); }
    a->setObjectName("loopA"); b->setObjectName("loopB");
    a->setValue(loopA_ >= 0 ? loopA_ : player_->position()); b->setValue(player_->property("ab-loop-b") == "no" ? qMin(player_->duration(),a->value()+5) : player_->property("ab-loop-b").toDouble());
    form->addRow(tr("Inicio A"),a); form->addRow(tr("Final B"),b);
    auto* loopRow = new QHBoxLayout; auto* start = new QPushButton(tr("Repetir A–B"),playback); auto* stop = new QPushButton(tr("Desactivar"),playback);
    start->setObjectName("startLoop"); loopRow->addWidget(start); loopRow->addWidget(stop); form->addRow(loopRow);
    auto* loopStatus = new QLabel(playback); form->addRow(loopStatus);
    connect(start,&QPushButton::clicked,&dialog,[&,this] {
        if (b->value() <= a->value() || !player_->seekable()) { loopStatus->setText(tr("Necesitas un intervalo válido en un archivo que permita desplazarse.")); return; }
        loopA_=a->value(); player_->setLoop(a->value(),b->value()); loopStatus->setText(tr("Repetición activa"));
    });
    connect(stop,&QPushButton::clicked,&dialog,[&,this] { player_->clearLoop(); loopA_=-1; loopStatus->setText(tr("Repetición desactivada")); });
    tabs->addTab(playback,tr("Reproducción"));
    auto* scenes = new QWidget(tabs); auto* sceneLayout = new QVBoxLayout(scenes);
    auto* markers = new QListWidget(scenes); markers->setObjectName("bookmarks"); sceneLayout->addWidget(markers,1);
    const QString markerKey = "bookmarks/" + sessionKey(player_->currentFile()).section('/',-1);
    const auto refresh = [this,markers,markerKey] {
        markers->clear();
        for (const auto& value : settings_->value(markerKey).toList()) {
            const auto map=value.toMap(); auto* item=new QListWidgetItem(PlayerControls::formatTime(map["time"].toDouble())+"  "+map["name"].toString(),markers); item->setData(Qt::UserRole,map["time"]);
        }
    }; refresh();
    auto* name = new QLineEdit(scenes); name->setPlaceholderText(tr("Nombre del momento")); name->setObjectName("bookmarkName"); sceneLayout->addWidget(name);
    auto* row = new QHBoxLayout; auto* add=new QPushButton(tr("Guardar momento actual"),scenes); add->setObjectName("addBookmark"); auto* remove=new QPushButton(tr("Quitar"),scenes); row->addWidget(add);row->addWidget(remove);sceneLayout->addLayout(row);
    add->setEnabled(player_->loaded());
    connect(add,&QPushButton::clicked,&dialog,[&,this] {
        auto values=settings_->value(markerKey).toList(); if(values.size()>=200)return;
        values.append(QVariantMap{{"name",name->text().trimmed().isEmpty()?tr("Escena %1").arg(values.size()+1):name->text().left(120)},{"time",player_->position()}});
        settings_->setValue(markerKey,values);settings_->sync();refresh();
    });
    connect(remove,&QPushButton::clicked,&dialog,[&,this] { int index=markers->currentRow();auto values=settings_->value(markerKey).toList();if(index>=0&&index<values.size()){values.removeAt(index);settings_->setValue(markerKey,values);refresh();} });
    connect(markers,&QListWidget::itemDoubleClicked,&dialog,[this](QListWidgetItem* item){player_->seekAbsolute(item->data(Qt::UserRole).toDouble());});
    sceneLayout->addWidget(new QLabel(tr("Doble clic para volver a una escena."),scenes)); tabs->addTab(scenes,tr("Marcadores"));
    auto* chapters = new QListWidget(tabs); chapters->setObjectName("chapters");
    for(const auto& chapter:player_->chapters()){auto* item=new QListWidgetItem(PlayerControls::formatTime(chapter.second)+"  "+chapter.first,chapters);item->setData(Qt::UserRole,chapter.second);}
    if(chapters->count()==0){auto* item=new QListWidgetItem(tr("Este archivo no contiene capítulos."),chapters);item->setFlags(Qt::NoItemFlags);}
    connect(chapters,&QListWidget::itemDoubleClicked,&dialog,[this](QListWidgetItem* item){if(item->data(Qt::UserRole).isValid())player_->seekAbsolute(item->data(Qt::UserRole).toDouble());});tabs->addTab(chapters,tr("Capítulos"));
    auto* info=new QLabel(tabs); info->setObjectName("mediaInfo"); info->setTextFormat(Qt::PlainText);info->setTextInteractionFlags(Qt::TextSelectableByMouse);info->setWordWrap(true);info->setAlignment(Qt::AlignTop|Qt::AlignLeft);info->setMargin(18);
    const auto updateInfo=[this,info]{
        const auto value=[this](const char* key){const QString text=player_->property(key);return text.isEmpty()?tr("No disponible"):text;};
        info->setText(tr("Vídeo: %1\nResolución: %2 × %3\nFPS del archivo: %4\nDecodificador GPU activo: %5\nAudio: %6\nFotogramas perdidos: %7\nVelocidad: %8 ×")
            .arg(value("video-codec"),value("width"),value("height"),value("container-fps"),value("hwdec-current"),value("audio-codec-name"),value("frame-drop-count"),value("speed")));
    };updateInfo();QTimer timer;timer.setInterval(1000);connect(&timer,&QTimer::timeout,&dialog,updateInfo);timer.start();tabs->addTab(info,tr("Información"));
    auto* keys=new QPushButton(tr("Personalizar atajos…"),&dialog);connect(keys,&QPushButton::clicked,&dialog,[this]{showShortcuts();});layout->addWidget(keys);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Close,&dialog);buttons->button(QDialogButtonBox::Close)->setText(tr("Cerrar"));connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);layout->addWidget(buttons);
    dialog.exec();settings_->sync();activity();
}

void MainWindow::updateShortcutHints() {
    const auto definitions=shortcutDefinitions();
    for (auto* button:findChildren<QPushButton*>()) {
        const QString id=button->property("shortcutId").toString();
        for(const auto& def:definitions) if(def.id==id) {
            const QString key=shortcut(id,def.key);
            button->setToolTip(button->property("baseTooltip").toString()+(key.isEmpty()?QString():" · "+QKeySequence::fromString(key,QKeySequence::PortableText).toString(QKeySequence::NativeText)));
            break;
        }
    }
    if(auto* hints=findChild<QLabel*>("keyboardHints")) hints->setText(tr("%1  Pausa     ·     %2  Captura     ·     %3  Pantalla completa")
        .arg(shortcut("pause","Space"),shortcut("capture","S"),shortcut("fullscreen","F11")));
}

void MainWindow::showCaptureHistory() {
    QDialog dialog(this);dialog.setWindowTitle(tr("Historial de capturas"));dialog.setObjectName("captureHistory");dialog.resize(660,550);stylePanel(&dialog);
    auto* layout=new QVBoxLayout(&dialog);layout->addWidget(heading(tr("Tus fotogramas"),&dialog));
    auto* list=new QListWidget(&dialog);list->setObjectName("captureHistoryList");layout->addWidget(list,1);
    QStringList history=settings_->value("captureHistory").toStringList();
    for(const auto& path:history){auto* item=new QListWidgetItem(QFileInfo(path).fileName()+(QFileInfo::exists(path)?QString():tr(" (no disponible)")),list);item->setData(Qt::UserRole,path);item->setToolTip(path);}
    auto* image=new QLabel(&dialog);image->setAlignment(Qt::AlignCenter);image->setMinimumHeight(150);layout->addWidget(image);
    connect(list,&QListWidget::currentItemChanged,&dialog,[image](QListWidgetItem* item){image->clear();if(!item)return;
        QImageReader reader(item->data(Qt::UserRole).toString());const QSize size=reader.size();if(size.isValid())reader.setScaledSize(size.scaled(500,200,Qt::KeepAspectRatio));
        const QImage preview=reader.read();if(preview.isNull())image->setText(QObject::tr("La imagen ya no está disponible."));else image->setPixmap(QPixmap::fromImage(preview));
    });
    auto* row=new QHBoxLayout;auto* open=new QPushButton(tr("Abrir"),&dialog);auto* folder=new QPushButton(tr("Carpeta"),&dialog);auto* clear=new QPushButton(tr("Vaciar historial"),&dialog);
    row->addWidget(open);row->addWidget(folder);row->addWidget(clear);layout->addLayout(row);
    connect(open,&QPushButton::clicked,&dialog,[list]{if(auto* item=list->currentItem())QDesktopServices::openUrl(QUrl::fromLocalFile(item->data(Qt::UserRole).toString()));});
    connect(folder,&QPushButton::clicked,&dialog,[this,list]{const auto* item=list->currentItem();QDesktopServices::openUrl(QUrl::fromLocalFile(item?QFileInfo(item->data(Qt::UserRole).toString()).absolutePath():player_->captureDirectory()));});
    connect(clear,&QPushButton::clicked,&dialog,[this,list,image]{settings_->remove("captureHistory");list->clear();image->clear();});
    layout->addWidget(new QLabel(tr("Vaciar el historial no borra las imágenes."),&dialog));
    if(list->count())list->setCurrentRow(0);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Close,&dialog);buttons->button(QDialogButtonBox::Close)->setText(tr("Cerrar"));connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);layout->addWidget(buttons);dialog.exec();activity();
}
void MainWindow::showDiagnostics() {
    QDialog dialog(this);dialog.setWindowTitle(tr("Diagnóstico"));dialog.setObjectName("diagnostics");dialog.resize(720,500);stylePanel(&dialog);
    auto* layout=new QVBoxLayout(&dialog);layout->addWidget(heading(tr("Información para resolver problemas"),&dialog));
    auto* text=new QTextEdit(&dialog);text->setObjectName("diagnosticText");text->setReadOnly(true);text->setPlainText(diagnosticText());layout->addWidget(text,1);
    auto* note=new QLabel(tr("El informe incluye rutas de archivos. Se guarda localmente y sólo se comparte si tú lo envías."),&dialog);note->setWordWrap(true);layout->addWidget(note);
    auto* save=new QPushButton(tr("Guardar informe…"),&dialog);layout->addWidget(save);
    connect(save,&QPushButton::clicked,&dialog,[this,text,&dialog]{const QString path=QFileDialog::getSaveFileName(&dialog,tr("Guardar diagnóstico"),"LuminaPlayer-diagnostico.txt","Texto (*.txt)");if(path.isEmpty())return;
        QSaveFile file(path);const QByteArray data=text->toPlainText().toUtf8();if(!file.open(QIODevice::WriteOnly)||file.write(data)!=data.size()||!file.commit()){recordError(file.errorString());text->append(tr("No se pudo guardar: ")+file.errorString());}
    });
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Close,&dialog);buttons->button(QDialogButtonBox::Close)->setText(tr("Cerrar"));connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);layout->addWidget(buttons);dialog.exec();activity();
}
