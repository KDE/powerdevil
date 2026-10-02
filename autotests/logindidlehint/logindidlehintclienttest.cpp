/*
 *   SPDX-FileCopyrightText: 2026 Méven Car <meven@kde.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "logindidlehintclient.h"

#include <QCoreApplication>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QSignalSpy>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using PowerDevil::BundledActions::LogindIdleHintClient;

// The Display property of a logind user: the id and the object path of its graphical session.
struct SessionRef {
    QString id;
    QDBusObjectPath path;
};
Q_DECLARE_METATYPE(SessionRef)

QDBusArgument &operator<<(QDBusArgument &argument, const SessionRef &session)
{
    argument.beginStructure();
    argument << session.id << session.path;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, SessionRef &session)
{
    argument.beginStructure();
    argument >> session.id >> session.path;
    argument.endStructure();
    return argument;
}

class FakeLogindUser : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.login1.User")
    Q_PROPERTY(SessionRef Display READ display)
public:
    SessionRef display() const
    {
        return m_display;
    }
    SessionRef m_display;
};

class FakeLogindSession : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.login1.Session")
public Q_SLOTS:
    void SetIdleHint(bool idle)
    {
        m_hints.append(idle);
        Q_EMIT idleHintSet();
    }
Q_SIGNALS:
    void idleHintSet();

public:
    QList<bool> m_hints;
};

class LogindIdleHintClientTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testSendsOnlyChanges();
    void testWaitsForTheSession();
    void testNoGraphicalSession();
    void testNoLogind();

private:
    // logind is served from a connection of its own, as it is a separate process for the client.
    QDBusConnection m_logindBus{QString()};
    QString m_service;
    FakeLogindUser *m_user = nullptr;
    FakeLogindSession *m_session = nullptr;
};

void LogindIdleHintClientTest::initTestCase()
{
    qDBusRegisterMetaType<SessionRef>();
    m_service = u"org.kde.powerdevil.test.login1.pid%1"_s.arg(QCoreApplication::applicationPid());
}

void LogindIdleHintClientTest::init()
{
    m_logindBus = QDBusConnection::connectToBus(QDBusConnection::SessionBus, u"fakelogind"_s);
    QVERIFY(m_logindBus.isConnected());
    m_user = new FakeLogindUser;
    m_user->m_display = {u"2"_s, QDBusObjectPath(u"/org/freedesktop/login1/session/_32"_s)};
    m_session = new FakeLogindSession;
    QVERIFY(m_logindBus.registerObject(u"/org/freedesktop/login1/user/self"_s, m_user, QDBusConnection::ExportAllProperties));
    QVERIFY(m_logindBus.registerObject(u"/org/freedesktop/login1/session/_32"_s, m_session, QDBusConnection::ExportAllSlots));
    QVERIFY(m_logindBus.registerService(m_service));
}

void LogindIdleHintClientTest::cleanup()
{
    m_logindBus.unregisterService(m_service);
    m_logindBus.unregisterObject(u"/org/freedesktop/login1/user/self"_s);
    m_logindBus.unregisterObject(u"/org/freedesktop/login1/session/_32"_s);
    QDBusConnection::disconnectFromBus(u"fakelogind"_s);
    delete m_user;
    delete m_session;
}

void LogindIdleHintClientTest::testSendsOnlyChanges()
{
    LogindIdleHintClient client(QDBusConnection::sessionBus(), m_service);
    // A hint left behind by an earlier PowerDevil is cleared first.
    QTRY_COMPARE(m_session->m_hints, QList<bool>({false}));

    client.setIdle(true);
    client.setIdle(true);
    QTRY_COMPARE(m_session->m_hints, QList<bool>({false, true}));
    client.setIdle(false);
    client.setIdle(false);
    client.setIdle(true);
    QTRY_COMPARE(m_session->m_hints, QList<bool>({false, true, false, true}));

    // Nothing more arrives.
    QTest::qWait(100); // UNAVOIDABLE: proving that no further call is made needs time for it to arrive
    QCOMPARE(m_session->m_hints, QList<bool>({false, true, false, true}));
}

void LogindIdleHintClientTest::testWaitsForTheSession()
{
    // Idle before logind answered which session it is: the hint goes out once the session is known.
    LogindIdleHintClient client(QDBusConnection::sessionBus(), m_service);
    client.setIdle(true);
    QTRY_COMPARE(m_session->m_hints, QList<bool>({true}));
}

void LogindIdleHintClientTest::testNoGraphicalSession()
{
    m_user->m_display = {QString(), QDBusObjectPath(u"/"_s)};
    LogindIdleHintClient client(QDBusConnection::sessionBus(), m_service);
    client.setIdle(true);
    QTest::qWait(200); // UNAVOIDABLE: proving that nothing is sent needs time for it to arrive
    QVERIFY(m_session->m_hints.isEmpty());
}

void LogindIdleHintClientTest::testNoLogind()
{
    LogindIdleHintClient client(QDBusConnection::sessionBus(), u"org.kde.powerdevil.test.nologind"_s);
    client.setIdle(true);
    client.setIdle(false);
    QTest::qWait(200); // UNAVOIDABLE: the failed lookup has to come back before the test ends
    QVERIFY(m_session->m_hints.isEmpty());
}

QTEST_GUILESS_MAIN(LogindIdleHintClientTest)

#include "logindidlehintclienttest.moc"
