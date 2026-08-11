/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2026 Jolla Mobile Ltd
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "runtime/qmozsurface_p.h"

#include <cstdio>

namespace {

int failures = 0;

void verify(bool condition, const char *expression, int line)
{
    if (!condition) {
        std::fprintf(stderr, "line %d: verification failed: %s\n",
                     line, expression);
        ++failures;
    }
}

#define VERIFY(expression) verify((expression), #expression, __LINE__)

class FakeSurface final : public QMozSurface
{
public:
    FakeSurface(QMozWindow *window, int *destructions)
        : mWindow(window)
        , mDestructions(destructions)
        , mDestroyRequests(0)
    {
    }

    ~FakeSurface() override
    {
        ++*mDestructions;
    }

    void requestDestroy() override
    {
        ++mDestroyRequests;
        QtMoz::takeWindowSurface(mWindow);
    }

    void backendDestroyed() override
    {
    }

    bool setSize(const QSize &) override
    {
        return true;
    }

    bool setContentOrientation(QMozSurfaceRotation) override
    {
        return true;
    }

    bool withPlatformImage(
            const QMozSurfaceImageCallback &callback) override
    {
        if (!callback) {
            return false;
        }
        callback({ reinterpret_cast<void *>(1), QSize(1, 1),
                   QMozSurfaceTextureTarget::Texture2D });
        return true;
    }

    bool clearPlatformImage() override
    {
        return true;
    }

    bool suspendRendering() override
    {
        return true;
    }

    bool resumeRendering() override
    {
        return true;
    }

    bool scheduleUpdate() override
    {
        return true;
    }

    int destroyRequests() const
    {
        return mDestroyRequests;
    }

private:
    QMozWindow *mWindow;
    int *mDestructions;
    int mDestroyRequests;
};

QMozWindow *fakeWindow(void *storage)
{
    return reinterpret_cast<QMozWindow *>(storage);
}

void testRegistryLifetime()
{
    void *storage = nullptr;
    QMozWindow * const window = fakeWindow(&storage);
    int destructions = 0;
    QSharedPointer<QMozSurface> surface(
            new FakeSurface(window, &destructions));

    VERIFY(QtMoz::installWindowSurface(window, surface));
    VERIFY(!QtMoz::installWindowSurface(window, surface));

    QSharedPointer<QMozSurface> snapshot = QtMoz::windowSurface(window);
    VERIFY(snapshot == surface);
    surface.clear();
    VERIFY(destructions == 0);

    QSharedPointer<QMozSurface> removed = QtMoz::takeWindowSurface(window);
    VERIFY(removed == snapshot);
    VERIFY(QtMoz::windowSurface(window).isNull());
    removed.clear();
    VERIFY(destructions == 0);
    snapshot.clear();
    VERIFY(destructions == 1);
}

void testReentrantRemoval()
{
    void *storage = nullptr;
    QMozWindow * const window = fakeWindow(&storage);
    int destructions = 0;
    QSharedPointer<FakeSurface> surface(
            new FakeSurface(window, &destructions));

    VERIFY(QtMoz::installWindowSurface(window, surface));
    QSharedPointer<QMozSurface> snapshot = QtMoz::windowSurface(window);
    snapshot->requestDestroy();
    VERIFY(surface->destroyRequests() == 1);
    VERIFY(QtMoz::windowSurface(window).isNull());

    surface.clear();
    VERIFY(destructions == 0);
    snapshot.clear();
    VERIFY(destructions == 1);
}

} // namespace

int main()
{
    testRegistryLifetime();
    testReentrantRemoval();
    return failures == 0 ? 0 : 1;
}
