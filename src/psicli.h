#ifndef PSICLI_H
#define PSICLI_H

#include "simplecli.h"
#include "applicationinfo.h"
#include <QApplication>
#include <QMessageBox>
#include <QFileInfo>

class PsiCli : public SimpleCli
{
	Q_OBJECT
public:
	PsiCli() {
#ifndef Q_WS_WIN
		defineSwitch("datadir", tr("Override data directory."));
#endif

		defineSwitch("help", tr("Show this help message and exit."));
		defineAlias("h", "help");
		defineAlias("?", "help");

		defineSwitch("version", tr("Show version information and exit."));
		defineAlias("v", "version");
	}

	void showHelp(int textWidth = 78) {
		QString output;
		output += optionsHelp(textWidth);
		show(output);
	}

	void showVersion() {
		show(QString("%1 %2\nQt %3\n")
			.arg(ApplicationInfo::name()).arg(ApplicationInfo::version())
			.arg(qVersion())
			+ QString(tr("Compiled with Qt %1", "%1 will contain Qt version number"))
			.arg(QT_VERSION_STR));
	}

	void show(const QString& text) {
#ifdef Q_WS_WIN
		QMessageBox::information(0, ApplicationInfo::name(), text);
#else
		puts(text.toUtf8());
#endif
	}

	virtual ~PsiCli() {}
};

#endif
