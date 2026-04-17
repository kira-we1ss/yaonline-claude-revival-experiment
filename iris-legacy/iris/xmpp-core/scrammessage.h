/*
 * scrammessage.h — SCRAM-SHA-1/256 per RFC 5802
 * Uses QCA 2.3.7 for PBKDF2, HMAC, SHA, and random nonce.
 */
#ifndef SCRAM_MESSAGE_H
#define SCRAM_MESSAGE_H

#include <QString>
#include <QByteArray>
#include <QtCrypto>

class SCRAMMessage
{
public:
    // hashName: "sha1" for SCRAM-SHA-1, "sha256" for SCRAM-SHA-256
    SCRAMMessage(const QString& hashName, const QString& username,
                 const QCA::SecureArray& password);

    // Step 0: produce client-first-message (with "n,," GS2 prefix)
    QByteArray clientFirstMessage();

    // Step 1: parse server-first-message, produce client-final-message
    QByteArray clientFinalMessage(const QByteArray& serverFirst);

    // Step 2: verify server-final-message ("v=<base64>")
    bool verifyServerFinal(const QByteArray& serverFinal);

    bool isValid() const { return isValid_; }

private:
    QString hashName_;
    QString username_;
    QCA::SecureArray password_;
    QByteArray clientNonce_;
    QByteArray serverNonce_;
    QByteArray salt_;
    int iterCount_;
    QByteArray authMessage_;
    QByteArray serverKeyBytes_;
    QByteArray clientFirstBare_;
    bool isValid_;

    QByteArray Hi(const QCA::SecureArray& str, const QByteArray& salt, int iters);
    QByteArray HMAC(const QByteArray& key, const QByteArray& data);
    QByteArray H(const QByteArray& data);
    QByteArray xorBytes(const QByteArray& a, const QByteArray& b);
};

#endif // SCRAM_MESSAGE_H
