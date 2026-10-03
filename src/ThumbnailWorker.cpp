#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <mpv/client.h>
#include <memory>
// Proceso efímero: una imagen, sin audio, subtítulos ni superficie de pantalla.
int main(int argc, char** argv) {
    QCoreApplication app(argc,argv);
    const auto args=app.arguments(); if(args.size()!=4 || !QFileInfo(args[1]).isFile())return 2;
    auto deleter=[](mpv_handle* p){if(p)mpv_terminate_destroy(p);};
    std::unique_ptr<mpv_handle,decltype(deleter)> mpv(mpv_create(),deleter); if(!mpv)return 3;
    const auto option=[&](const char* key,const QByteArray& value){return mpv_set_option_string(mpv.get(),key,value.constData())>=0;};
    if(!option("config","no")||!option("load-scripts","no")||!option("terminal","no")||!option("audio","no")
       ||!option("sub","no")||!option("vo","image")||!option("vo-image-format","png")
       ||!option("vo-image-outdir",args[3].toUtf8())||!option("frames","1")||!option("start",args[2].toUtf8())
       ||!option("vf","scale=320:-2")||!option("vd-lavc-threads","2")||!option("hwdec","no"))return 4;
    if(mpv_initialize(mpv.get())<0)return 5;
    const QByteArray file=QFileInfo(args[1]).absoluteFilePath().toUtf8();
    const char* command[]={"loadfile",file.constData(),nullptr};if(mpv_command(mpv.get(),command)<0)return 6;
    for(;;){const auto* event=mpv_wait_event(mpv.get(),1);if(event->event_id==MPV_EVENT_END_FILE){
        const auto* end=static_cast<mpv_event_end_file*>(event->data);return end->error<0?7:0;
    }if(event->event_id==MPV_EVENT_SHUTDOWN)return 8;}
}
