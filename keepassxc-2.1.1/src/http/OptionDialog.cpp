/**
 ***************************************************************************
 * @file OptionDialog.cpp
 *
 * @brief
 *
 * Copyright (C) 2013
 *
 * @author	Francois Ferrand
 * @date	4/2013
 ***************************************************************************
 */

#include "OptionDialog.h"
#include "ui_OptionDialog.h"
#include "HttpSettings.h"

#include <QMessageBox>

OptionDialog::OptionDialog(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::OptionDialog())
{
    ui->setupUi(this);
    connect(ui->removeSharedEncryptionKeys, SIGNAL(clicked()), this, SIGNAL(removeSharedEncryptionKeys()));
    connect(ui->removeStoredPermissions, SIGNAL(clicked()), this, SIGNAL(removeStoredPermissions()));
}

OptionDialog::~OptionDialog()
{
}

void OptionDialog::loadSettings()
{
    HttpSettings settings;
    ui->enableHttpServer->setChecked(settings.isEnabled());

    ui->showNotification->setChecked(settings.showNotification());
    ui->bestMatchOnly->setChecked(settings.bestMatchOnly());
    ui->unlockDatabase->setChecked(settings.unlockDatabase());
    ui->matchUrlScheme->setChecked(settings.matchUrlScheme());
    if (settings.sortByUsername())
        ui->sortByUsername->setChecked(true);
    else
        ui->sortByTitle->setChecked(true);
    ui->httpPort->setText(QString::number(settings.httpPort()));

/*
    ui->checkBoxLower->setChecked(settings.passwordUseLowercase());
    ui->checkBoxNumbers->setChecked(settings.passwordUseNumbers());
    ui->checkBoxUpper->setChecked(settings.passwordUseUppercase());
    ui->checkBoxSpecialChars->setChecked(settings.passwordUseSpecial());
    ui->checkBoxEnsureEvery->setChecked(settings.passwordEveryGroup());
    ui->checkBoxExcludeAlike->setChecked(settings.passwordExcludeAlike());
    ui->spinBoxLength->setValue(settings.passwordLength());
*/

    ui->alwaysAllowAccess->setChecked(settings.alwaysAllowAccess());
    ui->alwaysAllowUpdate->setChecked(settings.alwaysAllowUpdate());
    ui->searchInAllDatabases->setChecked(settings.searchInAllDatabases());
    ui->supportKphFields->setChecked(settings.supportKphFields());
}

void OptionDialog::saveSettings()
{
    HttpSettings settings;
    settings.setEnabled(ui->enableHttpServer->isChecked());

    settings.setShowNotification(ui->showNotification->isChecked());
    settings.setBestMatchOnly(ui->bestMatchOnly->isChecked());
    settings.setUnlockDatabase(ui->unlockDatabase->isChecked());
    settings.setMatchUrlScheme(ui->matchUrlScheme->isChecked());
    settings.setSortByUsername(ui->sortByUsername->isChecked());
    
    int port = ui->httpPort->text().toInt();
    if (port < 1024) {
        QMessageBox::warning(this, tr("Cannot bind to privileged ports"),
            tr("Cannot bind to privileged ports below 1024!\nUsing default port 19455."));
        port = 19455;
    }
    settings.setHttpPort(port);

/*
    settings.setPasswordUseLowercase(ui->checkBoxLower->isChecked());
    settings.setPasswordUseNumbers(ui->checkBoxNumbers->isChecked());
    settings.setPasswordUseUppercase(ui->checkBoxUpper->isChecked());
    settings.setPasswordUseSpecial(ui->checkBoxSpecialChars->isChecked());
    settings.setPasswordEveryGroup(ui->checkBoxEnsureEvery->isChecked());
    settings.setPasswordExcludeAlike(ui->checkBoxExcludeAlike->isChecked());
    settings.setPasswordLength(ui->spinBoxLength->value());
*/

    settings.setAlwaysAllowAccess(ui->alwaysAllowAccess->isChecked());
    settings.setAlwaysAllowUpdate(ui->alwaysAllowUpdate->isChecked());
    settings.setSearchInAllDatabases(ui->searchInAllDatabases->isChecked());
    settings.setSupportKphFields(ui->supportKphFields->isChecked());
}
