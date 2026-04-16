/*
 * serverlistquerier.h
 * Copyright (C) 2007  Remko Troncon
 * Qt5 port: QHttp -> QNetworkAccessManager
 */

#ifndef SERVERLISTQUERIER
#define SERVERLISTQUERIER

#include <QObject>
#include <QStringList>

class QNetworkAccessManager;
class QNetworkReply;

class ServerListQuerier : public QObject
{
	Q_OBJECT

public:
	ServerListQuerier(QObject* parent = nullptr);
	void getList();

signals:
	void listReceived(const QStringList&);
	void error(const QString&);

private slots:
	void replyFinished(QNetworkReply *reply);

private:
	QNetworkAccessManager *nam_;
};

#endif
