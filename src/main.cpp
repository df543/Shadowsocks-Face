#include "global.h"
#include "ui/MainWindow.h"
#include "tools/SingleInstanceDoorbell.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    global::init(&a);

    a.setApplicationName(global::shortName);
    a.setApplicationDisplayName(global::name);
    a.setApplicationVersion(global::version);

#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    a.setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

    QTranslator translator(&a);
    if (translator.load(QLocale::system(), "", "", ":/i18n"))
        a.installTranslator(&translator);

    SingleInstanceDoorbell doorbell(a.applicationName());
    if (!doorbell.setup()) {
        QTextStream(stderr) << QObject::tr("Another instance is running, the program will exit.\n");
        return 2;
    }

    MainWindow w;
    QObject::connect(&doorbell, &SingleInstanceDoorbell::rang, &w, &MainWindow::focus);
    w.checkShow();

    return a.exec();
}
