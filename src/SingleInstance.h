#pragma once
#include <QObject>
#include <QLocalServer>
#include <QLockFile>
#include <QStringList>
#include <memory>
class SingleInstance final : public QObject {
    Q_OBJECT
public:
    explicit SingleInstance(const QString& name, QObject* parent=nullptr);
    bool primary() const { return server_.isListening(); }
    bool forward(const QStringList& paths);
signals:
    void openRequested(const QStringList& paths);
private:
    QString name_;
    std::unique_ptr<QLockFile> lock_;
    QLocalServer server_;
};
