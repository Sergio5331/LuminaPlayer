#include "MediaFiles.h"
#include <QDirIterator>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QSaveFile>
#include <QTextStream>
#include <QCollator>
#include <QSet>
#include <QUrl>
#include <algorithm>
namespace {
const QStringList extensions={"mp4","mkv","avi","mov","webm","wmv","flv","mpg","mpeg","m4v","ts","mts","m2ts","vob","ogv","3gp","mxf","mp3","flac","wav","m4a","aac","ogg","opus","wma","aiff","ape"};
}
QStringList MediaFiles::expand(const QStringList& inputs,QStringList* warnings) {
    QStringList result;QSet<QString> lists;
    const auto warn=[warnings](const QString& text){if(warnings&&warnings->size()<100)warnings->append(text);};
    const auto visit=[&](auto&& self,const QString& input,int depth)->void {
        if(result.size()>=10000||depth>8){warn("Se alcanzó el límite de tamaño o anidación de listas.");return;}
        QFileInfo info(input);
        if(!info.exists()){warn("No existe: "+input);return;}
        if(info.isDir()) {
            QStringList files;QDirIterator it(info.absoluteFilePath(),QDir::Files|QDir::Readable|QDir::NoSymLinks,QDirIterator::Subdirectories);
            while(it.hasNext()&&files.size()<10000){const auto file=it.next();if(extensions.contains(QFileInfo(file).suffix().toLower()))files.append(file);}
            QCollator collator;collator.setNumericMode(true);collator.setCaseSensitivity(Qt::CaseInsensitive);
            std::sort(files.begin(),files.end(),[&](const QString&a,const QString&b){return collator.compare(a,b)<0;});
            for(const auto& file:files)self(self,file,depth+1);return;
        }
        const QString suffix=info.suffix().toLower();
        if(suffix=="m3u"||suffix=="m3u8") {
            const QString identity=info.canonicalFilePath().toLower();if(lists.contains(identity))return;lists.insert(identity);
            QFile file(info.absoluteFilePath());if(!file.open(QIODevice::ReadOnly)||file.size()>4*1024*1024){warn("No se pudo leer la lista: "+input);return;}
            QTextStream stream(&file);stream.setEncoding(QStringConverter::Utf8);
            while(!stream.atEnd()) {
                QString line=stream.readLine().trimmed();if(line.isEmpty()||line.startsWith('#'))continue;
                const QUrl url(line);if(url.isLocalFile())line=url.toLocalFile();
                else if(line.contains("://")){warn("Se omitió una dirección de red: "+line);continue;}
                self(self,QFileInfo(line).isAbsolute()?line:info.dir().absoluteFilePath(line),depth+1);
            }
        } else if(info.isFile()&&info.isReadable())result.append(info.absoluteFilePath());
    };
    for(const auto& input:inputs)visit(visit,input,0);
    return result;
}
bool MediaFiles::savePlaylist(const QString& path,const QStringList& files,QString* error) {
    QSaveFile output(path);if(!output.open(QIODevice::WriteOnly)){if(error)*error=output.errorString();return false;}
    QByteArray data("#EXTM3U\n");const QDir directory=QFileInfo(path).absoluteDir();
    for(const auto& file:files){QString relative=directory.relativeFilePath(file);
        if(relative.contains('\n')||relative.contains('\r'))continue;
        if(relative.startsWith('#'))relative="./"+relative;
        data+=relative.toUtf8()+'\n';}
    if(output.write(data)!=data.size()||!output.commit()){if(error)*error=output.errorString();return false;}return true;
}
