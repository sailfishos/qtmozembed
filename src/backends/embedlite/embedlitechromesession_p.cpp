/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2026 Jolla Mobile Ltd
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "embedlitechromesession_p.h"

#include "embedlitesurface_p.h"

#include <QByteArray>
#include <QEnableSharedFromThis>
#include <QString>

#include <mozilla/embedlite/EmbedLiteChromeSession.h>
#include <mozilla/embedlite/EmbedInputData.h>
#include <mozilla/embedlite/EmbedLiteWindow.h>

using namespace mozilla::embedlite;

namespace {

class EmbedLiteChromeSessionAdapter final
    : public QMozChromeSession
    , public EmbedLiteChromeSessionListener
    , public QEnableSharedFromThis<EmbedLiteChromeSessionAdapter>
{
public:
    explicit EmbedLiteChromeSessionAdapter(
            EmbedLiteChromeSession *session, quint32 uniqueId)
        : mSession(session)
        , mUniqueId(uniqueId)
    {
    }

    ~EmbedLiteChromeSessionAdapter() override
    {
        if (mSession) {
            mSession->SetListener(nullptr);
        }
    }

    quint32 uniqueId() const override
    {
        return mUniqueId;
    }

    void setCallbacks(
            const QMozChromeSessionCallbacks &callbacks) override
    {
        const QSharedPointer<EmbedLiteChromeSessionAdapter> self =
                sharedFromThis();
        if (self.isNull()) {
            return;
        }
        mCallbacks = callbacks;
        if (mSession) {
            mSession->SetListener(this);
        }
    }

    void clearCallbacks() override
    {
        const QSharedPointer<EmbedLiteChromeSessionAdapter> self =
                sharedFromThis();
        if (self.isNull()) {
            return;
        }
        if (mSession) {
            mSession->SetListener(nullptr);
        }
        mCallbacks = QMozChromeSessionCallbacks();
    }

    bool loadURL(const QString &url, bool fromExternal) override
    {
        const QByteArray encoded = url.toUtf8();
        return mSession
                && mSession->LoadURL(encoded.constData(), fromExternal);
    }

    bool goBack() override
    {
        return mSession && mSession->GoBack(false, true);
    }

    bool goForward() override
    {
        return mSession && mSession->GoForward(false, true);
    }

    bool stop() override
    {
        return mSession && mSession->StopLoad();
    }

    bool reload(bool hard) override
    {
        return mSession && mSession->Reload(hard);
    }

    bool setActive(bool active) override
    {
        return mSession && mSession->SetActive(active);
    }

    bool setFocused(bool focused) override
    {
        return mSession && mSession->SetFocused(focused);
    }

    bool receiveInputEvent(const EmbedTouchInput &event) override
    {
        return mSession && mSession->ReceiveInputEvent(event);
    }

    void OnLocationChanged(const char *location, bool canGoBack,
                           bool canGoForward) override
    {
        const QSharedPointer<EmbedLiteChromeSessionAdapter> self =
                sharedFromThis();
        if (self.isNull()) {
            return;
        }
        const auto callback = mCallbacks.locationChanged;
        if (callback) {
            callback(location, canGoBack, canGoForward);
        }
    }

    void OnLoadStarted(const char *location) override
    {
        const QSharedPointer<EmbedLiteChromeSessionAdapter> self =
                sharedFromThis();
        if (self.isNull()) {
            return;
        }
        const auto callback = mCallbacks.loadStarted;
        if (callback) {
            callback(location);
        }
    }

    void OnLoadFinished() override
    {
        const QSharedPointer<EmbedLiteChromeSessionAdapter> self =
                sharedFromThis();
        if (self.isNull()) {
            return;
        }
        const auto callback = mCallbacks.loadFinished;
        if (callback) {
            callback();
        }
    }

    void OnLoadProgress(int32_t progress, int64_t current,
                        int64_t total) override
    {
        const QSharedPointer<EmbedLiteChromeSessionAdapter> self =
                sharedFromThis();
        if (self.isNull()) {
            return;
        }
        const auto callback = mCallbacks.loadProgress;
        if (callback) {
            callback(progress, current, total);
        }
    }

    void OnTitleChanged(const char16_t *title) override
    {
        const QSharedPointer<EmbedLiteChromeSessionAdapter> self =
                sharedFromThis();
        if (self.isNull()) {
            return;
        }
        const auto callback = mCallbacks.titleChanged;
        if (callback) {
            callback(title);
        }
    }

    void ChromeSessionDestroyed() override
    {
        const QSharedPointer<EmbedLiteChromeSessionAdapter> self =
                sharedFromThis();
        if (self.isNull()) {
            return;
        }
        mSession = nullptr;
        const auto callback = mCallbacks.destroyed;
        mCallbacks = QMozChromeSessionCallbacks();
        if (callback) {
            callback();
        }
    }

private:
    EmbedLiteChromeSession *mSession;
    const quint32 mUniqueId;
    QMozChromeSessionCallbacks mCallbacks;
};

} // namespace

namespace QtMoz {

QSharedPointer<QMozChromeSession> createEmbedLiteChromeSession(
        const QSharedPointer<QMozSurface> &surface)
{
    EmbedLiteChromeSession *session = nullptr;
    quint32 uniqueId = 0;
    if (!withEmbedLiteWindow(surface, [&](EmbedLiteWindow *window) {
        session = window->GetChromeSession();
        uniqueId = window->GetUniqueID();
    }) || !session || uniqueId == 0) {
        return QSharedPointer<QMozChromeSession>();
    }

    const QSharedPointer<EmbedLiteChromeSessionAdapter> adapter(
            new EmbedLiteChromeSessionAdapter(session, uniqueId));
    return adapter;
}

} // namespace QtMoz
