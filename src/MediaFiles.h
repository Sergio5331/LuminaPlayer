#pragma once
#include <QStringList>
namespace MediaFiles {
QStringList expand(const QStringList& inputs, QStringList* warnings=nullptr);
bool savePlaylist(const QString& path,const QStringList& files,QString* error=nullptr);
}
