
#include "declarativemediamodel.h"
#include "declarativemediasource.h"
#include "declarativedbusinterface.h"
#include "declarativefileinfo.h"
#include "declarativewallpaper.h"

#include <QDeclarativeExtensionPlugin>
#include <qdeclarative.h>

#include <QtDebug>
#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDBusError>
#include <QDBusMetaType>
#include <QStringList>
#include <QVector>

class TestDBusService;

Q_DECLARE_METATYPE(QVector<QStringList>)

struct TrackerChange
{
    int graph;
    int subject;
    int predicate;
    int object;
};

QDBusArgument &operator <<(QDBusArgument &argument, const TrackerChange &change)
{
    argument.beginStructure();
    argument << change.graph << change.subject << change.predicate << change.object;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator >>(const QDBusArgument &argument, TrackerChange &change)
{
    argument.beginStructure();
    argument >> change.graph >> change.subject >> change.predicate >> change.object;
    argument.endStructure();
    return argument;
}

Q_DECLARE_METATYPE(TrackerChange)
Q_DECLARE_METATYPE(QVector<TrackerChange>)

class TestResourcesAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Tracker1.Resources")
public:
    TestResourcesAdaptor(TestDBusService *service);
    QVector<QStringList> returnValues;

public slots:
    QVector<QStringList> SparqlQuery(const QString &argument);

    void SparqlUpdate(const QString &argument);

signals:
    void GraphUpdated(const QString &className, const QVector<TrackerChange> &deletes, const QVector<TrackerChange> &inserts);

private:
    friend class TestDBusService;
    TestDBusService *m_service;
};

class TestDBusService : public QObject
{
    Q_OBJECT
public:
    TestDBusService(QObject *parent = 0)
        : QObject(parent)
        , m_resources(this)
    {
        if (!QDBusConnection::sessionBus().registerObject(
                QLatin1String("/org/freedesktop/Tracker1/Resources"), this)) {
            qWarning() << "Failed to register Resources object";
            qWarning() << QDBusConnection::sessionBus().lastError().message();
        } else if (!QDBusConnection::sessionBus().registerService(
                QLatin1String("org.freedesktop.Tracker1"))) {
            qWarning() << "Failed to register service";
            qWarning() << QDBusConnection::sessionBus().lastError().message();
        }
    }

    Q_INVOKABLE void updateGraph(const QString &className)
    {
        emit m_resources.GraphUpdated(className, QVector<TrackerChange>(), QVector<TrackerChange>());
    }

    Q_INVOKABLE void returnRow(const QStringList &row)
    {
        m_resources.returnValues.append(row);
    }

signals:
    void query(const QString &argument);
    void update(const QString &argument);

private:
    TestResourcesAdaptor m_resources;

    friend class TestResourcesAdaptor;
};

TestResourcesAdaptor::TestResourcesAdaptor(TestDBusService *service)
    : QDBusAbstractAdaptor(service)
    , m_service(service)
{
}

QVector<QStringList> TestResourcesAdaptor::SparqlQuery(const QString &argument)
{
    returnValues.clear();

    emit m_service->query(argument);

    return returnValues;
}

void TestResourcesAdaptor::SparqlUpdate(const QString &argument)
{
    emit m_service->update(argument);
}

class TestMediaModel : public DeclarativeMediaModel
{
public:
    TestMediaModel(QObject *parent = 0)
        : DeclarativeMediaModel(QLatin1String("/opt/tests/jollagallery/mediasources"), parent)
    {
    }
};

class TestDBusInterface : public DeclarativeDBusInterface
{
    Q_OBJECT
    Q_PROPERTY(QUrl removedFile READ removedFile WRITE removeFile NOTIFY removedFileChanged)
    Q_PROPERTY(bool allowRemove READ allowRemove WRITE setAllowRemove)
public:
    TestDBusInterface(QObject *parent = 0) : DeclarativeDBusInterface(parent), m_allowRemove(true) {}

    QUrl removedFile() const { return m_removedFile; }

    bool allowRemove() { return m_allowRemove; }
    void setAllowRemove(bool allow) { m_allowRemove = allow; }

    // Overwrite the function from DeclarativeDBusInterface.
    Q_INVOKABLE bool removeFile(const QUrl &url)
    {
        if (m_allowRemove) {
            m_removedFile = url;
            emit removedFileChanged();
        }
        return m_allowRemove;
    }

signals:
    void removedFileChanged();

private:
    QUrl m_removedFile;
    bool m_allowRemove;
};

class JollaGalleryPlugin : public QDeclarativeExtensionPlugin
{
public:
    virtual void registerTypes(const char *uri)
    {
        Q_ASSERT(QLatin1String(uri) == QLatin1String("com.jolla.gallery"));

        qDBusRegisterMetaType<QVector<QStringList> >();
        qDBusRegisterMetaType<TrackerChange>();
        qDBusRegisterMetaType<QVector<TrackerChange> >();

        qmlRegisterType<DeclarativeFileInfo>("com.jolla.gallery", 1, 0, "FileInfo");
        qmlRegisterType<DeclarativeMediaSource>("com.jolla.gallery", 1, 0, "MediaSource");
        qmlRegisterType<TestMediaModel>("com.jolla.gallery", 1, 0, "MediaSourceModel");
        qmlRegisterType<TestDBusInterface>("com.jolla.gallery", 1, 0, "DBusInterface");
        qmlRegisterType<DeclarativeWallpaper>("com.jolla.gallery", 1, 0, "Wallpaper");

        qmlRegisterType<TestDBusService>("com.jolla.gallery.test", 1, 0, "TestDBusService");

    }
};

#include "main.moc"

Q_EXPORT_PLUGIN2(jollagalleryplugin, JollaGalleryPlugin);
