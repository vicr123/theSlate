#include "commitsmodel.h"

#include "gitintegration.h"
#include <QPainter>

extern void tintImage(QImage &image, QColor tint);

struct CommitsModelPrivate {
    GitIntegration* gi;

    GitIntegration::CommitList shownCommits;

    struct Action {
        QString text;
        QString description;
        QIcon icon;
        QString key;
    };
    QList<Action> actions;
};

CommitsModel::CommitsModel(GitIntegration* integration, QObject *parent)
    : QAbstractListModel(parent)
{
    d = new CommitsModelPrivate();
    d->gi = integration;

    connect(d->gi, &GitIntegration::headCommitChanged, this, &CommitsModel::reloadData);
    connect(d->gi, &GitIntegration::commitInformationAvailable, this, [=](QString commitHash) {
        for (int i = 0; i < d->shownCommits.count(); i++) {
            if (d->shownCommits.at(i)->hash == commitHash) {
                emit dataChanged(index(i), index(i));
                return;
            }
        }
    });

    reloadData();
}

CommitsModel::~CommitsModel() {
    delete d;
}

int CommitsModel::rowCount(const QModelIndex &parent) const
{
    // For list models only the root node (an invalid parent) should return the list's size. For all
    // other (valid) parents, rowCount() should return 0 so that it does not become a tree model.
    if (parent.isValid()) {
        return 0;
    }

    return d->shownCommits.count() + d->actions.count();
}

QVariant CommitsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) return QVariant();

    if (index.row() < d->actions.count()) {
        CommitsModelPrivate::Action action = d->actions.at(index.row());
        switch (role) {
            case Qt::DisplayRole:
                return action.text;
            case Qt::DecorationRole:
                return action.icon;
            case Qt::UserRole:
                return action.description;
            case Qt::UserRole + 1:
                return action.key;
        }
    } else {
        GitIntegration::CommitPointer commit = d->shownCommits.at(index.row() - d->actions.count());
        switch (role) {
            case Qt::DisplayRole:
                return commit->hash;
            case Qt::UserRole:
                return QVariant::fromValue(commit);
        }
    }

    return QVariant();
}

void CommitsModel::reloadData() {
    d->shownCommits.clear();
    d->actions.clear();
    emit dataChanged(index(0), index(rowCount()));

    if (!d->gi->needsInit()) {
        //Cache the HEAD commit
        d->gi->getCommit("HEAD", true, true);

        d->gi->commits()->then([=](GitIntegration::CommitList commits) {
            d->shownCommits = commits;
            emit dataChanged(index(0), index(rowCount()));
        });

        //Create actions
        QString branchName;
        if (d->gi->branch().isNull()) {
            branchName = "...";
        } else {
            branchName = d->gi->branch()->name;
        }

        d->actions.append({tr("New Commit"), tr("Create new commit to %1").arg(branchName), QIcon::fromTheme("list-add", QIcon(":/icons/list-add.svg")), "new"});

        //Depending on the status of the repository, do different things
        int pull = d->gi->commitsPendingPull();
        int push = d->gi->commitsPendingPush();
        if (d->gi->upstreamBranch().isNull()) {
            d->actions.append({tr("No Upstream branch"), tr("No upstream branch has been configured."), QIcon(), "no-upstream"});
        } else if (pull > 0) {
            d->actions.append({tr("Pull Remote Changes"), tr("%n pending incoming commits", nullptr, pull), QIcon::fromTheme("go-down", QIcon(":/icons/go-down.svg")), "pull"});
        } else if (push > 0) {
            d->actions.append({tr("Push Local Changes"), tr("%n pending outgoing commits", nullptr, push), QIcon::fromTheme("go-up", QIcon(":/icons/go-up.svg")), "push"});
        } else {
            d->actions.append({tr("Up to date"), tr("Your local repository is up to date"), QIcon::fromTheme("dialog-ok", QIcon(":/icons/dialog-ok.svg")), "up-to-date"});
        }
    }
}

QSize CommitsModelDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const {
    QSize size;
    size.setWidth(option.rect.width());

    int height = 6; //padding
    height += option.fontMetrics.height() * 2;
    size.setHeight(height);
    return size;
}

void CommitsModelDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {
    GitIntegration::CommitPointer commit;
    commit = index.data(Qt::UserRole).value<GitIntegration::CommitPointer>();

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

    QRect informationRect = option.rect;
    informationRect.setX(informationRect.height());
    informationRect.moveTop(informationRect.top() + 3);
    informationRect.setHeight(informationRect.height() - 6);
    informationRect.setWidth(informationRect.width() - 3);

    //Draw any decorations
    QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
    if (!icon.isNull()) {
        QRect iconRect = option.rect;
        iconRect.setWidth(informationRect.left());

        QRect realRect;
        realRect.setSize(QSize(16, 16) * theLibsGlobal::getDPIScaling());
        realRect.moveCenter(iconRect.center());

        QImage image = icon.pixmap(realRect.size()).toImage();
        tintImage(image, foregroundCol);
        painter->drawImage(realRect, image);
    }

    //Draw short hash
    if (!commit.isNull()) {
        QString shortHash = commit->hash.left(7);
        QRect shortHashRect;
        shortHashRect.setTop(informationRect.top());
        shortHashRect.setWidth(fm.width(shortHash));
        shortHashRect.moveRight(informationRect.right());
        shortHashRect.setHeight(fm.height());

        painter->setFont(f);
        painter->setPen(grayCol);
        painter->drawText(shortHashRect, shortHash);

        //Make sure the commit is populated further before trying to draw anything else
        if (!commit->populated) {
            //Populate the commit
            commit = gi->getCommit(commit->hash, true, false);
        }
    }

    QString committer;
    QString commitMessage;
    if (commit.isNull()) {
        committer = index.data(Qt::DisplayRole).toString();
        commitMessage = index.data(Qt::UserRole).toString();
    } else {
        committer = commit->committer;
        commitMessage = commit->message;
    }

    //Draw committer's name
    QRect nameRect;
    nameRect.setTopLeft(informationRect.topLeft());
    nameRect.setWidth(informationRect.width());
    nameRect.setHeight(fm.height());

    QFont boldFont = f;
    boldFont.setBold(true);
    painter->setPen(foregroundCol);
    painter->setFont(boldFont);
    painter->drawText(nameRect, QFontMetrics(boldFont).elidedText(committer, Qt::ElideRight, nameRect.width()));

    //Draw commit message
    QRect commitMessageRect;
    commitMessageRect.setTop(nameRect.bottom());
    commitMessageRect.setLeft(informationRect.left());
    commitMessageRect.setWidth(informationRect.width());
    commitMessageRect.setHeight(fm.height());

    painter->setFont(f);
    painter->drawText(commitMessageRect, fm.elidedText(commitMessage, Qt::ElideRight, commitMessageRect.width()));
}