/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (c) 2019 Open Mobile Platform LLC.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef QMOZSECURITY_H
#define QMOZSECURITY_H

#include <QObject>
#include <QSslCertificate>
#include <QVariantMap>

#define CERT_PROPERTY(TYPE, NAME) \
    Q_PROPERTY(TYPE NAME READ NAME NOTIFY serverCertificateChanged)

#define CERT_SLOT(TYPE, NAME) \
    TYPE NAME () const;

namespace mozilla {
namespace embedlite {
class EmbedLiteApp;
} // namespace embedlite
} // namespace mozilla

class QMozContext;

class QMozSecurity : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool allGood READ allGood NOTIFY allGoodChanged)
    Q_PROPERTY(QSslCertificate serverCertificate READ serverCertificate NOTIFY serverCertificateChanged)
    Q_PROPERTY(bool domainMismatch READ domainMismatch NOTIFY domainMismatchChanged)
    Q_PROPERTY(bool notValidAtThisTime READ notValidAtThisTime NOTIFY notValidAtThisTimeChanged)
    Q_PROPERTY(bool untrusted READ untrusted NOTIFY untrustedChanged)
    Q_PROPERTY(bool extendedValidation READ extendedValidation NOTIFY extendedValidationChanged)
    Q_PROPERTY(TLS_VERSION protocolVersion READ protocolVersion NOTIFY protocolVersionChanged)
    Q_PROPERTY(QString cipherName READ cipherName NOTIFY cipherNameChanged)

    // State properties
    Q_PROPERTY(bool validState READ validState NOTIFY validStateChanged)
    Q_PROPERTY(bool isInsecure READ isInsecure NOTIFY isInsecureChanged)
    Q_PROPERTY(bool isBroken READ isBroken NOTIFY isBrokenChanged)
    Q_PROPERTY(bool isSecure READ isSecure NOTIFY isSecureChanged)
    Q_PROPERTY(bool blockedMixedActiveContent READ blockedMixedActiveContent NOTIFY blockedMixedActiveContentChanged)
    Q_PROPERTY(bool loadedMixedActiveContent READ loadedMixedActiveContent NOTIFY loadedMixedActiveContentChanged)
    Q_PROPERTY(bool blockedMixedDisplayContent READ blockedMixedDisplayContent NOTIFY blockedMixedDisplayContentChanged)
    Q_PROPERTY(bool loadedMixedDisplayContent READ loadedMixedDisplayContent NOTIFY loadedMixedDisplayContentChanged)
    Q_PROPERTY(bool blockedTrackingContent READ blockedTrackingContent NOTIFY blockedTrackingContentChanged)
    Q_PROPERTY(bool identityEvToplevel READ identityEvToplevel NOTIFY identityEvToplevelChanged)
    Q_PROPERTY(bool usesSSL3 READ usesSSL3 NOTIFY usesSSL3Changed)
    Q_PROPERTY(bool usesWeakCrypto READ usesWeakCrypto NOTIFY usesWeakCryptoChanged)

    // Certificate properites
    CERT_PROPERTY(const QString, subjectDisplayName)
    CERT_PROPERTY(const QString, issuerDisplayName)
    CERT_PROPERTY(const QString, subjectOrganization)
    CERT_PROPERTY(const QDateTime, effectiveDate)
    CERT_PROPERTY(const QDateTime, expiryDate)
    CERT_PROPERTY(bool, certIsNull)
    Q_PROPERTY(QVariantMap serverCertDetails READ serverCertDetails NOTIFY serverCertificateChanged)

public:
    explicit QMozSecurity(QObject *parent = nullptr);
    explicit QMozSecurity(const char *aStatus, unsigned int aState, QObject *parent = nullptr);

    enum TLS_VERSION {
        SSL_VERSION_INVALID = -1,
        SSL_VERSION_3 = 0,
        TLS_VERSION_1 = 1,
        TLS_VERSION_1_1 = 2,
        TLS_VERSION_1_2 = 3
    };
    Q_ENUM(TLS_VERSION)

    void setSecurityRaw(const char *aStatus, unsigned int aState);
    void setSecurity(QString status, uint state);
    void reset();

Q_SIGNALS:
    void allGoodChanged();
    void domainMismatchChanged();
    void notValidAtThisTimeChanged();
    void untrustedChanged();
    void extendedValidationChanged();
    void protocolVersionChanged();
    void cipherNameChanged();

    // State property signals
    void validStateChanged();
    void isInsecureChanged();
    void isBrokenChanged();
    void isSecureChanged();
    void blockedMixedActiveContentChanged();
    void loadedMixedActiveContentChanged();
    void blockedMixedDisplayContentChanged();
    void loadedMixedDisplayContentChanged();
    void blockedTrackingContentChanged();
    void identityEvToplevelChanged();
    void usesSSL3Changed();
    void usesWeakCryptoChanged();

    // Certificate property signals
    void serverCertificateChanged();

public Q_SLOTS:
    bool allGood() const;
    const QSslCertificate serverCertificate() const;
    bool domainMismatch() const;
    bool notValidAtThisTime() const;
    bool untrusted() const;
    bool extendedValidation() const;
    TLS_VERSION protocolVersion() const;
    QString cipherName() const;

    // State properties
    bool validState() const;
    bool isInsecure() const;
    bool isBroken() const;
    bool isSecure() const;
    bool blockedMixedActiveContent() const;
    bool loadedMixedActiveContent() const;
    bool blockedMixedDisplayContent() const;
    bool loadedMixedDisplayContent() const;
    bool blockedTrackingContent() const;
    bool identityEvToplevel() const;
    bool usesSSL3() const;
    bool usesWeakCrypto() const;

    // Certificate properites
    CERT_SLOT(const QString, subjectDisplayName)
    CERT_SLOT(const QString, issuerDisplayName)
    CERT_SLOT(const QString, subjectOrganization)
    CERT_SLOT(const QDateTime, effectiveDate)
    CERT_SLOT(const QDateTime, expiryDate)
    CERT_SLOT(bool, certIsNull)

    QVariantMap serverCertDetails() const;

private:
    void resetState(QQueue<void(QMozSecurity::*)()> *emissions);
    void resetStatus(QQueue<void(QMozSecurity::*)()> *emissions);
    void importState(const char *aStatus, unsigned int aState);
    void sendEmissions(const QQueue<void(QMozSecurity::*)()> &emissions);

private:
    uint32_t m_state;
    QSslCertificate m_serverCertificate;

    bool m_domainMismatch;
    bool m_notValidAtThisTime;
    bool m_untrusted;
    bool m_extendedValidation;
    TLS_VERSION m_protocolVersion;
    QString m_cipherName;
};

#endif // QMOZSECURITY_H
