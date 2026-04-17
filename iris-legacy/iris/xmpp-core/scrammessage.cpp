/*
 * scrammessage.cpp — SCRAM-SHA-1/256 per RFC 5802
 * Uses QCA 2.3.7 for PBKDF2, HMAC, SHA, and random nonce.
 */
#include "scrammessage.h"
#include <QtCrypto>

SCRAMMessage::SCRAMMessage(const QString& hashName, const QString& username,
                           const QCA::SecureArray& password)
    : hashName_(hashName), username_(username), password_(password),
      iterCount_(0), isValid_(true)
{
    // Generate 18 random bytes as client nonce, base64-encoded (no line breaks)
    QCA::SecureArray rand = QCA::Random::randomArray(18);
    QCA::Base64 encoder;
    clientNonce_ = encoder.arrayToString(rand).toLatin1();
}

QByteArray SCRAMMessage::clientFirstMessage()
{
    // client-first-message-bare: n=<user>,r=<clientNonce>
    clientFirstBare_ = "n=" + username_.toUtf8() + ",r=" + clientNonce_;
    // Full message with GS2 header "n,," (no channel binding, no authzid)
    return "n,," + clientFirstBare_;
}

QByteArray SCRAMMessage::clientFinalMessage(const QByteArray& serverFirst)
{
    // Parse server-first-message: r=<serverNonce>,s=<salt_b64>,i=<iterations>
    QByteArray serverNonce, saltB64;

    for (const QByteArray& part : serverFirst.split(',')) {
        if (part.startsWith("r="))
            serverNonce = part.mid(2);
        else if (part.startsWith("s="))
            saltB64 = part.mid(2);
        else if (part.startsWith("i="))
            iterCount_ = part.mid(2).toInt();
    }

    // Validate server nonce starts with client nonce
    if (!serverNonce.startsWith(clientNonce_)) {
        isValid_ = false;
        return QByteArray();
    }
    serverNonce_ = serverNonce;

    // Decode salt
    QCA::Base64 decoder(QCA::Decode);
    salt_ = decoder.stringToArray(QString::fromLatin1(saltB64)).toByteArray();

    if (iterCount_ <= 0 || salt_.isEmpty()) {
        isValid_ = false;
        return QByteArray();
    }

    // client-final-message-without-proof
    // channel-binding: base64("n,,")
    QCA::Base64 encoder;
    QByteArray channelBinding = encoder.arrayToString(QCA::SecureArray("n,,")).toLatin1();
    QByteArray clientFinalWithoutProof =
        "c=" + channelBinding + ",r=" + serverNonce_;

    // auth-message = client-first-bare + "," + server-first + "," + client-final-without-proof
    authMessage_ = clientFirstBare_ + "," + serverFirst + "," + clientFinalWithoutProof;

    // RFC 5802 §3
    QByteArray saltedPassword = Hi(password_, salt_, iterCount_);

    QByteArray clientKey = HMAC(saltedPassword, "Client Key");
    QByteArray storedKey = H(clientKey);
    QByteArray clientSignature = HMAC(storedKey, authMessage_);
    QByteArray clientProof = xorBytes(clientKey, clientSignature);

    // Store server key for verification in step 2
    serverKeyBytes_ = HMAC(saltedPassword, "Server Key");

    QCA::Base64 proofEncoder;
    QByteArray encodedProof = proofEncoder.arrayToString(QCA::SecureArray(clientProof)).toLatin1();

    return clientFinalWithoutProof + ",p=" + encodedProof;
}

bool SCRAMMessage::verifyServerFinal(const QByteArray& serverFinal)
{
    // server-final: v=<base64(ServerSignature)>
    QByteArray proof;
    for (const QByteArray& part : serverFinal.split(',')) {
        if (part.startsWith("v=")) {
            QCA::Base64 decoder(QCA::Decode);
            proof = decoder.stringToArray(
                QString::fromLatin1(part.mid(2))).toByteArray();
        }
    }

    if (proof.isEmpty()) {
        isValid_ = false;
        return false;
    }

    QByteArray expectedServerSig = HMAC(serverKeyBytes_, authMessage_);
    bool ok = (proof == expectedServerSig);
    if (!ok) isValid_ = false;
    return ok;
}

// RFC 5802: Hi(str, salt, i) = PBKDF2(HMAC-<hash>, str, salt, i, hashLen)
QByteArray SCRAMMessage::Hi(const QCA::SecureArray& str,
                             const QByteArray& salt, int iters)
{
    // QCA PBKDF2 constructor takes the hash algorithm name (e.g. "sha1", "sha256")
    QCA::PBKDF2 pbkdf(hashName_);
    unsigned int hashLen = (hashName_ == "sha256") ? 32 : 20;
    QCA::SymmetricKey key = pbkdf.makeKey(
        str,
        QCA::InitializationVector(QCA::SecureArray(salt)),
        hashLen,
        static_cast<unsigned int>(iters));
    return key.toByteArray();
}

QByteArray SCRAMMessage::HMAC(const QByteArray& key, const QByteArray& data)
{
    // QCA MAC type is "hmac(sha1)" or "hmac(sha256)"
    QCA::MessageAuthenticationCode mac(
        "hmac(" + hashName_ + ")",
        QCA::SymmetricKey(key));
    mac.update(QCA::SecureArray(data));
    return mac.final().toByteArray();
}

QByteArray SCRAMMessage::H(const QByteArray& data)
{
    QCA::Hash hash(hashName_);
    hash.update(QCA::SecureArray(data));
    return hash.final().toByteArray();
}

QByteArray SCRAMMessage::xorBytes(const QByteArray& a, const QByteArray& b)
{
    int len = qMin(a.size(), b.size());
    QByteArray result(len, 0);
    for (int i = 0; i < len; ++i)
        result[i] = a[i] ^ b[i];
    return result;
}
