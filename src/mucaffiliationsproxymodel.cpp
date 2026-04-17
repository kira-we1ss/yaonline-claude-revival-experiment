#include "mucaffiliationsproxymodel.h"

MUCAffiliationsProxyModel::MUCAffiliationsProxyModel(QObject* parent)
	: QSortFilterProxyModel(parent)
{
	sort(0, Qt::AscendingOrder);
	// Do NOT call setDynamicSortFilter(true) here: in Qt5, setDynamicSortFilter
	// immediately invalidates the filter mapping, which calls filterAcceptsRow(),
	// which calls sourceModel() — but the source model hasn't been set yet at
	// construction time, causing a null-deref crash (mapToSource offset 0x28).
	// setDynamicSortFilter(true) is now deferred to setSourceModel() via the
	// override below so it fires only after the source model is attached.
}

void MUCAffiliationsProxyModel::setSourceModel(QAbstractItemModel* model)
{
	QSortFilterProxyModel::setSourceModel(model);
	// Now that a source model is attached, enable dynamic sort filtering.
	// This must happen AFTER the base setSourceModel() call so that the
	// internal proxy mapping (d->source) is valid before filterAcceptsRow
	// is invoked — Qt5 crashes with null-deref if called before this point.
	if (model)
		setDynamicSortFilter(true);
}

bool MUCAffiliationsProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
	if (!sourceModel())
		return false;

	QModelIndex idx = sourceModel()->index(sourceRow, 0, sourceParent);
	if (!idx.isValid())
		return false;

	if (!idx.parent().isValid())
		return true;

	if (filterRegExp().indexIn(idx.data().toString()) >= 0)
		return true;

	return false;
}
