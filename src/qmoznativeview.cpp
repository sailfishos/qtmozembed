/* Copyright (c) 2026 Jolla Mobile Ltd
 * SPDX-License-Identifier: MPL-2.0 */
#include "qmoznativeview.h"
#include "qmozview_p.h"
#include "qmozwindow.h"
#include "runtime/qmozframestream_p.h"
#include "runtime/qmozchromehost_p.h"

#include <QGuiApplication>
#include <QPlatformSurfaceEvent>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLFramebufferObject>
#include <QOpenGLShaderProgram>
#include <QQuickWindow>
#include <QQuickImageProvider>
#include <QQmlEngine>
#include <QScreen>
#include <QSGNode>
#include <QTimer>
#include <QMutex>
#include <QMutexLocker>
#include <QWindow>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2ext.h>

using namespace QtMoz;

class QMozNativePresentation
{
public:
    QOpenGLContext context;
    QOpenGLShaderProgram *program = nullptr;
    QSharedPointer<QMozSurface> surface;
    QMozSurfaceFrame frame = {{0, 0}, {nullptr, QSize(), QMozSurfaceTextureTarget::Texture2D}, QMozSurfaceFrameFenceType::NoHandle};
    GLuint texture = 0;
    quint64 requirement = 0;

    bool releaseFrame()
    {
        if (!frame.token.isValid()) return true;
        QOpenGLFunctions *gl = context.functions();
        if (frame.releaseFenceType == QMozSurfaceFrameFenceType::EGLSync) {
            const auto create = reinterpret_cast<PFNEGLCREATESYNCKHRPROC>(eglGetProcAddress("eglCreateSyncKHR"));
            const auto destroy = reinterpret_cast<PFNEGLDESTROYSYNCKHRPROC>(eglGetProcAddress("eglDestroySyncKHR"));
            const EGLDisplay display = eglGetCurrentDisplay();
            if (create && destroy && display != EGL_NO_DISPLAY) {
                EGLSyncKHR sync = create(display, EGL_SYNC_FENCE_KHR, nullptr);
                if (sync != EGL_NO_SYNC_KHR) {
                    gl->glFlush();
                    if (surface->releasePlatformFrame({frame.token, QMozSurfaceFrameFenceType::EGLSync, sync})) {
                        frame.token = {0, 0};
                        return true;
                    }
                    destroy(display, sync);
                }
            }
        }
        gl->glFinish();
        if (!surface->releasePlatformFrame({frame.token, QMozSurfaceFrameFenceType::NoHandle, nullptr})) return false;
        frame.token = {0, 0};
        return true;
    }
};

namespace {
// All native presenters run on the GUI thread. A null owner retains a pending
// clear when a private view goes away while the shared window is unexposed.
QHash<QWindow *, QPointer<QMozNativeView>> nativeOwners;
struct NativeImages {
    QMutex mutex;
    QHash<QString, QWeakPointer<QImage>> images;
    quint64 next = 0;
};
class NativeImageProvider : public QQuickImageProvider
{
public:
    NativeImageProvider() : QQuickImageProvider(Image), state(new NativeImages) {}
    QImage requestImage(const QString &id, QSize *size, const QSize &requested) override
    {
        QMutexLocker lock(&state->mutex);
        const auto image = state->images.value(id).toStrongRef();
        if (!image) return QImage();
        if (size) *size = image->size();
        return requested.isValid() ? image->scaled(requested, Qt::KeepAspectRatio, Qt::SmoothTransformation) : *image;
    }
    QSharedPointer<NativeImages> state;
};
}

class QMozNativeGrabResult : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QUrl url READ url CONSTANT)
public:
    QMozNativeGrabResult(const QImage &image, const QSharedPointer<NativeImages> &state)
        : m_state(state), m_image(new QImage(image)), m_id(QString::number(++state->next))
    {
        QMutexLocker lock(&state->mutex);
        state->images.insert(m_id, m_image.toWeakRef());
    }
    ~QMozNativeGrabResult() override { QMutexLocker lock(&m_state->mutex); m_state->images.remove(m_id); }
    QUrl url() const { return QUrl(QStringLiteral("image://qmoznative/") + m_id); }
private:
    QSharedPointer<NativeImages> m_state;
    QSharedPointer<QImage> m_image;
    QString m_id;
};

QMozNativeView::QMozNativeView(QQuickItem *parent)
    : QuickMozView(parent), m_presentation(new QMozNativePresentation)
{
    setFlag(ItemHasContents, false);
    connect(this, &QuickMozView::backgroundColorChanged, this, &QMozNativeView::requestPresentationUpdate);
    connect(this, &QuickMozView::activeChanged, this, &QMozNativeView::requestPresentationUpdate);
    connect(this, &QQuickItem::visibleChanged, this, &QMozNativeView::requestPresentationUpdate);
    connect(this, &QQuickItem::windowChanged, this, &QMozNativeView::requestPresentationUpdate);
}

QMozNativeView::~QMozNativeView()
{
    // Keep the surface lease alive if its importing context cannot complete
    // the reads. Returning the producer frame in that case is unsafe.
    if (releasePresentation()) delete m_presentation;
}

QWindow *QMozNativeView::presentationWindow() const { return m_presentationWindow; }

void QMozNativeView::setPresentationWindow(QWindow *window)
{
    if (m_presentationWindow == window) return;
    if (!releasePresentation()) return;
    if (m_presentationWindow) m_presentationWindow->removeEventFilter(this);
    m_presentationWindow = window;
    if (window) {
        window->installEventFilter(this);
        connect(window, &QObject::destroyed, this, [window]() { nativeOwners.remove(window); });
    }
    Q_EMIT presentationWindowChanged();
    requestPresentationUpdate();
}

QColor QMozNativeView::surfaceColor() const { return m_surfaceColor; }

void QMozNativeView::setSurfaceColor(const QColor &color)
{
    if (m_surfaceColor == color) return;
    m_surfaceColor = color;
    Q_EMIT surfaceColorChanged();
    requestPresentationUpdate();
}

QSGNode *QMozNativeView::updatePaintNode(QSGNode *node, UpdatePaintNodeData *)
{
    delete node;
    return nullptr;
}

void QMozNativeView::requestPresentationUpdate()
{
    if (m_updatePending) return;
    m_updatePending = true;
    QTimer::singleShot(0, this, [this]() {
        m_updatePending = false;
        if (!active() || !isVisible()) {
            releasePresentation();
            return;
        }
        present();
    });
}

bool QMozNativeView::eventFilter(QObject *object, QEvent *event)
{
    if (object == m_presentationWindow) {
        if (event->type() == QEvent::Expose || event->type() == QEvent::Resize) requestPresentationUpdate();
        if (event->type() == QEvent::PlatformSurface) {
            auto *surfaceEvent = static_cast<QPlatformSurfaceEvent *>(event);
            if (surfaceEvent->surfaceEventType() == QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed) releasePresentation();
        }
    }
    return QuickMozView::eventFilter(object, event);
}

bool QMozNativeView::releasePresentation()
{
    auto *p = m_presentation;
    if (!p->context.isValid()) return true;
    if (!m_presentationWindow || !p->context.makeCurrent(m_presentationWindow)) {
        if (p->frame.token.isValid()) {
            qWarning("Cannot release native browser frame without its GL context");
            return false;
        }
        return true;
    }
    auto owner = nativeOwners.find(m_presentationWindow);
    if (owner != nativeOwners.end() && (!owner.value() || owner.value() == this)) {
        if (m_presentationWindow->isExposed()) {
            auto *gl = p->context.functions();
            gl->glDisable(GL_SCISSOR_TEST);
            gl->glClearColor(0, 0, 0, 1);
            gl->glClear(GL_COLOR_BUFFER_BIT);
            p->context.swapBuffers(m_presentationWindow);
            nativeOwners.erase(owner);
        } else {
            owner.value().clear();
        }
    }
    if (!p->releaseFrame()) {
        p->context.doneCurrent();
        return false;
    }
    if (p->texture) p->context.functions()->glDeleteTextures(1, &p->texture);
    p->texture = 0;
    delete p->program;
    p->program = nullptr;
    p->surface.clear();
    p->context.doneCurrent();
    return true;
}

bool QMozNativeView::prepareFrame()
{
    auto *p = m_presentation;
    if (!m_presentationWindow || !active() || !isVisible() || !d->mMozWindow) return false;
    if (!p->context.isValid()) {
        p->context.setFormat(m_presentationWindow->format());
        if (!p->context.create()) return false;
    }
    if (!p->context.makeCurrent(m_presentationWindow)) return false;
    const auto stream = windowFrameStream(d->mMozWindow.data());
    if (!stream) return false;
    auto *gl = p->context.functions();
    if (p->requirement != mPlatformFrameRequirement || !mComposited) {
        if (!p->releaseFrame()) return false;
        if (p->texture) gl->glDeleteTextures(1, &p->texture);
        p->texture = 0;
        p->requirement = mPlatformFrameRequirement;
    }
    if (!mComposited) return false;
    QMozSurfaceFrameToken token = {0, 0};
    if (stream->takePendingFrame(this, &token)) {
        const auto surface = stream->surface();
        GLuint texture = 0;
        QMozSurfaceFrame next;
        const auto bindImage = reinterpret_cast<PFNGLEGLIMAGETARGETTEXTURE2DOESPROC>(eglGetProcAddress("glEGLImageTargetTexture2DOES"));
        const bool acquired = bindImage && surface->acquirePlatformFrame(token, [&](const QMozSurfaceFrame &frame) {
            if (frame.token.epoch != token.epoch || frame.token.sequence != token.sequence
                    || !frame.image.handle || frame.image.size.isEmpty()) return false;
            // Framebuffer pixels and item pixels may differ by the display
            // scale. Match the orientation/aspect, allowing one pixel rounding.
            const QSize expected = d->mSize.toSize();
            const QSize actual = frame.image.size;
            const qint64 difference = qAbs(qint64(actual.width()) * expected.height()
                    - qint64(expected.width()) * actual.height());
            const qint64 tolerance = qMax(qMax(actual.width(), actual.height()),
                                          qMax(expected.width(), expected.height()));
            if (expected.isEmpty() || difference > tolerance) return false;
            const GLenum target = frame.image.textureTarget == QMozSurfaceTextureTarget::ExternalOES ? GL_TEXTURE_EXTERNAL_OES : GL_TEXTURE_2D;
            gl->glGenTextures(1, &texture);
            gl->glBindTexture(target, texture);
            gl->glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            gl->glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            gl->glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            gl->glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            bindImage(target, frame.image.handle);
            if (gl->glGetError() != GL_NO_ERROR) {
                gl->glDeleteTextures(1, &texture);
                texture = 0;
                return false;
            }
            next = frame;
            return true;
        });
        if (!acquired) {
            stream->restorePendingFrame(this, token);
        } else {
            // Complete all uses of the previous image before returning it.
            if (!p->releaseFrame()) {
                gl->glDeleteTextures(1, &texture);
                surface->releasePlatformFrame({next.token, QMozSurfaceFrameFenceType::NoHandle, nullptr});
                return false;
            }
            if (p->texture) gl->glDeleteTextures(1, &p->texture);
            if (p->program && p->frame.image.textureTarget != next.image.textureTarget) {
                delete p->program;
                p->program = nullptr;
            }
            p->frame = next;
            p->surface = surface;
            p->texture = texture;
            ++mPlatformFrameGeneration;
            Q_EMIT platformFrameGenerationChanged();
        }
    }
    return p->texture != 0 && p->frame.token.isValid();
}

bool QMozNativeView::draw(const QSize &size, const QRectF &rect, Qt::ScreenOrientation surfaceOrientation)
{
    auto *p = m_presentation;
    auto *gl = p->context.functions();
    const bool external = p->frame.image.textureTarget == QMozSurfaceTextureTarget::ExternalOES;
    if (!p->program) {
        auto *program = new QOpenGLShaderProgram;
        const char *vertex = "attribute highp vec2 pos; attribute highp vec2 uv; varying highp vec2 tex; void main(){gl_Position=vec4(pos,0.,1.);tex=uv;}";
        const char *fragment = external
            ? "#extension GL_OES_EGL_image_external : require\nvarying highp vec2 tex; uniform samplerExternalOES image; void main(){gl_FragColor=texture2D(image,tex);}"
            : "varying highp vec2 tex; uniform sampler2D image; void main(){gl_FragColor=texture2D(image,tex);}";
        if (!program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertex)
                || !program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragment) || !program->link()) {
            qWarning() << "Native browser shader failed:" << program->log();
            delete program;
            return false;
        }
        p->program = program;
    }
    const GLfloat vertices[] = {
        GLfloat(2 * rect.left() / size.width() - 1), GLfloat(1 - 2 * rect.top() / size.height()),
        GLfloat(2 * rect.left() / size.width() - 1), GLfloat(1 - 2 * rect.bottom() / size.height()),
        GLfloat(2 * rect.right() / size.width() - 1), GLfloat(1 - 2 * rect.top() / size.height()),
        GLfloat(2 * rect.right() / size.width() - 1), GLfloat(1 - 2 * rect.bottom() / size.height())
    };
    const GLfloat corners[][2] = {{0,1},{0,0},{1,1},{1,0}};
    const int order[][4] = {{0,1,2,3},{1,3,0,2},{3,2,1,0},{2,0,3,1}};
    const int rotation = qApp->primaryScreen()->angleBetween(orientation(), surfaceOrientation) / 90;
    GLfloat uv[8];
    for (int i = 0; i < 4; ++i) {
        uv[2*i] = corners[order[rotation % 4][i]][0];
        uv[2*i+1] = corners[order[rotation % 4][i]][1];
    }
    gl->glDisable(GL_DEPTH_TEST);
    gl->glDisable(GL_STENCIL_TEST);
    gl->glDisable(GL_SCISSOR_TEST);
    gl->glDisable(GL_BLEND);
    auto *program = p->program;
    if (!program->bind()) return false;
    program->setUniformValue("image", 0);
    const int pos = program->attributeLocation("pos"), tex = program->attributeLocation("uv");
    program->enableAttributeArray(pos);
    program->enableAttributeArray(tex);
    program->setAttributeArray(pos, GL_FLOAT, vertices, 2);
    program->setAttributeArray(tex, GL_FLOAT, uv, 2);
    gl->glActiveTexture(GL_TEXTURE0);
    gl->glBindTexture(external ? GL_TEXTURE_EXTERNAL_OES : GL_TEXTURE_2D, p->texture);
    gl->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    program->disableAttributeArray(pos);
    program->disableAttributeArray(tex);
    program->release();
    return gl->glGetError() == GL_NO_ERROR;
}

void QMozNativeView::present()
{
    if (!m_presentationWindow || !m_presentationWindow->isExposed()) return;
    const bool ready = prepareFrame();
    auto *p = m_presentation;
    if (!p->context.isValid() || !p->context.makeCurrent(m_presentationWindow)) return;
    const QSize size = m_presentationWindow->size() * m_presentationWindow->devicePixelRatio();
    auto *gl = p->context.functions();
    gl->glViewport(0, 0, size.width(), size.height());
    // The shell supplies the page theme colour for areas outside the content,
    // such as the display cutout. Fall back to Gecko's background if unset.
    const QColor color = m_surfaceColor.isValid() ? m_surfaceColor : d->mBackgroundColor;
    gl->glClearColor(color.redF(), color.greenF(), color.blueF(), 1);
    gl->glClear(GL_COLOR_BUFFER_BIT);
    if (ready && window()) {
        const QRectF rect = mapRectToScene(QRectF(d->renderingOffset(), d->mSize));
        const qreal ratio = m_presentationWindow->devicePixelRatio();
        draw(size, QRectF(rect.topLeft() * ratio, rect.size() * ratio), qApp->primaryScreen()->primaryOrientation());
    }
    p->context.swapBuffers(m_presentationWindow);
    nativeOwners.insert(m_presentationWindow, this);
    p->context.doneCurrent();
}

QImage QMozNativeView::captureImage(const QSize &size)
{
    auto *p = m_presentation;
    if (!size.isValid() || !prepareFrame()) {
        if (QOpenGLContext::currentContext() == &p->context) p->context.doneCurrent();
        return QImage();
    }
    QImage image;
    {
        QOpenGLFramebufferObject fbo(size);
        if (fbo.isValid() && fbo.bind()) {
            p->context.functions()->glViewport(0, 0, size.width(), size.height());
            if (draw(size, QRectF(QPointF(), size), orientation())) image = fbo.toImage();
            fbo.release();
        }
        // Destroy the framebuffer with its context current.
    }
    p->context.doneCurrent();
    return image;
}

bool QMozNativeView::grabNativeImage(const QJSValue &callback, const QSize &size)
{
    QQmlEngine *engine = qmlEngine(this);
    if (!engine || !callback.isCallable()) return false;
    const QImage image = captureImage(size);
    if (image.isNull()) return false;
    auto *provider = static_cast<NativeImageProvider *>(engine->imageProvider(QStringLiteral("qmoznative")));
    if (!provider) {
        provider = new NativeImageProvider;
        engine->addImageProvider(QStringLiteral("qmoznative"), provider);
    }
    auto *result = new QMozNativeGrabResult(image, provider->state);
    QQmlEngine::setObjectOwnership(result, QQmlEngine::JavaScriptOwnership);
    const QJSValue value = engine->newQObject(result);
    QTimer::singleShot(0, this, [callbackCopy = QJSValue(callback), value]() mutable { callbackCopy.call(QJSValueList{value}); });
    return true;
}

#include "qmoznativeview.moc"
