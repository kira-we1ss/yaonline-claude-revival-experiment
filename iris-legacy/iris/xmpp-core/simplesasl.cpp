/*
 * simplesasl.cpp - Simple SASL implementation
 * Copyright (C) 2003  Justin Karneges
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
 *
 */

#include "simplesasl.h"
#include "scrammessage.h"

#include <QHostAddress>
#include <QStringList>
#include <QList>
#include <qca.h>
#include <QByteArray>
#include <stdlib.h>
#include <QtCrypto>
#include <QDebug>
#include <QScopedPointer>

#ifdef YAPSI
#include <QUrl>
#include <QUrlQuery>
#include <QVariantMap>
#include <QDateTime>
#include "yaonline.h"
#endif

#define SIMPLESASL_PLAIN

namespace XMPP {
struct Prop
{
	QByteArray var, val;
};

class PropList : public QList<Prop>
{
public:
	PropList() : QList<Prop>()
	{
	}

	void set(const QByteArray &var, const QByteArray &val)
	{
		Prop p;
		p.var = var;
		p.val = val;
		append(p);
	}

	QByteArray get(const QByteArray &var)
	{
		for(ConstIterator it = begin(); it != end(); ++it) {
			if((*it).var == var)
				return (*it).val;
		}
		return QByteArray();
	}

	QByteArray toString() const
	{
		QByteArray str;
		bool first = true;
		for(ConstIterator it = begin(); it != end(); ++it) {
			if(!first)
				str += ',';
			if ((*it).var == "realm" || (*it).var == "nonce" || (*it).var == "username" || (*it).var == "cnonce" || (*it).var == "digest-uri" || (*it).var == "authzid")
				str += (*it).var + "=\"" + (*it).val + '\"';
			else 
				str += (*it).var + "=" + (*it).val;
			first = false;
		}
		return str;
	}

	bool fromString(const QByteArray &str)
	{
		PropList list;
		int at = 0;
		while(1) {
			while (at < str.length() && (str[at] == ',' || str[at] == ' ' || str[at] == '\t'))
				  ++at;
			int n = str.indexOf('=', at);
			if(n == -1)
				break;
			QByteArray var, val;
			var = str.mid(at, n-at);
			at = n + 1;
			if(str[at] == '\"') {
				++at;
				n = str.indexOf('\"', at);
				if(n == -1)
					break;
				val = str.mid(at, n-at);
				at = n + 1;
			}
			else {
				n = at;
				while (n < str.length() && str[n] != ',' && str[n] != ' ' && str[n] != '\t')
					++n;
				val = str.mid(at, n-at);
				at = n;
			}
			Prop prop;
			prop.var = var;
			if (var == "qop" || var == "cipher") {
				int a = 0;
				while (a < val.length()) {
					while (a < val.length() && (val[a] == ',' || val[a] == ' ' || val[a] == '\t'))
						++a;
					if (a == val.length())
						break;
					n = a+1;
					while (n < val.length() && val[n] != ',' && val[n] != ' ' && val[n] != '\t')
						++n;
					prop.val = val.mid(a, n-a);
					list.append(prop);
					a = n+1;
				}
			}
			else {
				prop.val = val;
				list.append(prop);
			}

			if(at >= str.size() - 1 || (str[at] != ',' && str[at] != ' ' && str[at] != '\t'))
				break;
		}

		// integrity check
		if(list.varCount("nonce") != 1)
			return false;
		if(list.varCount("algorithm") != 1)
			return false;
		*this = list;
		return true;
	}

	int varCount(const QByteArray &var)
	{
		int n = 0;
		for(ConstIterator it = begin(); it != end(); ++it) {
			if((*it).var == var)
				++n;
		}
		return n;
	}

	QStringList getValues(const QByteArray &var)
	{
		QStringList list;
		for(ConstIterator it = begin(); it != end(); ++it) {
			if((*it).var == var)
				list += (*it).val;
		}
		return list;
	}
};

class SimpleSASLContext : public QCA::SASLContext
{
	Q_OBJECT
public:
		class ParamsMutable
		{
		public:
			/**
			   User is held
			*/
			bool user;

			/**
			   Authorization ID is held
			*/
			bool authzid;

			/**
			   Password is held
			*/
			bool pass;

			/**
			   Realm is held
			*/
			bool realm;
		};
	// core props
	QString service, host;

	// state
	int step;
	bool capable;
	bool allow_plain;
#ifdef YAPSI
	bool allow_xFacebookPlatform;
#endif
	QByteArray out_buf, in_buf;
	QString mechanism_;
	QString out_mech;

	ParamsMutable need;
	ParamsMutable have;
	QString user, authz, realm;
	QCA::SecureArray pass;
	Result result_;
	QCA::SASL::AuthCondition authCondition_;
	QByteArray result_to_net_, result_to_app_;
	int encoded_;
	QScopedPointer<SCRAMMessage> scram_;

	SimpleSASLContext(QCA::Provider* p) : QCA::SASLContext(p)
	{
		reset();
	}

	~SimpleSASLContext()
	{
		reset();
	}

	void reset()
	{
		resetState();

		capable = true;
		allow_plain = false;
#ifdef YAPSI
		allow_xFacebookPlatform = false;
#endif
		need.user = false;
		need.authzid = false;
		need.pass = false;
		need.realm = false;
		have.user = false;
		have.authzid = false;
		have.pass = false;
		have.realm = false;
		user = QString();
		authz = QString();
		pass = QCA::SecureArray();
		realm = QString();
	}

	void resetState()
	{
		out_mech = QString();
		out_buf.resize(0);
		authCondition_ = QCA::SASL::AuthFail;
	}

	virtual void setConstraints(QCA::SASL::AuthFlags flags, int ssfMin, int) {
		if(flags & (QCA::SASL::RequireForwardSecrecy | QCA::SASL::RequirePassCredentials | QCA::SASL::RequireMutualAuth) || ssfMin > 0)
			capable = false;
		else
			capable = true;
		allow_plain = flags & QCA::SASL::AllowPlain;
#ifdef YAPSI
		// AllowXFacebookPlatform removed in QCA 2.3.x; Facebook XMPP dead since 2015.
		allow_xFacebookPlatform = false;
#endif
	}
	
	virtual void setup(const QString& _service, const QString& _host, const QCA::SASLContext::HostPort*, const QCA::SASLContext::HostPort*, const QString&, int) {
		service = _service;
		host = _host;
	}
	
	virtual void startClient(const QStringList &mechlist, bool allowClientSendFirst) {
		Q_UNUSED(allowClientSendFirst);

		mechanism_ = QString();
		{
		// Priority: X-FACEBOOK-PLATFORM > SCRAM-SHA-256 > SCRAM-SHA-1 > PLAIN
		// DIGEST-MD5 removed: deprecated by RFC 6331 (2011) and insecure.
		static const int PRIO_NONE     = 0;
		static const int PRIO_PLAIN    = 1;
		static const int PRIO_SCRAM1   = 2;
		static const int PRIO_SCRAM256 = 3;
		static const int PRIO_FBPLATFORM = 4;
		int bestPrio = PRIO_NONE;

		foreach(QString mech, mechlist) {
#ifdef YAPSI
			if (mech == "X-FACEBOOK-PLATFORM" && allow_xFacebookPlatform && bestPrio < PRIO_FBPLATFORM) {
				mechanism_ = "X-FACEBOOK-PLATFORM";
				bestPrio = PRIO_FBPLATFORM;
				break;
			}
#endif
			if (mech == "SCRAM-SHA-256" && bestPrio < PRIO_SCRAM256) {
				mechanism_ = "SCRAM-SHA-256";
				bestPrio = PRIO_SCRAM256;
			}
			else if (mech == "SCRAM-SHA-1" && bestPrio < PRIO_SCRAM1) {
				mechanism_ = "SCRAM-SHA-1";
				bestPrio = PRIO_SCRAM1;
			}
#ifdef SIMPLESASL_PLAIN
			else if (mech == "PLAIN" && allow_plain && bestPrio < PRIO_PLAIN) {
				mechanism_ = "PLAIN";
				bestPrio = PRIO_PLAIN;
			}
#endif
		}
		}

		if(!capable || mechanism_.isEmpty()) {
			result_ = Error;
			authCondition_ = QCA::SASL::NoMechanism;
			if (!capable)
				qWarning("simplesasl.cpp: Not enough capabilities");
			if (mechanism_.isEmpty()) 
				qWarning("simplesasl.cpp: No mechanism available");
			QMetaObject::invokeMethod(this, "resultsReady", Qt::QueuedConnection);
			return;
		}

		resetState();
		result_ = Continue;
		step = 0;
		tryAgain();
	}

	virtual void nextStep(const QByteArray &from_net) {
		in_buf = from_net;
		tryAgain();
	}

	virtual void tryAgain() {
		// All exits of the method must emit the ready signal
		// so all exits go through a goto ready; 
		if(step == 0) {
			out_mech = mechanism_;

			// SCRAM-SHA-1 / SCRAM-SHA-256 (RFC 5802)
			if (out_mech == "SCRAM-SHA-256" || out_mech == "SCRAM-SHA-1") {
				if(need.user || need.pass) {
					qWarning("simplesasl.cpp: Did not receive necessary auth parameters");
					result_ = Error;
					goto ready;
				}
				if(!have.user)
					need.user = true;
				if(!have.pass)
					need.pass = true;
				if(need.user || need.pass) {
					result_ = Params;
					goto ready;
				}
				QString hashName = (out_mech == "SCRAM-SHA-256") ? "sha256" : "sha1";
				scram_.reset(new SCRAMMessage(hashName, user, pass));
				out_buf = scram_->clientFirstMessage();
				++step;
				result_ = Continue;
				goto ready;
			}

#ifdef SIMPLESASL_PLAIN
			// PLAIN 
			if (out_mech == "PLAIN") {
				// First, check if we have everything
				if(need.user || need.pass) {
					qWarning("simplesasl.cpp: Did not receive necessary auth parameters");
					result_ = Error;
					goto ready;
				}
				if(!have.user)
					need.user = true;
				if(!have.pass)
					need.pass = true;
				if(need.user || need.pass) {
					result_ = Params;
					goto ready;
				}

			// Continue with authentication
			QByteArray plain;
			if (!authz.isEmpty())
				plain += authz.toUtf8();
		   	plain += '\0' + user.toUtf8() + '\0' + pass.toByteArray();
			out_buf.resize(plain.length());
			memcpy(out_buf.data(), plain.data(), out_buf.size());
			}
#endif
			++step;
			if (out_mech == "PLAIN")
				result_ = Success;
			else
				result_ = Continue;
		}
		else if(step == 1) {
			// SCRAM step 1: process server-first-message, send client-final-message
			if (out_mech == "SCRAM-SHA-256" || out_mech == "SCRAM-SHA-1") {
				if (!scram_) {
					result_ = Error;
					authCondition_ = QCA::SASL::BadProtocol;
					goto ready;
				}
				out_buf = scram_->clientFinalMessage(in_buf);
				if (!scram_->isValid()) {
					result_ = Error;
					authCondition_ = QCA::SASL::BadServer;
					goto ready;
				}
				++step;
				result_ = Continue;
				goto ready;
			}

#ifdef YAPSI
			if (out_mech == "X-FACEBOOK-PLATFORM") {
				QString fakeUrl = "http://facebook.com/?" + QString(in_buf);
				QUrl url = QUrl(fakeUrl, QUrl::TolerantMode);
				QUrlQuery urlQuery(url);

				QVariantMap map;
				map["method"]  = urlQuery.queryItemValue("method");
				map["nonce"]   = urlQuery.queryItemValue("nonce");
				map["call_id"] = QDateTime::currentDateTime().toTime_t();
				xFacebookPlatformLogin(map);
				return;
			}
#endif

			// No other mechanisms are supported at step 1; DIGEST-MD5 was removed
			// (deprecated by RFC 6331; SCRAM-SHA-1/256 are the modern replacements).
			result_ = Error;
			authCondition_ = QCA::SASL::NoMechanism;
		}
		/*else if (step == 2) {
			//Commenting this out is Justin's fix for updated QCA.
			out_buf.resize(0);
			result_ = Continue;
			++step;
		}*/
		else {
			// SCRAM step 2: verify server-final-message
			if (out_mech == "SCRAM-SHA-256" || out_mech == "SCRAM-SHA-1") {
				if (!scram_) {
					result_ = Error;
					authCondition_ = QCA::SASL::BadProtocol;
					goto ready;
				}
				out_buf.resize(0);
				if (!scram_->verifyServerFinal(in_buf)) {
					result_ = Error;
					authCondition_ = QCA::SASL::BadServer;
					goto ready;
				}
				result_ = Success;
				goto ready;
			}

			out_buf.resize(0);
			result_ = Success;
		}
ready:
		QMetaObject::invokeMethod(this, "resultsReady", Qt::QueuedConnection);
	}

	virtual void update(const QByteArray &from_net, const QByteArray &from_app) {
		result_to_app_ = from_net;
		result_to_net_ = from_app;
		encoded_ = from_app.size();
		result_ = Success;
		QMetaObject::invokeMethod(this, "resultsReady", Qt::QueuedConnection);
	}

	virtual bool waitForResultsReady(int msecs) {

		// TODO: for now, all operations block anyway
		Q_UNUSED(msecs);
		return true;
	}

	virtual Result result() const {
		return result_;
	}

	virtual QStringList mechlist() const {
		return QStringList();
	}
	
	virtual QString mech() const {
		return out_mech;
	}
	
	virtual bool haveClientInit() const {
		return out_mech == "PLAIN" ||
		       out_mech == "SCRAM-SHA-1" ||
		       out_mech == "SCRAM-SHA-256";
	}
	
	virtual QByteArray stepData() const {
		return out_buf;
	}
	
	virtual QByteArray to_net() {
		return result_to_net_;
	}
	
	virtual int encoded() const {
		return encoded_;
	}
	
	virtual QByteArray to_app() {
		return result_to_app_;
	}

	virtual int ssf() const {
		return 0;
	}

	virtual QCA::SASL::AuthCondition authCondition() const {
		return authCondition_;
	}

	virtual QCA::SASL::Params clientParams() const {
		return QCA::SASL::Params(need.user, need.authzid, need.pass, need.realm);
	}
	
	virtual void setClientParams(const QString *_user, const QString *_authzid, const QCA::SecureArray *_pass, const QString *_realm) {
		if(_user) {
			user = *_user;
			need.user = false;
			have.user = true;
		}
		if(_authzid) {
			authz = *_authzid;
			need.authzid = false;
			have.authzid = true;
		}
		if(_pass) {
			pass = *_pass;
			need.pass = false;
			have.pass = true;
		}
		if(_realm) {
			realm = *_realm;
			need.realm = false;
			have.realm = true;
		}
	}

	virtual QStringList realmlist() const
	{
		// TODO
		return QStringList();
	}

	virtual QString username() const {
		return QString();
	}

	virtual QString authzid() const {
		return QString();
	}

	virtual QCA::Provider::Context* clone() const {
		SimpleSASLContext* s = new SimpleSASLContext(provider());
		// TODO: Copy all the members
		return s;
	}
	
	virtual void startServer(const QString &, bool) {
		result_ =  QCA::SASLContext::Error;
		QMetaObject::invokeMethod(this, "resultsReady", Qt::QueuedConnection);
	}
	virtual void serverFirstStep(const QString &, const QByteArray *) {
		result_ =  QCA::SASLContext::Error;
		QMetaObject::invokeMethod(this, "resultsReady", Qt::QueuedConnection);
	}

public slots:
#ifdef YAPSI
	void xFacebookPlatformLogin(QVariantMap map) {
#ifdef YAPSI_ACTIVEX_SERVER
		YaOnlineHelper::instance()->xFacebookPlatformLogin(map, this, "xFacebookPlatformDataReady");
#else
		QString apiKey     = "API_KEY";
		QString sessionKey = "SESSION_KEY";
		QString secret     = "SECRET";

		map["api_key"] = apiKey;
		map["session_key"] = sessionKey;
		map["v"] = "1.0";

		QStringList signingList;
		QStringList resultList;
		QMapIterator<QString, QVariant> it(map);
		while (it.hasNext()) {
			it.next();

			signingList += QString("%1=%2")
			               .arg(it.key())
			               .arg(it.value().toString());
			resultList += QString("%1=%2")
			               .arg(it.key())
			               .arg(QUrl::toPercentEncoding(it.value().toString()).constData());
		}

		QString signingData = signingList.join("") + secret;
		QString resultData = resultList.join("&");

		QString signature = QCA::Hash("md5").hashToString(signingData.toUtf8());
		resultData += "&sig=" + signature;

		// qWarning("signingData = '%s'", qPrintable(signingData));
		// qWarning("resultData = '%s'", qPrintable(resultData));
		xFacebookPlatformDataReady(resultData);
#endif
	}

	void xFacebookPlatformDataReady(const QString& data) {
		Q_ASSERT(out_mech == "X-FACEBOOK-PLATFORM");
		out_buf = data.toUtf8();
		++step;
		result_ = Success;

		QMetaObject::invokeMethod(this, "resultsReady", Qt::QueuedConnection);
	}
#endif
};

class QCASimpleSASL : public QCA::Provider
{
public:
	QCASimpleSASL() {}
	~QCASimpleSASL() {}

	void init()
	{
	}

	QString name() const {
		return "simplesasl";
	}

	QStringList features() const {
		return QStringList("sasl");
	}

	QCA::Provider::Context* createContext(const QString& cap)
	{
		if(cap == "sasl")
			return new SimpleSASLContext(this);
		return 0;
	}
	int qcaVersion() const
	{
		return QCA_VERSION;
	}
};

QCA::Provider *createProviderSimpleSASL()
{
	return (new QCASimpleSASL);
}

}

#include "simplesasl.moc"
