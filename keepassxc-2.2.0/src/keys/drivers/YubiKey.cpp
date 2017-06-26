/*
*  Copyright (C) 2014 Kyle Manna <kyle@kylemanna.com>
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

#include <stdio.h>

#include <QDebug>

#include <ykcore.h>
#include <yubikey.h>
#include <ykdef.h>
#include <ykstatus.h>

#include "core/Global.h"
#include "crypto/Random.h"

#include "YubiKey.h"

// Cast the void pointer from the generalized class definition
// to the proper pointer type from the now included system headers
#define m_yk (static_cast<YK_KEY*>(m_yk_void))
#define m_ykds (static_cast<YK_STATUS*>(m_ykds_void))

YubiKey::YubiKey() : m_yk_void(NULL), m_ykds_void(NULL), m_mutex(QMutex::Recursive)
{
}

YubiKey* YubiKey::m_instance(Q_NULLPTR);

YubiKey* YubiKey::instance()
{
    if (!m_instance) {
        m_instance = new YubiKey();
    }

    return m_instance;
}

bool YubiKey::init()
{
    m_mutex.lock();

    // previously initialized
    if (m_yk != NULL && m_ykds != NULL) {

        if (yk_get_status(m_yk, m_ykds)) {
            // Still connected
            m_mutex.unlock();
            return true;
        } else {
            // Initialized but not connected anymore, re-init
            deinit();
        }
    }

    if (!yk_init()) {
        m_mutex.unlock();
        return false;
    }

    // TODO: handle multiple attached hardware devices
    m_yk_void = static_cast<void*>(yk_open_first_key());
    if (m_yk == NULL) {
        m_mutex.unlock();
        return false;
    }

    m_ykds_void = static_cast<void*>(ykds_alloc());
    if (m_ykds == NULL) {
        yk_close_key(m_yk);
        m_yk_void = NULL;
        m_mutex.unlock();
        return false;
    }

    m_mutex.unlock();
    return true;
}

bool YubiKey::deinit()
{
    m_mutex.lock();

    if (m_yk) {
        yk_close_key(m_yk);
        m_yk_void = NULL;
    }

    if (m_ykds) {
        ykds_free(m_ykds);
        m_ykds_void = NULL;
    }

    m_mutex.unlock();

    return true;
}

void YubiKey::detect()
{
    if (init()) {
        for (int i = 1; i < 3; i++) {
            YubiKey::ChallengeResult result;
            QByteArray rand = randomGen()->randomArray(1);
            QByteArray resp;

            result = challenge(i, false, rand, resp);
            if (result == YubiKey::ALREADY_RUNNING) {
                emit alreadyRunning();
                return;
            } else if (result != YubiKey::ERROR) {
                emit detected(i, result == YubiKey::WOULDBLOCK);
                return;
            }
        }
    }
    emit notFound();
}

bool YubiKey::getSerial(unsigned int& serial)
{
    m_mutex.lock();
    int result = yk_get_serial(m_yk, 1, 0, &serial);
    m_mutex.unlock();

    if (!result) {
        return false;
    }

    return true;
}

YubiKey::ChallengeResult YubiKey::challenge(int slot, bool mayBlock, const QByteArray& challenge, QByteArray& response)
{
    if (!m_mutex.tryLock()) {
        return ALREADY_RUNNING;
    }

    int yk_cmd = (slot == 1) ? SLOT_CHAL_HMAC1 : SLOT_CHAL_HMAC2;
    QByteArray paddedChallenge = challenge;

    // ensure that YubiKey::init() succeeded
    if (m_yk == NULL) {
        m_mutex.unlock();
        return ERROR;
    }

    // yk_challenge_response() insists on 64 byte response buffer */
    response.resize(64);

    /* The challenge sent to the yubikey should always be 64 bytes for
     * compatibility with all configurations.  Follow PKCS7 padding.
     *
     * There is some question whether or not 64 byte fixed length
     * configurations even work, some docs say avoid it.
     */
    const int padLen = 64 - paddedChallenge.size();
    if (padLen > 0) {
        paddedChallenge.append(QByteArray(padLen, padLen));
    }

    const unsigned char *c;
    unsigned char *r;
    c = reinterpret_cast<const unsigned char*>(paddedChallenge.constData());
    r = reinterpret_cast<unsigned char*>(response.data());

    int ret = yk_challenge_response(m_yk, yk_cmd, mayBlock, paddedChallenge.size(), c, response.size(), r);
    emit challenged();

    m_mutex.unlock();

    if (!ret) {
        if (yk_errno == YK_EWOULDBLOCK) {
            return WOULDBLOCK;
        } else if (yk_errno == YK_ETIMEOUT) {
            return ERROR;
        } else if (yk_errno) {

            /* Something went wrong, close the key, so that the next call to
             * can try to re-open.
             *
             * Likely caused by the YubiKey being unplugged.
             */

            if (yk_errno == YK_EUSBERR) {
                qWarning() << "USB error:" << yk_usb_strerror();
            } else {
                qWarning() << "YubiKey core error:" << yk_strerror(yk_errno);
            }

            return ERROR;
        }
    }

    // actual HMAC-SHA1 response is only 20 bytes
    response.resize(20);

    return SUCCESS;
}
