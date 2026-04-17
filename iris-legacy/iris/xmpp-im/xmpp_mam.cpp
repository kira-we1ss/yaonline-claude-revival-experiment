/*
 * xmpp_mam.cpp — XEP-0313 Message Archive Management (MAM v2)
 * Namespace: urn:xmpp:mam:2  RSM: urn:xmpp:rsm:0
 * Copyright (C) 2026  YaChat Authors
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "xmpp_mam.h"
#include "xmpp_xmlcommon.h"
#include "im.h"
#include "xmpp_stream.h"

#include <QUuid>
#include <QDomElement>
#include <QDateTime>

using namespace XMPP;

static const char* MAM_NS     = "urn:xmpp:mam:2";
static const char* RSM_NS     = "urn:xmpp:rsm:0";
static const char* FWD_NS     = "urn:xmpp:forward:0";
static const char* DELAY_NS   = "urn:xmpp:delay";
static const char* DATAFORM_NS = "jabber:x:data";

// ---------------------------------------------------------------------------
// JT_MAMQuery
// ---------------------------------------------------------------------------

JT_MAMQuery::JT_MAMQuery(Task* parent)
    : Task(parent)
    , max_(50)
    , complete_(false)
    , resultCount_(-1)
    , beforeSet_(false)
{
    queryId_ = buildQueryId();
}

QString JT_MAMQuery::buildQueryId() const
{
    // Produce a short unique query ID such as "mam-3f5a1b2c"
    QString uuid = QUuid::createUuid().toString(); // "{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}"
    return QString("mam-") + uuid.mid(1, 8);
}

void JT_MAMQuery::setWith(const Jid& jid)        { with_ = jid; }
void JT_MAMQuery::setStart(const QDateTime& dt)  { start_ = dt; }
void JT_MAMQuery::setEnd(const QDateTime& dt)    { end_ = dt; }

void JT_MAMQuery::setBefore(const QString& id)
{
    before_ = id;
    beforeSet_ = true;
}

void JT_MAMQuery::setAfter(const QString& id)    { after_ = id; }
void JT_MAMQuery::setMax(int max)                { max_ = max; }
void JT_MAMQuery::setQueryId(const QString& q)   { queryId_ = q; }

QList<Message> JT_MAMQuery::results() const      { return results_; }
bool JT_MAMQuery::isComplete() const             { return complete_; }
QString JT_MAMQuery::lastId() const              { return lastId_; }
QString JT_MAMQuery::firstId() const             { return firstId_; }
int JT_MAMQuery::resultCount() const             { return resultCount_; }
QString JT_MAMQuery::queryId() const             { return queryId_; }

void JT_MAMQuery::onGo()
{
    // Build: <iq type='set' id='...' to='own-bare-jid'>
    //          <query xmlns='urn:xmpp:mam:2' queryid='...'>
    //            [ <x type='submit' xmlns='jabber:x:data'> ... </x> ]
    //            [ <set xmlns='urn:xmpp:rsm:0'> ... </set> ]
    //          </query>
    //        </iq>

    QDomDocument* d = doc();

    QDomElement iq = createIQ(d, "set", client()->jid().bare(), id());

    QDomElement query = d->createElementNS(QString::fromLatin1(MAM_NS), "query");
    query.setAttribute("queryid", queryId_);

    // ---- Optional jabber:x:data filter form ----
    bool hasFilters = with_.isValid() || start_.isValid() || end_.isValid();
    if (hasFilters) {
        QDomElement x = d->createElementNS(QString::fromLatin1(DATAFORM_NS), "x");
        x.setAttribute("type", "submit");

        // FORM_TYPE hidden field — required by XEP-0313
        {
            QDomElement f = d->createElement("field");
            f.setAttribute("var", "FORM_TYPE");
            f.setAttribute("type", "hidden");
            QDomElement v = d->createElement("value");
            v.appendChild(d->createTextNode(QString::fromLatin1(MAM_NS)));
            f.appendChild(v);
            x.appendChild(f);
        }

        if (with_.isValid()) {
            QDomElement f = d->createElement("field");
            f.setAttribute("var", "with");
            QDomElement v = d->createElement("value");
            v.appendChild(d->createTextNode(with_.bare()));
            f.appendChild(v);
            x.appendChild(f);
        }
        if (start_.isValid()) {
            QDomElement f = d->createElement("field");
            f.setAttribute("var", "start");
            QDomElement v = d->createElement("value");
            v.appendChild(d->createTextNode(
                start_.toUTC().toString(Qt::ISODate)));
            f.appendChild(v);
            x.appendChild(f);
        }
        if (end_.isValid()) {
            QDomElement f = d->createElement("field");
            f.setAttribute("var", "end");
            QDomElement v = d->createElement("value");
            v.appendChild(d->createTextNode(
                end_.toUTC().toString(Qt::ISODate)));
            f.appendChild(v);
            x.appendChild(f);
        }
        query.appendChild(x);
    }

    // ---- RSM <set> block (pagination) ----
    bool needRsm = (max_ >= 0) || beforeSet_ || !after_.isEmpty();
    if (needRsm) {
        QDomElement set = d->createElementNS(QString::fromLatin1(RSM_NS), "set");

        if (max_ >= 0) {
            QDomElement maxEl = d->createElement("max");
            maxEl.appendChild(d->createTextNode(QString::number(max_)));
            set.appendChild(maxEl);
        }
        if (beforeSet_) {
            QDomElement beforeEl = d->createElement("before");
            if (!before_.isEmpty())  // non-empty = cursor value
                beforeEl.appendChild(d->createTextNode(before_));
            // empty before_ means "before the last" (request last page)
            set.appendChild(beforeEl);
        }
        if (!after_.isEmpty()) {
            QDomElement afterEl = d->createElement("after");
            afterEl.appendChild(d->createTextNode(after_));
            set.appendChild(afterEl);
        }
        query.appendChild(set);
    }

    iq.appendChild(query);
    send(iq);
}

bool JT_MAMQuery::take(const QDomElement& x)
{
    // ---- Case 1: IQ result/error — the final <fin> stanza ----
    if (x.tagName() == "iq" && x.attribute("id") == id()) {
        if (x.attribute("type") == "result") {
            QDomElement fin = x.elementsByTagNameNS(
                QString::fromLatin1(MAM_NS), "fin").item(0).toElement();
            if (!fin.isNull()) {
                complete_ = (fin.attribute("complete") == "true");
                QDomElement set = fin.elementsByTagNameNS(
                    QString::fromLatin1(RSM_NS), "set").item(0).toElement();
                if (!set.isNull()) {
                    lastId_   = set.firstChildElement("last").text();
                    firstId_  = set.firstChildElement("first").text();
                    QString countStr = set.firstChildElement("count").text();
                    if (!countStr.isEmpty())
                        resultCount_ = countStr.toInt();
                }
            }
            setSuccess();
        } else {
            setError(x);
        }
        return true;
    }

    // ---- Case 2: Intermediate <message> push with <result queryid='...'> ----
    if (x.tagName() == "message") {
        QDomElement result = x.elementsByTagNameNS(
            QString::fromLatin1(MAM_NS), "result").item(0).toElement();
        if (result.isNull())
            return false;
        if (result.attribute("queryid") != queryId_)
            return false;  // belongs to a different query

        QDomElement forwarded = result.elementsByTagNameNS(
            QString::fromLatin1(FWD_NS), "forwarded").item(0).toElement();
        if (forwarded.isNull())
            return true;  // malformed but ours — consume

        QDomElement innerMsg = forwarded.firstChildElement("message");
        if (innerMsg.isNull())
            return true;

        // Parse the forwarded <message> element using the Stream's stanza factory.
        // client()->stream() returns a Stream& which has createStanza(QDomElement).
        Stanza s = client()->stream().createStanza(innerMsg);
        Message m;
        if (m.fromStanza(s, 0)) {
            m.setSpooled(true);  // mark as archived / historical

            // Override timestamp from <delay xmlns='urn:xmpp:delay'>
            QDomElement delay = forwarded.elementsByTagNameNS(
                QString::fromLatin1(DELAY_NS), "delay").item(0).toElement();
            if (!delay.isNull()) {
                QString stamp = delay.attribute("stamp");
                if (!stamp.isEmpty()) {
                    // Prosody sends ISO-8601 UTC timestamps: "2024-01-15T12:34:56Z"
                    QDateTime ts = QDateTime::fromString(stamp, Qt::ISODate);
                    if (!ts.isValid()) {
                        // Try without trailing Z by normalizing
                        ts = QDateTime::fromString(stamp.left(19), "yyyy-MM-ddTHH:mm:ss");
                        ts.setTimeSpec(Qt::UTC);
                    } else {
                        ts.setTimeSpec(Qt::UTC);
                    }
                    if (ts.isValid())
                        m.setTimeStamp(ts.toLocalTime());
                }
            }

            results_.append(m);
            emit messageReceived(m);
        }
        return true;  // consumed
    }

    return false;
}
