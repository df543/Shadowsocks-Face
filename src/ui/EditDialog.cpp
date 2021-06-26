#include "EditDialog.h"
#include "ui_EditDialog.h"

EditDialog::EditDialog(const SSConfig &config, DialogType dialogType, QWidget *parent)
    : QDialog(parent), ui(new Ui::EditDialog), config(config)
{
    ui->setupUi(this);
    if (dialogType == DialogType::NEW_CONFIG)
        setWindowTitle(tr("New"));
    else if (dialogType == DialogType::EDIT_CONFIG)
        setWindowTitle(tr("Edit"));

    ui->lineEdit_name->setText(config.name);
    ui->lineEdit_serverAddress->setText(config.server_address);
    ui->spinBox_serverPort->setValue(config.server_port);
    ui->comboBox_method->setCurrentText(config.method);
    ui->lineEdit_password->setText(config.password);
    ui->lineEdit_localAddress->setText(config.local_address);
    ui->spinBox_localPort->setValue(config.local_port);
    ui->spinBox_timeout->setValue(config.timeout);
    ui->comboBox_mode->setCurrentText(config.mode);
    ui->checkBox_fastopen->setChecked(config.fast_open);
}

EditDialog::~EditDialog()
{ delete ui; }

void EditDialog::on_pushButton_localAddress_localhost_clicked()
{ ui->lineEdit_localAddress->setText("127.0.0.1"); }

void EditDialog::on_pushButton_localAddress_lan_clicked()
{ ui->lineEdit_localAddress->setText("0.0.0.0"); }

void EditDialog::on_buttonBox_accepted()
{
    bool valid = true;

    if (ui->lineEdit_serverAddress->text().size() == 0) {
        ui->lineEdit_serverAddress->setStyleSheet("background-color: pink");
        valid = false;
    } else ui->lineEdit_serverAddress->setStyleSheet("background-color:");

    if (ui->comboBox_method->currentIndex() == -1) {
        ui->comboBox_method->setStyleSheet("background-color: pink");
        valid = false;
    } else ui->comboBox_method->setStyleSheet("background-color:");

    if (ui->comboBox_mode->currentIndex() == -1) {
        ui->comboBox_mode->setStyleSheet("background-color: pink");
        valid = false;
    } else ui->comboBox_mode->setStyleSheet("background-color:");

    if (!valid) return;

    config.name = ui->lineEdit_name->text();
    config.server_address = ui->lineEdit_serverAddress->text();
    config.server_port = ui->spinBox_serverPort->value();
    config.local_port = ui->spinBox_localPort->value();
    config.password = ui->lineEdit_password->text();
    config.timeout = ui->spinBox_timeout->value();
    config.method = ui->comboBox_method->currentText();
    config.mode = ui->comboBox_mode->currentText();
    config.fast_open = ui->checkBox_fastopen->isChecked();
    config.local_address = ui->lineEdit_localAddress->text();

    emit saveConfig(config);
    this->accept();
}
