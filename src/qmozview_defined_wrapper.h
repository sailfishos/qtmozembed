/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (c) 2013 - 2019 Jolla Ltd.
 * Copyright (c) 2019 Open Mobile Platform LLC.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef qmozview_defined_wrapper_h
#define qmozview_defined_wrapper_h

#include <QVariant>
#include <QColor>
#include <QJSValue>
#include <QUrl>
#include <QRect>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QPoint>
#include <QPointF>
#include <QMargins>

class QMozScrollDecorator;

class QMozReturnValue : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariant message READ getMessage WRITE setMessage FINAL)

public:
    QMozReturnValue(QObject *parent = 0) : QObject(parent) {}
    QMozReturnValue(const QMozReturnValue &aMsg) : QObject(nullptr)
    {
        mMessage = aMsg.mMessage;
    }
    virtual ~QMozReturnValue() {}

    QVariant getMessage() const
    {
        return mMessage;
    }
    void setMessage(const QVariant &msg)
    {
        mMessage = msg;
    }

private:
    QVariant mMessage;
};

Q_DECLARE_METATYPE(QMozReturnValue)

#define Q_MOZ_RETURN_VALUE \
class QMozReturnValue : public QObject \
{ \
    Q_OBJECT \
    Q_PROPERTY(QVariant message READ getMessage WRITE setMessage FINAL) \
public: \
    QMozReturnValue(QObject *parent = 0) : QObject(parent) {} \
    QMozReturnValue(const QMozReturnValue &aMsg) : QObject(nullptr) { mMessage = aMsg.mMessage; } \
    virtual ~QMozReturnValue() {} \
    QVariant getMessage() const { return mMessage; } \
    void setMessage(const QVariant &msg) { mMessage = msg; } \
private: \
    QVariant mMessage; \
}; \
Q_DECLARE_METATYPE(QMozReturnValue) \

#define Q_MOZ_VIEW_PROPERTIES \
    Q_PROPERTY(QUrl url READ url WRITE setUrl NOTIFY urlChanged) \
    Q_PROPERTY(QString title READ title NOTIFY titleChanged) \
    Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY canGoBackChanged FINAL) \
    Q_PROPERTY(bool canGoForward READ canGoForward NOTIFY canGoForwardChanged FINAL) \
    Q_PROPERTY(int loadProgress READ loadProgress NOTIFY loadProgressChanged) \
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged FINAL) \
    Q_PROPERTY(QRectF contentRect READ contentRect NOTIFY viewAreaChanged FINAL) \
    Q_PROPERTY(qreal contentWidth READ contentWidth NOTIFY contentWidthChanged FINAL) \
    Q_PROPERTY(qreal contentHeight READ contentHeight NOTIFY contentHeightChanged FINAL) \
    Q_PROPERTY(QSizeF scrollableSize READ scrollableSize NOTIFY scrollableSizeChanged FINAL) \
    Q_PROPERTY(QPointF scrollableOffset READ scrollableOffset NOTIFY scrollableOffsetChanged FINAL) \
    Q_PROPERTY(bool atXBeginning READ atXBeginning NOTIFY atXBeginningChanged FINAL) \
    Q_PROPERTY(bool atXEnd READ atXEnd NOTIFY atXEndChanged FINAL) \
    Q_PROPERTY(bool atYBeginning READ atYBeginning NOTIFY atYBeginningChanged FINAL) \
    Q_PROPERTY(bool atYEnd READ atYEnd NOTIFY atYEndChanged FINAL) \
    Q_PROPERTY(float resolution READ resolution NOTIFY resolutionChanged FINAL) \
    Q_PROPERTY(bool painted READ isPainted NOTIFY firstPaint FINAL) \
    Q_PROPERTY(QColor backgroundColor READ backgroundColor NOTIFY backgroundColorChanged FINAL) \
    Q_PROPERTY(bool dragging READ dragging NOTIFY draggingChanged FINAL) \
    Q_PROPERTY(bool moving READ moving NOTIFY movingChanged FINAL) \
    Q_PROPERTY(bool pinching READ pinching NOTIFY pinchingChanged FINAL) \
    Q_PROPERTY(QMozScrollDecorator *verticalScrollDecorator READ verticalScrollDecorator NOTIFY verticalScrollDecoratorChanged FINAL) \
    Q_PROPERTY(QMozScrollDecorator *horizontalScrollDecorator READ horizontalScrollDecorator NOTIFY horizontalScrollDecoratorChanged FINAL) \
    Q_PROPERTY(bool chrome READ chrome WRITE setChrome NOTIFY chromeChanged FINAL) \
    Q_PROPERTY(bool chromeGestureEnabled READ chromeGestureEnabled WRITE setChromeGestureEnabled NOTIFY chromeGestureEnabledChanged FINAL) \
    Q_PROPERTY(qreal chromeGestureThreshold READ chromeGestureThreshold WRITE setChromeGestureThreshold NOTIFY chromeGestureThresholdChanged FINAL) \
    Q_PROPERTY(QMozSecurity *security READ security NOTIFY securityChanged FINAL) \
    Q_PROPERTY(bool desktopMode READ desktopMode WRITE setDesktopMode NOTIFY desktopModeChanged FINAL) \
    Q_PROPERTY(int parentId READ parentId NOTIFY parentIdChanged FINAL) \
    Q_PROPERTY(int uniqueId READ uniqueId NOTIFY uniqueIdChanged FINAL) \
    Q_PROPERTY(QString httpUserAgent READ httpUserAgent WRITE setHttpUserAgent NOTIFY httpUserAgentChanged) \
    Q_PROPERTY(bool domContentLoaded READ domContentLoaded NOTIFY domContentLoadedChanged FINAL) \

#define Q_MOZ_VIEW_PUBLIC_METHODS \
    QUrl url() const; \
    void setUrl(const QUrl&); \
    bool isUrlResolved() const; \
    QString title() const; \
    int loadProgress() const; \
    bool canGoBack() const; \
    bool canGoForward() const; \
    bool loading() const; \
    QRectF contentRect() const; \
    qreal contentWidth() const; \
    qreal contentHeight() const; \
    QSizeF scrollableSize() const; \
    QPointF scrollableOffset() const; \
    bool atXBeginning() const; \
    bool atXEnd() const; \
    bool atYBeginning() const; \
    bool atYEnd() const; \
    float resolution() const; \
    bool isPainted() const; \
    QColor backgroundColor() const; \
    void forceViewActiveFocus(); \
    bool dragging() const; \
    bool moving() const; \
    bool pinching() const; \
    QMozScrollDecorator *verticalScrollDecorator() const; \
    QMozScrollDecorator *horizontalScrollDecorator() const; \
    bool chromeGestureEnabled() const; \
    void setChromeGestureEnabled(bool value); \
    bool chrome() const; \
    void setChrome(bool value); \
    qreal chromeGestureThreshold() const; \
    void setChromeGestureThreshold(qreal value); \
    int dynamicToolbarHeight() const; \
    void setDynamicToolbarHeight(int height); \
    QMargins margins() const; \
    void setMargins(QMargins); \
    Q_INVOKABLE void scrollTo(int x, int y); \
    Q_INVOKABLE void scrollBy(int x, int y); \
    QMozSecurity *security(); \
    void addMessageListeners(const std::vector<std::string> &messageNamesList); \
    bool desktopMode() const; \
    void setDesktopMode(bool); \
    int parentId() const; \
    QString httpUserAgent() const; \
    void setHttpUserAgent(const QString &httpUserAgent); \
    bool domContentLoaded() const; \
    Q_INVOKABLE void runJavaScript(const QString &script, \
                               const QJSValue &callback = QJSValue::UndefinedValue, \
                               const QJSValue &errorCallback = QJSValue::UndefinedValue); \

#define Q_MOZ_VIEW_PUBLIC_SLOTS \
    void loadHtml(const QString &html, const QUrl &baseUrl = QUrl()); \
    void loadText(const QString &text, const QString &mimeType); \
    void goBack(); \
    void goForward(); \
    void stop(); \
    void reload(); \
    void load(const QString&, bool fromExternal); \
    void sendAsyncMessage(const QString &name, const QVariant &variant); \
    void addMessageListener(const QString &name); \
    void loadFrameScript(const QString &name); \
    void newWindow(const QString &url = "about:blank"); \
    quint32 uniqueId() const; \
    void setParentId(unsigned parentId); \
    void setParentBrowsingContext(uintptr_t parentBrowsingContext); \
    void synthTouchBegin(const QVariant &touches); \
    void synthTouchMove(const QVariant &touches); \
    void synthTouchEnd(const QVariant &touches); \
    void suspendView(); \
    void resumeView(); \

#define Q_MOZ_VIEW_SIGNALS \
    void viewInitialized(); \
    void urlChanged(); \
    void titleChanged(); \
    void loadProgressChanged(); \
    void canGoBackChanged(); \
    void canGoForwardChanged(); \
    void loadingChanged(); \
    void viewDestroyed(); \
    void windowCloseRequested(); \
    void recvAsyncMessage(const QString message, const QVariant data); \
    bool recvSyncMessage(const QString message, const QVariant data, QMozReturnValue *response); \
    void loadRedirect(); \
    void securityChanged(QString status, uint state); \
    void firstPaint(int offx, int offy); \
    void viewAreaChanged(); \
    void scrollableOffsetChanged(); \
    void atXBeginningChanged(); \
    void atXEndChanged(); \
    void atYBeginningChanged(); \
    void atYEndChanged(); \
    void resolutionChanged(); \
    void handleLongTap(QPoint point, QMozReturnValue *retval); \
    void handleSingleTap(QPoint point, QMozReturnValue *retval); \
    void handleDoubleTap(QPoint point, QMozReturnValue *retval); \
    void imeNotification(int state, bool open, int cause, int focusChange, const QString &type); \
    void backgroundColorChanged(); \
    void draggingChanged(); \
    void movingChanged(); \
    void pinchingChanged(); \
    void contentWidthChanged(); \
    void contentHeightChanged(); \
    void verticalScrollDecoratorChanged(); \
    void horizontalScrollDecoratorChanged(); \
    void chromeGestureEnabledChanged(); \
    void chromeChanged(); \
    void chromeGestureThresholdChanged(); \
    void dynamicToolbarHeightChanged(); \
    void marginsChanged(); \
    void desktopModeChanged(); \
    void parentIdChanged(); \
    void uniqueIdChanged(); \
    void httpUserAgentChanged(); \
    void domContentLoadedChanged(); \
    void scrollableSizeChanged(); \

#endif /* qmozview_defined_wrapper_h */
