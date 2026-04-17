#ifndef XMPP_HTTPUPLOAD_H
#define XMPP_HTTPUPLOAD_H

#include "xmpp_task.h"
#include "xmpp_jid.h"
#include <QUrl>
#include <QMap>
#include <QString>

namespace XMPP {

// XEP-0363: request an upload slot from the server's upload service
class JT_HttpUploadSlot : public Task
{
    Q_OBJECT
public:
    JT_HttpUploadSlot(Task* parent);

    // Set before go(): the upload service JID (from ServerInfoManager::httpUploadService())
    void setServiceJid(const Jid& jid);
    // File metadata for the slot request
    void setFilename(const QString& name);
    void setSize(qint64 size);
    void setContentType(const QString& mime);

    // Results (valid after finished() and success())
    QUrl putUrl() const;
    QUrl getUrl() const;
    QMap<QString,QString> putHeaders() const; // extra headers for the PUT request

    void onGo() override;
    bool take(const QDomElement& x) override;

private:
    Jid     serviceJid_;
    QString filename_;
    qint64  size_;
    QString contentType_;

    QUrl putUrl_, getUrl_;
    QMap<QString,QString> putHeaders_;
};

} // namespace XMPP
#endif
