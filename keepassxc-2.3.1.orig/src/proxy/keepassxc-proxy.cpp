/*
*  Copyright (C) 2017 Sami Vänttinen <sami.vanttinen@protonmail.com>
*  Copyright (C) 2017 KeePassXC Team <team@keepassxc.org>
*
*  This program is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation, either version 3 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <QCoreApplication>
#include <iostream>
#include "NativeMessagingHost.h"

#ifndef Q_OS_WIN
#include <initializer_list>
#include <signal.h>
#include <unistd.h>

// (C) Gist: https://gist.github.com/azadkuh/a2ac6869661ebd3f8588
void ignoreUnixSignals(std::initializer_list<int> ignoreSignals) {
    for (int sig : ignoreSignals) {
        signal(sig, SIG_IGN);
    }
}

void catchUnixSignals(std::initializer_list<int> quitSignals) {
    auto handler = [](int sig) -> void {
        std::cerr << sig;
        QCoreApplication::quit();
    };

    sigset_t blocking_mask;
    sigemptyset(&blocking_mask);
    for (auto sig : quitSignals) {
        sigaddset(&blocking_mask, sig);
    }

    struct sigaction sa;
    sa.sa_handler = handler;
    sa.sa_mask    = blocking_mask;
    sa.sa_flags   = 0;

    for (auto sig : quitSignals) {
        sigaction(sig, &sa, nullptr);
    }
}
#endif

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
#if defined(Q_OS_UNIX) || defined(Q_OS_LINUX)
    catchUnixSignals({SIGQUIT, SIGINT, SIGTERM, SIGHUP});
#endif
    NativeMessagingHost host;
    return a.exec();
}
