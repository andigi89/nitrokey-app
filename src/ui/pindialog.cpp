/*
 * Author: Copyright (C) Andrzej Surowiec 2012
 * Copyright (c) 2012-2018 Nitrokey UG
 *
 * This file is part of Nitrokey App.
 *
 * Nitrokey App is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * Nitrokey App is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Nitrokey App. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0
 */


#include "src/GUI/ManageWindow.h"
#include "mcvs-wrapper.h"
#include "pindialog.h"
#include "nitrokey-applet.h"
#include "libada.h"
#include "src/utils/bool_values.h"

#define LOCAL_PASSWORD_SIZE 40 // Todo make define global

PinDialog::PinDialog(PinType pinType, QWidget *parent):
    QDialog(parent),
    _pinType(pinType)
{
  ui = std::make_shared<Ui::PinDialog>();
  ui->setupUi(this);

  ui->status->setText(tr("Tries left: %1").arg("..."));
  QString title, label;
  switch (pinType){
    case USER_PIN:
      title = tr("Enter user PIN");
      label = tr("User PIN:");
      break;
    case ADMIN_PIN:
      title = tr("Enter admin PIN");
      label = tr("Admin PIN:");
      break;
    case FIRMWARE_PIN:
      title = tr("Enter Firmware Password");
      label = tr("Enter Firmware Password:");
      ui->status->setVisible(false);
      break;
    case OTHER:
        break;
    case HIDDEN_VOLUME:
      title = tr("Enter password for hidden volume");
      label = tr("Enter password for hidden volume:");
      ui->status->setVisible(false);
      break;
  }

  connect(&worker_thread, SIGNAL(started()), &worker, SLOT(fetch_device_data()));
  connect(&worker, SIGNAL(finished()), this, SLOT(updateTryCounter()));
  worker.moveToThread(&worker_thread);
  worker_thread.start();

  connect(ui->okButton, SIGNAL(clicked()), this, SLOT(onOkButtonClicked()));

  // Setup title and label
  this->setWindowTitle(title);
  ui->label->setText(label);
  ui->lineEdit->setAccessibleName(label);
  ui->lineEdit->setMaxLength(UI_PASSWORD_LENGTH_MAXIMUM);

  // ui->status->setVisible(false);

  ui->lineEdit->setFocus();

  this->setModal(true);
  ManageWindow::bringToFocus(this);
}

PinDialog::~PinDialog() {
  worker_thread.quit();
  worker_thread.wait();
}

void PinDialog::getPassword(QString &pin) {
  pin = ui->lineEdit->text();
  clearBuffers();
}

std::string PinDialog::getPassword() {
  std::string pin = ui->lineEdit->text().toStdString();
  clearBuffers();
  return pin;
}

void PinDialog::on_checkBox_toggled(bool checked) {
  ui->lineEdit->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
}


void PinDialog::onOkButtonClicked() {
  int n;

  // Check the password length
  auto passwordString = ui->lineEdit->text().toLatin1();
  n = passwordString.size();
  if (PIN_LENGTH_MAXIMUM <= n)
  {
      csApplet()->warningBox(tr("Your PIN is too long! Use not more than 30 characters.")); //FIXME use %1 instead of constant in translation
    ui->lineEdit->clear();
    return;
  }
  if (PIN_LENGTH_MINIMUM > n) {
      csApplet()->warningBox(tr("Your PIN is too short. Use at least 6 characters.")); //FIXME use %1 instead of constant in translation
    ui->lineEdit->clear();
    return;
  }

  // Check for default pin
  if (passwordString == "123456" || passwordString == "12345678") {
      csApplet()->warningBox(tr("Warning: Default PIN is used.\nPlease change the PIN."));
  }

  done(Accepted);
}

int PinDialog::exec() {
  if (!libada::i()->isDeviceInitialized()) {
        UI_deviceNotInitialized();
        done(Rejected);
        return QDialog::Rejected;
  }
  return QDialog::exec();
}

void PinDialog::updateTryCounter() {
  int triesLeft = 0;

  switch (_pinType) {
  case ADMIN_PIN:
    triesLeft = worker.devdata.retry_admin_count;
    break;
  case USER_PIN:
    triesLeft = worker.devdata.retry_user_count;
    break;
  case HIDDEN_VOLUME:
  case FIRMWARE_PIN:
  case OTHER:
    // Hide tries left field
    ui->status->setVisible(false);
    break;
  }

  // Update 'tries-left' field
  ui->status->setText(tr("Tries left: %1").arg(triesLeft));
}

void PinDialog::UI_deviceNotInitialized() const {
  csApplet()->warningBox(tr("Device is not yet initialized. Please try again later."));
}

void PinDialog::clearBuffers() {
  //FIXME securely delete string in UI
  //FIXME make sure compiler will not ignore this
  ui->lineEdit->clear();
  ui->lineEdit->setText(ui->lineEdit->placeholderText());
}

//TODO get only the one interesting counter
void PinDialogUI::Worker::fetch_device_data() {

  try {
    devdata.retry_admin_count = libada::i()->getAdminPasswordRetryCount();
    devdata.retry_user_count = libada::i()->getUserPasswordRetryCount();
    emit finished();
  }
  catch (...){
    //ignore
  }
}

