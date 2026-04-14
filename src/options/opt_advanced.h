#ifndef OPT_ADVANCED_H
#define OPT_ADVANCED_H

#include "optionstab.h"

class QWidget;
struct Options;

class OptionsTabAdvanced : public OptionsTab
{
	Q_OBJECT
public:
	OptionsTabAdvanced(QObject *parent);
	~OptionsTabAdvanced();

	QWidget *widget();
	void applyOptions(Options *opt);
	void restoreOptions(const Options *opt);

protected:
	bool spellCheckerAvailable() const;

private:
	QWidget *w;
};

#endif
