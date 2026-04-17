#ifndef MUCAFFILIATIONSPROXYMODEL_H
#define MUCAFFILIATIONSPROXYMODEL_H

#include <QSortFilterProxyModel>

class MUCAffiliationsProxyModel : public QSortFilterProxyModel
{
	Q_OBJECT
public:
	MUCAffiliationsProxyModel(QObject* parent = 0);

	// Override to enable dynamicSortFilter only after source model is set,
	// avoiding a Qt5 null-deref in mapToSource during construction.
	void setSourceModel(QAbstractItemModel* sourceModel) override;

protected:
	bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
};

#endif
