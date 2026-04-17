/*
 * omemomanager.cpp — XEP-0384 OMEMO per-account manager
 *
 * Uses libsignal-protocol-c 2.3.3 (Double Ratchet / X3DH)
 * Uses OpenSSL EVP AES-128-GCM for message payload encryption
 * Uses QCA for HMAC-SHA256, SHA-512, AES-256-CBC (Signal crypto callbacks)
 * Uses QCA::Random for random bytes
 *
 * Storage: QSettings / JSON in
 *   QStandardPaths::AppDataLocation + "/omemo/<accountId>/"
 */

#include "omemomanager.h"
#include "xmpp_omemo.h"

#include <QDebug>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSettings>
#include <QStandardPaths>
#include <QRandomGenerator>
#include <QMutex>
#include <QTimer>
#include <QDateTime>
#include <QDomDocument>
#include <QSet>
#include <QTextStream>

#include <QtCrypto>

// OpenSSL AES-128-GCM, HMAC, SHA-512
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/err.h>

// libsignal-protocol-c
#include <signal/signal_protocol.h>
#include <signal/session_builder.h>
#include <signal/session_cipher.h>
#include <signal/key_helper.h>
#include <signal/session_pre_key.h>
#include <signal/protocol.h>
#include <signal/curve.h>

#include <xmpp_jid.h>
#include <xmpp_client.h>
#include <xmpp_tasks.h>
#include <xmpp_pubsubitem.h>
#include "pepmanager.h"

// ──────────────────────────────────────────────────────────────────
// Crypto callbacks for libsignal-protocol-c
// ──────────────────────────────────────────────────────────────────

static int omemo_random_func(uint8_t* data, size_t len, void* /*user_data*/)
{
    // Use OpenSSL RAND for cryptographically-secure randomness
    if (RAND_bytes(data, static_cast<int>(len)) != 1)
        return SG_ERR_UNKNOWN;
    return SG_SUCCESS;
}

// HMAC-SHA256 via OpenSSL (known-correct; replaced QCA which produced wrong
// output in libsignal context and caused -1000 on every X3DH verification).
static int omemo_hmac_sha256_init(void** hmac_context, const uint8_t* key, size_t key_len, void* /*user_data*/)
{
    HMAC_CTX* ctx = HMAC_CTX_new();
    if (!ctx)
        return SG_ERR_UNKNOWN;
    if (!HMAC_Init_ex(ctx, key, static_cast<int>(key_len), EVP_sha256(), nullptr)) {
        HMAC_CTX_free(ctx);
        return SG_ERR_UNKNOWN;
    }
    *hmac_context = ctx;
    return SG_SUCCESS;
}

static int omemo_hmac_sha256_update(void* hmac_context, const uint8_t* data, size_t data_len, void* /*user_data*/)
{
    HMAC_CTX* ctx = static_cast<HMAC_CTX*>(hmac_context);
    if (!HMAC_Update(ctx, data, data_len))
        return SG_ERR_UNKNOWN;
    return SG_SUCCESS;
}

static int omemo_hmac_sha256_final(void* hmac_context, signal_buffer** output, void* /*user_data*/)
{
    HMAC_CTX* ctx = static_cast<HMAC_CTX*>(hmac_context);
    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned int mdLen = 0;
    if (!HMAC_Final(ctx, md, &mdLen))
        return SG_ERR_UNKNOWN;
    *output = signal_buffer_create(md, static_cast<size_t>(mdLen));
    return SG_SUCCESS;
}

static void omemo_hmac_sha256_cleanup(void* hmac_context, void* /*user_data*/)
{
    HMAC_CTX_free(static_cast<HMAC_CTX*>(hmac_context));
}

// SHA-512 via OpenSSL EVP (same rationale as HMAC above).
static int omemo_sha512_digest_init(void** digest_context, void* /*user_data*/)
{
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx)
        return SG_ERR_UNKNOWN;
    if (EVP_DigestInit_ex(ctx, EVP_sha512(), nullptr) != 1) {
        EVP_MD_CTX_free(ctx);
        return SG_ERR_UNKNOWN;
    }
    *digest_context = ctx;
    return SG_SUCCESS;
}

static int omemo_sha512_digest_update(void* digest_context, const uint8_t* data, size_t data_len, void* /*user_data*/)
{
    EVP_MD_CTX* ctx = static_cast<EVP_MD_CTX*>(digest_context);
    if (EVP_DigestUpdate(ctx, data, data_len) != 1)
        return SG_ERR_UNKNOWN;
    return SG_SUCCESS;
}

static int omemo_sha512_digest_final(void* digest_context, signal_buffer** output, void* /*user_data*/)
{
    EVP_MD_CTX* ctx = static_cast<EVP_MD_CTX*>(digest_context);
    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned int mdLen = 0;
    if (EVP_DigestFinal_ex(ctx, md, &mdLen) != 1)
        return SG_ERR_UNKNOWN;
    *output = signal_buffer_create(md, static_cast<size_t>(mdLen));
    // libsignal may call update() again after final() — reinit for reuse
    EVP_DigestInit_ex(ctx, EVP_sha512(), nullptr);
    return SG_SUCCESS;
}

static void omemo_sha512_digest_cleanup(void* digest_context, void* /*user_data*/)
{
    EVP_MD_CTX_free(static_cast<EVP_MD_CTX*>(digest_context));
}

// Signal's crypto provider callback for symmetric encryption.
// libsignal calls this in two modes per signal_protocol.h:
//   SG_CIPHER_AES_CTR_NOPADDING (1): AES-CTR with no padding — used by the
//       Double Ratchet for per-message keys
//   SG_CIPHER_AES_CBC_PKCS5     (2): AES-CBC with PKCS5 padding — used by
//       other parts of libsignal
// Previously this callback ignored `cipher` and always used AES-CBC with
// implicit PKCS7 padding. When libsignal asked for AES-CTR (which it does
// during ratchet operations), we returned CBC ciphertext, causing
// session_builder_process_pre_key_bundle to fail with SG_ERR_UNKNOWN (-1000)
// because the internal signature verification used corrupted key material.
static const EVP_CIPHER* selectCipher(int cipher, size_t key_len)
{
    if (cipher == SG_CIPHER_AES_CTR_NOPADDING) {
        if (key_len == 32) return EVP_aes_256_ctr();
        if (key_len == 16) return EVP_aes_128_ctr();
    } else if (cipher == SG_CIPHER_AES_CBC_PKCS5) {
        if (key_len == 32) return EVP_aes_256_cbc();
        if (key_len == 16) return EVP_aes_128_cbc();
    }
    return nullptr;
}

static int omemo_encrypt(signal_buffer** output,
                         int cipher,
                         const uint8_t* key, size_t key_len,
                         const uint8_t* iv, size_t iv_len,
                         const uint8_t* plaintext, size_t plaintext_len,
                         void* /*user_data*/)
{
    Q_UNUSED(iv_len);

    const EVP_CIPHER* evp_cipher = selectCipher(cipher, key_len);
    if (!evp_cipher) {
        qWarning("[OMEMO] omemo_encrypt: unsupported cipher=%d key_len=%zu", cipher, key_len);
        return SG_ERR_UNKNOWN;
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        return SG_ERR_UNKNOWN;

    if (EVP_EncryptInit_ex(ctx, evp_cipher, nullptr,
                           reinterpret_cast<const unsigned char*>(key),
                           reinterpret_cast<const unsigned char*>(iv)) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return SG_ERR_UNKNOWN;
    }
    // CTR is a stream cipher with no padding; CBC uses PKCS7 (OpenSSL default).
    if (cipher == SG_CIPHER_AES_CTR_NOPADDING)
        EVP_CIPHER_CTX_set_padding(ctx, 0);

    QByteArray out(static_cast<int>(plaintext_len) + EVP_MAX_BLOCK_LENGTH, '\0');
    int len1 = 0, len2 = 0;
    if (EVP_EncryptUpdate(ctx, reinterpret_cast<unsigned char*>(out.data()), &len1,
                          reinterpret_cast<const unsigned char*>(plaintext),
                          static_cast<int>(plaintext_len)) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return SG_ERR_UNKNOWN;
    }
    if (EVP_EncryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(out.data()) + len1, &len2) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return SG_ERR_UNKNOWN;
    }
    EVP_CIPHER_CTX_free(ctx);

    *output = signal_buffer_create(reinterpret_cast<const uint8_t*>(out.constData()),
                                   static_cast<size_t>(len1 + len2));
    return SG_SUCCESS;
}

static int omemo_decrypt(signal_buffer** output,
                         int cipher,
                         const uint8_t* key, size_t key_len,
                         const uint8_t* iv, size_t iv_len,
                         const uint8_t* ciphertext, size_t ciphertext_len,
                         void* /*user_data*/)
{
    Q_UNUSED(iv_len);

    const EVP_CIPHER* evp_cipher = selectCipher(cipher, key_len);
    if (!evp_cipher) {
        qWarning("[OMEMO] omemo_decrypt: unsupported cipher=%d key_len=%zu", cipher, key_len);
        return SG_ERR_UNKNOWN;
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        return SG_ERR_UNKNOWN;

    if (EVP_DecryptInit_ex(ctx, evp_cipher, nullptr,
                           reinterpret_cast<const unsigned char*>(key),
                           reinterpret_cast<const unsigned char*>(iv)) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return SG_ERR_UNKNOWN;
    }
    if (cipher == SG_CIPHER_AES_CTR_NOPADDING)
        EVP_CIPHER_CTX_set_padding(ctx, 0);

    QByteArray out(static_cast<int>(ciphertext_len) + EVP_MAX_BLOCK_LENGTH, '\0');
    int len1 = 0, len2 = 0;
    if (EVP_DecryptUpdate(ctx, reinterpret_cast<unsigned char*>(out.data()), &len1,
                          reinterpret_cast<const unsigned char*>(ciphertext),
                          static_cast<int>(ciphertext_len)) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return SG_ERR_UNKNOWN;
    }
    if (EVP_DecryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(out.data()) + len1, &len2) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return SG_ERR_UNKNOWN;
    }
    EVP_CIPHER_CTX_free(ctx);

    *output = signal_buffer_create(reinterpret_cast<const uint8_t*>(out.constData()),
                                   static_cast<size_t>(len1 + len2));
    return SG_SUCCESS;
}

// ──────────────────────────────────────────────────────────────────
// AES-128-GCM helpers (OpenSSL EVP)
// ──────────────────────────────────────────────────────────────────

// Encrypts plaintext with AES-128-GCM.
// Returns ciphertext + 16-byte GCM auth tag concatenated.
// Encrypts plaintext with AES-128-GCM. Returns ciphertext only (no tag).
// The 16-byte GCM tag is returned via the tagOut parameter.
// This matches the Conversations OMEMO spec where <payload> carries
// ciphertext only and the tag is appended to the AES key for Signal-encryption.
static QByteArray aes128gcm_encrypt(const QByteArray& key, const QByteArray& iv,
                                     const QByteArray& plaintext, QByteArray& tagOut)
{
    Q_ASSERT(key.size() == 16);
    Q_ASSERT(iv.size() == 12);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        return {};

    if (EVP_EncryptInit_ex(ctx, EVP_aes_128_gcm(), nullptr, nullptr, nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv.size(), nullptr);
    if (EVP_EncryptInit_ex(ctx, nullptr, nullptr,
                           reinterpret_cast<const unsigned char*>(key.constData()),
                           reinterpret_cast<const unsigned char*>(iv.constData())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    QByteArray ciphertext(plaintext.size() + 16, '\0');
    int len = 0;
    if (EVP_EncryptUpdate(ctx, reinterpret_cast<unsigned char*>(ciphertext.data()), &len,
                          reinterpret_cast<const unsigned char*>(plaintext.constData()),
                          plaintext.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    int totalLen = len;
    if (EVP_EncryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(ciphertext.data()) + totalLen, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    totalLen += len;
    ciphertext.resize(totalLen);

    // Extract 16-byte GCM tag separately
    tagOut.resize(16);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16,
                        reinterpret_cast<unsigned char*>(tagOut.data()));
    EVP_CIPHER_CTX_free(ctx);

    return ciphertext;
}

// Decrypts ciphertext with AES-128-GCM using a separate tag.
// Matches Conversations OMEMO spec: tag comes from the Signal-encrypted
// key material, not from the <payload>.
static QByteArray aes128gcm_decrypt(const QByteArray& key, const QByteArray& iv,
                                     const QByteArray& ciphertext, const QByteArray& tag)
{
    Q_ASSERT(key.size() == 16);
    Q_ASSERT(iv.size() == 12);
    if (tag.size() != 16)
        return {};

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        return {};

    if (EVP_DecryptInit_ex(ctx, EVP_aes_128_gcm(), nullptr, nullptr, nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv.size(), nullptr);
    if (EVP_DecryptInit_ex(ctx, nullptr, nullptr,
                           reinterpret_cast<const unsigned char*>(key.constData()),
                           reinterpret_cast<const unsigned char*>(iv.constData())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    QByteArray plaintext(ciphertext.size(), '\0');
    int len = 0;
    if (EVP_DecryptUpdate(ctx, reinterpret_cast<unsigned char*>(plaintext.data()), &len,
                          reinterpret_cast<const unsigned char*>(ciphertext.constData()),
                          ciphertext.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    int totalLen = len;

    // Set expected tag
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16,
                        const_cast<char*>(tag.constData()));

    if (EVP_DecryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(plaintext.data()) + totalLen, &len) <= 0) {
        // Tag verification failed
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    totalLen += len;
    plaintext.resize(totalLen);
    EVP_CIPHER_CTX_free(ctx);

    return plaintext;
}

// ──────────────────────────────────────────────────────────────────
// Storage helpers
// ──────────────────────────────────────────────────────────────────

struct TrustEntry {
    uint32_t deviceId;
    QByteArray identityKey;
    OmemoManager::TrustLevel level;
};

// Forward declaration so IdentityContext can reference it
struct OmemoStore;

// Passed as user_data to the identity_key_store callbacks.
// Holds a pointer to the OmemoStore (for trusted-identity storage) AND
// a pointer to the live ratchet_identity_key_pair (needed by
// ik_get_identity_key_pair to serve the public/private EC key buffers
// that libsignal needs for X3DH during session_builder_process_pre_key_bundle).
struct IdentityContext {
    OmemoStore* store = nullptr;
    ratchet_identity_key_pair** identityKeyPairRef = nullptr; // ptr-to-ptr because Private::identityKeyPair may be set later
};

// ──────────────────────────────────────────────────────────────────
// Signal Protocol store callbacks
// ──────────────────────────────────────────────────────────────────

struct OmemoStore {
    QString dataDir;
    uint32_t localDeviceId = 0;
    signal_buffer* identityKeyPair = nullptr; // serialized ec_key_pair

    // Maps: "jid:deviceId" -> identity key bytes
    QHash<QString, QByteArray> trustedIdentities;

    // prekeys: id -> serialized session_pre_key_record
    QHash<uint32_t, QByteArray> preKeys;

    // signed prekey: id -> serialized signed_pre_key_record
    QHash<uint32_t, QByteArray> signedPreKeys;

    // sessions: "jid:deviceId" -> serialized session_record
    QHash<QString, QByteArray> sessions;

    // trust level: "jid:deviceId" -> int
    QHash<QString, int> trustLevels;

    void save() {
        // Save to JSON
        QJsonObject root;
        root[QLatin1String("deviceId")] = QString::number(localDeviceId);

        if (identityKeyPair) {
            QByteArray ikpBytes(reinterpret_cast<const char*>(signal_buffer_data(identityKeyPair)),
                                static_cast<int>(signal_buffer_len(identityKeyPair)));
            root[QLatin1String("identityKeyPair")] = QString::fromLatin1(ikpBytes.toBase64());
        }

        QJsonObject preKeysObj;
        for (auto it = preKeys.constBegin(); it != preKeys.constEnd(); ++it)
            preKeysObj[QString::number(it.key())] = QString::fromLatin1(it.value().toBase64());
        root[QLatin1String("preKeys")] = preKeysObj;

        QJsonObject signedPreKeysObj;
        for (auto it = signedPreKeys.constBegin(); it != signedPreKeys.constEnd(); ++it)
            signedPreKeysObj[QString::number(it.key())] = QString::fromLatin1(it.value().toBase64());
        root[QLatin1String("signedPreKeys")] = signedPreKeysObj;

        QJsonObject sessionsObj;
        for (auto it = sessions.constBegin(); it != sessions.constEnd(); ++it)
            sessionsObj[it.key()] = QString::fromLatin1(it.value().toBase64());
        root[QLatin1String("sessions")] = sessionsObj;

        QJsonObject trustObj;
        for (auto it = trustedIdentities.constBegin(); it != trustedIdentities.constEnd(); ++it)
            trustObj[it.key()] = QString::fromLatin1(it.value().toBase64());
        root[QLatin1String("trustedIdentities")] = trustObj;

        QJsonObject trustLevelsObj;
        for (auto it = trustLevels.constBegin(); it != trustLevels.constEnd(); ++it)
            trustLevelsObj[it.key()] = it.value();
        root[QLatin1String("trustLevels")] = trustLevelsObj;

        QDir dir(dataDir);
        dir.mkpath(QLatin1String("."));
        QFile f(dataDir + QLatin1String("/store.json"));
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            f.write(QJsonDocument(root).toJson());
        }
    }

    bool load() {
        QFile f(dataDir + QLatin1String("/store.json"));
        if (!f.open(QIODevice::ReadOnly))
            return false;

        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
        if (err.error != QJsonParseError::NoError)
            return false;

        QJsonObject root = doc.object();
        localDeviceId = root[QLatin1String("deviceId")].toString().toUInt();

        QString ikpB64 = root[QLatin1String("identityKeyPair")].toString();
        if (!ikpB64.isEmpty()) {
            QByteArray ikpBytes = QByteArray::fromBase64(ikpB64.toLatin1());
            identityKeyPair = signal_buffer_create(
                reinterpret_cast<const uint8_t*>(ikpBytes.constData()),
                static_cast<size_t>(ikpBytes.size()));
        }

        QJsonObject preKeysObj = root[QLatin1String("preKeys")].toObject();
        for (auto it = preKeysObj.constBegin(); it != preKeysObj.constEnd(); ++it)
            preKeys[it.key().toUInt()] = QByteArray::fromBase64(it.value().toString().toLatin1());

        QJsonObject signedPreKeysObj = root[QLatin1String("signedPreKeys")].toObject();
        for (auto it = signedPreKeysObj.constBegin(); it != signedPreKeysObj.constEnd(); ++it)
            signedPreKeys[it.key().toUInt()] = QByteArray::fromBase64(it.value().toString().toLatin1());

        QJsonObject sessionsObj = root[QLatin1String("sessions")].toObject();
        for (auto it = sessionsObj.constBegin(); it != sessionsObj.constEnd(); ++it)
            sessions[it.key()] = QByteArray::fromBase64(it.value().toString().toLatin1());

        QJsonObject trustObj = root[QLatin1String("trustedIdentities")].toObject();
        for (auto it = trustObj.constBegin(); it != trustObj.constEnd(); ++it)
            trustedIdentities[it.key()] = QByteArray::fromBase64(it.value().toString().toLatin1());

        QJsonObject trustLevelsObj = root[QLatin1String("trustLevels")].toObject();
        for (auto it = trustLevelsObj.constBegin(); it != trustLevelsObj.constEnd(); ++it)
            trustLevels[it.key()] = it.value().toInt();

        return localDeviceId != 0;
    }
};

// ──────────────────────────────────────────────────────────────────
// Signal Protocol store callback implementations
// ──────────────────────────────────────────────────────────────────

// Identity key store
// Called by libsignal during X3DH (session_builder_process_pre_key_bundle) to get
// our public+private identity EC keys. Without this libsignal returns SG_ERR_UNKNOWN
// and every outgoing session establishment fails with -1000.
static int ik_get_identity_key_pair(signal_buffer** public_data, signal_buffer** private_data, void* user_data)
{
    fprintf(stderr, "[OMEMO] >>> ik_get_identity_key_pair CALLED\n"); fflush(stderr);
    auto* ctx = static_cast<IdentityContext*>(user_data);
    if (!ctx || !ctx->identityKeyPairRef || !*ctx->identityKeyPairRef) {
        fprintf(stderr, "[OMEMO] ik_get_identity_key_pair: no identity key pair loaded (ctx=%p)\n", (void*)ctx); fflush(stderr);
        qWarning("[OMEMO] ik_get_identity_key_pair: no identity key pair loaded");
        return SG_ERR_UNKNOWN;
    }
    ratchet_identity_key_pair* ikp = *ctx->identityKeyPairRef;

    ec_public_key* pub = ratchet_identity_key_pair_get_public(ikp);
    ec_private_key* priv = ratchet_identity_key_pair_get_private(ikp);
    if (!pub || !priv) {
        qWarning("[OMEMO] ik_get_identity_key_pair: null pub/priv");
        return SG_ERR_UNKNOWN;
    }

    int r = ec_public_key_serialize(public_data, pub);
    if (r != SG_SUCCESS) {
        qWarning("[OMEMO] ec_public_key_serialize failed: %d", r);
        return r;
    }
    r = ec_private_key_serialize(private_data, priv);
    if (r != SG_SUCCESS) {
        qWarning("[OMEMO] ec_private_key_serialize failed: %d", r);
        signal_buffer_free(*public_data);
        *public_data = nullptr;
        return r;
    }
    return SG_SUCCESS;
}

static int ik_get_local_registration_id(void* user_data, uint32_t* registration_id)
{
    auto* ctx = static_cast<IdentityContext*>(user_data);
    *registration_id = ctx->store->localDeviceId;
    return SG_SUCCESS;
}

static int ik_save_identity(const signal_protocol_address* address,
                            uint8_t* key_data, size_t key_len, void* user_data)
{
    auto* ctx = static_cast<IdentityContext*>(user_data);
    QString key = QString::fromUtf8(address->name, static_cast<int>(address->name_len))
                  + QLatin1Char(':') + QString::number(address->device_id);
    ctx->store->trustedIdentities[key] = QByteArray(reinterpret_cast<const char*>(key_data),
                                                     static_cast<int>(key_len));
    ctx->store->save();
    return SG_SUCCESS;
}

static int ik_is_trusted_identity(const signal_protocol_address* address,
                                  uint8_t* key_data, size_t key_len, void* user_data)
{
    fprintf(stderr, "[OMEMO] >>> ik_is_trusted_identity CALLED for %.*s:%d (key_len=%zu)\n",
        (int)address->name_len, address->name, address->device_id, key_len);
    fflush(stderr);
    auto* ctx = static_cast<IdentityContext*>(user_data);
    OmemoStore* store = ctx->store;
    QString key = QString::fromUtf8(address->name, static_cast<int>(address->name_len))
                  + QLatin1Char(':') + QString::number(address->device_id);

    if (!store->trustedIdentities.contains(key)) {
        // TOFU: trust on first use — save and trust
        store->trustedIdentities[key] = QByteArray(reinterpret_cast<const char*>(key_data),
                                                   static_cast<int>(key_len));
        store->trustLevels[key] = OmemoManager::TrustOnFirstUse;
        store->save();
        return 1; // trusted
    }

    int level = store->trustLevels.value(key, OmemoManager::TrustOnFirstUse);
    if (level == OmemoManager::Untrusted)
        return 0;

    // Verify key matches what we stored
    QByteArray stored = store->trustedIdentities[key];
    QByteArray incoming(reinterpret_cast<const char*>(key_data), static_cast<int>(key_len));
    return (stored == incoming) ? 1 : 0;
}

// Pre-key store
static int pk_load_pre_key(signal_buffer** record, uint32_t pre_key_id, void* user_data)
{
    auto* store = static_cast<OmemoStore*>(user_data);
    if (!store->preKeys.contains(pre_key_id))
        return SG_ERR_INVALID_KEY_ID;
    QByteArray d = store->preKeys[pre_key_id];
    *record = signal_buffer_create(reinterpret_cast<const uint8_t*>(d.constData()),
                                   static_cast<size_t>(d.size()));
    return SG_SUCCESS;
}

static int pk_store_pre_key(uint32_t pre_key_id, uint8_t* record, size_t record_len, void* user_data)
{
    auto* store = static_cast<OmemoStore*>(user_data);
    store->preKeys[pre_key_id] = QByteArray(reinterpret_cast<const char*>(record),
                                            static_cast<int>(record_len));
    store->save();
    return SG_SUCCESS;
}

static int pk_contains_pre_key(uint32_t pre_key_id, void* user_data)
{
    auto* store = static_cast<OmemoStore*>(user_data);
    return store->preKeys.contains(pre_key_id) ? 1 : 0;
}

static int pk_remove_pre_key(uint32_t pre_key_id, void* user_data)
{
    auto* store = static_cast<OmemoStore*>(user_data);
    store->preKeys.remove(pre_key_id);
    store->save();
    return SG_SUCCESS;
}

// Signed pre-key store
static int spk_load_signed_pre_key(signal_buffer** record, uint32_t signed_pre_key_id, void* user_data)
{
    auto* store = static_cast<OmemoStore*>(user_data);
    if (!store->signedPreKeys.contains(signed_pre_key_id))
        return SG_ERR_INVALID_KEY_ID;
    QByteArray d = store->signedPreKeys[signed_pre_key_id];
    *record = signal_buffer_create(reinterpret_cast<const uint8_t*>(d.constData()),
                                   static_cast<size_t>(d.size()));
    return SG_SUCCESS;
}

static int spk_store_signed_pre_key(uint32_t signed_pre_key_id, uint8_t* record, size_t record_len, void* user_data)
{
    auto* store = static_cast<OmemoStore*>(user_data);
    store->signedPreKeys[signed_pre_key_id] = QByteArray(reinterpret_cast<const char*>(record),
                                                          static_cast<int>(record_len));
    store->save();
    return SG_SUCCESS;
}

static int spk_contains_signed_pre_key(uint32_t signed_pre_key_id, void* user_data)
{
    auto* store = static_cast<OmemoStore*>(user_data);
    return store->signedPreKeys.contains(signed_pre_key_id) ? 1 : 0;
}

static int spk_remove_signed_pre_key(uint32_t signed_pre_key_id, void* user_data)
{
    auto* store = static_cast<OmemoStore*>(user_data);
    store->signedPreKeys.remove(signed_pre_key_id);
    store->save();
    return SG_SUCCESS;
}

// Session store
// Per libsignal API contract (signal_protocol.h):
//   Returns 1 if the session was loaded, 0 if the session was not found,
//   negative on failure. Previously we returned SG_ERR_UNKNOWN (-1000) on
//   "not found", which libsignal treats as a hard failure, short-circuiting
//   session_builder_process_pre_key_bundle with -1000 — THIS was the root
//   cause of "still TLS-only, never encrypts" after every other layer was fixed.
static int sess_load_session(signal_buffer** record, signal_buffer** user_record,
                             const signal_protocol_address* address, void* user_data)
{
    Q_UNUSED(user_record);
    auto* store = static_cast<OmemoStore*>(user_data);
    QString key = QString::fromUtf8(address->name, static_cast<int>(address->name_len))
                  + QLatin1Char(':') + QString::number(address->device_id);
    if (!store->sessions.contains(key))
        return 0; // not found — NOT an error
    QByteArray d = store->sessions[key];
    *record = signal_buffer_create(reinterpret_cast<const uint8_t*>(d.constData()),
                                   static_cast<size_t>(d.size()));
    return 1; // loaded
}

static int sess_get_sub_device_sessions(signal_int_list** sessions,
                                        const char* name, size_t name_len, void* user_data)
{
    auto* store = static_cast<OmemoStore*>(user_data);
    QString prefix = QString::fromUtf8(name, static_cast<int>(name_len)) + QLatin1Char(':');
    signal_int_list* list = signal_int_list_alloc();
    for (auto it = store->sessions.constBegin(); it != store->sessions.constEnd(); ++it) {
        if (it.key().startsWith(prefix)) {
            QString devIdStr = it.key().mid(prefix.length());
            signal_int_list_push_back(list, devIdStr.toInt());
        }
    }
    *sessions = list;
    return SG_SUCCESS;
}

static int sess_store_session(const signal_protocol_address* address,
                              uint8_t* record, size_t record_len,
                              uint8_t* /*user_record*/, size_t /*user_record_len*/,
                              void* user_data)
{
    auto* store = static_cast<OmemoStore*>(user_data);
    QString key = QString::fromUtf8(address->name, static_cast<int>(address->name_len))
                  + QLatin1Char(':') + QString::number(address->device_id);
    store->sessions[key] = QByteArray(reinterpret_cast<const char*>(record),
                                      static_cast<int>(record_len));
    store->save();
    return SG_SUCCESS;
}

static int sess_contains_session(const signal_protocol_address* address, void* user_data)
{
    auto* store = static_cast<OmemoStore*>(user_data);
    QString key = QString::fromUtf8(address->name, static_cast<int>(address->name_len))
                  + QLatin1Char(':') + QString::number(address->device_id);
    return store->sessions.contains(key) ? 1 : 0;
}

static int sess_delete_session(const signal_protocol_address* address, void* user_data)
{
    auto* store = static_cast<OmemoStore*>(user_data);
    QString key = QString::fromUtf8(address->name, static_cast<int>(address->name_len))
                  + QLatin1Char(':') + QString::number(address->device_id);
    store->sessions.remove(key);
    store->save();
    return SG_SUCCESS;
}

static int sess_delete_all_sessions(const char* name, size_t name_len, void* user_data)
{
    auto* store = static_cast<OmemoStore*>(user_data);
    QString prefix = QString::fromUtf8(name, static_cast<int>(name_len)) + QLatin1Char(':');
    for (auto it = store->sessions.begin(); it != store->sessions.end(); ) {
        if (it.key().startsWith(prefix))
            it = store->sessions.erase(it);
        else
            ++it;
    }
    store->save();
    return SG_SUCCESS;
}

// ──────────────────────────────────────────────────────────────────
// OmemoManager::Private
// ──────────────────────────────────────────────────────────────────

class OmemoManager::Private {
public:
    QString accountId;
    XMPP::Client* client = nullptr;
    PEPManager* pepManager = nullptr;
    bool initialized = false;

    OmemoStore store;

    signal_context* signalCtx = nullptr;
    signal_protocol_store_context* storeCtx = nullptr;

    // Identity key pair (kept in memory after init).
    // ikContext.identityKeyPairRef points to this member so the callback
    // sees updates (e.g. regeneration) without re-registering the store.
    ratchet_identity_key_pair* identityKeyPair = nullptr;
    IdentityContext ikContext;
    // Signed prekey
    session_signed_pre_key* signedPreKey = nullptr;
    // One-time prekeys
    session_pre_key_bundle* currentBundle = nullptr;

    // Per-JID enabled state
    QHash<QString, bool> enabledMap;

    // Contact device lists: jid -> list of device ids
    QHash<QString, QList<uint32_t>> deviceLists;

    // Debounce: timestamp (ms since epoch, QDateTime::currentMSecsSinceEpoch)
    // of the last publishBundle() dispatch. We ignore any publishBundle()
    // call that arrives within kPublishBundleDebounceMs of the last one.
    // Prevents infinite loops when another misbehaving client keeps
    // overwriting our devicelist on every publish (each overwrite echoes
    // back as a PEP push; we used to republish → server echoes → republish
    // ...). Also rate-limits human-triggered reconnects.
    qint64 lastPublishBundleMs = 0;
    static constexpr qint64 kPublishBundleDebounceMs = 10 * 1000; // 10s

    // MUC own-echo plaintext cache. In MUC rooms libsignal refuses
    // self-sessions, so we can't decrypt our own echoed MUC messages
    // via the normal Signal path. Before sending a MUC-OMEMO message,
    // encryptForMuc() stashes {iv → plaintext}. When the MUC server
    // echoes our own ciphertext back to us, the decrypt() path consults
    // this cache using the incoming stanza's iv; if it matches, we emit
    // the cached plaintext as a successful decryption result.
    //
    // The iv is 12 bytes of crypto RNG per send — more than enough
    // uniqueness for a session-lifetime cache, and leaks nothing
    // sensitive if it ages out. We cap to kMucEchoCacheMax entries
    // (LIFO evict) so the hash can't grow unbounded in a busy room.
    //
    // PERSISTED: saved to muc_echo.json in dataDir on every write and
    // reloaded on OmemoManager init so that after a restart the MUC
    // room history replay (which arrives as [This message is OMEMO
    // encrypted] ciphertext with our iv) can still be resolved to
    // plaintext. Without persistence every restart turns your own MUC
    // messages into opaque placeholders forever.
    QHash<QByteArray, QString> mucEchoPlaintext;
    QList<QByteArray> mucEchoIvOrder; // insertion order for LIFO eviction
    static constexpr int kMucEchoCacheMax = 2048; // ~a few months of chat

    void saveMucEchoCache();
    void loadMucEchoCache();

    QString dataDir;

    ~Private() {
        if (identityKeyPair)
            SIGNAL_UNREF(identityKeyPair);
        if (signedPreKey)
            SIGNAL_UNREF(signedPreKey);
        if (storeCtx)
            signal_protocol_store_context_destroy(storeCtx);
        if (signalCtx)
            signal_context_destroy(signalCtx);
    }

    bool initialize();
    bool generateKeysIfNeeded();
    void setupStoreContext();
};

static void signalLogFunc(int level, const char* message, size_t len, void* /*user_data*/)
{
    qDebug("[OMEMO/signal L%d] %.*s", level, static_cast<int>(len), message);
    fprintf(stderr, "[OMEMO/signal L%d] %.*s\n", level, static_cast<int>(len), message);
    fflush(stderr);
}

bool OmemoManager::Private::initialize()
{
    dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
              + QLatin1String("/omemo/") + accountId;

    QDir().mkpath(dataDir);
    store.dataDir = dataDir;

    // Try to load existing store
    bool loaded = store.load();

    // MUC own-echo plaintext cache — persists across restarts so that when
    // the MUC room replays history on re-join, we can resolve our own
    // ciphertext echoes back to their original plaintext (libsignal can't
    // decrypt them since self-sessions aren't allowed).
    loadMucEchoCache();

    // Set up signal context
    signal_context_create(&signalCtx, this);

    signal_crypto_provider crypto = {};
    crypto.random_func = omemo_random_func;
    crypto.hmac_sha256_init_func = omemo_hmac_sha256_init;
    crypto.hmac_sha256_update_func = omemo_hmac_sha256_update;
    crypto.hmac_sha256_final_func = omemo_hmac_sha256_final;
    crypto.hmac_sha256_cleanup_func = omemo_hmac_sha256_cleanup;
    crypto.sha512_digest_init_func = omemo_sha512_digest_init;
    crypto.sha512_digest_update_func = omemo_sha512_digest_update;
    crypto.sha512_digest_final_func = omemo_sha512_digest_final;
    crypto.sha512_digest_cleanup_func = omemo_sha512_digest_cleanup;
    crypto.encrypt_func = omemo_encrypt;
    crypto.decrypt_func = omemo_decrypt;
    signal_context_set_crypto_provider(signalCtx, &crypto);
    signal_context_set_log_function(signalCtx, signalLogFunc);

    // Set up store context
    setupStoreContext();

    // Generate keys if this is a new install
    if (!loaded || store.localDeviceId == 0) {
        qDebug() << "[OMEMO] Generating new identity keys for account" << accountId;
        if (!generateKeysIfNeeded())
            return false;
    } else {
        // Reload identity key pair from stored bytes
        if (store.identityKeyPair) {
            ratchet_identity_key_pair* ikp = nullptr;
            if (ratchet_identity_key_pair_deserialize(&ikp,
                    signal_buffer_data(store.identityKeyPair),
                    signal_buffer_len(store.identityKeyPair),
                    signalCtx) == SG_SUCCESS) {
                identityKeyPair = ikp;  // keep ratchet_identity_key_pair alive
            } else {
                qWarning() << "[OMEMO] Failed to deserialize stored identity key pair, regenerating";
                if (!generateKeysIfNeeded())
                    return false;
            }
        }
    }

    initialized = true;
    qDebug() << "[OMEMO] Initialized. Device ID:" << store.localDeviceId;
    return true;
}

void OmemoManager::Private::saveMucEchoCache()
{
    if (dataDir.isEmpty()) return;

    QJsonArray arr;
    // Persist in insertion order so a future LIFO-evict replay is stable.
    for (const QByteArray& iv : mucEchoIvOrder) {
        auto it = mucEchoPlaintext.constFind(iv);
        if (it == mucEchoPlaintext.constEnd()) continue;
        QJsonObject o;
        o[QLatin1String("iv")]   = QString::fromLatin1(iv.toBase64());
        o[QLatin1String("body")] = it.value();
        arr.append(o);
    }

    QFile f(dataDir + QLatin1String("/muc_echo.json"));
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

void OmemoManager::Private::loadMucEchoCache()
{
    if (dataDir.isEmpty()) return;

    QFile f(dataDir + QLatin1String("/muc_echo.json"));
    if (!f.open(QIODevice::ReadOnly))
        return;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray())
        return;

    mucEchoPlaintext.clear();
    mucEchoIvOrder.clear();
    for (const QJsonValue& v : doc.array()) {
        QJsonObject o = v.toObject();
        QByteArray iv = QByteArray::fromBase64(
            o.value(QLatin1String("iv")).toString().toLatin1());
        QString body = o.value(QLatin1String("body")).toString();
        if (iv.isEmpty()) continue;
        mucEchoPlaintext.insert(iv, body);
        mucEchoIvOrder.append(iv);
    }
    qDebug() << "[OMEMO] Loaded" << mucEchoPlaintext.size()
             << "MUC echo plaintext entries from disk";
}

bool OmemoManager::Private::generateKeysIfNeeded()
{
    // Generate identity key pair
    ratchet_identity_key_pair* ikp = nullptr;
    if (signal_protocol_key_helper_generate_identity_key_pair(&ikp, signalCtx) != SG_SUCCESS) {
        qWarning() << "[OMEMO] Failed to generate identity key pair";
        return false;
    }
    // Store the ratchet_identity_key_pair directly (frees old one if any)
    if (identityKeyPair)
        SIGNAL_UNREF(identityKeyPair);
    identityKeyPair = ikp;

    // Serialize and store ikp
    signal_buffer* ikpBuf = nullptr;
    ratchet_identity_key_pair_serialize(&ikpBuf, ikp);
    if (store.identityKeyPair)
        signal_buffer_free(store.identityKeyPair);
    store.identityKeyPair = ikpBuf;

    // Generate registration ID (device ID)
    uint32_t regId = 0;
    signal_protocol_key_helper_generate_registration_id(&regId, 0, signalCtx);
    store.localDeviceId = regId;

    // Generate signed prekey
    uint32_t signedPreKeyId = 1;
    session_signed_pre_key* spk = nullptr;
    if (signal_protocol_key_helper_generate_signed_pre_key(
            &spk, ikp, signedPreKeyId,
            static_cast<uint64_t>(QDateTime::currentMSecsSinceEpoch()),
            signalCtx) != SG_SUCCESS) {
        qWarning() << "[OMEMO] Failed to generate signed pre key";
        return false;
    }

    // Serialize and save signed prekey
    signal_buffer* spkBuf = nullptr;
    session_signed_pre_key_serialize(&spkBuf, spk);
    store.signedPreKeys[signedPreKeyId] = QByteArray(
        reinterpret_cast<const char*>(signal_buffer_data(spkBuf)),
        static_cast<int>(signal_buffer_len(spkBuf)));
    signal_buffer_free(spkBuf);
    SIGNAL_UNREF(spk);

    // Generate 100 one-time prekeys starting at ID 1
    signal_protocol_key_helper_pre_key_list_node* preKeyList = nullptr;
    signal_protocol_key_helper_generate_pre_keys(&preKeyList, 1, 100, signalCtx);

    for (auto* node = preKeyList; node; node = signal_protocol_key_helper_key_list_next(node)) {
        session_pre_key* pk = signal_protocol_key_helper_key_list_element(node);
        uint32_t pkId = session_pre_key_get_id(pk);
        signal_buffer* pkBuf = nullptr;
        session_pre_key_serialize(&pkBuf, pk);
        store.preKeys[pkId] = QByteArray(
            reinterpret_cast<const char*>(signal_buffer_data(pkBuf)),
            static_cast<int>(signal_buffer_len(pkBuf)));
        signal_buffer_free(pkBuf);
    }
    signal_protocol_key_helper_key_list_free(preKeyList);

    store.save();
    qDebug() << "[OMEMO] Generated keys. Device ID:" << store.localDeviceId;
    return true;
}

void OmemoManager::Private::setupStoreContext()
{
    signal_protocol_store_context_create(&storeCtx, signalCtx);

    // Identity key store — user_data is an IdentityContext so the callbacks
    // can access both the OmemoStore (for trust) and the live
    // ratchet_identity_key_pair (for X3DH in get_identity_key_pair).
    ikContext.store = &store;
    ikContext.identityKeyPairRef = &identityKeyPair;
    signal_protocol_identity_key_store ikStore = {};
    ikStore.get_identity_key_pair = ik_get_identity_key_pair;
    ikStore.get_local_registration_id = ik_get_local_registration_id;
    ikStore.save_identity = ik_save_identity;
    ikStore.is_trusted_identity = ik_is_trusted_identity;
    ikStore.destroy_func = nullptr;
    ikStore.user_data = &ikContext;
    signal_protocol_store_context_set_identity_key_store(storeCtx, &ikStore);

    // Pre-key store
    signal_protocol_pre_key_store pkStore = {};
    pkStore.load_pre_key = pk_load_pre_key;
    pkStore.store_pre_key = pk_store_pre_key;
    pkStore.contains_pre_key = pk_contains_pre_key;
    pkStore.remove_pre_key = pk_remove_pre_key;
    pkStore.destroy_func = nullptr;
    pkStore.user_data = &store;
    signal_protocol_store_context_set_pre_key_store(storeCtx, &pkStore);

    // Signed pre-key store
    signal_protocol_signed_pre_key_store spkStore = {};
    spkStore.load_signed_pre_key = spk_load_signed_pre_key;
    spkStore.store_signed_pre_key = spk_store_signed_pre_key;
    spkStore.contains_signed_pre_key = spk_contains_signed_pre_key;
    spkStore.remove_signed_pre_key = spk_remove_signed_pre_key;
    spkStore.destroy_func = nullptr;
    spkStore.user_data = &store;
    signal_protocol_store_context_set_signed_pre_key_store(storeCtx, &spkStore);

    // Session store
    signal_protocol_session_store sessStore = {};
    sessStore.load_session_func = sess_load_session;
    sessStore.get_sub_device_sessions_func = sess_get_sub_device_sessions;
    sessStore.store_session_func = sess_store_session;
    sessStore.contains_session_func = sess_contains_session;
    sessStore.delete_session_func = sess_delete_session;
    sessStore.delete_all_sessions_func = sess_delete_all_sessions;
    sessStore.destroy_func = nullptr;
    sessStore.user_data = &store;
    signal_protocol_store_context_set_session_store(storeCtx, &sessStore);
}

// ──────────────────────────────────────────────────────────────────
// OmemoManager public interface
// ──────────────────────────────────────────────────────────────────

OmemoManager::OmemoManager(const QString& accountId, XMPP::Client* client, QObject* parent)
    : QObject(parent)
    , d(new Private)
{
    d->accountId = accountId;
    d->client = client;

    // Initialize asynchronously to avoid blocking the constructor
    QTimer::singleShot(0, this, [this]() {
        if (!d->initialize()) {
            qWarning() << "[OMEMO] Initialization failed for account" << d->accountId;
        }
    });
}

OmemoManager::~OmemoManager()
{
    delete d;
}

bool OmemoManager::isInitialized() const
{
    return d->initialized;
}

bool OmemoManager::isEnabled(const XMPP::Jid& contact) const
{
    return d->enabledMap.value(contact.bare(), false);
}

void OmemoManager::setEnabled(const XMPP::Jid& contact, bool enabled)
{
    d->enabledMap[contact.bare()] = enabled;
}

uint32_t OmemoManager::deviceId() const
{
    return d->store.localDeviceId;
}

void OmemoManager::setPepManager(PEPManager* pepManager)
{
    d->pepManager = pepManager;

    // Listen for devicelist PEP notifications so that when a peer (or our
    // own other device) republishes its devicelist, we update our cache and
    // can encrypt for the right devices on the next send. Without this,
    // adding a new device on the Android companion (or a contact installing
    // a new client) requires an app restart to pick up their new device id.
    connect(d->pepManager, &PEPManager::itemPublished,
        this, [this](const XMPP::Jid& from, const QString& node,
                      const XMPP::PubSubItem& item) {
            if (node != QString::fromLatin1(XmppOmemo::NS_DEVICELIST))
                return;

            QDomElement listEl = item.payload();
            if (listEl.isNull()) return;

            QList<uint32_t> devices;
            QDomElement devEl = listEl.firstChildElement(QLatin1String("device"));
            while (!devEl.isNull()) {
                bool ok = false;
                uint32_t devId = devEl.attribute(QLatin1String("id")).toUInt(&ok);
                if (ok && devId != 0) devices.append(devId);
                devEl = devEl.nextSiblingElement(QLatin1String("device"));
            }

            QString bareFrom = from.bare();
            qDebug() << "[OMEMO] PEP devicelist update from" << bareFrom
                     << "→" << devices.size() << "devices:" << devices;
            d->deviceLists[bareFrom] = devices;

            // Intentionally DO NOT auto-republish if our id is missing from
            // our own devicelist here — that path used to run and caused an
            // infinite republish loop in real life:
            //   [publishBundle → merge → publish 6 devices]
            //     ↓ fetchContactBundles(ownJid) → get(own,devicelist)
            //     ↓ itemPublished fires with server's pre-commit state (5 dev)
            //     ↓ this handler fires → "missing, republish" → goto 1
            // A cold-start / restart-triggered merge happens exactly once in
            // publishBundle()'s continuation. Drifting "missing" states are
            // fixed on the next publishBundle (e.g. next app launch).
            //
            // We still update the deviceLists cache above so encrypt() sees
            // peer device changes in real time.
            Q_UNUSED(bareFrom);
        });
}

void OmemoManager::publishBundle()
{
    if (!d->initialized) {
        qWarning() << "[OMEMO] publishBundle called before initialization";
        emit bundlePublished(false);
        return;
    }

    if (!d->pepManager) {
        qWarning() << "[OMEMO] publishBundle: no PEPManager available";
        emit bundlePublished(false);
        return;
    }

    // Debounce: reject republish-storms triggered by misbehaving peer clients
    // that keep overwriting our devicelist without merging. Each overwrite
    // generates a PEP push; without this gate we'd re-publish instantly,
    // the peer would re-overwrite, and we'd melt the CPU + flood the network.
    qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (d->lastPublishBundleMs != 0
        && nowMs - d->lastPublishBundleMs < Private::kPublishBundleDebounceMs) {
        qDebug() << "[OMEMO] publishBundle: debounced ("
                 << (nowMs - d->lastPublishBundleMs) << "ms since last publish)";
        emit bundlePublished(true); // caller treated as success; we already published recently
        return;
    }
    d->lastPublishBundleMs = nowMs;

    uint32_t myDevId = d->store.localDeviceId;

    // CRITICAL: Must FETCH the existing devicelist first, then MERGE our device
    // into it, THEN publish. Otherwise we overwrite Conversations/Cheogram's
    // own device entries and the user's other clients lose their addressing
    // (symptom: "Message was not encrypted for this device." on their phone
    // for every message WE send, because senders no longer see their device id).
    //
    // Publish the bundle immediately (it's keyed by our device id, no conflict),
    // then fetch devicelist → merge → republish in a continuation.
    publishOwnBundleData();

    // Fetch-merge-publish devicelist
    XMPP::Jid ownJid(d->accountId);
    auto onItem = std::make_shared<QMetaObject::Connection>();
    auto onErr  = std::make_shared<QMetaObject::Connection>();
    bool* done  = new bool(false);

    auto publishMerged = [this, myDevId, done](QList<uint32_t> existing) {
        if (*done) return;
        *done = true;

        QSet<uint32_t> merged(existing.begin(), existing.end());
        bool wasMissing = !merged.contains(myDevId);
        merged.insert(myDevId);

        QStringList idStrs;
        for (uint32_t x : merged) idStrs << QString::number(x);
        qDebug() << "[OMEMO] Publishing merged devicelist ("
                 << (wasMissing ? "adding our device" : "already present")
                 << "):" << idStrs.join(",");

        QDomDocument doc;
        QDomElement list = doc.createElementNS(
            QString::fromLatin1(XmppOmemo::NS), QLatin1String("list"));
        for (uint32_t devId : merged) {
            QDomElement devEl = doc.createElementNS(
                QString::fromLatin1(XmppOmemo::NS), QLatin1String("device"));
            devEl.setAttribute(QLatin1String("id"), QString::number(devId));
            list.appendChild(devEl);
        }
        d->pepManager->publish(
            QString::fromLatin1(XmppOmemo::NS_DEVICELIST),
            XMPP::PubSubItem(QLatin1String("current"), list));

        // Remember the merged list locally so we can encrypt-for-self
        d->deviceLists[d->accountId] = merged.values();

        // Fetch bundles for our own OTHER devices so we can encrypt
        // for them (and thus read our own outgoing messages on phone).
        // fetchContactBundles handles all the plumbing.
        bool hasOtherOwnDevices = false;
        for (uint32_t devId : merged) if (devId != myDevId) { hasOtherOwnDevices = true; break; }
        if (hasOtherOwnDevices) {
            qDebug() << "[OMEMO] Fetching our own other-device bundles for"
                     << (merged.size() - 1) << "device(s) of" << d->accountId;
            fetchContactBundles(XMPP::Jid(d->accountId));
        }

        delete done;
    };

    // Listen for success (item comes back) OR error (no existing list yet)
    *onItem = connect(d->pepManager, &PEPManager::itemPublished,
        this, [this, ownJid, onItem, onErr, publishMerged](
                const XMPP::Jid& from, const QString& node,
                const XMPP::PubSubItem& item) {
            if (!from.compare(ownJid, false)) return;
            if (node != QString::fromLatin1(XmppOmemo::NS_DEVICELIST)) return;
            QObject::disconnect(*onItem);
            QObject::disconnect(*onErr);

            // Parse existing devicelist
            QList<uint32_t> existing;
            QDomElement listEl = item.payload();
            QDomElement devEl = listEl.firstChildElement(QLatin1String("device"));
            while (!devEl.isNull()) {
                bool ok = false;
                uint32_t devId = devEl.attribute(QLatin1String("id")).toUInt(&ok);
                if (ok && devId != 0) existing.append(devId);
                devEl = devEl.nextSiblingElement(QLatin1String("device"));
            }
            publishMerged(existing);
        });

    *onErr = connect(d->pepManager, &PEPManager::getError,
        this, [this, ownJid, onItem, onErr, publishMerged](
                const XMPP::Jid& errJid, const QString& errNode) {
            if (!errJid.compare(ownJid, false)) return;
            if (errNode != QString::fromLatin1(XmppOmemo::NS_DEVICELIST)) return;
            QObject::disconnect(*onItem);
            QObject::disconnect(*onErr);
            // No existing devicelist — publish with just our device
            publishMerged(QList<uint32_t>());
        });

    qDebug() << "[OMEMO] publishBundle: fetching own devicelist from" << ownJid.full();
    d->pepManager->get(ownJid, QString::fromLatin1(XmppOmemo::NS_DEVICELIST),
                       QLatin1String("current"));
}

void OmemoManager::publishOwnBundleData()
{
    // This is the bundle-publish logic that used to be inline in publishBundle.
    // The devicelist publish is now done separately after fetching existing list.
    uint32_t myDevId = d->store.localDeviceId;
    QDomDocument doc;

    // Build bundle payload: eu.siacs.conversations.axolotl.bundles:<devId>
    // The bundle contains the identity key, signed prekey, and one-time prekeys
    QDomElement bundle = doc.createElementNS(
        QString::fromLatin1(XmppOmemo::NS), QLatin1String("bundle"));

    // signedPreKeyPublic
    if (!d->store.signedPreKeys.isEmpty()) {
        uint32_t spkId = d->store.signedPreKeys.keys().first();
        QByteArray spkData = d->store.signedPreKeys[spkId];

        // Deserialize to get the public key
        session_signed_pre_key* spk = nullptr;
        if (session_signed_pre_key_deserialize(&spk,
                reinterpret_cast<const uint8_t*>(spkData.constData()),
                static_cast<size_t>(spkData.size()), d->signalCtx) == SG_SUCCESS) {
            ec_key_pair* spkKeyPair = session_signed_pre_key_get_key_pair(spk);
            ec_public_key* spkPubKey = ec_key_pair_get_public(spkKeyPair);

            signal_buffer* spkPubBuf = nullptr;
            ec_public_key_serialize(&spkPubBuf, spkPubKey);

            QByteArray spkPubBytes(
                reinterpret_cast<const char*>(signal_buffer_data(spkPubBuf)),
                static_cast<int>(signal_buffer_len(spkPubBuf)));
            signal_buffer_free(spkPubBuf);

            // signature — returns const uint8_t*, not signal_buffer*
            const uint8_t* sigData = session_signed_pre_key_get_signature(spk);
            size_t sigLen = session_signed_pre_key_get_signature_len(spk);
            QByteArray sigBytes(
                reinterpret_cast<const char*>(sigData),
                static_cast<int>(sigLen));

            QDomElement spkEl = doc.createElementNS(
                QString::fromLatin1(XmppOmemo::NS), QLatin1String("signedPreKeyPublic"));
            spkEl.setAttribute(QLatin1String("signedPreKeyId"), QString::number(spkId));
            spkEl.appendChild(doc.createTextNode(
                QString::fromLatin1(spkPubBytes.toBase64())));
            bundle.appendChild(spkEl);

            QDomElement sigEl = doc.createElementNS(
                QString::fromLatin1(XmppOmemo::NS), QLatin1String("signedPreKeySignature"));
            sigEl.appendChild(doc.createTextNode(
                QString::fromLatin1(sigBytes.toBase64())));
            bundle.appendChild(sigEl);

            SIGNAL_UNREF(spk);
        }
    }

    // identityKey
    if (d->identityKeyPair) {
        ec_public_key* idPubKey = ratchet_identity_key_pair_get_public(d->identityKeyPair);
        signal_buffer* idPubBuf = nullptr;
        ec_public_key_serialize(&idPubBuf, idPubKey);
        QByteArray idPubBytes(
            reinterpret_cast<const char*>(signal_buffer_data(idPubBuf)),
            static_cast<int>(signal_buffer_len(idPubBuf)));
        signal_buffer_free(idPubBuf);

        QDomElement idEl = doc.createElementNS(
            QString::fromLatin1(XmppOmemo::NS), QLatin1String("identityKey"));
        idEl.appendChild(doc.createTextNode(
            QString::fromLatin1(idPubBytes.toBase64())));
        bundle.appendChild(idEl);
    }

    // prekeys
    QDomElement prekeys = doc.createElementNS(
        QString::fromLatin1(XmppOmemo::NS), QLatin1String("prekeys"));
    int pkCount = 0;
    for (auto it = d->store.preKeys.constBegin(); it != d->store.preKeys.constEnd() && pkCount < 100; ++it, ++pkCount) {
        session_pre_key* pk = nullptr;
        QByteArray pkData = it.value();
        if (session_pre_key_deserialize(&pk,
                reinterpret_cast<const uint8_t*>(pkData.constData()),
                static_cast<size_t>(pkData.size()), d->signalCtx) == SG_SUCCESS) {
            ec_key_pair* pkKeyPair = session_pre_key_get_key_pair(pk);
            ec_public_key* pkPubKey = ec_key_pair_get_public(pkKeyPair);
            signal_buffer* pkPubBuf = nullptr;
            ec_public_key_serialize(&pkPubBuf, pkPubKey);
            QByteArray pkPubBytes(
                reinterpret_cast<const char*>(signal_buffer_data(pkPubBuf)),
                static_cast<int>(signal_buffer_len(pkPubBuf)));
            signal_buffer_free(pkPubBuf);

            QDomElement pkEl = doc.createElementNS(
                QString::fromLatin1(XmppOmemo::NS), QLatin1String("preKeyPublic"));
            pkEl.setAttribute(QLatin1String("preKeyId"), QString::number(it.key()));
            pkEl.appendChild(doc.createTextNode(
                QString::fromLatin1(pkPubBytes.toBase64())));
            prekeys.appendChild(pkEl);

            SIGNAL_UNREF(pk);
        }
    }
    bundle.appendChild(prekeys);

    QString bundleNode = QString::fromLatin1(XmppOmemo::NS_BUNDLES)
                         + QLatin1Char(':') + QString::number(myDevId);
    d->pepManager->publish(bundleNode, XMPP::PubSubItem(QLatin1String("current"), bundle));

    qDebug() << "[OMEMO] Published bundle (device ID:" << myDevId << ")";
    emit bundlePublished(true);
}

// ──────────────────────────────────────────────────────────────────
// Bundle parsing helper: parse a <bundle> element into Signal pre-key bundle
// Returns SG_SUCCESS or negative error code.
// ──────────────────────────────────────────────────────────────────
static int parseAndProcessBundle(const QDomElement& bundleEl,
                                 uint32_t deviceId,
                                 const QString& contactJid,
                                 signal_protocol_store_context* storeCtx,
                                 signal_context* signalCtx)
{
    // Serialize bundle for debugging
    {
        QString dumpStr;
        QTextStream ts(&dumpStr);
        bundleEl.save(ts, 2);
        qDebug() << "[OMEMO] parseAndProcessBundle XML:\n" << dumpStr.left(2000);
    }

    // <signedPreKeyPublic signedPreKeyId='N'>base64</signedPreKeyPublic>
    QDomElement spkEl = bundleEl.firstChildElement(QLatin1String("signedPreKeyPublic"));
    if (spkEl.isNull()) {
        qWarning("[OMEMO] Bundle missing signedPreKeyPublic for device %u", deviceId);
        return SG_ERR_INVALID_KEY;
    }
    uint32_t signedPreKeyId = spkEl.attribute(QLatin1String("signedPreKeyId")).toUInt();
    QByteArray spkPubBytes = QByteArray::fromBase64(spkEl.text().toLatin1());

    // <signedPreKeySignature>base64</signedPreKeySignature>
    QDomElement sigEl = bundleEl.firstChildElement(QLatin1String("signedPreKeySignature"));
    if (sigEl.isNull()) {
        qWarning("[OMEMO] Bundle missing signedPreKeySignature for device %u", deviceId);
        return SG_ERR_INVALID_KEY;
    }
    QByteArray sigBytes = QByteArray::fromBase64(sigEl.text().toLatin1());

    // <identityKey>base64</identityKey>
    QDomElement idKeyEl = bundleEl.firstChildElement(QLatin1String("identityKey"));
    if (idKeyEl.isNull()) {
        qWarning("[OMEMO] Bundle missing identityKey for device %u", deviceId);
        return SG_ERR_INVALID_KEY;
    }
    QByteArray idKeyBytes = QByteArray::fromBase64(idKeyEl.text().toLatin1());

    // Pick a RANDOM prekey from <prekeys> — Conversations does this to balance
    // one-time-prekey consumption across a contact's many available prekeys.
    // Picking the first one (which we used to do) means every sender to this
    // contact would use the same OPK, defeating the purpose of one-time keys.
    QDomElement prekeysEl = bundleEl.firstChildElement(QLatin1String("prekeys"));
    uint32_t preKeyId = 0;
    QByteArray pkPubBytes;
    if (!prekeysEl.isNull()) {
        QList<QDomElement> pkList;
        for (QDomElement pk = prekeysEl.firstChildElement(QLatin1String("preKeyPublic"));
             !pk.isNull();
             pk = pk.nextSiblingElement(QLatin1String("preKeyPublic"))) {
            pkList.append(pk);
        }
        if (!pkList.isEmpty()) {
            int idx = static_cast<int>(QRandomGenerator::global()->bounded(pkList.size()));
            QDomElement pkEl = pkList.at(idx);
            preKeyId = pkEl.attribute(QLatin1String("preKeyId")).toUInt();
            pkPubBytes = QByteArray::fromBase64(pkEl.text().toLatin1());
        }
    }

    qDebug() << "[OMEMO]   signedPreKeyId=" << signedPreKeyId
             << "spkPubBytes.size=" << spkPubBytes.size()
             << "sigBytes.size=" << sigBytes.size()
             << "idKeyBytes.size=" << idKeyBytes.size()
             << "preKeyId=" << preKeyId
             << "pkPubBytes.size=" << pkPubBytes.size();

    // Deserialize EC keys
    ec_public_key* spkPub = nullptr;
    if (curve_decode_point(&spkPub,
            reinterpret_cast<const uint8_t*>(spkPubBytes.constData()),
            static_cast<size_t>(spkPubBytes.size()),
            signalCtx) != SG_SUCCESS) {
        qWarning("[OMEMO] Failed to decode signed pre-key for device %u", deviceId);
        return SG_ERR_INVALID_KEY;
    }

    ec_public_key* idKey = nullptr;
    if (curve_decode_point(&idKey,
            reinterpret_cast<const uint8_t*>(idKeyBytes.constData()),
            static_cast<size_t>(idKeyBytes.size()),
            signalCtx) != SG_SUCCESS) {
        SIGNAL_UNREF(spkPub);
        qWarning("[OMEMO] Failed to decode identity key for device %u", deviceId);
        return SG_ERR_INVALID_KEY;
    }

    ec_public_key* pkPub = nullptr;
    if (!pkPubBytes.isEmpty()) {
        curve_decode_point(&pkPub,
            reinterpret_cast<const uint8_t*>(pkPubBytes.constData()),
            static_cast<size_t>(pkPubBytes.size()),
            signalCtx);
    }

    // Create Signal pre-key bundle
    session_pre_key_bundle* bundle = nullptr;
    int result = session_pre_key_bundle_create(&bundle,
        deviceId,            // registration_id (reuse device id)
        static_cast<int>(deviceId), // device_id
        preKeyId,            // pre_key_id (0 if none)
        pkPub,               // pre_key_public (may be null)
        signedPreKeyId,      // signed_pre_key_id
        spkPub,              // signed_pre_key_public
        reinterpret_cast<const uint8_t*>(sigBytes.constData()),
        static_cast<size_t>(sigBytes.size()),
        idKey                // identity_key
    );

    if (result != SG_SUCCESS) {
        qWarning("[OMEMO] session_pre_key_bundle_create failed for device %u: %d",
                 deviceId, result);
        SIGNAL_UNREF(spkPub);
        SIGNAL_UNREF(idKey);
        if (pkPub) SIGNAL_UNREF(pkPub);
        return result;
    }

    // Build the Signal session via the bundle
    QByteArray jidUtf8 = contactJid.toUtf8();
    signal_protocol_address addr;
    addr.name = jidUtf8.constData();
    addr.name_len = static_cast<size_t>(jidUtf8.size());
    addr.device_id = static_cast<int32_t>(deviceId);

    session_builder* builder = nullptr;
    result = session_builder_create(&builder, storeCtx, &addr, signalCtx);
    if (result == SG_SUCCESS) {
        result = session_builder_process_pre_key_bundle(builder, bundle);
        session_builder_free(builder);
        if (result == SG_SUCCESS) {
            qDebug("[OMEMO] Session established with %s device %u",
                   qPrintable(contactJid), deviceId);
        } else {
            qWarning("[OMEMO] session_builder_process_pre_key_bundle failed for %s device %u: %d",
                     qPrintable(contactJid), deviceId, result);
        }
    }

    SIGNAL_UNREF(bundle);
    // Keys are ref'd by the bundle; SIGNAL_UNREF them after bundle is destroyed
    if (spkPub) SIGNAL_UNREF(spkPub);
    if (idKey) SIGNAL_UNREF(idKey);
    if (pkPub) SIGNAL_UNREF(pkPub);

    return result;
}

void OmemoManager::fetchContactBundles(const XMPP::Jid& contact)
{
    if (!d->initialized) {
        emit sessionsEstablished(contact, false);
        return;
    }

    if (!d->pepManager) {
        qWarning() << "[OMEMO] fetchContactBundles: no PEPManager — cannot fetch bundles for" << contact.bare();
        // Still emit success so we try to encrypt; we may have sessions from earlier
        emit sessionsEstablished(contact, true);
        return;
    }

    // Check if we already have sessions for this contact's known devices
    QString bareJid = contact.bare();
    QList<uint32_t> knownDevices = d->deviceLists.value(bareJid);
    bool allHaveSessions = !knownDevices.isEmpty();
    if (allHaveSessions) {
        for (uint32_t devId : knownDevices) {
            QByteArray jidUtf8 = bareJid.toUtf8();
            signal_protocol_address addr;
            addr.name = jidUtf8.constData();
            addr.name_len = static_cast<size_t>(jidUtf8.size());
            addr.device_id = static_cast<int32_t>(devId);
            if (!sess_contains_session(&addr, &d->store)) {
                allHaveSessions = false;
                break;
            }
        }
    }

    if (allHaveSessions) {
        qDebug() << "[OMEMO] fetchContactBundles: sessions already established for" << bareJid;
        emit sessionsEstablished(contact, true);
        return;
    }

    qDebug() << "[OMEMO] fetchContactBundles: fetching devicelist for" << bareJid;

    // Step 1: fetch devicelist node
    // PEPManager::get() emits itemPublished(jid, node, item) via getFinished()
    // We connect once, one-shot style
    OmemoManager* self = this;
    XMPP::Jid contactCopy = contact;

    // Use a shared state for the multi-fetch operation.
    // FetchState is a QObject parented to OmemoManager; it owns the timeout timer
    // as a child, so the timer dies together with state if anything goes wrong.
    struct FetchState : public QObject {
        XMPP::Jid contact;
        QString bareJid;
        int pendingBundles = 0;
        bool anySuccess = false;
        bool done = false;  // guard: ensures sessionsEstablished is emitted exactly once
        OmemoManager* manager = nullptr;
        // Connections we need to disconnect
        QMetaObject::Connection devicelistConn;
        QMetaObject::Connection bundleConn;
        QMetaObject::Connection errorConn;       // bundle-phase getError
        QMetaObject::Connection devlistErrConn;  // devicelist-phase getError
        QTimer* timeoutTimer = nullptr;          // 10s safety timer, child of `this`
    };
    FetchState* state = new FetchState();
    state->setParent(this);            // lifetime bound to OmemoManager
    state->contact = contact;
    state->bareJid = bareJid;
    state->manager = this;

    // Helper lambda: finish with result, guarded by state->done (one-shot).
    // CRITICAL: stop and destroy the timeout timer FIRST, before scheduling state
    // deletion — otherwise the timer fires 10s later on freed `state` memory
    // (crash in qPrintable(state->bareJid) → QTextCodec::fromUnicode → null deref).
    auto finishFetch = [this, state](bool success) {
        if (state->done)
            return;
        state->done = true;
        if (state->timeoutTimer) {
            state->timeoutTimer->stop();
            state->timeoutTimer->disconnect();
            state->timeoutTimer->deleteLater();
            state->timeoutTimer = nullptr;
        }
        disconnect(state->devicelistConn);
        disconnect(state->bundleConn);
        disconnect(state->errorConn);
        disconnect(state->devlistErrConn);
        emit sessionsEstablished(state->contact, success);
        state->deleteLater();
    };

    // 10-second safety timeout: if PEP never replies (404 not wired, network stall, etc.),
    // unblock the UI and fall back to plaintext send.
    // The timer is a CHILD of `state` so it cannot outlive it even if deleteLater races.
    state->timeoutTimer = new QTimer(state);
    state->timeoutTimer->setSingleShot(true);
    state->timeoutTimer->setInterval(10000);
    connect(state->timeoutTimer, &QTimer::timeout, this, [state, finishFetch]() {
        if (state->done)
            return;  // success/failure already reported; ignore
        qWarning("[OMEMO] fetchContactBundles timeout for %s — no sessions established within 10s",
                 qPrintable(state->bareJid));
        finishFetch(state->anySuccess);
    });
    state->timeoutTimer->start();

    // Connect to devicelist arrival
    state->devicelistConn = connect(d->pepManager, &PEPManager::itemPublished,
        this, [this, state, finishFetch](const XMPP::Jid& from, const QString& node,
                            const XMPP::PubSubItem& item) mutable {
            if (state->done)
                return;
            // Only handle devicelist for our target contact
            if (!from.compare(state->contact, false))
                return;
            if (node != QString::fromLatin1(XmppOmemo::NS_DEVICELIST))
                return;

            // Disconnect devicelist handler now that we got it
            disconnect(state->devicelistConn);

            QDomElement listEl = item.payload();
            if (listEl.isNull()) {
                qWarning() << "[OMEMO] Empty devicelist for" << state->bareJid;
                finishFetch(false);
                return;
            }

            // Parse device IDs
            QList<uint32_t> devices;
            QDomElement devEl = listEl.firstChildElement(QLatin1String("device"));
            while (!devEl.isNull()) {
                bool ok = false;
                uint32_t devId = devEl.attribute(QLatin1String("id")).toUInt(&ok);
                if (ok && devId != 0)
                    devices.append(devId);
                devEl = devEl.nextSiblingElement(QLatin1String("device"));
            }

            if (devices.isEmpty()) {
                qWarning() << "[OMEMO] No devices in devicelist for" << state->bareJid;
                finishFetch(false);
                return;
            }

            // Update our known device list
            d->deviceLists[state->bareJid] = devices;

            // Filter to only devices we don't already have sessions for.
            // If fetching our OWN devicelist, skip our own device id — we can
            // never session-encrypt to ourselves and libsignal rejects it.
            QList<uint32_t> needSession;
            bool isOwnJid = (state->bareJid == d->accountId);
            for (uint32_t devId : devices) {
                if (isOwnJid && devId == d->store.localDeviceId)
                    continue;
                QByteArray jidUtf8 = state->bareJid.toUtf8();
                signal_protocol_address addr;
                addr.name = jidUtf8.constData();
                addr.name_len = static_cast<size_t>(jidUtf8.size());
                addr.device_id = static_cast<int32_t>(devId);
                if (!sess_contains_session(&addr, &d->store))
                    needSession.append(devId);
            }

            if (needSession.isEmpty()) {
                qDebug() << "[OMEMO] All sessions already established for" << state->bareJid;
                finishFetch(true);
                return;
            }

            qDebug() << "[OMEMO] Need to fetch bundles for" << needSession.size()
                     << "devices of" << state->bareJid;

            state->pendingBundles = needSession.size();
            state->anySuccess = false;

            // Connect bundle success handler
            state->bundleConn = connect(d->pepManager, &PEPManager::itemPublished,
                this, [this, state, needSession, finishFetch](const XMPP::Jid& from2,
                                                  const QString& node2,
                                                  const XMPP::PubSubItem& item2) mutable {
                    if (state->done)
                        return;
                    if (!from2.compare(state->contact, false))
                        return;

                    // Check if this is one of the bundle nodes we need
                    uint32_t matchedDevId = 0;
                    for (uint32_t devId : needSession) {
                        QString expectedNode = QString::fromLatin1(XmppOmemo::NS_BUNDLES)
                                               + QLatin1Char(':') + QString::number(devId);
                        if (node2 == expectedNode) {
                            matchedDevId = devId;
                            break;
                        }
                    }
                    if (matchedDevId == 0)
                        return;

                    // Process this bundle
                    QDomElement bundleEl = item2.payload();
                    int result = parseAndProcessBundle(
                        bundleEl, matchedDevId, state->bareJid,
                        d->storeCtx, d->signalCtx);
                    if (result == SG_SUCCESS)
                        state->anySuccess = true;

                    state->pendingBundles--;
                    if (state->pendingBundles <= 0) {
                        // If no new sessions could be established, fall back to
                        // checking whether ANY stored session exists for this JID.
                        // If so, we can still encrypt to that existing session even
                        // if the contact has since retracted or rolled their device.
                        bool hasAnySession = state->anySuccess;
                        if (!hasAnySession) {
                            QString prefix = state->bareJid + QLatin1Char(':');
                            for (auto it = d->store.sessions.constBegin();
                                 it != d->store.sessions.constEnd(); ++it) {
                                if (it.key().startsWith(prefix)) { hasAnySession = true; break; }
                            }
                        }
                        finishFetch(hasAnySession);
                    }
                });

            // Connect bundle error handler: PEP 404 / empty / network failure for bundles
            state->errorConn = connect(d->pepManager, &PEPManager::getError,
                this, [this, state, needSession, finishFetch](const XMPP::Jid& errJid,
                                                               const QString& errNode) mutable {
                    if (state->done)
                        return;
                    if (!errJid.compare(state->contact, false))
                        return;

                    // Check if this error matches one of the bundle nodes we're waiting for
                    bool matched = false;
                    for (uint32_t devId : needSession) {
                        QString expectedNode = QString::fromLatin1(XmppOmemo::NS_BUNDLES)
                                               + QLatin1Char(':') + QString::number(devId);
                        if (errNode == expectedNode) {
                            matched = true;
                            break;
                        }
                    }
                    if (!matched)
                        return;

                    qWarning("[OMEMO] Bundle fetch error for %s node %s — counting as failure",
                             qPrintable(state->bareJid), qPrintable(errNode));

                    state->pendingBundles--;
                    if (state->pendingBundles <= 0) {
                        // Same fallback as the success handler: if any stored
                        // session for this JID exists, we can still encrypt.
                        bool hasAnySession = state->anySuccess;
                        if (!hasAnySession) {
                            QString prefix = state->bareJid + QLatin1Char(':');
                            for (auto it = d->store.sessions.constBegin();
                                 it != d->store.sessions.constEnd(); ++it) {
                                if (it.key().startsWith(prefix)) { hasAnySession = true; break; }
                            }
                        }
                        finishFetch(hasAnySession);
                    }
                });

            // Fetch a bundle for each device that needs a session
            for (uint32_t devId : needSession) {
                QString bundleNode = QString::fromLatin1(XmppOmemo::NS_BUNDLES)
                                     + QLatin1Char(':') + QString::number(devId);
                qDebug() << "[OMEMO] Fetching bundle node" << bundleNode << "for" << state->bareJid;
                d->pepManager->get(state->contact, bundleNode, QLatin1String("current"));
            }
        });

    // Also listen for a getError on the devicelist itself (PEP 404 / unavailable).
    // Stored on state->devlistErrConn so finishFetch() disconnects it cleanly;
    // avoids heap-allocated Connection* that could leak or outlive state.
    state->devlistErrConn = connect(d->pepManager, &PEPManager::getError,
        this, [this, state, finishFetch](const XMPP::Jid& errJid,
                                          const QString& errNode) {
            if (state->done)
                return;
            if (!errJid.compare(state->contact, false))
                return;
            if (errNode != QString::fromLatin1(XmppOmemo::NS_DEVICELIST))
                return;

            // Devicelist fetch failed — contact has no OMEMO devices (or PEP unavailable)
            qWarning("[OMEMO] Devicelist fetch failed for %s — no OMEMO sessions possible",
                     qPrintable(state->bareJid));
            finishFetch(false);
        });

    // Kick off devicelist fetch
    d->pepManager->get(contact, QString::fromLatin1(XmppOmemo::NS_DEVICELIST),
                       QLatin1String("current"));

    Q_UNUSED(self);
    Q_UNUSED(contactCopy);
}

void OmemoManager::encrypt(const XMPP::Jid& to, const QString& body)
{
    if (!d->initialized) {
        emit encryptDone(to, QDomElement(), body, false);
        return;
    }

    // Collect target device IDs: union of the current PEP devicelist AND any
    // stored Signal sessions we have for this JID. This survives the case
    // where the contact retracted a device from their devicelist but we still
    // have a valid ratchet session with it (common in practice — Conversations
    // / Cheogram roll device IDs on reinstall, so sometimes the only usable
    // session is one for a device no longer in the current devicelist).
    QString toJid = to.bare();
    QSet<uint32_t> targetDevices;
    for (uint32_t d0 : d->deviceLists.value(toJid))
        targetDevices.insert(d0);

    // Scan stored sessions for "toJid:DEVICE_ID"
    QString sessionPrefix = toJid + QLatin1Char(':');
    for (auto it = d->store.sessions.constBegin(); it != d->store.sessions.constEnd(); ++it) {
        if (it.key().startsWith(sessionPrefix)) {
            bool ok = false;
            uint32_t devId = it.key().mid(sessionPrefix.size()).toUInt(&ok);
            if (ok)
                targetDevices.insert(devId);
        }
    }

    QList<uint32_t> deviceIds = targetDevices.values();
    qDebug() << "[OMEMO] encrypt() for" << toJid << "— target devices:" << deviceIds.size()
             << "(advertised:" << d->deviceLists.value(toJid).size()
             << " stored sessions:" << (deviceIds.size() - d->deviceLists.value(toJid).size()) << ")";

    if (deviceIds.isEmpty()) {
        // No known devices AND no stored sessions — can't encrypt.
        qWarning() << "[OMEMO] No device sessions for" << toJid << "— cannot encrypt";
        emit encryptDone(to, QDomElement(), body, false);
        return;
    }

    // Generate 16-byte random AES key + 12-byte random IV
    QByteArray aesKey(16, '\0');
    QByteArray iv(12, '\0');
    RAND_bytes(reinterpret_cast<unsigned char*>(aesKey.data()), 16);
    RAND_bytes(reinterpret_cast<unsigned char*>(iv.data()), 12);

    // AES-128-GCM encrypt the message body.
    // ciphertext = encrypted body (no tag); authTag is returned separately.
    QByteArray authTag;
    QByteArray plaintext = body.toUtf8();
    QByteArray ciphertext = aes128gcm_encrypt(aesKey, iv, plaintext, authTag);
    if (ciphertext.isEmpty() || authTag.size() != 16) {
        qWarning() << "[OMEMO] AES-128-GCM encryption failed";
        emit encryptDone(to, QDomElement(), body, false);
        return;
    }

    // Per Conversations OMEMO spec, the key material Signal-encrypts to each
    // device is (16-byte AES key || 16-byte GCM auth tag) = 32 bytes.
    // The <payload> then carries ciphertext ONLY.
    QByteArray keyMaterial = aesKey + authTag;
    Q_ASSERT(keyMaterial.size() == 32);

    // Encrypt keyMaterial for each device using their Signal session.
    // libsignal handles both the "fresh session" (emits PreKeySignalMessage) and
    // "established session" (emits SignalMessage) cases automatically.
    // Also include our own other devices so they can see our sent messages.
    QList<XmppOmemo::OmemoKey> keys;

    auto encryptToDevice = [&](const QString& jid, uint32_t devId) {
        QByteArray jidUtf8 = jid.toUtf8();
        signal_protocol_address addr;
        addr.name = jidUtf8.constData();
        addr.name_len = static_cast<size_t>(jidUtf8.size());
        addr.device_id = static_cast<int32_t>(devId);

        session_cipher* cipher = nullptr;
        if (session_cipher_create(&cipher, d->storeCtx, &addr, d->signalCtx) != SG_SUCCESS) {
            qWarning() << "[OMEMO] Failed to create session cipher for" << jid << "device" << devId;
            return;
        }

        ciphertext_message* encryptedKey = nullptr;
        int result = session_cipher_encrypt(cipher,
                                             reinterpret_cast<const uint8_t*>(keyMaterial.constData()),
                                             static_cast<size_t>(keyMaterial.size()),
                                             &encryptedKey);
        session_cipher_free(cipher);

        if (result != SG_SUCCESS || !encryptedKey) {
            qWarning() << "[OMEMO] session_cipher_encrypt failed for" << jid
                       << "device" << devId << "result:" << result;
            return;
        }

        signal_buffer* serialized = ciphertext_message_get_serialized(encryptedKey);
        XmppOmemo::OmemoKey k;
        k.rid = devId;
        k.data = QByteArray(reinterpret_cast<const char*>(signal_buffer_data(serialized)),
                            static_cast<int>(signal_buffer_len(serialized)));
        k.prekey = (ciphertext_message_get_type(encryptedKey) == CIPHERTEXT_PREKEY_TYPE);
        keys.append(k);
        qDebug() << "[OMEMO]   -> encrypted for" << jid << "device" << devId
                 << (k.prekey ? "(PreKey)" : "(SignalMsg)") << k.data.size() << "bytes";

        SIGNAL_UNREF(encryptedKey);
    };

    // Encrypt for recipient's devices
    for (uint32_t devId : deviceIds)
        encryptToDevice(toJid, devId);

    // Also encrypt for our OTHER devices (not ourselves) so our other clients
    // can read what we sent (Conversations does this by design)
    QString ourJid = d->accountId;
    if (ourJid != toJid) {
        QList<uint32_t> ownDevs = d->deviceLists.value(ourJid);
        for (uint32_t devId : ownDevs) {
            if (devId != d->store.localDeviceId)
                encryptToDevice(ourJid, devId);
        }
    }

    if (keys.isEmpty()) {
        qWarning() << "[OMEMO] encrypt: no Signal sessions available — all encryptToDevice calls failed";
        emit encryptDone(to, QDomElement(), body, false);
        return;
    }

    // Build <encrypted> element with ciphertext-only payload.
    // The parent QDomDocument owns the element tree; we return the element
    // which stays valid as long as caller keeps a QDomNode referencing the
    // tree. addExtension() stores the QDomElement in unknownExtensions[]
    // which holds the tree alive implicitly via QDomNode ref-counting.
    QDomDocument doc;
    QDomElement encrypted = XmppOmemo::buildEncrypted(doc, d->store.localDeviceId, iv, keys, ciphertext);
    doc.appendChild(encrypted);

    qDebug() << "[OMEMO] encrypt: SUCCESS — produced" << keys.size() << "<key> entries,"
             << ciphertext.size() << "bytes ciphertext";
    emit encryptDone(to, encrypted, body, true);
}

bool OmemoManager::hasMucSessions(const XMPP::Jid& /*roomJid*/,
                                   const QHash<QString, XMPP::Jid>& nickToRealJid) const
{
    if (!d->initialized)
        return false;

    for (auto it = nickToRealJid.constBegin(); it != nickToRealJid.constEnd(); ++it) {
        const XMPP::Jid& realJid = it.value();
        if (realJid.isEmpty())
            continue;

        QString bareJid = realJid.bare();
        QList<uint32_t> devices = d->deviceLists.value(bareJid);
        for (uint32_t devId : devices) {
            QByteArray jidUtf8 = bareJid.toUtf8();
            signal_protocol_address addr;
            addr.name = jidUtf8.constData();
            addr.name_len = static_cast<size_t>(jidUtf8.size());
            addr.device_id = static_cast<int32_t>(devId);
            if (sess_contains_session(&addr, &d->store))
                return true;
        }
    }
    return false;
}

void OmemoManager::encryptForMuc(const XMPP::Jid& roomJid, const QString& body,
                                  const QHash<QString, XMPP::Jid>& nickToRealJid)
{
    if (!d->initialized) {
        emit encryptDone(roomJid, QDomElement(), body, false);
        return;
    }

    // Generate 16-byte AES key + 12-byte IV
    QByteArray aesKey(16, '\0');
    QByteArray iv(12, '\0');
    RAND_bytes(reinterpret_cast<unsigned char*>(aesKey.data()), 16);
    RAND_bytes(reinterpret_cast<unsigned char*>(iv.data()), 12);

    // AES-128-GCM encrypt body; tag returned separately per OMEMO spec
    QByteArray authTag;
    QByteArray plaintext = body.toUtf8();
    QByteArray ciphertext = aes128gcm_encrypt(aesKey, iv, plaintext, authTag);
    if (ciphertext.isEmpty() || authTag.size() != 16) {
        qWarning() << "[OMEMO] encryptForMuc: AES encryption failed";
        emit encryptDone(roomJid, QDomElement(), body, false);
        return;
    }

    // Signal-encrypt 32-byte key material (key || tag) per Conversations spec
    QByteArray keyMaterial = aesKey + authTag;

    // Encrypt key material for each participant device that has a session
    QList<XmppOmemo::OmemoKey> keys;

    for (auto it = nickToRealJid.constBegin(); it != nickToRealJid.constEnd(); ++it) {
        const XMPP::Jid& realJid = it.value();
        if (realJid.isEmpty())
            continue;

        QString bareJid = realJid.bare();
        QList<uint32_t> devices = d->deviceLists.value(bareJid);

        for (uint32_t devId : devices) {
            QByteArray jidUtf8 = bareJid.toUtf8();
            signal_protocol_address addr;
            addr.name = jidUtf8.constData();
            addr.name_len = static_cast<size_t>(jidUtf8.size());
            addr.device_id = static_cast<int32_t>(devId);

            // libsignal handles both fresh and established sessions —
            // no pre-check needed.

            session_cipher* cipher = nullptr;
            if (session_cipher_create(&cipher, d->storeCtx, &addr, d->signalCtx) != SG_SUCCESS) {
                qWarning() << "[OMEMO] encryptForMuc: failed to create cipher for"
                           << bareJid << "device" << devId;
                continue;
            }

            ciphertext_message* encryptedKey = nullptr;
            int result = session_cipher_encrypt(cipher,
                             reinterpret_cast<const uint8_t*>(keyMaterial.constData()),
                             static_cast<size_t>(keyMaterial.size()),
                             &encryptedKey);
            session_cipher_free(cipher);

            if (result != SG_SUCCESS || !encryptedKey) {
                qWarning() << "[OMEMO] encryptForMuc: encrypt failed for" << bareJid << devId
                           << "result:" << result;
                continue;
            }

            signal_buffer* serialized = ciphertext_message_get_serialized(encryptedKey);
            XmppOmemo::OmemoKey k;
            k.rid = devId;
            k.data = QByteArray(reinterpret_cast<const char*>(signal_buffer_data(serialized)),
                                static_cast<int>(signal_buffer_len(serialized)));
            k.prekey = (ciphertext_message_get_type(encryptedKey) == CIPHERTEXT_PREKEY_TYPE);
            keys.append(k);

            SIGNAL_UNREF(encryptedKey);
        }
    }

    if (keys.isEmpty()) {
        qWarning() << "[OMEMO] encryptForMuc: no sessions — falling back to plain";
        emit encryptDone(roomJid, QDomElement(), body, false);
        return;
    }

    QDomDocument doc;
    QDomElement encrypted = XmppOmemo::buildEncrypted(doc, d->store.localDeviceId, iv, keys, ciphertext);
    doc.appendChild(encrypted);

    qDebug() << "[OMEMO] encryptForMuc: encrypted for" << keys.size() << "devices in room" << roomJid.bare();

    // Stash plaintext so the MUC echo (which we cannot Signal-decrypt because
    // libsignal refuses self-sessions) can be displayed as plaintext when it
    // arrives back from the server. Keyed by iv (12B random per send), LIFO
    // evict past kMucEchoCacheMax to keep the hash bounded. Persisted to
    // muc_echo.json so history replay after restart also resolves.
    d->mucEchoPlaintext.insert(iv, body);
    d->mucEchoIvOrder.append(iv);
    while (d->mucEchoIvOrder.size() > Private::kMucEchoCacheMax) {
        QByteArray old = d->mucEchoIvOrder.takeFirst();
        d->mucEchoPlaintext.remove(old);
    }
    d->saveMucEchoCache();

    emit encryptDone(roomJid, encrypted, body, true);
}

void OmemoManager::decrypt(const XMPP::Jid& from, const QDomElement& encryptedElement)
{
    if (!d->initialized) {
        emit decryptDone(from, QString(), 0, false);
        return;
    }

    XmppOmemo::OmemoPayload payload = XmppOmemo::parseEncrypted(encryptedElement);
    if (!payload.isValid) {
        qWarning() << "[OMEMO] Invalid encrypted element from" << from.bare();
        emit decryptDone(from, QString(), 0, false);
        return;
    }

    uint32_t myDevId = d->store.localDeviceId;

    // Self-MUC-echo shortcut: if the sender's bare JID matches our own
    // account JID and the iv was produced by our own encryptForMuc call
    // earlier, return the cached plaintext instead of trying to Signal-
    // decrypt (which would fail — libsignal does not allow self-sessions).
    // This is how our own MUC OMEMO messages render as plaintext in the
    // room history we see. See Private::mucEchoPlaintext for the producer
    // side in encryptForMuc().
    //
    // IMPORTANT: we do NOT evict the cache entry on lookup. The MUC server
    // replays room history on every re-join (after restart, reconnect, or
    // window re-open), so the same iv may legitimately be decrypted many
    // times over the client's lifetime. Eviction only happens by LIFO when
    // the cache grows past kMucEchoCacheMax.
    if (from.bare() == d->accountId && d->mucEchoPlaintext.contains(payload.iv)) {
        QString body = d->mucEchoPlaintext.value(payload.iv);
        qDebug() << "[OMEMO] Own MUC echo — serving cached plaintext ("
                 << body.size() << "chars)";
        emit decryptDone(from, body, payload.sid, true);
        return;
    }

    // Find our key entry
    const XmppOmemo::OmemoKey* ourKey = nullptr;
    for (const XmppOmemo::OmemoKey& k : payload.keys) {
        if (k.rid == myDevId) {
            ourKey = &k;
            break;
        }
    }

    if (!ourKey) {
        qDebug() << "[OMEMO] Message from" << from.bare()
                 << "is not addressed to us (our device ID:" << myDevId << ")";
        emit decryptDone(from, QString(), payload.sid, false);
        return;
    }

    // Decrypt the AES key using our Signal session
    QString fromJid = from.bare();
    QByteArray fromJidUtf8 = fromJid.toUtf8();
    signal_protocol_address addr;
    addr.name = fromJidUtf8.constData();
    addr.name_len = static_cast<size_t>(fromJidUtf8.size());
    addr.device_id = static_cast<int32_t>(payload.sid);

    session_cipher* cipher = nullptr;
    if (session_cipher_create(&cipher, d->storeCtx, &addr, d->signalCtx) != SG_SUCCESS) {
        qWarning() << "[OMEMO] Failed to create session cipher for" << fromJid;
        emit decryptDone(from, QString(), payload.sid, false);
        return;
    }

    signal_buffer* decryptedKeyBuf = nullptr;
    int result = SG_ERR_UNKNOWN;

    if (ourKey->prekey) {
        // PreKeySignalMessage
        pre_key_signal_message* preKeyMsg = nullptr;
        result = pre_key_signal_message_deserialize(&preKeyMsg,
                    reinterpret_cast<const uint8_t*>(ourKey->data.constData()),
                    static_cast<size_t>(ourKey->data.size()),
                    d->signalCtx);
        if (result == SG_SUCCESS) {
            result = session_cipher_decrypt_pre_key_signal_message(
                cipher, preKeyMsg, nullptr, &decryptedKeyBuf);
            SIGNAL_UNREF(preKeyMsg);
        }
    } else {
        // SignalMessage
        signal_message* sigMsg = nullptr;
        result = signal_message_deserialize(&sigMsg,
                    reinterpret_cast<const uint8_t*>(ourKey->data.constData()),
                    static_cast<size_t>(ourKey->data.size()),
                    d->signalCtx);
        if (result == SG_SUCCESS) {
            result = session_cipher_decrypt_signal_message(
                cipher, sigMsg, nullptr, &decryptedKeyBuf);
            SIGNAL_UNREF(sigMsg);
        }
    }

    session_cipher_free(cipher);

    if (result != SG_SUCCESS || !decryptedKeyBuf) {
        qWarning() << "[OMEMO] Failed to decrypt AES key from" << fromJid
                   << "(error:" << result << ")";
        emit decryptDone(from, QString(), payload.sid, false);
        return;
    }

    // We got the decrypted key material.
    // Per Conversations OMEMO spec, this is (16-byte AES key || 16-byte GCM tag).
    // Older/variant senders may send just 16 bytes (key only, tag was in payload).
    QByteArray keyMaterial(reinterpret_cast<const char*>(signal_buffer_data(decryptedKeyBuf)),
                            static_cast<int>(signal_buffer_len(decryptedKeyBuf)));
    signal_buffer_free(decryptedKeyBuf);

    qDebug() << "[OMEMO] Decrypted key material from" << fromJid
             << "— size:" << keyMaterial.size() << "bytes";

    // Key-only handshake messages (used by Conversations/Cheogram to sync sessions
    // and pre-establish ratchets) have NO <payload>, and the key material may be
    // as small as 16 bytes or as large as 32. These are NOT errors — successful
    // Signal decryption + empty payload means the session advanced. Report
    // success-but-empty so psiaccount drops the stanza (instead of showing the
    // fallback "I sent you an OMEMO encrypted message" body).
    if (payload.payload.isEmpty()) {
        qDebug() << "[OMEMO] Key-only handshake from" << fromJid
                 << "(keyMat=" << keyMaterial.size() << "B) — session advanced";
        emit decryptDone(from, QString(), payload.sid, true);
        return;
    }

    QByteArray aesKey;
    QByteArray authTag;

    if (keyMaterial.size() == 32) {
        // Conversations / XEP-0384 v0.3+: key||tag (most common)
        aesKey  = keyMaterial.left(16);
        authTag = keyMaterial.right(16);
    } else if (keyMaterial.size() == 16 && payload.payload.size() >= 16) {
        // Legacy: 16-byte key, tag is last 16 bytes of <payload>
        aesKey  = keyMaterial;
        authTag = payload.payload.right(16);
        payload.payload = payload.payload.left(payload.payload.size() - 16);
    } else {
        qWarning() << "[OMEMO] Unexpected key material size:" << keyMaterial.size()
                   << "(payload size:" << payload.payload.size() << ")";
        emit decryptDone(from, QString(), payload.sid, false);
        return;
    }

    // AES-128-GCM decrypt the <payload> using key+tag.
    QByteArray plaintext = aes128gcm_decrypt(aesKey, payload.iv, payload.payload, authTag);
    if (plaintext.isEmpty()) {
        qWarning() << "[OMEMO] AES-128-GCM decryption failed (tag mismatch?) from" << fromJid;
        emit decryptDone(from, QString(), payload.sid, false);
        return;
    }

    QString body = QString::fromUtf8(plaintext);
    qDebug() << "[OMEMO] Decrypted message from" << fromJid << "(device" << payload.sid
             << "):" << body.left(50);
    emit decryptDone(from, body, payload.sid, true);
}

OmemoManager::TrustLevel OmemoManager::trustLevel(const XMPP::Jid& jid, uint32_t deviceId,
                                                   const QByteArray& /*identityKey*/) const
{
    QString key = jid.bare() + QLatin1Char(':') + QString::number(deviceId);
    return static_cast<TrustLevel>(d->store.trustLevels.value(key, TrustOnFirstUse));
}

void OmemoManager::setTrusted(const XMPP::Jid& jid, uint32_t deviceId,
                               const QByteArray& identityKey, TrustLevel level)
{
    QString key = jid.bare() + QLatin1Char(':') + QString::number(deviceId);
    d->store.trustLevels[key] = static_cast<int>(level);
    d->store.trustedIdentities[key] = identityKey;
    d->store.save();
}

QList<QPair<uint32_t, QByteArray>> OmemoManager::trustedKeys(const XMPP::Jid& jid) const
{
    QList<QPair<uint32_t, QByteArray>> result;
    QString prefix = jid.bare() + QLatin1Char(':');
    for (auto it = d->store.trustedIdentities.constBegin();
         it != d->store.trustedIdentities.constEnd(); ++it) {
        if (it.key().startsWith(prefix)) {
            uint32_t devId = it.key().mid(prefix.size()).toUInt();
            result.append(qMakePair(devId, it.value()));
        }
    }
    return result;
}
