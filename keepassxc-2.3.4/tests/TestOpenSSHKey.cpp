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

#include "TestOpenSSHKey.h"
#include "TestGlobal.h"
#include "sshagent/OpenSSHKey.h"
#include "crypto/Crypto.h"

QTEST_GUILESS_MAIN(TestOpenSSHKey)

void TestOpenSSHKey::initTestCase()
{
    QVERIFY(Crypto::init());
}

void TestOpenSSHKey::testParse()
{
    // mixed line endings and missing ones are intentional, we only require 3 lines total
    const QString keyString = QString(
        "\r\n\r"
        "-----BEGIN OPENSSH PRIVATE KEY-----\n"
        "b3BlbnNzaC1rZXktdjEAAAAABG5vbmUAAAAEbm9uZQAAAAAAAAABAAAAMwAAAAtzc2gtZW"
        "QyNTUxOQAAACDdlO5F2kF2WzedrBAHBi9wBHeISzXZ0IuIqrp0EzeazAAAAKjgCfj94An4"
        "/QAAAAtzc2gtZWQyNTUxOQAAACDdlO5F2kF2WzedrBAHBi9wBHeISzXZ0IuIqrp0EzeazA"
        "AAAEBe1iilZFho8ZGAliiSj5URvFtGrgvmnEKdiLZow5hOR92U7kXaQXZbN52sEAcGL3AE"
        "d4hLNdnQi4iqunQTN5rMAAAAH29wZW5zc2hrZXktdGVzdC1wYXJzZUBrZWVwYXNzeGMBAg"
        "MEBQY=\r"
        "-----END OPENSSH PRIVATE KEY-----\r\n\r"
    );

    const QByteArray keyData = keyString.toLatin1();

    OpenSSHKey key;
    QVERIFY(key.parse(keyData));
    QVERIFY(!key.encrypted());
    QCOMPARE(key.cipherName(), QString("none"));
    QCOMPARE(key.type(), QString("ssh-ed25519"));
    QCOMPARE(key.comment(), QString("opensshkey-test-parse@keepassxc"));
    QCOMPARE(key.fingerprint(), QString("SHA256:D1fVmA15YXzaJ5sdO9dXxo5coHL/pnNaIfCvokHzTA4"));

    QByteArray publicKey, privateKey;
    BinaryStream publicStream(&publicKey), privateStream(&privateKey);

    QVERIFY(key.writePublic(publicStream));
    QVERIFY(key.writePrivate(privateStream));

    QVERIFY(publicKey.length() == 51);
    QVERIFY(privateKey.length() == 154);
}

void TestOpenSSHKey::testParseDSA()
{
    const QString keyString = QString(
        "-----BEGIN DSA PRIVATE KEY-----\n"
        "MIIBuwIBAAKBgQCudjbvSh8JxQOr2laCqZM1t4kNWBETVOXz5vgk9iw6Z5opB9/k\n"
        "g4nFc1PVq7fdAIc8W/5WCAjugKcxPb9PIHfcwY2fimmiPWFK68/eHKLoCuIn2wxB\n"
        "63ig2hAhx5U5aYG9QHkNCaT6VX7rc19nToSeZXlpja4x54/DaQaqOEWYsQIVAOer\n"
        "UQWfccz7KXUu6+x7heGob6I3AoGAVDRFJIlL0DI/4nePIcgwgwbfgs2ojSu21g4w\n"
        "dQoXvqU34XydPgPQ985XIIuiDkaomRw4yYd/Sh4ZapFcrP++iJ1V+WS6kLcWPHMq\n"
        "poYwk8mq6GLbPFLEjr+n6HgX5ln15n3i4WAopNH7mEl0glY9L0rxmcN0XOpqw6Ux\n"
        "ETGEfAwCgYAiOeYwblMkkTIGtVx5NvNsOlfrBYL4GqUP9oQMO5I+xLZLWQIf+7Jp\n"
        "8t6mwxSBz0RHjNVQ11vZowNjq3587aLy57bVwf2lIm9KSvS6z9HoNbHgQimcBorR\n"
        "J9l9RUrj7TnsZgiVw66j2r34nHRHRtggiO+qrMtw7MJc0Q7jiuTmzgIVAMXbk0T9\n"
        "nBfSLWQz/L8RexU2GR4e\n"
        "-----END DSA PRIVATE KEY-----\n"
    );

    const QByteArray keyData = keyString.toLatin1();

    OpenSSHKey key;
    QVERIFY(key.parse(keyData));
    QVERIFY(!key.encrypted());
    QCOMPARE(key.cipherName(), QString("none"));
    QCOMPARE(key.type(), QString("ssh-dss"));
    QCOMPARE(key.comment(), QString(""));
    QCOMPARE(key.fingerprint(), QString("SHA256:tbbNuLN1hja8JNASDTlLOZQsbTlJDzJlz/oAGK3sX18"));
}

void TestOpenSSHKey::testDecryptRSAAES128CBC()
{
    const QString keyString = QString(
        "-----BEGIN RSA PRIVATE KEY-----\n"
        "Proc-Type: 4,ENCRYPTED\n"
        "DEK-Info: AES-128-CBC,804E4D214D1263FF94E3743FE799DBB4\n"
        "\n"
        "lM9TDfOTbiRhaGGDh7Hn+rqw8CCWcYBZYu7smyYLdnWKXKPmbne8CQFZBAS1FJwZ\n"
        "6Mj6n075yFGyzN9/OfeqKiUA4adlbwLbGwB+yyKsC2FlsvRIEr4hup02WWM47vHj\n"
        "DS4TRmNkE7MKFLhpNCyt5OGGM45s+/lwVTw51K0Hm99TBd72IrX4jfY9ZxAVbL3l\n"
        "aTohL8x6oOTe7q318QgJoFi+DjJhDWLGLLJ7fBqD2imz2fmrY4j8Jpw2sDe1rj82\n"
        "gMqqNG3FrfN0S4uYlWYH5pAh+BUcB1UdmTU/rV5wJMK1oUytmZv/J2+X/0k3Y93F\n"
        "aw6JWOy28OizW+TQXvv8gREWsp5PEclqUZhhGQbVbCQCiDOxg+xiXNySdRH1IqjR\n"
        "zQiKgD4SPzkxQekExPaIQT/KutWZdMNYybEqooCx8YyeDoN31z7Wa2rv6OulOn/j\n"
        "wJFvyd2PT/6brHKI4ky8RYroDf4FbVYKfyEW5CSAg2OyL/tY/kSPgy/k0WT7fDwq\n"
        "dPSuYM9yeWNL6kAhDqDOv8+s3xvOVEljktBvQvItQwVLmHszC3E2AcnaxzdblKPu\n"
        "e3+mBT80NXHjERK2ht+/9JYseK1ujNbNAaG8SbKfU3FF0VlyJ0QW6TuIEdpNnymT\n"
        "0fm0cDfKNaoeJIFnBRZhgIOJAic9DM0cTe/vSG69DaUYsaQPp36al7Fbux3GpFHS\n"
        "OtJEySYGro/6zvJ9dDIEfIGZjA3RaMt6+DuyJZXQdT2RNXa9j60xW7dXh0En4n82\n"
        "JUKTxYhDPLS5c8BzpJqoopxpKwElmrJ7Y3xpd6z2vIlD8ftuZrkk6siTMNQ2s7MI\n"
        "Xl332O+0H4k7uSfczHPOOw36TFhNjGQAP0b7O+0/RVG0ttOIoAn7ZkX3nfdbtG5B\n"
        "DWKvDaopvrcC2/scQ5uLUnqnBiGw1XiYpdg5ang7knHNzHZAIekVaYYZigpCAKp+\n"
        "OtoaDeUEzqFhYVmF8ad1fgvC9ZUsuxS4XUHCKl0H6CJcvW9MJPVbveqYoK+j9qKd\n"
        "iMIkQBP1kE2rzGZVGUkZTpM9LVD9nP0nsbr6E8BatFcNgRirsg2BTJglNpXlCmY6\n"
        "ldzJ/ELBbzoXIn+0wTGai0o4eBPx55baef69JfPuZqEB9pLNE+mHstrqIwcfqYu4\n"
        "M+Vzun1QshRMj9a1PVkIHfs1fLeebI4QCHO0vJlc9K4iYPM4rsDNO3YaAgGRuARS\n"
        "f3McGiGFxkv5zxe8i05ZBnn+exE77jpRKxd223jAMe2wu4WiFB7ZVo4Db6b5Oo2T\n"
        "TPh3VuY7TNMEKkcUi+mGLKjroocQ5j8WQYlfnyOaTalUVQDzOTNb67QIIoiszR0U\n"
        "+AXGyxHj0QtotZFoPME+AbS9Zqy3SgSOuIzPBPU5zS4uoKNdD5NPE5YAuafCjsDy\n"
        "MT4DVy+cPOQYUK022S7T2nsA1btmvUvD5LL2Mc8VuKsWOn/7FKZua6OCfipt6oX0\n"
        "1tzYrw0/ALK+CIdVdYIiPPfxGZkr+JSLOOg7u50tpmen9GzxgNTv63miygwUAIDF\n"
        "u0GbQwOueoA453/N75FcXOgrbqTdivyadUbRP+l7YJk/SfIytyJMOigejp+Z1lzF\n"
        "-----END RSA PRIVATE KEY-----\n"
    );

    const QByteArray keyData = keyString.toLatin1();

    OpenSSHKey key;
    QVERIFY(key.parse(keyData));
    QVERIFY(key.encrypted());
    QCOMPARE(key.cipherName(), QString("AES-128-CBC"));
    QVERIFY(!key.openPrivateKey("incorrectpassphrase"));
    QVERIFY(key.openPrivateKey("correctpassphrase"));
    QCOMPARE(key.type(), QString("ssh-rsa"));
    QCOMPARE(key.comment(), QString(""));
    QCOMPARE(key.fingerprint(), QString("SHA256:1Hsebt2WWnmc72FERsUOgvaajIGHkrMONxXylcmk87U"));
}

void TestOpenSSHKey::testParseRSA()
{
    const QString keyString = QString(
        "-----BEGIN RSA PRIVATE KEY-----\n"
        "MIIEpAIBAAKCAQEAsCHtJicDPWnvHSIKbnTZaJkIB9vgE0pmLdK580JUqBuonVbB\n"
        "y1QTy0ZQ7/TtqvLPgwPK88TR46OLO/QGCzo2+XxgJ85uy0xfuyUYRmSuw0drsErN\n"
        "mH8vU91lSBxsGDp9LtBbgHKoR23vMWZ34IxFRc55XphrIH48ijsMaL6bXBwF/3tD\n"
        "9T3lm2MpP1huyVNnIY9+GRRWCy4f9LMj/UGu/n4RtwwfpOZBBRwYkq5QkzA9lPm/\n"
        "VzF3MP1rKTMkvAw+Nfb383mkmc6MRnsa6uh6iDa9aVB7naegM13UJQX/PY1Ks6pO\n"
        "XDpy/MQ7iCh+HmYNq5dRmARyaNl9xIXJNhz1cQIDAQABAoIBAQCnEUc1LUQxeM5K\n"
        "wANNCqE+SgoIClPdeHC7fmrLh1ttqe6ib6ybBUFRS31yXs0hnfefunVEDKlaV8K2\n"
        "N52UAMAsngFHQNRvGh6kEWeZPd9Xc+N98TZbNCjcT+DGKc+Om8wqH5DrodZlCq4c\n"
        "GaoT4HnE4TjWtZTH2XXrWF9I66PKFWf070R44nvyVcvaZi4pC2YmURRPuGF6K1iK\n"
        "dH8zM6HHG1UGu2W6hLNn+K01IulG0Lb8eWNaNYMmtQWaxyp7I2IWkkecUs3nCuiR\n"
        "byFOoomCjdh8r9yZFvwxjGUhgtkALN9GCU0Mwve+s11IB2gevruN+q9/Qejbyfdm\n"
        "IlgLAeTRAoGBANRcVzW9CYeobCf+U9hKJFEOur8XO+J2mTMaELA0EjWpTJFAeIT7\n"
        "KeRpCRG4/vOSklxxRF6vP1EACA4Z+5BlN+FTipHHs+bSEgqkPZiiANDH7Zot5Iqv\n"
        "1q0fRyldNRZNZK7DWp08BPNVWGA/EnEuKJiURxnxBaxNXbUyMCdjxvMvAoGBANRT\n"
        "utbrqS/bAa/DcHKn3V6DRqBl3TDOfvCNjiKC84a67F2uXgzLIdMktr4d1NyCZVJd\n"
        "7/zVgWORLIdg1eAi6rYGoOvNV39wwga7CF+m9sBY0wAaKYCELe6L26r4aQHVCX6n\n"
        "rnIgUv+4o4itmU2iP0r3wlmDC9pDRQP82vfvQPlfAoGASwhleANW/quvq2HdViq8\n"
        "Mje2HBalfhrRfpDTHK8JUBSFjTzuWG42GxJRtgVbb8x2ElujAKGDCaetMO5VSGu7\n"
        "Fs5hw6iAFCpdXY0yhl+XUi2R8kwM2EPQ4lKO3jqkq0ClNmqn9a5jQWcCVt9yMLNS\n"
        "fLbHeI8EpiCf34ngIcrLXNkCgYEAzlcEZuKkC46xB+dNew8pMTUwSKZVm53BfPKD\n"
        "44QRN6imFbBjU9mAaJnwQbfp6dWKs834cGPolyM4++MeVfB42iZ88ksesgmZdUMD\n"
        "szkl6O0pOJs0I+HQZVdjRbadDZvD22MHQ3+oST1dJ3FVXz3Cdo9qPuT8esMO6f4r\n"
        "qfDH2s8CgYAXC/lWWHQ//PGP0pH4oiEXisx1K0X1u0xMGgrChxBRGRiKZUwNMIvJ\n"
        "TqUu7IKizK19cLHF/NBvxHYHFw+m7puNjn6T1RtRCUjRZT7Dx1VHfVosL9ih5DA8\n"
        "tpbZA5KGKcvHtB5DDgT0MHwzBZnb4Q//Rhovzn+HXZPsJTTgHHy3NQ==\n"
        "-----END RSA PRIVATE KEY-----\n"
    );

    const QByteArray keyData = keyString.toLatin1();

    OpenSSHKey key;
    QVERIFY(key.parse(keyData));
    QVERIFY(!key.encrypted());
    QCOMPARE(key.cipherName(), QString("none"));
    QCOMPARE(key.type(), QString("ssh-rsa"));
    QCOMPARE(key.comment(), QString(""));
    QCOMPARE(key.fingerprint(), QString("SHA256:DYdaZciYNxCejr+/8x+OKYxeTU1D5UsuIFUG4PWRFkk"));
}

void TestOpenSSHKey::testParseRSACompare()
{
    const QString oldKeyString = QString(
        "-----BEGIN RSA PRIVATE KEY-----\n"
        "MIIEpAIBAAKCAQEAsCHtJicDPWnvHSIKbnTZaJkIB9vgE0pmLdK580JUqBuonVbB\n"
        "y1QTy0ZQ7/TtqvLPgwPK88TR46OLO/QGCzo2+XxgJ85uy0xfuyUYRmSuw0drsErN\n"
        "mH8vU91lSBxsGDp9LtBbgHKoR23vMWZ34IxFRc55XphrIH48ijsMaL6bXBwF/3tD\n"
        "9T3lm2MpP1huyVNnIY9+GRRWCy4f9LMj/UGu/n4RtwwfpOZBBRwYkq5QkzA9lPm/\n"
        "VzF3MP1rKTMkvAw+Nfb383mkmc6MRnsa6uh6iDa9aVB7naegM13UJQX/PY1Ks6pO\n"
        "XDpy/MQ7iCh+HmYNq5dRmARyaNl9xIXJNhz1cQIDAQABAoIBAQCnEUc1LUQxeM5K\n"
        "wANNCqE+SgoIClPdeHC7fmrLh1ttqe6ib6ybBUFRS31yXs0hnfefunVEDKlaV8K2\n"
        "N52UAMAsngFHQNRvGh6kEWeZPd9Xc+N98TZbNCjcT+DGKc+Om8wqH5DrodZlCq4c\n"
        "GaoT4HnE4TjWtZTH2XXrWF9I66PKFWf070R44nvyVcvaZi4pC2YmURRPuGF6K1iK\n"
        "dH8zM6HHG1UGu2W6hLNn+K01IulG0Lb8eWNaNYMmtQWaxyp7I2IWkkecUs3nCuiR\n"
        "byFOoomCjdh8r9yZFvwxjGUhgtkALN9GCU0Mwve+s11IB2gevruN+q9/Qejbyfdm\n"
        "IlgLAeTRAoGBANRcVzW9CYeobCf+U9hKJFEOur8XO+J2mTMaELA0EjWpTJFAeIT7\n"
        "KeRpCRG4/vOSklxxRF6vP1EACA4Z+5BlN+FTipHHs+bSEgqkPZiiANDH7Zot5Iqv\n"
        "1q0fRyldNRZNZK7DWp08BPNVWGA/EnEuKJiURxnxBaxNXbUyMCdjxvMvAoGBANRT\n"
        "utbrqS/bAa/DcHKn3V6DRqBl3TDOfvCNjiKC84a67F2uXgzLIdMktr4d1NyCZVJd\n"
        "7/zVgWORLIdg1eAi6rYGoOvNV39wwga7CF+m9sBY0wAaKYCELe6L26r4aQHVCX6n\n"
        "rnIgUv+4o4itmU2iP0r3wlmDC9pDRQP82vfvQPlfAoGASwhleANW/quvq2HdViq8\n"
        "Mje2HBalfhrRfpDTHK8JUBSFjTzuWG42GxJRtgVbb8x2ElujAKGDCaetMO5VSGu7\n"
        "Fs5hw6iAFCpdXY0yhl+XUi2R8kwM2EPQ4lKO3jqkq0ClNmqn9a5jQWcCVt9yMLNS\n"
        "fLbHeI8EpiCf34ngIcrLXNkCgYEAzlcEZuKkC46xB+dNew8pMTUwSKZVm53BfPKD\n"
        "44QRN6imFbBjU9mAaJnwQbfp6dWKs834cGPolyM4++MeVfB42iZ88ksesgmZdUMD\n"
        "szkl6O0pOJs0I+HQZVdjRbadDZvD22MHQ3+oST1dJ3FVXz3Cdo9qPuT8esMO6f4r\n"
        "qfDH2s8CgYAXC/lWWHQ//PGP0pH4oiEXisx1K0X1u0xMGgrChxBRGRiKZUwNMIvJ\n"
        "TqUu7IKizK19cLHF/NBvxHYHFw+m7puNjn6T1RtRCUjRZT7Dx1VHfVosL9ih5DA8\n"
        "tpbZA5KGKcvHtB5DDgT0MHwzBZnb4Q//Rhovzn+HXZPsJTTgHHy3NQ==\n"
        "-----END RSA PRIVATE KEY-----\n"
    );

    const QString newKeyString = QString(
        "-----BEGIN OPENSSH PRIVATE KEY-----\n"
        "b3BlbnNzaC1rZXktdjEAAAAABG5vbmUAAAAEbm9uZQAAAAAAAAABAAABFwAAAAdzc2gtcn\n"
        "NhAAAAAwEAAQAAAQEAsCHtJicDPWnvHSIKbnTZaJkIB9vgE0pmLdK580JUqBuonVbBy1QT\n"
        "y0ZQ7/TtqvLPgwPK88TR46OLO/QGCzo2+XxgJ85uy0xfuyUYRmSuw0drsErNmH8vU91lSB\n"
        "xsGDp9LtBbgHKoR23vMWZ34IxFRc55XphrIH48ijsMaL6bXBwF/3tD9T3lm2MpP1huyVNn\n"
        "IY9+GRRWCy4f9LMj/UGu/n4RtwwfpOZBBRwYkq5QkzA9lPm/VzF3MP1rKTMkvAw+Nfb383\n"
        "mkmc6MRnsa6uh6iDa9aVB7naegM13UJQX/PY1Ks6pOXDpy/MQ7iCh+HmYNq5dRmARyaNl9\n"
        "xIXJNhz1cQAAA8DLsKINy7CiDQAAAAdzc2gtcnNhAAABAQCwIe0mJwM9ae8dIgpudNlomQ\n"
        "gH2+ATSmYt0rnzQlSoG6idVsHLVBPLRlDv9O2q8s+DA8rzxNHjo4s79AYLOjb5fGAnzm7L\n"
        "TF+7JRhGZK7DR2uwSs2Yfy9T3WVIHGwYOn0u0FuAcqhHbe8xZnfgjEVFznlemGsgfjyKOw\n"
        "xovptcHAX/e0P1PeWbYyk/WG7JU2chj34ZFFYLLh/0syP9Qa7+fhG3DB+k5kEFHBiSrlCT\n"
        "MD2U+b9XMXcw/WspMyS8DD419vfzeaSZzoxGexrq6HqINr1pUHudp6AzXdQlBf89jUqzqk\n"
        "5cOnL8xDuIKH4eZg2rl1GYBHJo2X3Ehck2HPVxAAAAAwEAAQAAAQEApxFHNS1EMXjOSsAD\n"
        "TQqhPkoKCApT3Xhwu35qy4dbbanuom+smwVBUUt9cl7NIZ33n7p1RAypWlfCtjedlADALJ\n"
        "4BR0DUbxoepBFnmT3fV3PjffE2WzQo3E/gxinPjpvMKh+Q66HWZQquHBmqE+B5xOE41rWU\n"
        "x9l161hfSOujyhVn9O9EeOJ78lXL2mYuKQtmJlEUT7hheitYinR/MzOhxxtVBrtluoSzZ/\n"
        "itNSLpRtC2/HljWjWDJrUFmscqeyNiFpJHnFLN5wrokW8hTqKJgo3YfK/cmRb8MYxlIYLZ\n"
        "ACzfRglNDML3vrNdSAdoHr67jfqvf0Ho28n3ZiJYCwHk0QAAAIAXC/lWWHQ//PGP0pH4oi\n"
        "EXisx1K0X1u0xMGgrChxBRGRiKZUwNMIvJTqUu7IKizK19cLHF/NBvxHYHFw+m7puNjn6T\n"
        "1RtRCUjRZT7Dx1VHfVosL9ih5DA8tpbZA5KGKcvHtB5DDgT0MHwzBZnb4Q//Rhovzn+HXZ\n"
        "PsJTTgHHy3NQAAAIEA1FxXNb0Jh6hsJ/5T2EokUQ66vxc74naZMxoQsDQSNalMkUB4hPsp\n"
        "5GkJEbj+85KSXHFEXq8/UQAIDhn7kGU34VOKkcez5tISCqQ9mKIA0Mftmi3kiq/WrR9HKV\n"
        "01Fk1krsNanTwE81VYYD8ScS4omJRHGfEFrE1dtTIwJ2PG8y8AAACBANRTutbrqS/bAa/D\n"
        "cHKn3V6DRqBl3TDOfvCNjiKC84a67F2uXgzLIdMktr4d1NyCZVJd7/zVgWORLIdg1eAi6r\n"
        "YGoOvNV39wwga7CF+m9sBY0wAaKYCELe6L26r4aQHVCX6nrnIgUv+4o4itmU2iP0r3wlmD\n"
        "C9pDRQP82vfvQPlfAAAABmlkX3JzYQECAwQ=\n"
        "-----END OPENSSH PRIVATE KEY-----\n"
    );

    const QByteArray oldKeyData = oldKeyString.toLatin1();
    const QByteArray newKeyData = newKeyString.toLatin1();

    OpenSSHKey newKey, oldKey;
    QByteArray oldPrivateKey, newPrivateKey;
    BinaryStream oldPrivateStream(&oldPrivateKey), newPrivateStream(&newPrivateKey);

    QVERIFY(oldKey.parse(oldKeyData));
    QVERIFY(newKey.parse(newKeyData));

    // comment is not part of the old format and writePrivate() includes it
    oldKey.setComment("id_rsa");

    QVERIFY(oldKey.writePrivate(oldPrivateStream));
    QVERIFY(newKey.writePrivate(newPrivateStream));

    QCOMPARE(oldKey.type(), newKey.type());
    QCOMPARE(oldKey.fingerprint(), newKey.fingerprint());
    QCOMPARE(oldPrivateKey, newPrivateKey);
}

void TestOpenSSHKey::testDecryptOpenSSHAES256CBC()
{
    const QString keyString = QString(
        "-----BEGIN OPENSSH PRIVATE KEY-----\n"
        "b3BlbnNzaC1rZXktdjEAAAAACmFlczI1Ni1jYmMAAAAGYmNyeXB0AAAAGAAAABD2A0agtd\n"
        "oGtJiI9JvIxYbTAAAAEAAAAAEAAAAzAAAAC3NzaC1lZDI1NTE5AAAAIDPvDXmi0w1rdMoX\n"
        "fOeyZ0Q/v+wqq/tPFgJwxnW5ADtfAAAAsC3UPsf035hrF5SgZ48p55iDFPiyGfZC/C3vQx\n"
        "+THzpQo8DTUmFokdPn8wvDYGQoIcr9q0RzJuKV87eMQf3zzvZfJthtLYBlt330Deivv9AQ\n"
        "MbKdhPZ4SfwRvv0grgT2EVId3GQAPgSVBhXYQTOf2CdmbXV4kieFLTmSsBMy+v6Qn5Rqur\n"
        "PDWBwuLQgamcVDZuhrkUEqIVJZU2zAiRU2oAXsw/XOgFV6+Y5UZmLwWJQZ\n"
        "-----END OPENSSH PRIVATE KEY-----\n"
    );

    const QByteArray keyData = keyString.toLatin1();

    OpenSSHKey key;
    QVERIFY(key.parse(keyData));
    QVERIFY(key.encrypted());
    QCOMPARE(key.cipherName(), QString("aes256-cbc"));
    QVERIFY(!key.openPrivateKey("incorrectpassphrase"));
    QVERIFY(key.openPrivateKey("correctpassphrase"));
    QCOMPARE(key.type(), QString("ssh-ed25519"));
    QCOMPARE(key.comment(), QString("opensshkey-test-aes256cbc@keepassxc"));

    QByteArray publicKey, privateKey;
    BinaryStream publicStream(&publicKey), privateStream(&privateKey);

    QVERIFY(key.writePublic(publicStream));
    QVERIFY(key.writePrivate(privateStream));

    QVERIFY(publicKey.length() == 51);
    QVERIFY(privateKey.length() == 158);
}

void TestOpenSSHKey::testDecryptRSAAES256CBC()
{
    const QString keyString = QString(
        "-----BEGIN RSA PRIVATE KEY-----\n"
        "Proc-Type: 4,ENCRYPTED\n"
        "DEK-Info: AES-256-CBC,D51E3F558B621BD9384627762CBD16AC\n"
        "\n"
        "b6nr/06Gj8/Nw3ZFMePFyZeuBodExvZZtZPSH3t/2ArcxXOkoqUhLmlcY/JrvnBF\n"
        "JHc34wx/6Yng7mqtUMuk2iMemTzOj3JRx8zHUhwPLnjM/tmeOm0wBUb3WB4+rFZW\n"
        "s1PaIgeKywKgFK0UkcSRpMuSaxheWmHrtJkBsHTF7Tg3ogPL8Dc+nhQlbe/ZGaQb\n"
        "vMdSYcBMaXngS5ZiOafXeY8+l+IMMOZwy5vPTFQGqKHIzOxFhShs1hSExnwOXy69\n"
        "wxrA/QftjNEy5ixIeGT7iQfRB04tEVg0DjYphTPmI2ophzFlwJVBjhj2cmmnsMZg\n"
        "a2TdT/78KZsw2cA5ieMcU6d7Yz5p5nu5dyTbZonn6qWficdZRJwZnVb5ikPnJYbZ\n"
        "1YJRHHND+RWtpanxz7WhStscTCLeI9w9j2gqBJSjKDPgJaoMiA+tyEoakNlPYg+9\n"
        "DadJkBGP0g5E9zw0n4niqQ7eCxk7qolmW6Wtn2zL4UyeJKGi9NWFSGW9x/PmAIse\n"
        "E2KVodiJMRNa8/qUZcW58ZG2uRnFTsW4BXdmzOy/Zp53TVGWStBVLDcldSD03ItD\n"
        "JIWQWDgWp5xyVqPl+8mkW7xDY0GRVSJCyRkctQeGTGysy0BcNjgQQtiA3lPC0rY5\n"
        "m2VxrCYU1KuyHsAjs/V8THcW4a1UdPcVBg1QbCh29bMoM6u4MuXVt7rkwxAV9HJa\n"
        "VbwPsKy7V6G60KaAFIiOs0wdOzBZBoPGd9vBQOEzATh2FYJruDo2OfzEnhv25RxE\n"
        "1q+C/Jds9cWqaNY8kNtUG799XIKkjrC6KvnoV6UA4BkGs2DAcO9rnwtl/hToEoBe\n"
        "ZVj72dlTuS6l9rHqKaz2GI0k0SEt/ZoakPHeDRgPNcDvEZWitV8MuD6Mwb47Y88u\n"
        "sjBmS5k4sJOtB4bLg/UShcqYfkv2OTsK90qGQtba9vMk04Xh1FuxB4fHa5VoKrsX\n"
        "Th/LB34xoYugd16NPmLuawhSo70o4bT70GYpxnb4brGfjWiuthRdegAG9ESSX+M6\n"
        "rNKQPnn2GSroIpkoA4k0PaflcE5tpzeIiJdv0h65N3vw6MFnCaWy8sRSy9fMyRim\n"
        "U8QZB2jcp+YjUU/eny3scuh0Vqt6g1tfFbI84pCC5bArBirf63MeMtwDU/IVImax\n"
        "xzKOzl7k8ropA+rhAJ4Z9X35EmUncBXhf8g39w6nFuSlqjE6rMxCrsrehljQ1Iuz\n"
        "bujaJ2PKpf98OejHDKnMDOfBBq0DdeERCYWlCcqWSgrEgHh4vB5dEQAPP5bAkdZj\n"
        "m0Dq+gF99yadioxf3/MUZVTa1dHklBJJkXTYVPeyH07Th5j7bGCcVb9Zd2Ao/Dia\n"
        "MPWf6xViCC6d0njCLQY2R8mOR5OMVsdlFrsKZMQ/lqjS/WSM6URDkuGb0Cq94TQd\n"
        "7DoblcA50FTwYrVXMygWygbjzJxhcoJDHztzwoqLT/ghh+6zRg6R/fY222tHHbhz\n"
        "nePf421NILzfxnuW+GOwRCM5+IHE3OBS/PYDGijjRFHU4ky0rRRDE64m9CeFzeBh\n"
        "CnFvW6Yx3Hrh5tXBP7kRZ6KjyrPP7tI4ciVSJceSBLRzFmoBr10kRMm+VsUh2xZH\n"
        "-----END RSA PRIVATE KEY-----\n"
    );

    const QByteArray keyData = keyString.toLatin1();

    OpenSSHKey key;
    QVERIFY(key.parse(keyData));
    QVERIFY(key.encrypted());
    QCOMPARE(key.cipherName(), QString("AES-256-CBC"));
    QVERIFY(!key.openPrivateKey("incorrectpassphrase"));
    QVERIFY(key.openPrivateKey("correctpassphrase"));
    QCOMPARE(key.type(), QString("ssh-rsa"));
    QCOMPARE(key.comment(), QString(""));
    QCOMPARE(key.fingerprint(), QString("SHA256:1Hsebt2WWnmc72FERsUOgvaajIGHkrMONxXylcmk87U"));
}

void TestOpenSSHKey::testDecryptOpenSSHAES256CTR()
{
    const QString keyString = QString(
        "-----BEGIN OPENSSH PRIVATE KEY-----\n"
        "b3BlbnNzaC1rZXktdjEAAAAACmFlczI1Ni1jdHIAAAAGYmNyeXB0AAAAGAAAABAMhIAypt\n"
        "WP4tZJBmMwq0tTAAAAEAAAAAEAAAAzAAAAC3NzaC1lZDI1NTE5AAAAIErNsS8ROy43XoWC\n"
        "nO9Sn2lEFBJYcDVtRPM1t6WB7W7OAAAAsFKXMOlPILoTmMj2JmcqzjaYAhaCezx18HDp76\n"
        "VrNxaZTd0T28EGFSkzrReeewpJWy/bWlhLoXR5fRyOSSto+iMg/pibIvIJMrD5sqxlxr/e\n"
        "c5lSeSZUzIK8Rv+ou/3EFDcY5jp8hVXqA4qNtoM/3fV52vmwlNje5d1V5Gsr4U8443+i+p\n"
        "swqksozfatkynk51uR/9QFoOJKlsL/Z3LkK1S/apYz/K331iU1f5ozFELf\n"
        "-----END OPENSSH PRIVATE KEY-----\n"
    );

    const QByteArray keyData = keyString.toLatin1();

    OpenSSHKey key;
    QVERIFY(key.parse(keyData));
    QVERIFY(key.encrypted());
    QCOMPARE(key.cipherName(), QString("aes256-ctr"));
    QVERIFY(!key.openPrivateKey("incorrectpassphrase"));
    QVERIFY(key.openPrivateKey("correctpassphrase"));
    QCOMPARE(key.type(), QString("ssh-ed25519"));
    QCOMPARE(key.comment(), QString("opensshkey-test-aes256ctr@keepassxc"));

    QByteArray publicKey, privateKey;
    BinaryStream publicStream(&publicKey), privateStream(&privateKey);

    QVERIFY(key.writePublic(publicStream));
    QVERIFY(key.writePrivate(privateStream));

    QVERIFY(publicKey.length() == 51);
    QVERIFY(privateKey.length() == 158);
}

void TestOpenSSHKey::testDecryptRSAAES256CTR()
{
    const QString keyString = QString(
        "-----BEGIN RSA PRIVATE KEY-----\n"
        "Proc-Type: 4,ENCRYPTED\n"
        "DEK-Info: AES-256-CTR,192421854316290DFA8F469A1E8CB9BB\n"
        "\n"
        "3h7gUWua+jcvhYj1vUusbMdOG9j8SmNWFV5Hfersi8nF4ddsWEQDnMrRuhtIn4tU\n"
        "GcLY+SXguim5XXwF8gG1tmvpvFMhudTfX+0cIAX7eAVmqLy2RTA18DWqDwWokVP0\n"
        "RJPgRJJSorjMtu2F0YGVVjElW7pHIal7luNk3BDgYUMlgSg0BGOWb+9BkXcEnfc8\n"
        "KEwsJw2onoR2eXo6rYnczGaqPhAPu+I+PfUn0J8PFiffWc1KebRntLdRWeNsBS4p\n"
        "oxtqByzMYIu/WPSJJ5iFoNdKaWQPiZJB+juwI1wNLEtpzKkhpc7/6mOy87h+0eGV\n"
        "fF7javrbHv37eE+k2iZXrcLfvRpiBqt5+uhhCaM8TivBeUho5J38ru/wt/dk+OvY\n"
        "tzXboWA4zVnaYmqta1CkXYKOmb5a8TWEwtxmAuE6kCz/n3pPa6gwkwsyGI65IEyX\n"
        "ycJsbwUilAzXTcz5bIruGx38Sa9fndAN9llOQMS/hdyNs5W5dO1XZ5gU+ARPce+j\n"
        "+A2R8oCUv+2ciEu8z3F++U9aTRmTlD3xeIM0IWUFXKt8Y9fSRC5XoPCbZYNxnV6/\n"
        "hn9NPKCb890Faxies3MABOB5IZ0aTPWkx9ntxFhMaXyfkX2YthNO0GzAENPP9Knt\n"
        "DYhQePlKQ7sNi8+wzsHNeDxNuL/+Rib2MN3ankDtHIsqFz/Em+rD0+3ya8bLy3pP\n"
        "eeUiNpezL+uxI5llq/pikzK4sOgvH1r5YEkMxt9I09grkBwxR7DMBo0vgRE2MLiL\n"
        "nlri8TDwArC1+0gE8NspkkClsBOHXuVlGZo5lup2tn5MzERQcLvuFnAby/GnaVXQ\n"
        "Hm76teb1wMdL58FrdZsKR6e80E+F6JpTsz0a3XJqptgAwGsoxqizkUNJG5hRP8bi\n"
        "NBCFQZPeYi/GxgN5O2UkxhgRkKAcrHg+G87nhLk1ipsc214rb6iOspNizP6fGDuv\n"
        "/bsNTpYRgMNxCLh5Nv0HSUqckoNKOcIVe/9nF5/LLFGfhz95agjKTbBygThFK28N\n"
        "bnHq5fO9yKCMrGCRBQ6No1wwexyS4IAq17LcQP3k4w4n+Wt2GjO5HIldGOEyGqCE\n"
        "zeHYrPpGXF/yf3XTm00XghdQtVtRJptdddXVGZN3EN2w7/ghOSIIlsJO9C4IRgU3\n"
        "WkhX7oOpSE4wmXd5Ada+D1U46snW5nWANWko2NmQNVDeJcvuymL6t2ccNYeFWiA+\n"
        "Hlv0avBnqng7ZWPxYacqZI3+vU0rN9usN1pzwY/4NsBa34o3M7u6KvzEkyewbyUT\n"
        "VfXLJ8XRzb2u4NqQv0WiTBIRxvVB1sRPcrwB4HWKHwRFT8T7f1fefteROrKV7aKm\n"
        "Q48pckidDM0ORh1yIET8u24Mreo5eeWXjVJ9MHoM0486VySYxMwk8yp4tnaHx5kd\n"
        "fGlBbbwFOifhzjAk0u3KJRAG85t2GZhfTMo1IHS2kdu4Xs1N00ZmK0hjeGG+DCwy\n"
        "06ZKHOF5BHWU3SpQEjCsPDwfIpOINWGAJJnp6NIVf7FkHwViV50GWWGSZal4NqZy\n"
        "kR19buHiOb7KnGoPCw8MUmIym8v30FePhM4YQ7ASmRtsXlAhQNRX\n"
        "-----END RSA PRIVATE KEY-----\n"
    );

    const QByteArray keyData = keyString.toLatin1();

    OpenSSHKey key;
    QVERIFY(key.parse(keyData));
    QVERIFY(key.encrypted());
    QCOMPARE(key.cipherName(), QString("AES-256-CTR"));
    QVERIFY(!key.openPrivateKey("incorrectpassphrase"));
    QVERIFY(key.openPrivateKey("correctpassphrase"));
    QCOMPARE(key.type(), QString("ssh-rsa"));
    QCOMPARE(key.comment(), QString(""));
    QCOMPARE(key.fingerprint(), QString("SHA256:1Hsebt2WWnmc72FERsUOgvaajIGHkrMONxXylcmk87U"));
}

void TestOpenSSHKey::testDecryptUTF8()
{
    const QString keyString = QString(
	"-----BEGIN OPENSSH PRIVATE KEY-----\n"
	"b3BlbnNzaC1rZXktdjEAAAAACmFlczI1Ni1jdHIAAAAGYmNyeXB0AAAAGAAAABDtSl4OvT\n"
	"H/wHay2dvjOnpIAAAAEAAAAAEAAAAzAAAAC3NzaC1lZDI1NTE5AAAAIIhrBrn6rb+d3GwF\n"
	"ifpJ6gYut95lXvwypiQmu9ZpA8H9AAAAsD85Gpn2mbVEWq3ygx11wBnN5mUQXnMuP48rLv\n"
	"0qwm12IihOkrR925ledwN2Sa5mkkL0XjDz6SsKfIFhFa84hUHQdw5zPR8yVGRWLzkNDmo7\n"
	"WXNpnoE4ebsX2j0TsBNjP80RUcJdjSXidkt3+aZjaCfquO8cBQn4GJJSDSPwFJYlJeSD/h\n"
	"vpb72MEQchOD3NNMORYTJ5sOJ73RayhhmwjTVlrG+zYAw6fXW0YXX3+5LE\n"
	"-----END OPENSSH PRIVATE KEY-----\n"
    );

    const QByteArray keyData = keyString.toLatin1();

    OpenSSHKey key;
    QVERIFY(key.parse(keyData));
    QVERIFY(key.encrypted());
    QCOMPARE(key.cipherName(), QString("aes256-ctr"));
    QVERIFY(!key.openPrivateKey("incorrectpassphrase"));
    QVERIFY(key.openPrivateKey("äåéëþüúíóö"));
    QCOMPARE(key.fingerprint(), QString("SHA256:EfUXwvH4rOoys+AlbznCqjMwzIVW8KuhoWu9uT03FYA"));
    QCOMPARE(key.type(), QString("ssh-ed25519"));
    QCOMPARE(key.comment(), QString("opensshkey-test-utf8@keepassxc"));
}
