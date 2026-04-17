// xmpp_omemo.h — XEP-0384 OMEMO stanza helpers
// Namespace: eu.siacs.conversations.axolotl (legacy, widely compatible)
#ifndef XMPP_OMEMO_H
#define XMPP_OMEMO_H

#include <QByteArray>
#include <QDomDocument>
#include <QDomElement>
#include <QList>
#include <QPair>
#include <QString>

namespace XmppOmemo {
    static const char* NS          = "eu.siacs.conversations.axolotl";
    static const char* NS_DEVICELIST = "eu.siacs.conversations.axolotl.devicelist";
    static const char* NS_BUNDLES    = "eu.siacs.conversations.axolotl.bundles";

    struct OmemoKey {
        uint32_t rid;      // recipient device id
        QByteArray data;   // encrypted key material (base64-encoded in XML)
        bool prekey;       // true = PreKeySignalMessage
    };

    struct OmemoPayload {
        uint32_t sid;           // sender device id
        QByteArray iv;          // 12 bytes for AES-128-GCM
        QList<OmemoKey> keys;
        QByteArray payload;     // AES-128-GCM ciphertext+tag
        bool isValid;
    };

    // Parse <encrypted xmlns='eu.siacs.conversations.axolotl'> from a received message
    OmemoPayload parseEncrypted(const QDomElement& el);

    // Build <encrypted xmlns='eu.siacs.conversations.axolotl'> element for outgoing message
    QDomElement buildEncrypted(QDomDocument& doc, uint32_t sid,
                               const QByteArray& iv,
                               const QList<OmemoKey>& keys,
                               const QByteArray& payload);
}

#endif // XMPP_OMEMO_H
