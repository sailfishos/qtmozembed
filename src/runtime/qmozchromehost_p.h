/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2026 Jolla Mobile Ltd
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef QMOZCHROMEHOST_P_H
#define QMOZCHROMEHOST_P_H

#include <QByteArray>
#include <QObject>
#include <QVariant>

namespace QtMoz {

// Private opt-in for the normal Gecko chrome-window backend. Keeping hosted
// selection separate from the optional initial URL permits a genuine
// zero-tab window, while QObject properties avoid changing installed class
// layout.
static const char ChromeInitialUrlProperty[] =
        "_qmozChromeInitialUrl";
static const char ChromeHostedProperty[] =
        "_qmozChromeHosted";
static const char ChromePrivateProperty[] =
        "_qmozChromePrivate";
static const char ChromeInitializationFailedProperty[] =
        "_qmozChromeInitializationFailed";
static const char ChromeInitializedProperty[] =
        "_qmozChromeInitialized";
static const char ChromeQuickOwnedProperty[] =
        "_qmozChromeQuickOwned";

inline QByteArray chromeInitialUrl(
        const QObject *object, const QByteArray &defaultUrl)
{
    if (!object) {
        return defaultUrl;
    }

    const QVariant value = object->property(ChromeInitialUrlProperty);
    return value.isValid() ? value.toByteArray() : defaultUrl;
}

inline QByteArray chromeInitialUrl(const QObject *object)
{
    return chromeInitialUrl(object, QByteArray());
}

inline bool isChromeHosted(const QObject *object)
{
    return object && object->property(ChromeHostedProperty).toBool();
}

inline void setChromeHosted(QObject *object, bool hosted)
{
    if (object) {
        object->setProperty(ChromeHostedProperty, hosted);
    }
}

inline bool isChromePrivate(const QObject *object)
{
    return object && object->property(ChromePrivateProperty).toBool();
}

inline void setChromePrivate(QObject *object, bool privateBrowsing)
{
    if (object) {
        object->setProperty(ChromePrivateProperty, privateBrowsing);
    }
}

inline void setChromeInitialUrl(QObject *object, const QByteArray &url)
{
    if (object) {
        object->setProperty(ChromeInitialUrlProperty, url);
        if (!url.isEmpty()) {
            setChromeHosted(object, true);
        }
    }
}

inline bool chromeInitializationFailed(const QObject *object)
{
    return object
            && object->property(
                ChromeInitializationFailedProperty).toBool();
}

inline void markChromeInitializationFailed(QObject *object)
{
    if (object) {
        object->setProperty(ChromeInitializationFailedProperty, true);
    }
}

inline bool chromeInitialized(const QObject *object)
{
    return object && object->property(ChromeInitializedProperty).toBool();
}

inline void markChromeInitialized(QObject *object)
{
    if (object) {
        object->setProperty(ChromeInitializedProperty, true);
    }
}

inline void clearChromeInitialized(QObject *object)
{
    if (object) {
        object->setProperty(ChromeInitializedProperty, false);
    }
}

inline bool isChromeQuickOwned(const QObject *object)
{
    return object && object->property(ChromeQuickOwnedProperty).toBool();
}

inline void setChromeQuickOwned(QObject *object)
{
    if (object) {
        object->setProperty(ChromeQuickOwnedProperty, true);
    }
}

inline bool chromeQuickWindowShouldDelete(bool quickOwned,
                                          bool initializationFailed,
                                          bool reserved)
{
    return quickOwned && initializationFailed && !reserved;
}

inline bool windowFrameIsValid(bool chromeHosted, bool composited,
                               bool painted, bool viewInitialized,
                               bool hasCompositor, bool hasRegisteredWindow,
                               bool hasViewWindow)
{
    return composited
            && (chromeHosted || painted)
            && (chromeHosted || viewInitialized)
            && hasCompositor
            && hasRegisteredWindow
            && hasViewWindow;
}

} // namespace QtMoz

#endif // QMOZCHROMEHOST_P_H
