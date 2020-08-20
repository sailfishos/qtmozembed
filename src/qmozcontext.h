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
class QMozViewCreator;
class QMozWindow;

class QMozContext : public QObject
{
    Q_OBJECT
public:
    typedef void (*TaskCallback)(void *data);
    typedef void *TaskHandle;

    explicit QMozContext(QObject *parent = 0);
    virtual ~QMozContext();

    mozilla::embedlite::EmbedLiteApp *GetApp();
    void setPixelRatio(float ratio);
    float pixelRatio() const;
    Q_INVOKABLE bool initialized() const;
    Q_INVOKABLE bool isAccelerated() const;

    void registerWindow(QMozWindow *window);
    QMozWindow *registeredWindow() const;

    static QMozContext *GetInstance();
    static QMozContext *instance();

    TaskHandle PostUITask(TaskCallback, void *data, int timeout = 0);
    TaskHandle PostCompositorTask(TaskCallback, void *data, int timeout = 0);
    void CancelTask(TaskHandle);

Q_SIGNALS:
    void onInitialized();
    void contextDestroyed();
    void lastViewDestroyed();
    void lastWindowDestroyed();
    void recvObserve(const QString message, const QVariant data);

public Q_SLOTS:
    void setIsAccelerated(bool aIsAccelerated);
    void addComponentManifest(const QString &manifestPath);
    void addObserver(const std::string &aTopic);
    void addObservers(const std::vector<std::string> &aObserversList);

    void notifyObservers(const QString &topic, const QString &value);
    void notifyObservers(const QString &topic, const QVariant &value);

    void sendObserve(const QString &aTopic, const QString &value);
    void sendObserve(const QString &aTopic, const QVariant &value);

    // running this without delay specified will execute Gecko/Qt nested main loop
    // and block this call until stopEmbedding called
    void runEmbedding(int aDelay = -1);
    void stopEmbedding();
    void setPref(const QString &aName, const QVariant &aPref);
    void notifyFirstUIInitialized();
    void setProfile(const QString &);

    void setViewCreator(QMozViewCreator *viewCreator);
    quint32 createView(const quint32 &parentId = 0);

private:
    QMozContextPrivate *d;
    Q_DISABLE_COPY(QMozContext)
    Q_DECLARE_PRIVATE(QMozContext)
};

#endif /* qmozcontext_h */
