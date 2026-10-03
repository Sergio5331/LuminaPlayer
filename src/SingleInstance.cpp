#include "SingleInstance.h"
#include <QLocalSocket>
#include <QStandardPaths>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTimer>
#include <QElapsedTimer>
#include <stdexcept>
#include <QThread>
SingleInstance::SingleInstance(const QString& name,QObject* parent):QObject(parent),name_(name) {
    const QString folder=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(folder);
    lock_=std::make_unique<QLockFile>(folder+"/"+name+".lock");
    if(!lock_->tryLock(0))return;
    QLocalServer::removeServer(name_); // Sólo quien posee el bloqueo limpia una instancia obsoleta.
    server_.setSocketOptions(QLocalServer::UserAccessOption);
    if(!server_.listen(name_))throw std::runtime_error(server_.errorString().toStdString());
    connect(&server_,&QLocalServer::newConnection,this,[this]{
        while(auto* socket=server_.nextPendingConnection()) {
            connect(socket,&QLocalSocket::disconnected,socket,&QObject::deleteLater);
            QTimer::singleShot(5000,socket,[socket]{socket->disconnectFromServer();});
            connect(socket,&QLocalSocket::readyRead,this,[this,socket]{
                QByteArray data=socket->property("buffer").toByteArray()+socket->readAll();
                if(data.size()>1024*1024){socket->disconnectFromServer();return;}
                if(!data.contains('\n')){socket->setProperty("buffer",data);return;}
                const auto document=QJsonDocument::fromJson(data.left(data.indexOf('\n')));
                if(!document.isArray()){socket->disconnectFromServer();return;}
                QStringList paths;for(const auto& value:document.array())if(value.isString())paths.append(value.toString());
                socket->write("OK\n");socket->flush();socket->disconnectFromServer();emit openRequested(paths);
            });
        }
    });
}
bool SingleInstance::forward(const QStringList& paths) {
    QElapsedTimer timer;timer.start();
    while(timer.elapsed()<4000){
        QLocalSocket socket;socket.connectToServer(name_);
        if(!socket.waitForConnected(250)){QThread::msleep(25);continue;}
        const QByteArray data=QJsonDocument(QJsonArray::fromStringList(paths)).toJson(QJsonDocument::Compact)+'\n';
        if(data.size()>1024*1024)return false;
        socket.write(data);
        if(!socket.waitForBytesWritten(1000))return false;
        if(socket.bytesAvailable()==0&&!socket.waitForReadyRead(3000))return false;
        return socket.readAll().startsWith("OK");
    }
    return false;
}
