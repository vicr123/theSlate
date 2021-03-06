#include "statusmodel.h"

#include "gitintegration.h"
#include <QPainter>
#include <QMouseEvent>

struct StatusModelPrivate {
    GitIntegration* gi;

    QList<StatusModel::Status> currentStatus;
};

StatusModel::StatusModel(GitIntegration* integration, QObject *parent)
    : QAbstractListModel(parent)
{
    d = new StatusModelPrivate();
    d->gi = integration;

    reloadStatus();
}

StatusModel::~StatusModel() {
    delete d;
}

int StatusModel::rowCount(const QModelIndex &parent) const
{
    // For list models only the root node (an invalid parent) should return the list's size. For all
    // other (valid) parents, rowCount() should return 0 so that it does not become a tree model.
    if (parent.isValid()) {
        return 0;
    }

    // FIXME: Implement me!
    return d->currentStatus.count();
}

QVariant StatusModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    // FIXME: Implement me!
    Status s = d->currentStatus.value(index.row());
    switch (role) {
        case Qt::DisplayRole:
            return s.file;
        case Qt::UserRole: {
            if (s.status.contains('U') || s.status == "AA" || s.status == "DD") {
                //Merge conflict
                return tr("conflicting");
            } else {
                QChar testChar;
                if (s.status.at(0) == ' ') {
                    testChar = s.status.at(1);
                } else {
                    testChar = s.status.at(0);
                }

                switch (testChar.toLatin1()) {
                    case '?': return tr("untracked");
                    case 'M': return tr("modified");
                    case 'A': return tr("added");
                    case 'D': return tr("deleted");
                    case 'R': return tr("renamed");
                    case 'C': return tr("copied");
                }
            }
        }
        case Qt::UserRole + 1:
            return QVariant::fromValue(s);
        case Qt::CheckStateRole:
            if (!canChangeCheckState(index.row())) {
                //Can't have partial commits during a conflict
                return true;
            } else {
                return s.staged;
            }
    }
    return QVariant();
}

bool StatusModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    if (role == Qt::CheckStateRole) {
        Status s = d->currentStatus.value(index.row());
        s.staged = value.toBool();
        d->currentStatus.replace(index.row(), s);
        emit itemCheckedCountChanged();
        return true;
    }
    return false;
}

Qt::ItemFlags StatusModel::flags(const QModelIndex &index) const {
    if (!index.isValid()) return Qt::NoItemFlags;

    Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (canChangeCheckState(index.row())) {
        flags |= Qt::ItemIsUserCheckable;
    }
    return flags;
}

bool StatusModel::canChangeCheckState(int row) const {
    if (!d->gi->isConflicting()) return true;
    if (d->currentStatus.value(row).status == "??") return true;

    return false;
}

void StatusModel::reloadStatus() {
    d->currentStatus.clear();

    QList<QByteArray> files = d->gi->status().split('\0');
    for (QByteArray file : files) {
        if (file.isEmpty()) continue;
        QString status = file.left(2);
        QString filename = file.mid(3);

        Status s;
        s.file = filename;
        s.status = status;

        if (status == "??") {
            s.staged = false;
        } else {
            s.staged = true;
        }
        d->currentStatus.append(s);
    }
    emit dataChanged(index(0), index(rowCount()));
}

void StatusModel::selectAllTrackedFiles(bool select) {
    for (int i = 0; i < d->currentStatus.count(); i++) {
        Status s = d->currentStatus.at(i);
        if (s.status != "??") s.staged = select;
        d->currentStatus.replace(i, s);
    }
    emit dataChanged(index(0), index(rowCount()));
    emit itemCheckedCountChanged();
}

int StatusModel::numberOfTrackedFiles() {
    int num = 0;
    for (Status s : d->currentStatus) {
        if (s.status != "??") num++;
    }
    return num;
}

int StatusModel::numberOfSelectedFiles() {
    int num = 0;
    for (Status s : d->currentStatus) {
        if (s.staged) num++;
    }
    return num;
}

Qt::CheckState StatusModel::allTrackedFilesSelected() {
    bool allSelected = true;
    bool noneSelected = true;
    bool atLeastOneExists = false;
    for (int i = 0; i < d->currentStatus.count(); i++) {
        Status s = d->currentStatus.at(i);
        if (s.status != "??") {
            atLeastOneExists = true;
            if (s.staged) noneSelected = false;
            if (!s.staged) allSelected = false;
        }
        d->currentStatus.replace(i, s);
    }

    if (!atLeastOneExists) {
        return Qt::Unchecked;
    } else if (allSelected) {
        return Qt::Checked;
    } else if (noneSelected) {
        return Qt::Unchecked;
    } else {
        return Qt::PartiallyChecked;
    }
}

QSize StatusModelDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const {
//    QRect checkRect = checkboxRect(option);
    QSize normalSize = option.rect.size();
    normalSize.setHeight(SC_DPI(24));

    return normalSize;
}

void StatusModelDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {
    QColor foregroundCol;
    QColor grayCol;
    QColor backgroundCol;

    if (option.state & QStyle::State_Selected) {
        foregroundCol = option.palette.color(QPalette::HighlightedText);
        grayCol = option.palette.color(QPalette::Disabled, QPalette::HighlightedText);
        backgroundCol = option.palette.color(QPalette::Highlight);
    } else {
        foregroundCol = option.palette.color(QPalette::Text);
        grayCol = option.palette.color(QPalette::Disabled, QPalette::Text);
        backgroundCol = option.palette.color(QPalette::Base);
    }

    QFont f = option.font;
    QFontMetrics fm = option.fontMetrics;

    painter->setBrush(backgroundCol);
    painter->setPen(Qt::transparent);
    painter->drawRect(option.rect);
    painter->setBrush(Qt::transparent);

    //Draw checkbox
    QRect checkRect = checkboxRect(option);

    QStyleOptionButton checkboxOptions;
    checkboxOptions.rect = checkRect;
    if (index.data(Qt::CheckStateRole).toBool()) {
        checkboxOptions.state |= QStyle::State_On;
    } else {
        checkboxOptions.state |= QStyle::State_Off;
    }

    if (index.flags() & Qt::ItemIsUserCheckable) {
        checkboxOptions.state |= QStyle::State_Enabled;
    }

    QApplication::style()->drawControl(QStyle::CE_CheckBox, &checkboxOptions, painter);

    QRect informationRect = option.rect;
    informationRect.setX(checkRect.right() + SC_DPI(3));
    informationRect.moveTop(informationRect.top() + SC_DPI(3));
    informationRect.setHeight(informationRect.height() - SC_DPI(6));
    informationRect.setWidth(informationRect.width() - SC_DPI(3));

    //Draw filename
    QString name = index.data(Qt::DisplayRole).toString();
    QRect nameRect;
    nameRect.setWidth(fm.horizontalAdvance(name) + 1);
    nameRect.setHeight(fm.height());
    nameRect.moveCenter(informationRect.center());
    nameRect.moveLeft(informationRect.left());

    painter->setPen(foregroundCol);
    painter->setFont(f);
    painter->drawText(nameRect, name);

    //Draw status
    QString status = " · " + index.data(Qt::UserRole).toString();
    QRect statusRect = nameRect;
    statusRect.moveLeft(nameRect.right());
    statusRect.setWidth(fm.horizontalAdvance(status) + 1);

    painter->setPen(grayCol);
    painter->drawText(statusRect, status);
}

bool StatusModelDelegate::editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index) {
    if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent* e = static_cast<QMouseEvent*>(event);
        if (e->button() == Qt::LeftButton && checkboxRect(option).contains(e->pos())) {
            bool oldChecked = index.data(Qt::CheckStateRole).toBool();
            model->setData(index, !oldChecked, Qt::CheckStateRole);
            return true;
        }
    }
    return QAbstractItemDelegate::editorEvent(event, model, option, index);
}

QRect StatusModelDelegate::checkboxRect(const QStyleOptionViewItem &option) const {
    QRect checkRect;
    checkRect.moveLeft(option.rect.left() + SC_DPI(3));
    checkRect.moveTop(option.rect.top() + SC_DPI(3));
    checkRect.setHeight(option.rect.height() - SC_DPI(6));
    checkRect.setWidth(checkRect.height());
    return checkRect;
}
