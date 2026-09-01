/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef qmozcontext_h
#define qmozcontext_h

#include <QObject>
#include <QVariant>
#include <QStringList>

class QMozContextPrivate;

namespace mozilla {
namespace embedlite {
class EmbedLiteApp;
}
}

class QMozContext : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool initialized READ isInitialized NOTIFY initialized)
public:
    typedef void (*TaskCallback)(void *data);
    typedef void *TaskHandle;

    static QMozContext *instance();

    explicit QMozContext(QObject *parent = 0);
    virtual ~QMozContext();

    mozilla::embedlite::EmbedLiteApp *GetApp();

    Q_INVOKABLE bool isInitialized() const;
    Q_INVOKABLE bool isAccelerated() const;

    TaskHandle PostUITask(TaskCallback, void *data, int timeout = 0);
    TaskHandle PostCompositorTask(TaskCallback, void *data, int timeout = 0);
    void CancelTask(TaskHandle);

    void addObservers(const std::vector<std::string> &aObserversList);
    void removeObservers(const std::vector<std::string> &aObserversList);

    int getNumberOfWindows() const;

Q_SIGNALS:
    void initialized();
    void contextDestroyed();
    void lastWindowDestroyed();
    void recvObserve(const QString message, const QVariant data);

public Q_SLOTS:
    void setIsAccelerated(bool aIsAccelerated);
    void addComponentManifest(const QString &manifestPath);
    void addObserver(const QString &aTopic);
    void removeObserver(const QString &aTopic);

    void notifyObservers(const QString &topic, const QString &value);
    void notifyObservers(const QString &topic, const QVariant &value);
    void loadUserStyleSheet(const QString &uri, bool enable = true);

    // Starts Gecko on the Qt application thread. The argument remains only
    // for source compatibility and is ignored.
    void runEmbedding(int aDelay = -1);
    void stopEmbedding();
    void notifyFirstUIInitialized();
    void setProfile(const QString &);

private:
    QMozContextPrivate *d;
    Q_DISABLE_COPY(QMozContext)
    Q_DECLARE_PRIVATE(QMozContext)
};

#endif /* qmozcontext_h */
