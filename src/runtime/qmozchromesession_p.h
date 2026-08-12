/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2026 Jolla Mobile Ltd
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef QMOZCHROMESESSION_P_H
#define QMOZCHROMESESSION_P_H

#include <QtGlobal>

#include <functional>

class QMozWindow;
class QString;

struct QMozChromeSessionCallbacks final
{
    std::function<void(const char *, bool, bool)> locationChanged;
    std::function<void(const char *)> loadStarted;
    std::function<void()> loadFinished;
    std::function<void(int, qint64, qint64)> loadProgress;
    std::function<void(const char16_t *)> titleChanged;
    std::function<void()> destroyed;
};

class Q_DECL_HIDDEN QMozChromeSession
{
public:
    virtual ~QMozChromeSession() {}

    virtual void setCallbacks(
            const QMozChromeSessionCallbacks &callbacks) = 0;
    virtual void clearCallbacks() = 0;
    virtual bool loadURL(const QString &url, bool fromExternal) = 0;
    virtual bool goBack() = 0;
    virtual bool goForward() = 0;
    virtual bool stop() = 0;
    virtual bool reload(bool hard) = 0;
    virtual bool setActive(bool active) = 0;
    virtual bool setFocused(bool focused) = 0;
};

namespace QtMoz {

Q_DECL_HIDDEN bool attachChromeSession(
        const void *consumer, QMozWindow *window,
        const QMozChromeSessionCallbacks &callbacks);
Q_DECL_HIDDEN void detachChromeSession(const void *consumer);
Q_DECL_HIDDEN bool chromeSessionLoadURL(
        const void *consumer, const QString &url, bool fromExternal);
Q_DECL_HIDDEN bool chromeSessionGoBack(const void *consumer);
Q_DECL_HIDDEN bool chromeSessionGoForward(const void *consumer);
Q_DECL_HIDDEN bool chromeSessionStop(const void *consumer);
Q_DECL_HIDDEN bool chromeSessionReload(const void *consumer, bool hard);
Q_DECL_HIDDEN bool chromeSessionSetActive(
        const void *consumer, bool active);
Q_DECL_HIDDEN bool chromeSessionSetFocused(
        const void *consumer, bool focused);

} // namespace QtMoz

#endif // QMOZCHROMESESSION_P_H
