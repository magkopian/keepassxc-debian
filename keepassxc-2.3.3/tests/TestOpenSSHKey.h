/*
 *  Copyright (C) 2017 Toni Spets <toni.spets@iki.fi>
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

#ifndef TESTOPENSSHKEY_H
#define TESTOPENSSHKEY_H

#include <QObject>

class OpenSSHKey;

class TestOpenSSHKey : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testParse();
    void testParseDSA();
    void testParseRSA();
    void testDecryptRSAAES128CBC();
    void testDecryptOpenSSHAES256CBC();
    void testDecryptRSAAES256CBC();
    void testDecryptOpenSSHAES256CTR();
    void testDecryptRSAAES256CTR();
};

#endif // TESTOPENSSHKEY_H
