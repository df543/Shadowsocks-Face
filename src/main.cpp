#include "global.h"
#include "ui/MainWindow.h"
#include "tools/SingleInstanceDoorbell.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    global::init();

    a.setApplicationName(global::shortName);
    a.setApplicationDisplayName(global::name);
    a.setApplicationVersion(global::version);

    QTranslator translator(&a);
    if (translator.load(QLocale::system(), "", "", ":/i18n"))
        a.installTranslator(&translator);

    MainWindow w;
    w.checkShow();

    SingleInstanceDoorbell doorbell(a.applicationName());
    QObject::connect(&doorbell, &SingleInstanceDoorbell::rang, &w, &MainWindow::focus);
    if (!doorbell.setup()) {
        std::cerr << "Found another instance, the program is quiting.\n";
        return 2;
    }

    return a.exec();
}
