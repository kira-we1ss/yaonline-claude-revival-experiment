/*
 * omemomanager.h — XEP-0384 OMEMO per-account manager
 * Uses libsignal-protocol-c 2.3.3 for Double Ratchet / X3DH
 * Uses OpenSSL EVP AES-128-GCM for payload crypto
 */
#ifndef OMEMO_MANAGER_H
#define OMEMO_MANAGER_H

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QHash>
#include <QSet>
#include <QList>
#include <QPair>
#include <QDomDocument>
#include <QDomElement>

// Forward-declare libsignal types to avoid polluting headers
struct signal_context;
struct signal_protocol_store_context;

namespace XMPP {
class Jid;
class Client;
}

class PEPManager;

class OmemoManager : public QObject {
    Q_OBJECT
public:
    explicit OmemoManager(const QString& accountId, XMPP::Client* client, QObject* parent = nullptr);
    void setPepManager(PEPManager* pepManager);
    ~OmemoManager();

    // Returns true after initialize() completes successfully
    bool isInitialized() const;

    // Per-chat OMEMO toggle
    bool isEnabled(const XMPP::Jid& contact) const;
    void setEnabled(const XMPP::Jid& contact, bool enabled);

    // Own device id (uint32, random, persisted)
    uint32_t deviceId() const;

    // Publish own device list + bundle to PEP (call after login)
    void publishBundle();

    // Fetch contact's device list and bundles (async; emits sessionsEstablished when ready)
    void fetchContactBundles(const XMPP::Jid& contact);

    // Encrypt: async — emits encryptDone(to, encryptedElement, plainBody, success)
    void encrypt(const XMPP::Jid& to, const QString& body);

    // MUC encryption helpers
    // Returns true if at least one participant in the nickToRealJid map has an
    // established Signal session (i.e. we can encrypt for this MUC).
    bool hasMucSessions(const XMPP::Jid& roomJid,
                        const QHash<QString, XMPP::Jid>& nickToRealJid) const;

    // Encrypt body for all MUC participants that have sessions.
    // Emits encryptDone(roomJid, encrypted, plainBody, success) when done.
    void encryptForMuc(const XMPP::Jid& roomJid, const QString& body,
                       const QHash<QString, XMPP::Jid>& nickToRealJid);

    // Decrypt: async — emits decryptDone(from, plaintextBody, senderDeviceId, success)
    void decrypt(const XMPP::Jid& from, const QDomElement& encryptedElement);

    // Trust management
    enum TrustLevel { Untrusted = 0, TrustOnFirstUse = 1, Trusted = 2 };
    TrustLevel trustLevel(const XMPP::Jid& jid, uint32_t deviceId, const QByteArray& identityKey) const;
    void setTrusted(const XMPP::Jid& jid, uint32_t deviceId, const QByteArray& identityKey, TrustLevel level);
    QList<QPair<uint32_t, QByteArray>> trustedKeys(const XMPP::Jid& jid) const;

signals:
    void encryptDone(const XMPP::Jid& to, const QDomElement& encrypted, const QString& plainBody, bool success);
    void decryptDone(const XMPP::Jid& from, const QString& plainBody, uint32_t senderDeviceId, bool success);
    void sessionsEstablished(const XMPP::Jid& contact, bool success);
    void bundlePublished(bool success);

private:
    // Internal: publish the bundle data (signedPreKey, identityKey, prekeys).
    // Called from publishBundle() after the devicelist fetch/merge step.
    void publishOwnBundleData();

    class Private;
    Private* d;
};

#endif // OMEMO_MANAGER_H
