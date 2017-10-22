/*
 *  Copyright (C) 2013 Felix Geyer <debfx@fobos.de>
 *  Copyright (C) 2017 KeePassXC Team <team@keepassxc.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 or (at your option)
 *  version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef KEEPASSX_PASSWORDGENERATORWIDGET_H
#define KEEPASSX_PASSWORDGENERATORWIDGET_H

#include <QWidget>
#include <QComboBox>
#include <QLabel>

#include "core/PasswordGenerator.h"
#include "core/PassphraseGenerator.h"

namespace Ui {
    class PasswordGeneratorWidget;
}

class PasswordGenerator;
class PassphraseGenerator;

class PasswordGeneratorWidget : public QWidget
{
    Q_OBJECT

public:
    enum GeneratorTypes
    {
        Password = 0,
        Diceware = 1
    };
    explicit PasswordGeneratorWidget(QWidget* parent = nullptr);
    ~PasswordGeneratorWidget();
    void loadSettings();
    void saveSettings();
    void reset();
    void setStandaloneMode(bool standalone);
public Q_SLOTS:
    void regeneratePassword();
    
signals:
    void appliedPassword(const QString& password);
    void dialogTerminated();

private slots:
    void applyPassword();
    void copyPassword();
    void updateButtonsEnabled(const QString& password);
    void updatePasswordStrength(const QString& password);
    void togglePasswordShown(bool hidden);

    void passwordSliderMoved();
    void passwordSpinBoxChanged();
    void dicewareSliderMoved();
    void dicewareSpinBoxChanged();
    void colorStrengthIndicator(double entropy);

    void updateGenerator();

private:
    bool m_updatingSpinBox;
    bool m_standalone = false;

    PasswordGenerator::CharClasses charClasses();
    PasswordGenerator::GeneratorFlags generatorFlags();

    const QScopedPointer<PasswordGenerator> m_passwordGenerator;
    const QScopedPointer<PassphraseGenerator> m_dicewareGenerator;
    const QScopedPointer<Ui::PasswordGeneratorWidget> m_ui;
};

#endif // KEEPASSX_PASSWORDGENERATORWIDGET_H
