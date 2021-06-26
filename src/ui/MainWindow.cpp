#include "MainWindow.h"
#include "ui_MainWindow.h"

#include "EditDialog.h"
#include "ShareDialog.h"
#include "global.h"
#include "tools/latencytester.h"

MainWindow::MainWindow(QWidget *parent):
    QMainWindow(parent), ui(new Ui::MainWindow),
    dirPath(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation))
{
    if (!test_client()) {
        QMessageBox::critical(this, tr("Start Error"), tr("Failed to start ss-local process. Please check shadowsocks-libev installation."));
        QApplication::exit(1);
    }

    ui->setupUi(this);
    setWindowTitle(global::name);
    ui->configTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    connect(ui->configTable, &QTableWidget::itemSelectionChanged, this, &MainWindow::checkCurrentRow);

    if (!QDir(dirPath).mkpath("."))
        throw std::runtime_error("couldn't create config dir");
    configManager = new ConfigManager(dirPath, this);
    processManager = new ProcessManager(dirPath, this);
    connect(processManager, &ProcessManager::procChanged, this, &MainWindow::saveAutoConnect);
    connect(processManager, &ProcessManager::procChanged, this, &MainWindow::sync);

    reloadConfig();
    loadAutoConnect();
    sync();
    checkCurrentRow();

    connect(ui->configTable, &QTableWidget::itemDoubleClicked, ui->actionEdit, &QAction::triggered);
    for (auto i : ui->mainToolBar->actions())
        ui->configTable->addAction(i);

    systray.setIcon(QIcon(":/icon/this"));
    auto *systrayMenu = new QMenu(this);
    systrayMenu->addAction(ui->actionShow);
    systrayMenu->addAction(ui->actionQuit);
    systray.setContextMenu(systrayMenu);
    connect(&systray, &QSystemTrayIcon::activated, this, &MainWindow::focus);
    systray.show();
}

MainWindow::~MainWindow()
{ delete ui; }

void MainWindow::focus()
{
    show();
    activateWindow();
}

void MainWindow::testLatency(SSConfig &config)
{
    if (processManager->isRunning(config.id)) {
        auto *tester = new LatencyTester(
            QNetworkProxy(QNetworkProxy::Socks5Proxy, config.local_address, config.local_port),
            QUrl("https://google.com"),
            this
        );
        connect(tester, &LatencyTester::testFinished, [&config, this](int latencyMs) {
            if (processManager->isRunning(config.id)) {
//                config.latencyMs = latencyMs;
                sync();
            }
        });
        tester->start();
    }
}

void MainWindow::sync()
{
    ui->configTable->setRowCount(configData.size());
    for (int i = 0; i < configData.size(); i++) {
        const SSConfig &config = configData[i];
        ui->configTable->setItem(i, 0, new QTableWidgetItem(config.getName()));

        QString latency;
//        if (config.latencyMs == TIMEOUT)
//            latency = tr("timeout");
//        else if (config.latencyMs >= 0)
//            latency = QString("%1 ms").arg(config.latencyMs);
        ui->configTable->setItem(i, 1, new QTableWidgetItem(latency));

        QString local;
        if (config.local_address != "127.0.0.1") local += config.local_address + ":";
        local += QString::number(config.local_port);
        ui->configTable->setItem(i, 2, new QTableWidgetItem(local));
        if (processManager->isRunning(config.id))
            setRow(i, true);
    }
    ui->configTable->clearSelection();
}

void MainWindow::reloadConfig()
{
    // save latency
//    QHash<qint64, int> latency;
//    for (const auto &i : configData)
//        latency[i.id] = i.latencyMs;
    // reload
    configData = configManager->query();
    // restore latency
//    for (auto &i : configData)
//        if (latency.contains(i.id))
//            i.latencyMs = latency[i.id];
    // refresh view
    sync();
}

void MainWindow::setRow(int row, bool bold)
{
    QFont font;
    font.setBold(bold);
    for (int i = 0; i < 3; i++)
        ui->configTable->item(row, i)->setFont(font);
}

void MainWindow::checkCurrentRow()
{
    if (!ui->configTable->selectedRanges().empty()) {
        int row = ui->configTable->currentRow();
        qint64 id = configData[row].id;
        bool connected = processManager->isRunning(id);
        ui->actionConnect->setEnabled(!connected);
        ui->actionDisconnect->setEnabled(connected);
        ui->actionEdit->setEnabled(!connected);
        ui->actionRemove->setEnabled(!connected);
        ui->actionShare->setEnabled(true);
        ui->actionTestLatency->setEnabled(connected);
    } else {
        ui->actionConnect->setEnabled(false);
        ui->actionDisconnect->setEnabled(false);
        ui->actionEdit->setEnabled(false);
        ui->actionRemove->setEnabled(false);
        ui->actionShare->setEnabled(false);
        ui->actionTestLatency->setEnabled(false);
    }
}

void MainWindow::startConfig(SSConfig &config)
{
    auto p = processManager->start(config.id);
    if (p) {
        connect(p, &QProcess::readyReadStandardOutput, [this, config, p] {
            ui->logArea->append(
                "<b>" + config.getName() + "</b><br>"
                "<span style='color:DimGray'>" + QString(p->readAllStandardOutput()).replace("\n", "<br>") + "</span>"
            );
        });
        connect(p, &QProcess::readyReadStandardError, [this, config, p] {
            ui->logArea->append(
                "<b>" + config.getName() + "</b><br>"
                "<span style='color:BlueViolet'>" + QString(p->readAllStandardError()).replace("\n", "<br>") + "</span>"
            );
        });
    } else {
        ui->logArea->append(
            "<b>" + config.getName() + "</b><br />"
            "<span style='color:Red'>Failed to start</span>"
        );
    }

    QTimer::singleShot(100, [&config, this] {
        testLatency(config);
    });
}

void MainWindow::loadAutoConnect()
{
    QFile f{QDir{dirPath}.filePath("last_connected.txt")};
    QSet<qint64> startIds;
    if (f.open(QIODevice::ReadOnly)) {
        QTextStream in(&f);
        for (;;) {
            QString line = in.readLine();
            if (line.isNull()) break;
            else startIds.insert(line.toInt());
        }
        f.close();
    }
    if (!startIds.empty()) hideFirst = true;
    for (auto &i : configData)
        if (startIds.contains(i.id))
            startConfig(i);
}

void MainWindow::saveAutoConnect()
{
    QFile f{QDir{dirPath}.filePath("last_connected.txt")};
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QTextStream out(&f);
        for (const auto &i : configData)
            if (processManager->isRunning(i.id))
                out << i.id << "\n";
        f.close();
    }
}

void MainWindow::closeEvent(QCloseEvent *e)
{
    if (e->spontaneous()) {
        e->ignore();
        hide();
    } else {
        QMainWindow::closeEvent(e);
    }
}

void MainWindow::on_actionConnect_triggered()
{
    int row = ui->configTable->currentRow();
    SSConfig &toConnect = configData[row];

    // check port use
    for (const auto &i : configData)
        if (i.id != toConnect.id && i.local_port == toConnect.local_port && processManager->isRunning(i.id)) {
            if (QMessageBox::question(
                    this,
                    tr("Port repeat"),
                    tr("Port %1 already used by config '%2', kill it?").arg(i.local_port).arg(i.getName()),
                    QMessageBox::StandardButtons(QMessageBox::Yes | QMessageBox::No),
                    QMessageBox::Yes
                ) == QMessageBox::Yes) {
                processManager->terminate(i.id);
                break;
            } else return;
        }

    startConfig(toConnect);
}

void MainWindow::on_actionDisconnect_triggered()
{
    int row = ui->configTable->currentRow();
    qint64 id = configData[row].id;
    processManager->terminate(id);
}

void MainWindow::on_actionEdit_triggered()
{
    int row = ui->configTable->currentRow();
    SSConfig &toEdit = configData[row];
    if (!processManager->isRunning(toEdit.id)) {
        EditDialog editDialog(toEdit, this);
        if (editDialog.exec() == QDialog::Accepted) {
            configManager->edit(toEdit);
            sync();
        }
    }
}

void MainWindow::on_actionShare_triggered()
{
    int row = ui->configTable->currentRow();
    SSConfig &toShare = configData[row];
    auto *shareDialog = new ShareDialog(toShare, this);
    shareDialog->show();
}

void MainWindow::on_actionImport_triggered()
{
    configManager->importGUIConfig(
        QFileDialog::getOpenFileName(
            this,
            QString(),
            QString(),
            "GUI Config(*.json)"
        )
    );
    reloadConfig();
}

void MainWindow::on_actionExport_triggered()
{
    configManager->exportGUIConfig(
        QFileDialog::getSaveFileName(
            this,
            QString(),
            "gui-config.json",
            "GUI Config(*.json)"
        )
    );
}

void MainWindow::on_actionQuit_triggered()
{ QApplication::quit(); }

void MainWindow::on_actionAbout_triggered()
{
    QString content{tr(
                        "<h1>%1</h1>"

                        "<p><i>%2</i> is a <a href='https://github.com/shadowsocks/shadowsocks-libev'>shadowsocks-libev</a> client wrapper using qt5.</p>"

                        "<p>"
                        "Special thanks to <a href='https://github.com/shadowsocks/shadowsocks-qt5'>Shadowsocks-Qt5</a> project;<br>"
                        "Use <a href='https://github.com/nayuki/QR-Code-generator'>nayuki/QR-Code-generator</a> (<a href='https://opensource.org/licenses/MIT'>MIT</a>) to generate QR Code."
                        "</p>"

                        "<hr>"
                        "Version: %3<br>"
                        "License: <a href='https://www.gnu.org/licenses/gpl.html'>GPL-3.0</a><br>"
                        "Project Home: <a href='https://github.com/df543/Shadowsocks-Face'>df543/Shadowsocks-Face</a>"
                    ).arg(
                        qApp->applicationDisplayName(),
                        qApp->applicationName(),
                        qApp->applicationVersion()
                    )};
    QMessageBox::about(this, tr("About"), content);
}

void MainWindow::on_actionAboutQt_triggered()
{ QMessageBox::aboutQt(this); }

void MainWindow::on_actionManually_triggered()
{
    SSConfig toAdd;
    EditDialog editDialog(toAdd, this);
    if (editDialog.exec() == QDialog::Accepted) {
        configManager->add(toAdd);
        configData.append(toAdd);
        sync();
    }
}

void MainWindow::on_actionRemove_triggered()
{
    int row = ui->configTable->currentRow();
    SSConfig &toRemove = configData[row];
    if (QMessageBox::question(
            this,
            tr("Confirm removing"),
            tr("Are you sure to remove config '%1'?").arg(toRemove.getName()),
            QMessageBox::StandardButtons(QMessageBox::Yes | QMessageBox::No),
            QMessageBox::No
        ) == QMessageBox::Yes) {
        configManager->remove(toRemove);
        configData.removeAt(row);
        sync();
    }
}

void MainWindow::on_actionRefresh_triggered()
{ reloadConfig(); }

void MainWindow::on_actionPaste_triggered()
{
    QClipboard *clipboard = QGuiApplication::clipboard();
    QString s = clipboard->text();

    SSConfig toAdd;
    try {
        toAdd = SSConfig::fromURI(s);
    } catch (const std::invalid_argument &) {
        return;
    }

    EditDialog editDialog(toAdd, this);
    if (editDialog.exec() == QDialog::Accepted) {
        configManager->add(toAdd);
        configData.append(toAdd);
        sync();
    }
}

void MainWindow::on_actionShow_triggered()
{ focus(); }

void MainWindow::on_actionTestLatency_triggered()
{
    int row = ui->configTable->currentRow();
    SSConfig &config = configData[row];
    testLatency(config);
}
