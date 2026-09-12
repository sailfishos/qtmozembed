/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2026 Jolla Mobile Ltd
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "qmozsurface_p.h"

#include <QHash>
#include <QMutex>

namespace {

class MutexLocker final
{
public:
    explicit MutexLocker(QMutex *mutex)
        : mMutex(mutex)
    {
        mMutex->lock();
    }

    ~MutexLocker()
    {
        mMutex->unlock();
    }

private:
    QMutex *mMutex;

    Q_DISABLE_COPY(MutexLocker)
};

struct SurfaceRegistry
{
    QMutex mutex;
    QHash<QMozWindow *, QSharedPointer<QMozSurface>> surfaces;
};

Q_GLOBAL_STATIC(SurfaceRegistry, surfaceRegistry)

} // namespace

QMozSurface::~QMozSurface()
{
}

namespace QtMoz {

bool installWindowSurface(QMozWindow *window,
                          const QSharedPointer<QMozSurface> &surface)
{
    SurfaceRegistry * const registry = surfaceRegistry();
    if (!registry || !window || surface.isNull()) {
        return false;
    }

    MutexLocker lock(&registry->mutex);
    if (registry->surfaces.contains(window)) {
        return false;
    }
    registry->surfaces.insert(window, surface);
    return true;
}

QSharedPointer<QMozSurface> windowSurface(QMozWindow *window)
{
    SurfaceRegistry * const registry = surfaceRegistry();
    if (!registry || !window) {
        return QSharedPointer<QMozSurface>();
    }

    // Return a strong snapshot so callers never hold the registry lock while
    // invoking backend code or user callbacks.
    MutexLocker lock(&registry->mutex);
    return registry->surfaces.value(window);
}

QSharedPointer<QMozSurface> takeWindowSurface(QMozWindow *window)
{
    SurfaceRegistry * const registry = surfaceRegistry();
    if (!registry || !window) {
        return QSharedPointer<QMozSurface>();
    }

    MutexLocker lock(&registry->mutex);
    return registry->surfaces.take(window);
}

} // namespace QtMoz
