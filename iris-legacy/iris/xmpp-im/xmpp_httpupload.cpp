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

#include <QDomElement>
#include <QDomDocument>

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

    QDomElement req = doc()->createElementNS(UPLOAD_NS, "request");
    req.setAttribute("filename", filename_);
    req.setAttribute("size", QString::number(size_));
    if (!contentType_.isEmpty())
        req.setAttribute("content-type", contentType_);
    iq.appendChild(req);

    send(iq);
}

bool JT_HttpUploadSlot::take(const QDomElement& x)
{
    if (!iqVerify(x, Jid(), id()))
        return false;

    if (x.attribute("type") == "result") {
        QDomNodeList slotNodes = x.elementsByTagNameNS(UPLOAD_NS, "slot");
        if (!slotNodes.isEmpty()) {
            QDomElement slotElem = slotNodes.item(0).toElement();
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
