// xmpp_omemo.cpp — XEP-0384 OMEMO stanza helpers implementation
#include "xmpp_omemo.h"
#include <QDomDocument>
#include <QByteArray>

namespace XmppOmemo {

OmemoPayload parseEncrypted(const QDomElement& el)
{
    OmemoPayload result;
    result.isValid = false;
    result.sid = 0;

    if (el.isNull())
        return result;

    if (el.namespaceURI() != QString::fromLatin1(NS))
        return result;

    // <header sid='12345'>
    QDomElement header = el.firstChildElement(QLatin1String("header"));
    if (header.isNull())
        return result;

    bool ok = false;
    result.sid = header.attribute(QLatin1String("sid")).toUInt(&ok);
    if (!ok)
        return result;

    // <iv>base64</iv>
    QDomElement ivEl = header.firstChildElement(QLatin1String("iv"));
    if (!ivEl.isNull())
        result.iv = QByteArray::fromBase64(ivEl.text().toLatin1());

    // <key rid='...' prekey='true'>base64</key>
    QDomElement keyEl = header.firstChildElement(QLatin1String("key"));
    while (!keyEl.isNull()) {
        OmemoKey k;
        k.rid = keyEl.attribute(QLatin1String("rid")).toUInt();
        QString prekey = keyEl.attribute(QLatin1String("prekey"));
        k.prekey = (prekey == QLatin1String("true") || prekey == QLatin1String("1"));
        k.data = QByteArray::fromBase64(keyEl.text().toLatin1());
        result.keys.append(k);
        keyEl = keyEl.nextSiblingElement(QLatin1String("key"));
    }

    // <payload>base64</payload> (may be absent for key-only messages)
    QDomElement payloadEl = el.firstChildElement(QLatin1String("payload"));
    if (!payloadEl.isNull())
        result.payload = QByteArray::fromBase64(payloadEl.text().toLatin1());

    result.isValid = true;
    return result;
}

QDomElement buildEncrypted(QDomDocument& doc, uint32_t sid,
                           const QByteArray& iv,
                           const QList<OmemoKey>& keys,
                           const QByteArray& payload)
{
    QDomElement encrypted = doc.createElementNS(
        QString::fromLatin1(NS), QLatin1String("encrypted"));

    QDomElement header = doc.createElementNS(
        QString::fromLatin1(NS), QLatin1String("header"));
    header.setAttribute(QLatin1String("sid"), QString::number(sid));

    for (const OmemoKey& k : keys) {
        QDomElement keyEl = doc.createElementNS(
            QString::fromLatin1(NS), QLatin1String("key"));
        keyEl.setAttribute(QLatin1String("rid"), QString::number(k.rid));
        if (k.prekey)
            keyEl.setAttribute(QLatin1String("prekey"), QLatin1String("true"));
        keyEl.appendChild(doc.createTextNode(
            QString::fromLatin1(k.data.toBase64())));
        header.appendChild(keyEl);
    }

    QDomElement ivEl = doc.createElementNS(
        QString::fromLatin1(NS), QLatin1String("iv"));
    ivEl.appendChild(doc.createTextNode(
        QString::fromLatin1(iv.toBase64())));
    header.appendChild(ivEl);

    encrypted.appendChild(header);

    if (!payload.isEmpty()) {
        QDomElement payloadEl = doc.createElementNS(
            QString::fromLatin1(NS), QLatin1String("payload"));
        payloadEl.appendChild(doc.createTextNode(
            QString::fromLatin1(payload.toBase64())));
        encrypted.appendChild(payloadEl);
    }

    return encrypted;
}

} // namespace XmppOmemo
