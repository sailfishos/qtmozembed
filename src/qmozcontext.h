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
    Q_PROPERTY(bool initialized READ isInitialized NOTIFY initialized)
public:
    typedef void (*TaskCallback)(void *data);
    typedef void *TaskHandle;

    explicit QMozContext(QObject *parent = 0);
    virtual ~QMozContext();

    mozilla::embedlite::EmbedLiteApp *GetApp();
    Q_INVOKABLE bool isInitialized() const;
    Q_INVOKABLE bool isAccelerated() const;

    void registerWindow(QMozWindow *window);
    QMozWindow *registeredWindow() const;

    static QMozContext *instance();

    TaskHandle PostUITask(TaskCallback, void *data, int timeout = 0);
    TaskHandle PostCompositorTask(TaskCallback, void *data, int timeout = 0);
    void CancelTask(TaskHandle);

    void addObservers(const std::vector<std::string> &aObserversList);
    void removeObservers(const std::vector<std::string> &aObserversList);

    int getNumberOfViews() const;
    int getNumberOfWindows() const;

Q_SIGNALS:
    void initialized();
    void contextDestroyed();
    void lastViewDestroyed();
    void lastWindowDestroyed();
    void recvObserve(const QString message, const QVariant data);

public Q_SLOTS:
    void setIsAccelerated(bool aIsAccelerated);
    void addComponentManifest(const QString &manifestPath);
    void addObserver(const QString &aTopic);
    void removeObserver(const QString &aTopic);

    void notifyObservers(const QString &topic, const QString &value);
    void notifyObservers(const QString &topic, const QVariant &value);

    // running this without delay specified will execute Gecko/Qt nested main loop
    // and block this call until stopEmbedding called
    void runEmbedding(int aDelay = -1);
    void stopEmbedding();
    void notifyFirstUIInitialized();
    void setProfile(const QString &);

    void setViewCreator(QMozViewCreator *viewCreator);
    quint32 createView(const quint32 &parentId = 0, const uintptr_t &parentBrowsingContext = 0, const bool hidden = false);

private:
    QMozContextPrivate *d;
    Q_DISABLE_COPY(QMozContext)
    Q_DECLARE_PRIVATE(QMozContext)
};

#endif /* qmozcontext_h */
