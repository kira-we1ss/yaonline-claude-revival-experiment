/*
 * xmpp_httpupload.cpp
 * XEP-0363: HTTP File Upload — slot request task
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include "xmpp_httpupload.h"
#include "xmpp_xmlcommon.h"

#include <QDebug>
#include <QDomElement>
#include <QDomDocument>
#include <QTextStream>

using namespace XMPP;

static const char* UPLOAD_NS = "urn:xmpp:http:upload:0";

JT_HttpUploadSlot::JT_HttpUploadSlot(Task* parent)
    : Task(parent), size_(0)
{}

void JT_HttpUploadSlot::setServiceJid(const Jid& j)      { serviceJid_ = j; }
void JT_HttpUploadSlot::setFilename(const QString& n)     { filename_ = n; }
void JT_HttpUploadSlot::setSize(qint64 s)                 { size_ = s; }
void JT_HttpUploadSlot::setContentType(const QString& m)  { contentType_ = m; }

QUrl JT_HttpUploadSlot::putUrl() const                    { return putUrl_; }
QUrl JT_HttpUploadSlot::getUrl() const                    { return getUrl_; }
QMap<QString,QString> JT_HttpUploadSlot::putHeaders() const { return putHeaders_; }

void JT_HttpUploadSlot::onGo()
{
    QDomElement iq = createIQ(doc(), "get", serviceJid_.full(), id());

    // Note: use createElement + setAttribute("xmlns", ...) (not
    // createElementNS) because Iris's send() path serializes QDomDocument
    // children directly — Qt's DOM serialization drops NS-declared
    // attributes unless the namespace was declared on the document root.
    // Matches the pattern used by xmpp_discoinfotask.cpp et al.
    QDomElement req = doc()->createElement("request");
    req.setAttribute("xmlns", UPLOAD_NS);
    req.setAttribute("filename", filename_);
    req.setAttribute("size", QString::number(size_));
    if (!contentType_.isEmpty())
        req.setAttribute("content-type", contentType_);
    iq.appendChild(req);

    // Diagnostic: log the outgoing slot-request so we can see exactly what
    // we send to the upload service. Prosody 0.12+ refusals are almost
    // always due to missing/incorrect attribute names or the request being
    // addressed to the core server JID rather than the upload component
    // sub-JID.
    {
        QString xml;
        QTextStream ts(&xml);
        iq.save(ts, 0);
        qDebug().noquote() << "[HttpUpload] → slot request to"
                           << serviceJid_.full() << ":" << xml;
    }

    send(iq);
}

bool JT_HttpUploadSlot::take(const QDomElement& x)
{
    if (!iqVerify(x, Jid(), id()))
        return false;

    // Diagnostic: log the incoming iq (success or error) verbatim so we
    // can inspect what the upload service said when slot requests fail.
    {
        QString xml;
        QTextStream ts(&xml);
        x.save(ts, 0);
        qDebug().noquote() << "[HttpUpload] ← response:" << xml;
    }

    if (x.attribute("type") == "result") {
        // Find <slot> by tag name + xmlns attribute — elementsByTagNameNS
        // only matches when the server used a real NS declaration rather
        // than an xmlns= attribute (Prosody uses the latter for PEP/XEP
        // responses).
        QDomElement slotElem;
        for (QDomNode n = x.firstChild(); !n.isNull(); n = n.nextSibling()) {
            QDomElement e = n.toElement();
            if (!e.isNull() && e.tagName() == "slot"
                && (e.attribute("xmlns") == UPLOAD_NS || e.namespaceURI() == UPLOAD_NS)) {
                slotElem = e;
                break;
            }
        }
        if (!slotElem.isNull()) {
            QDomElement put = slotElem.firstChildElement("put");
            QDomElement get = slotElem.firstChildElement("get");
            if (!put.isNull()) {
                putUrl_ = QUrl(put.attribute("url"));
                // Optional extra headers (e.g. Authorization, Cookie)
                QDomElement hdr = put.firstChildElement("header");
                while (!hdr.isNull()) {
                    putHeaders_[hdr.attribute("name")] = hdr.text();
                    hdr = hdr.nextSiblingElement("header");
                }
            }
            if (!get.isNull())
                getUrl_ = QUrl(get.attribute("url"));
        }
        setSuccess();
    } else {
        setError(x);
    }
    return true;
}
