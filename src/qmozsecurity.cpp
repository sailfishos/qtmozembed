/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (c) 2019 Open Mobile Platform LLC.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <QQueue>

#include <nsIWebProgressListener.h>
#include <nsITransportSecurityInfo.h>
#include <nsIX509Cert.h>
#include <mozilla/embedlite/EmbedLiteApp.h>
#include <mozilla/embedlite/EmbedLiteSecurity.h>
#include <systemsettings/certificatemodel.h>

#include "qmozembedlog.h"
#include "qmozcontext.h"
#include "qmozsecurity.h"

// Ensure the enum values in QMozSecurity match the enum values in nsISSLStatus
static_assert((uint16_t)QMozSecurity::SSL_VERSION_3 == (uint16_t)nsITransportSecurityInfo::SSL_VERSION_3, "SSL/TLS version numbering mismatch: SSL_VERSION_3");
static_assert((uint16_t)QMozSecurity::TLS_VERSION_1 == (uint16_t)nsITransportSecurityInfo::TLS_VERSION_1, "SSL/TLS version numbering mismatch: TLS_VERSION_1");
static_assert((uint16_t)QMozSecurity::TLS_VERSION_1_1 == (uint16_t)nsITransportSecurityInfo::TLS_VERSION_1_1, "SSL/TLS version numbering mismatch: TLS_VERSION_1_1");
static_assert((uint16_t)QMozSecurity::TLS_VERSION_1_2 == (uint16_t)nsITransportSecurityInfo::TLS_VERSION_1_2, "SSL/TLS version numbering mismatch. TLS_VERSION_1_2");

#define STATUS_GETTER(METHODNAME, FLAGMASK) \
    bool QMozSecurity:: METHODNAME() const \
    { \
        return (m_state & nsIWebProgressListener:: FLAGMASK); \
    }

#define CERT_SLOT_BODY(TYPE, NAME, CALL) \
    TYPE QMozSecurity:: NAME () const \
    { \
        return m_serverCertificate.##CALL \
    } \

#define STATUS_EMISSION(METHODNAME, FLAGMASK) \
    if ((m_state & nsIWebProgressListener:: FLAGMASK) != (aState & nsIWebProgressListener:: FLAGMASK)) { \
        emissions << &QMozSecurity:: METHODNAME ## Changed; \
    }

QMozSecurity::QMozSecurity(QObject *parent)
    : QObject(parent)
{
    resetState(nullptr);
    resetStatus(nullptr);
}

QMozSecurity::QMozSecurity(const char *aStatus, unsigned int aState, QObject *parent)
    : QObject(parent)
{
    resetState(nullptr);
    resetStatus(nullptr);
    importState(aStatus, aState);
}

void QMozSecurity::reset()
{
    QQueue<void(QMozSecurity::*)()> emissions;
    bool allGood;

    allGood = this->allGood();

    resetState(&emissions);
    resetStatus(&emissions);

    if (allGood != this->allGood()) {
        emissions << &QMozSecurity::allGoodChanged;
    }
    sendEmissions(emissions);
}

void QMozSecurity::resetState(QQueue<void(QMozSecurity::*)()> *emissions)
{
    // Default to invalid state
    // Note this does not affect the status. Use resetStatus() for that
    if (m_state != 0 && emissions) {
        *emissions << &QMozSecurity::validStateChanged;
    }
    m_state = 0;
}

void QMozSecurity::resetStatus(QQueue<void(QMozSecurity::*)()> *emissions)
{
    // Default to least secure or invalid status
    // Note this does not affect the state. Use resetState() for that
    if (emissions) {
        if (!m_serverCertificate.isNull()) {
            *emissions << &QMozSecurity::serverCertificateChanged;
        }
        if (!m_domainMismatch) {
            *emissions << &QMozSecurity::domainMismatchChanged;
        }
        if (!m_notValidAtThisTime) {
            *emissions << &QMozSecurity::notValidAtThisTimeChanged;
        }
        if (!m_untrusted) {
            *emissions << &QMozSecurity::untrustedChanged;
        }
        if (m_extendedValidation) {
            *emissions << &QMozSecurity::extendedValidationChanged;
        }
        if (m_protocolVersion != SSL_VERSION_INVALID) {
            *emissions << &QMozSecurity::protocolVersionChanged;
        }
        if (!m_cipherName.isEmpty()) {
            *emissions << &QMozSecurity::cipherNameChanged;
        }
    }

    m_serverCertificate.clear();
    m_domainMismatch = true;
    m_notValidAtThisTime = true;
    m_untrusted = true;
    m_extendedValidation = false;
    m_protocolVersion = SSL_VERSION_INVALID;
    m_cipherName.clear();
}

bool QMozSecurity::allGood() const
{
    const static uint disallowed =
            nsIWebProgressListener::STATE_IS_INSECURE
            | nsIWebProgressListener::STATE_IS_BROKEN
            | nsIWebProgressListener::STATE_LOADED_MIXED_ACTIVE_CONTENT
            | nsIWebProgressListener::STATE_USES_WEAK_CRYPTO
            | nsIWebProgressListener::STATE_LOADED_MIXED_DISPLAY_CONTENT;
    const static uint required = nsIWebProgressListener::STATE_IS_SECURE;

    return !m_domainMismatch && !m_notValidAtThisTime && !m_untrusted
            && !subjectDisplayName().isEmpty()
            && (m_state & disallowed) == 0 && (m_state & required) == required;
}

const QSslCertificate QMozSecurity::serverCertificate() const
{
    return m_serverCertificate;
}

bool QMozSecurity::domainMismatch() const
{
    return m_domainMismatch;
}

bool QMozSecurity::notValidAtThisTime() const
{
    return m_notValidAtThisTime;
}

bool QMozSecurity::untrusted() const
{
    return m_untrusted;
}

bool QMozSecurity::extendedValidation() const
{
    return m_extendedValidation;

}

QMozSecurity::TLS_VERSION QMozSecurity::protocolVersion() const
{
    return m_protocolVersion;
}

QString QMozSecurity::cipherName() const
{
    return m_cipherName;
}

STATUS_GETTER(isInsecure, STATE_IS_INSECURE)
STATUS_GETTER(isBroken, STATE_IS_BROKEN)
STATUS_GETTER(isSecure, STATE_IS_SECURE)
STATUS_GETTER(blockedMixedActiveContent, STATE_BLOCKED_MIXED_ACTIVE_CONTENT)
STATUS_GETTER(loadedMixedActiveContent, STATE_LOADED_MIXED_ACTIVE_CONTENT)
STATUS_GETTER(blockedMixedDisplayContent, STATE_BLOCKED_MIXED_DISPLAY_CONTENT)
STATUS_GETTER(loadedMixedDisplayContent, STATE_LOADED_MIXED_DISPLAY_CONTENT)
STATUS_GETTER(blockedTrackingContent, STATE_BLOCKED_TRACKING_CONTENT)
STATUS_GETTER(identityEvToplevel, STATE_IDENTITY_EV_TOPLEVEL)
STATUS_GETTER(usesSSL3, STATE_USES_SSL_3)
STATUS_GETTER(usesWeakCrypto, STATE_USES_WEAK_CRYPTO)

void QMozSecurity::setSecurityRaw(const char *aStatus, unsigned int aState)
{
    importState(aStatus, aState);
}

void QMozSecurity::setSecurity(QString status, uint state)
{
    importState(status.toLocal8Bit().data(), state);
}

void QMozSecurity::importState(const char *aStatus, unsigned int aState)
{
    QQueue<void(QMozSecurity::*)()> emissions;
    bool allGood;
    mozilla::embedlite::EmbedLiteSecurity *embedSecurity = nullptr;
    QMozContext *context = QMozContext::instance();

    allGood = this->allGood();

    if (m_state != aState) {
        STATUS_EMISSION(isInsecure, STATE_IS_INSECURE)
        STATUS_EMISSION(isBroken, STATE_IS_BROKEN)
        STATUS_EMISSION(isSecure, STATE_IS_SECURE)
        STATUS_EMISSION(blockedMixedActiveContent, STATE_BLOCKED_MIXED_ACTIVE_CONTENT)
        STATUS_EMISSION(loadedMixedActiveContent, STATE_LOADED_MIXED_ACTIVE_CONTENT)
        STATUS_EMISSION(blockedMixedDisplayContent, STATE_BLOCKED_MIXED_DISPLAY_CONTENT)
        STATUS_EMISSION(loadedMixedDisplayContent, STATE_LOADED_MIXED_DISPLAY_CONTENT)
        STATUS_EMISSION(blockedTrackingContent, STATE_BLOCKED_TRACKING_CONTENT)
        STATUS_EMISSION(identityEvToplevel, STATE_IDENTITY_EV_TOPLEVEL)
        STATUS_EMISSION(usesSSL3, STATE_USES_SSL_3)
        STATUS_EMISSION(usesWeakCrypto, STATE_USES_WEAK_CRYPTO)

        if ((m_state == 0) != (aState == 0)) {
            emissions << &QMozSecurity::validStateChanged;
        }

        m_state = aState;
    }

    // Move implementation to the embedlite (JB#50947)
    // If the status is empty, leave it as it was
    if (aStatus && *aStatus && context) {
        embedSecurity = context->GetApp()->CreateSecurity(aStatus, aState);
    }
    else {
        if (!aStatus || !*aStatus) {
            qDebug() << "Security state could not be imported: empty status";
        }
        if (!context) {
            qDebug() << "Security state could not be imported: no app context";
        }
    }

    if (embedSecurity && embedSecurity->populated()) {
        if (m_domainMismatch != embedSecurity->domainMismatch()) {
            m_domainMismatch = embedSecurity->domainMismatch();
            emissions << &QMozSecurity::domainMismatchChanged;
        }

        QString cipherName = QString::fromStdString(embedSecurity->cipherName());
        if (m_cipherName != cipherName) {
            m_cipherName = cipherName;
            emissions << &QMozSecurity::cipherNameChanged;
        }

        if (m_notValidAtThisTime != embedSecurity->notValidAtThisTime()) {
            m_notValidAtThisTime = embedSecurity->notValidAtThisTime();
            emissions << &QMozSecurity::notValidAtThisTimeChanged;
        }

        if (m_untrusted != embedSecurity->untrusted()) {
            m_untrusted = embedSecurity->untrusted();
            emissions << &QMozSecurity::untrustedChanged;
        }

        if (m_extendedValidation != embedSecurity->extendedValidation()) {
            m_extendedValidation = embedSecurity->extendedValidation();
            emissions << &QMozSecurity::extendedValidationChanged;
        }

        if (m_protocolVersion != static_cast<TLS_VERSION>(embedSecurity->protocolVersion())) {
            m_protocolVersion = static_cast<TLS_VERSION>(embedSecurity->protocolVersion());
            emissions << &QMozSecurity::protocolVersionChanged;
        }

        QByteArray const rawDER(embedSecurity->rawDER().c_str(), embedSecurity->rawDER().length());
        QSslCertificate serverCertificate = QSslCertificate(rawDER, QSsl::EncodingFormat::Der);

        if (m_serverCertificate != serverCertificate) {
            m_serverCertificate = serverCertificate;
            emissions << &QMozSecurity::serverCertificateChanged;
        }
    }
    else {
        // There was a deserialisation error
        resetStatus(&emissions);
        qCDebug(lcEmbedLiteExt) << "Security state change: deserialisation failed";
    }

    if (allGood != this->allGood()) {
        emissions << &QMozSecurity::allGoodChanged;
    }

    if (embedSecurity) {
        context->GetApp()->DestroySecurity(embedSecurity);
    }

    sendEmissions(emissions);
}

const QString QMozSecurity::subjectDisplayName() const
{
    // Matches QSslCertificate::subjectDisplayName() introducd in Qt 5.12
    // Returns a name that describes the subject. It returns the
    // QSslCertificate::CommonName if available, otherwise falls back to the
    // first QSslCertificate::Organization or the first
    // QSslCertificate::OrganizationalUnitName.

    QStringList values;
    values = m_serverCertificate.subjectInfo(QSslCertificate::CommonName);
    if (!values.isEmpty()) {
        return values.constFirst();
    }

    values = m_serverCertificate.subjectInfo(QSslCertificate::Organization);
    if (!values.isEmpty()) {
        return values.constFirst();
    }

    values = m_serverCertificate.subjectInfo(QSslCertificate::OrganizationalUnitName);
    if (!values.isEmpty()) {
        return values.constFirst();
    }

    return QString();
}

const QString QMozSecurity::issuerDisplayName() const
{
    // Matches QSslCertificate::issuerDisplayName() introducd in Qt 5.12
    // Returns a name that describes the issuer. It returns the
    // QSslCertificate::CommonName if available, otherwise falls back to the
    // first QSslCertificate::Organization or the first
    // QSslCertificate::OrganizationalUnitName.

    QStringList values;
    values = m_serverCertificate.issuerInfo(QSslCertificate::CommonName);
    if (!values.isEmpty()) {
        return values.constFirst();
    }

    values = m_serverCertificate.issuerInfo(QSslCertificate::Organization);
    if (!values.isEmpty()) {
        return values.constFirst();
    }

    values = m_serverCertificate.issuerInfo(QSslCertificate::OrganizationalUnitName);
    if (!values.isEmpty()) {
        return values.constFirst();
    }

    return QString();
}

const QString QMozSecurity::subjectOrganization() const
{
    return m_serverCertificate.subjectInfo(QSslCertificate::Organization).value(0, QString());
}

const QDateTime QMozSecurity::effectiveDate() const
{
    return m_serverCertificate.effectiveDate();
}

const QDateTime QMozSecurity::expiryDate() const
{
    return m_serverCertificate.expiryDate();
}

QVariantMap QMozSecurity::serverCertDetails() const
{
    QList<Certificate> certList = CertificateModel::getCertificates(m_serverCertificate.toPem());

    if (certList.isEmpty()) {
        return QVariantMap();
    }

    return certList.first().details();
}

bool QMozSecurity::certIsNull() const
{
    return m_serverCertificate.isNull();
}

bool QMozSecurity::validState() const
{
    return m_state != 0;
}

void QMozSecurity::sendEmissions(const QQueue<void(QMozSecurity::*)()> &emissions)
{
    for (auto emission : emissions) {
        Q_EMIT (this->*emission)();
    }
}
