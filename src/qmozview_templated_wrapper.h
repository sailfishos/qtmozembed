/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef qmozview_templated_wrapper_h
#define qmozview_templated_wrapper_h

class QPoint;
class QString;
class QRect;
class QMozReturnValue;
class IMozQViewIface
{
public:
    virtual ~IMozQViewIface() {}
    // Methods
    virtual void setInputMethodHints(Qt::InputMethodHints hints) = 0;
    virtual void forceViewActiveFocus() = 0;

    // Signals
    virtual void viewInitialized() = 0;
    virtual void urlChanged() = 0;
    virtual void titleChanged() = 0;
    virtual void loadProgressChanged() = 0;
    virtual void canGoBackChanged() = 0;
    virtual void canGoForwardChanged() = 0;
    virtual void loadingChanged() = 0;
    virtual void loadedChanged() = 0;
    virtual void viewDestroyed() = 0;
    virtual void windowCloseRequested() = 0;
    virtual void recvAsyncMessage(const QString message, const QVariant data) = 0;
    virtual bool recvSyncMessage(const QString message, const QVariant data, QMozReturnValue *response) = 0;
    virtual void loadRedirect() = 0;
    virtual void securityChanged(QString status, uint state) = 0;
    virtual void firstPaint(int offx, int offy) = 0;
    virtual void contentWidthChanged() = 0;
    virtual void contentHeightChanged() = 0;
    virtual void viewAreaChanged() = 0;
    virtual void scrollableOffsetChanged() = 0;
    virtual void atXBeginningChanged() = 0;
    virtual void atXEndChanged() = 0;
    virtual void atYBeginningChanged() = 0;
    virtual void atYEndChanged() = 0;
    virtual void handleLongTap(QPoint point, QMozReturnValue *retval) = 0;
    virtual void handleSingleTap(QPoint point, QMozReturnValue *retval) = 0;
    virtual void handleDoubleTap(QPoint point, QMozReturnValue *retval) = 0;
    virtual void imeNotification(int state, bool open, int cause, int focusChange, const QString &type) = 0;
    virtual void backgroundColorChanged() = 0;
    virtual void useQmlMouse(bool value) = 0;
    virtual void draggingChanged() = 0;
    virtual void movingChanged() = 0;
    virtual void pinchingChanged() = 0;
    virtual void marginsChanged() = 0;
    virtual void scrollableSizeChanged() = 0;

    virtual void desktopModeChanged() = 0;
    virtual void httpUserAgentChanged() = 0;
    virtual void chromeGestureEnabledChanged() = 0;
    virtual void chromeGestureThresholdChanged() = 0;
    virtual void chromeChanged() = 0;

    virtual void parentIdChanged() = 0;
    virtual void uniqueIdChanged() = 0;
};

template<class TMozQView>
class IMozQView : public IMozQViewIface
{
public:
    IMozQView(TMozQView &aView) : view(aView) {}

    void setInputMethodHints(Qt::InputMethodHints hints)
    {
        view.setInputMethodHints(hints);
    }

    void forceViewActiveFocus()
    {
        view.forceViewActiveFocus();
    }
    void viewInitialized()
    {
        Q_EMIT view.viewInitialized();
    }
    void urlChanged()
    {
        Q_EMIT view.urlChanged();
    }
    void titleChanged()
    {
        Q_EMIT view.titleChanged();
    }
    void loadProgressChanged()
    {
        Q_EMIT view.loadProgressChanged();
    }
    void canGoBackChanged()
    {
        Q_EMIT view.canGoBackChanged();
    }
    void canGoForwardChanged()
    {
        Q_EMIT view.canGoForwardChanged();
    }
    void loadingChanged()
    {
        Q_EMIT view.loadingChanged();
    }

    void loadedChanged() override
    {
        Q_EMIT view.loadedChanged();
    }

    void viewDestroyed()
    {
        Q_EMIT view.viewDestroyed();
    }
    void windowCloseRequested()
    {
        Q_EMIT view.windowCloseRequested();
    }
    void recvAsyncMessage(const QString message, const QVariant data)
    {
        Q_EMIT view.recvAsyncMessage(message, data);
    }
    bool recvSyncMessage(const QString message, const QVariant data, QMozReturnValue *response)
    {
        return Q_EMIT view.recvSyncMessage(message, data, response);
    }
    void loadRedirect()
    {
        Q_EMIT view.loadRedirect();
    }
    void securityChanged(QString status, uint state)
    {
        Q_EMIT view.securityChanged(status, state);
    }
    void firstPaint(int offx, int offy)
    {
        Q_EMIT view.firstPaint(offx, offy);
    }
    void viewAreaChanged()
    {
        Q_EMIT view.viewAreaChanged();
    }
    void scrollableOffsetChanged()
    {
        Q_EMIT view.scrollableOffsetChanged();
    }
    void atXBeginningChanged()
    {
        Q_EMIT view.atXBeginningChanged();
    }
    void atXEndChanged()
    {
        Q_EMIT view.atXEndChanged();
    }
    void atYBeginningChanged()
    {
        Q_EMIT view.atYBeginningChanged();
    }
    void atYEndChanged()
    {
        Q_EMIT view.atYEndChanged();
    }

    void chromeGestureEnabledChanged() override
    {
        Q_EMIT view.chromeGestureEnabledChanged();
    }

    void chromeGestureThresholdChanged() override
    {
        Q_EMIT view.chromeGestureThresholdChanged();
    }

    void chromeChanged() override
    {
        Q_EMIT view.chromeChanged();
    }

    void parentIdChanged() override
    {
        Q_EMIT view.parentIdChanged();
    }

    void uniqueIdChanged() override
    {
        Q_EMIT view.uniqueIdChanged();
    }

    void handleLongTap(QPoint point, QMozReturnValue *retval)
    {
        Q_EMIT view.handleLongTap(point, retval);
    }
    void handleSingleTap(QPoint point, QMozReturnValue *retval)
    {
        Q_EMIT view.handleSingleTap(point, retval);
    }
    void handleDoubleTap(QPoint point, QMozReturnValue *retval)
    {
        Q_EMIT view.handleDoubleTap(point, retval);
    }
    void imeNotification(int state, bool open, int cause, int focusChange, const QString &type)
    {
        Q_EMIT view.imeNotification(state, open, cause, focusChange, type);
    }
    void backgroundColorChanged()
    {
        Q_EMIT view.backgroundColorChanged();
    }

    void useQmlMouse(bool value)
    {
        Q_EMIT view.useQmlMouse(value);
    }

    void draggingChanged()
    {
        Q_EMIT view.draggingChanged();
    }

    void movingChanged()
    {
        Q_EMIT view.movingChanged();
    }

    void pinchingChanged()
    {
        Q_EMIT view.pinchingChanged();
    }

    void marginsChanged()
    {
        Q_EMIT view.marginsChanged();
    }

    void contentWidthChanged()
    {
        Q_EMIT view.contentWidthChanged();
    }

    void contentHeightChanged()
    {
        Q_EMIT view.contentHeightChanged();
    }

    void desktopModeChanged() override
    {
        Q_EMIT view.desktopModeChanged();
    }

    void httpUserAgentChanged()
    {
        Q_EMIT view.httpUserAgentChanged();
    }

    void scrollableSizeChanged()
    {
        Q_EMIT view.scrollableSizeChanged();
    }

    TMozQView &view;
};

#endif /* qmozview_templated_wrapper_h */
