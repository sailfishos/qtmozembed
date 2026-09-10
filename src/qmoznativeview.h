/* Copyright (c) 2026 Jolla Mobile Ltd
 * SPDX-License-Identifier: MPL-2.0 */
#ifndef QMOZNATIVEVIEW_H
#define QMOZNATIVEVIEW_H

#include "quickmozview.h"
#include <QImage>
#include <QJSValue>
#include <QPointer>

class QWindow;
class QMozNativePresentation;

// The item supplies geometry, input and hosted-session APIs only. Web content
// is drawn into the native window, never into the Qt Quick scene graph.
class QMozNativeView : public QuickMozView
{
    Q_OBJECT
    Q_PROPERTY(QColor surfaceColor READ surfaceColor WRITE setSurfaceColor NOTIFY surfaceColorChanged)
    Q_PROPERTY(QWindow *presentationWindow READ presentationWindow WRITE setPresentationWindow NOTIFY presentationWindowChanged)
public:
    explicit QMozNativeView(QQuickItem *parent = nullptr);
    ~QMozNativeView() override;
    QWindow *presentationWindow() const;
    void setPresentationWindow(QWindow *window);
    QColor surfaceColor() const;
    void setSurfaceColor(const QColor &color);
    QImage captureImage(const QSize &size);
    Q_INVOKABLE bool grabNativeImage(const QJSValue &callback, const QSize &size);

Q_SIGNALS:
    void presentationWindowChanged();
    void surfaceColorChanged();

protected:
    void requestPresentationUpdate();
    bool releasePresentation();
    bool eventFilter(QObject *object, QEvent *event) override;
    QSGNode *updatePaintNode(QSGNode *node, UpdatePaintNodeData *) override;

private:
    friend class QuickMozView;
    bool prepareFrame();
    void present();
    bool draw(const QSize &size, const QRectF &rect, Qt::ScreenOrientation orientation);
    QPointer<QWindow> m_presentationWindow;
    QMozNativePresentation *m_presentation;
    QColor m_surfaceColor;
    bool m_updatePending = false;
};
#endif
