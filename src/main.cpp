#include "MainWindow.h"
#include "SingleInstance.h"
#include <QFileInfo>
#include "WindowsComApartment.h"

#include <QApplication>
#include <QMessageBox>
#include <QTimer>
#include <exception>

int main(int argc, char* argv[])
{
    WindowsComApartment com;
    QApplication app(argc, argv);
    QApplication::setApplicationName("LuminaPlayer");
    QApplication::setApplicationVersion("0.9.0");
    QApplication::setOrganizationName("LuminaPlayer");
    QApplication::setStyle("Fusion");
    try {
        QStringList paths;
        for (const auto& argument : app.arguments().mid(1)) paths.append(QFileInfo(argument).absoluteFilePath());
        SingleInstance instance("LuminaPlayer-desktop-v1");
        if (!instance.primary()) {
            if(instance.forward(paths))return 0;
            QMessageBox::warning(nullptr,"LuminaPlayer","La ventana existente no respondió. Inténtalo de nuevo en unos segundos.");
            return 1;
        }
        MainWindow window;
        QObject::connect(&instance,&SingleInstance::openRequested,&window,[&window](const QStringList& files){
            if(window.isMinimized())window.showNormal();window.show();window.raise();window.activateWindow();
            if(!files.isEmpty())window.openFiles(files);
        });
        window.show();
        if (app.arguments().size() > 1) {

            QTimer::singleShot(0, &window, [&window, paths] { window.openFiles(paths); });
        }
        return app.exec();
    } catch (const std::exception& error) {
        QMessageBox::critical(nullptr, "LuminaPlayer", QString::fromUtf8(error.what()));
        return 1;
    }
}
