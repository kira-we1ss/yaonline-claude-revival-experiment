/*
 * xmpp_mam.h — XEP-0313 Message Archive Management (MAM v2)
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

#ifndef XMPP_MAM_H
#define XMPP_MAM_H

#include "xmpp_task.h"
#include "xmpp_message.h"
#include <QDateTime>
#include <QList>

namespace XMPP {

class JT_MAMQuery : public Task
{
    Q_OBJECT
public:
    explicit JT_MAMQuery(Task* parent);

    // Optional filter: only messages to/from this JID (bare)
    void setWith(const Jid& jid);
    // Optional filter: only messages after this timestamp
    void setStart(const QDateTime& dt);
    // Optional filter: only messages before this timestamp
    void setEnd(const QDateTime& dt);
    // RSM: request messages before this result id (for backward pagination)
    // Pass empty string for "before the very last message"
    void setBefore(const QString& id);
    // RSM: request messages after this result id (for forward pagination)
    void setAfter(const QString& id);
    // Max results per page (default 50; -1 = don't include <max>)
    void setMax(int max);
    // QueryId to correlate push results (auto-generated if not called)
    void setQueryId(const QString& qid);

    // Results
    QList<Message> results() const;
    bool isComplete() const;    // true when <fin complete='true'/>
    QString lastId() const;     // RSM <last> value for next page
    QString firstId() const;    // RSM <first> value
    int resultCount() const;    // RSM <count> value (-1 if unknown)
    QString queryId() const;    // the queryid used in this query

    // Task interface
    void onGo() override;
    bool take(const QDomElement& x) override;

signals:
    // Emitted for each incoming result stanza during the query
    void messageReceived(const XMPP::Message& msg);

private:
    QString buildQueryId() const;

    Jid       with_;
    QDateTime start_, end_;
    QString   before_;   // null = not set; empty string = "before last"
    QString   after_;
    int       max_;
    QString   queryId_;

    QList<Message> results_;
    bool      complete_;
    QString   lastId_, firstId_;
    int       resultCount_;
    bool      beforeSet_;       // track whether setBefore() was called
};

} // namespace XMPP

#endif // XMPP_MAM_H
