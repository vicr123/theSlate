#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "exitsavedialog.h"
#include <QMimeData>
#include <QFileIconProvider>
#include <tmessagebox.h>
#include <QLineEdit>
#include <ttoast.h>
#include <QDesktopServices>
#include "settingsdialog.h"
#include "picturetabbar.h"
#include <QInputDialog>
#include <QShortcut>
#include <QScroller>
#include "plugins/pluginmanager.h"
#include "managers/recentfilesmanager.h"
#include "managers/updatemanager.h"
#include "textwidget.h"
#include <taboutdialog.h>
#include <tapplication.h>

#include <Repository>
#include <SyntaxHighlighter>
#include <Definition>
#include <Theme>

#ifdef Q_OS_MAC
    extern void setToolbarItemWidget(QMacToolBarItem* item, QWidget* widget);
#endif

extern QLinkedList<QString> clipboardHistory;
extern PluginManager* plugins;
extern RecentFilesManager* recentFiles;
extern UpdateManager* updateManager;
extern KSyntaxHighlighting::Repository* highlightRepo;

QList<MainWindow*> MainWindow::openWindows = QList<MainWindow*>();

struct MainWindowPrivate {
    QFileSystemModel* fileModel;
    QString currentProjectFile = "";
    QString projectType = "";
    QSettings settings;
    QTabBar* tabBar;
    QToolButton* menuButton;
    QAction* menuAction = nullptr;
    QMap<TextWidget*, TopNotification*> primaryTopNotifications;

    QUrl previouslyOpenLocalFile;
};

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    d = new MainWindowPrivate();

    openWindows.append(this);

    ui->mainToolBar->setIconSize(ui->mainToolBar->iconSize() * theLibsGlobal::getDPIScaling());

    //This causes problems with opening files in new tabs
    //ui->tabs->setCurrentAnimation(tStackedWidget::SlideHorizontal);

    for (FileBackendFactory* factory : plugins->fileBackends()) {
        if (factory != plugins->getLocalFileBackend()) {
            QAction* openAction = factory->makeOpenAction(this, std::bind(&MainWindow::getOpenOption, this, std::placeholders::_1));
            ui->menuOpenFrom->addAction(openAction);
        }
        connect(factory, &FileBackendFactory::openFile, [=](FileBackend* backend) {
            newTab(backend);
        });
    }

    #ifdef Q_OS_WIN
        //Set up special palette for Windows
        QPalette pal = this->palette();
        pal.setColor(QPalette::Window, QColor(255, 255, 255));
        this->setPalette(pal);
        //ui->tabFrame->setPalette(pal);
    #endif

    #ifdef Q_OS_MAC
        //Set up Mac toolbar
        ui->mainToolBar->setVisible(false);
        ui->mainToolBar->setParent(nullptr);
        ui->actionUse_Menubar->setVisible(false); //Menu bar is always visible on macOS

        QMacToolBar* toolbar = new QMacToolBar("mainwindow-toolbar");

        QList<QMacToolBarItem*> allowedItems;

        QMacToolBarItem* newItem = new QMacToolBarItem();
        newItem->setText(tr("New"));
        newItem->setIcon(QApplication::style()->standardIcon(QStyle::SP_FileIcon));
        newItem->setProperty("name", "new");
        connect(newItem, &QMacToolBarItem::activated, this, &MainWindow::on_actionNew_triggered);
        allowedItems.append(newItem);

        QMacToolBarItem* openItem = new QMacToolBarItem();
        openItem->setText(tr("Open"));
        openItem->setIcon(QApplication::style()->standardIcon(QStyle::SP_DialogOpenButton));
        openItem->setProperty("name", "open");
        connect(openItem, &QMacToolBarItem::activated, this, &MainWindow::on_actionOpen_triggered);
        allowedItems.append(openItem);

        QMacToolBarItem* saveItem = new QMacToolBarItem();
        saveItem->setText(tr("Save"));
        saveItem->setIcon(QApplication::style()->standardIcon(QStyle::SP_DialogSaveButton));
        saveItem->setProperty("name", "save");
        connect(saveItem, &QMacToolBarItem::activated, this, &MainWindow::on_actionSave_triggered);
        allowedItems.append(saveItem);

        ui->tabFrame->setVisible(false);

        d->tabBar = new QTabBar(this);
        d->tabBar->setDocumentMode(true);
        d->tabBar->setTabsClosable(true);
        d->tabBar->setMovable(true);
        d->tabBar->setUsesScrollButtons(true);
        d->tabBar->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        connect(d->tabBar, &QTabBar::currentChanged, [=](int index) {
            ui->tabs->setCurrentIndex(index);
        });
        connect(d->tabBar, &QTabBar::tabCloseRequested, [=](int index) {
            ui->tabs->setCurrentIndex(index);
            closeCurrentTab();
        });
        connect(d->tabBar, &QTabBar::tabMoved, [=](int from, int to) {
            QWidget* w = ui->tabs->widget(from);
            ui->tabs->removeWidget(w);
            ui->tabs->insertWidget(to, w);
        });
        ((QBoxLayout*) ui->centralWidget->layout())->insertWidget(0, d->tabBar);

        /*PictureTabBar* b = new PictureTabBar(this);

        QMacToolBarItem* tabBarItem = new QMacToolBarItem();
        setToolbarItemWidget(tabBarItem, b);
        allowedItems.append(tabBarItem);*/

        toolbar->setAllowedItems(allowedItems);
        toolbar->setItems(allowedItems);

        toolbar->attachToWindow(this->windowHandle());
        setupMacOS();
    #else
        //Set up single menu except on macOS
        QMenu* editMenu = new QMenu();
        editMenu->setTitle(tr("Edit"));
        editMenu->addAction(ui->actionComment);
        editMenu->addSeparator();
        editMenu->addAction(ui->actionUppercase);
        editMenu->addAction(ui->actionLowercase);
        editMenu->addAction(ui->actionTitle_Case);

        QMenu* singleMenu = new QMenu();
        singleMenu->addAction(ui->actionNew);
        singleMenu->addAction(ui->actionNew_Window);
        singleMenu->addSeparator();
        singleMenu->addAction(ui->actionOpen);
        singleMenu->addMenu(ui->menuOpenFrom);
        singleMenu->addMenu(ui->menuOpen_Recent);
        singleMenu->addSeparator();
        singleMenu->addAction(ui->actionSave);
        singleMenu->addAction(ui->actionSave_As);
        singleMenu->addAction(ui->actionSave_All);
        singleMenu->addAction(ui->actionRevert);
        singleMenu->addSeparator();
        singleMenu->addAction(ui->actionUndo);
        singleMenu->addAction(ui->actionRedo);
        singleMenu->addMenu(editMenu);
        singleMenu->addSeparator();
        singleMenu->addAction(ui->actionCut);
        singleMenu->addAction(ui->actionCopy);
        singleMenu->addAction(ui->actionPaste);
        singleMenu->addMenu(ui->menuPaste_from_Clipboard_History);
        singleMenu->addMenu(ui->menuTransform);
        singleMenu->addSeparator();
        singleMenu->addAction(ui->actionPrint);
        singleMenu->addAction(ui->actionFind_and_Replace);
        singleMenu->addAction(ui->actionSelect_All);
        singleMenu->addMenu(ui->menuOpen_Auxiliary_Pane);
        singleMenu->addSeparator();
        singleMenu->addAction(ui->actionChange_Syntax_Highlighting);
        singleMenu->addMenu(ui->menuWindow);
        singleMenu->addMenu(ui->menuGo);
        singleMenu->addSeparator();
        singleMenu->addAction(ui->actionSettings);
        singleMenu->addMenu(ui->menuHelp);
        singleMenu->addAction(ui->actionExit);

        d->menuButton = new QToolButton();
        d->menuButton->setPopupMode(QToolButton::InstantPopup);
        d->menuButton->setMenu(singleMenu);
        d->menuButton->setArrowType(Qt::NoArrow);
        d->menuButton->setIcon(QIcon::fromTheme("theslate", QIcon(":/icons/icon.svg")));
        d->menuButton->setIconSize(ui->mainToolBar->iconSize());
        d->menuAction = ui->mainToolBar->insertWidget(ui->actionNew, d->menuButton);
        connect(updateManager, &UpdateManager::updateAvailable, [=] {
            //Create icon to notify user that an update is available

            //Change the theSlate toolbar icon
            {
                QPixmap pixmap = QIcon::fromTheme("theslate", QIcon(":/icons/icon.svg")).pixmap(ui->mainToolBar->iconSize());
                QPainter p(&pixmap);
                p.setRenderHint(QPainter::Antialiasing);

                QSize iconSize = ui->mainToolBar->iconSize() / 2;
                QPoint iconLoc(iconSize.width(), iconSize.height());
                QRect circleRect(iconLoc, iconSize);
                p.setPen(Qt::transparent);
                p.setBrush(QColor(200, 0, 0));
                p.drawEllipse(circleRect);

                d->menuButton->setIcon(QIcon(pixmap));
            }

            //Change the help menu icon
            if (!ui->actionUse_Menubar->isChecked()) {
                int size = QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize);
                QImage image = QIcon::fromTheme("help-contents", QIcon(":/icons/help-contents.svg")).pixmap(size, size).toImage();
                QPainter p(&image);
                p.setRenderHint(QPainter::Antialiasing);

                p.setCompositionMode(QPainter::CompositionMode_SourceAtop);
                p.fillRect(0, 0, image.width(), image.height(), QColor(Qt::white));

                p.setCompositionMode(QPainter::CompositionMode_SourceOver);
                QSize iconSize = QSize(size, size) / 2;
                QPoint iconLoc(iconSize.width(), iconSize.height());
                QRect circleRect(iconLoc, iconSize);
                p.setPen(Qt::transparent);
                p.setBrush(QColor(200, 0, 0));
                p.drawEllipse(circleRect);

                ui->menuHelp->setIcon(QIcon(QPixmap::fromImage(image)));
            }
        });

        ui->actionUse_Menubar->setChecked(d->settings.value("appearance/menubar", false).toBool());
        on_actionUse_Menubar_toggled(d->settings.value("appearance/menubar", false).toBool());
    #endif

    connect(ui->menuPaste_from_Clipboard_History, &QMenu::aboutToShow, [=] {
        ui->menuPaste_from_Clipboard_History->clear();

        if (clipboardHistory.size() == 0) {
            QAction* action = new QAction();
            action->setText(tr("No Clipboard History available"));
            action->setEnabled(false);
            ui->menuPaste_from_Clipboard_History->addAction(action);
        } else {
            QFontMetrics metrics = ui->menuPaste_from_Clipboard_History->fontMetrics();
            for (auto i = clipboardHistory.constBegin(); i != clipboardHistory.constEnd(); i++) {
                QString text = *i;

                //Remove any line breaks
                text = text.replace("\n", "");
                text = text.replace("\r", "");

                //Add the action
                QAction* action = new QAction();
                action->setText(metrics.elidedText(text, Qt::ElideRight, 500 * theLibsGlobal::getDPIScaling()));
                action->setEnabled(currentEditor() != nullptr);
                connect(action, &QAction::triggered, [=] {
                    currentEditor()->insertPlainText(*i);
                });
                ui->menuPaste_from_Clipboard_History->addAction(action);
            }
        }
    });
    connect(ui->menuOpen_Auxiliary_Pane, &QMenu::aboutToShow, [=] {
        ui->menuOpen_Auxiliary_Pane->clear();

        if (currentEditor() == nullptr) {
            QAction* action = ui->menuOpen_Auxiliary_Pane->addAction(tr("No opened files"));
            action->setEnabled(false);
        } else {
            QList<AuxiliaryPaneCapabilities> panes = plugins->auxiliaryPanesForUrl(currentEditor()->fileUrl());
            if (panes.count() == 0) {
                QAction* action = ui->menuOpen_Auxiliary_Pane->addAction(tr("No supported auxiliary panes"));
                action->setEnabled(false);
            } else {
                for (AuxiliaryPaneCapabilities pane : panes) {
                    ui->menuOpen_Auxiliary_Pane->addAction(pane.name, [=] {
                        currentDocument()->openAuxPane(pane.makePane());
                    });
                }
            }

            ui->menuOpen_Auxiliary_Pane->addSeparator();
            ui->menuOpen_Auxiliary_Pane->addMenu(ui->menuAll_Auxiliary_Panes);
        }
    });
    connect(ui->menuAll_Auxiliary_Panes, &QMenu::aboutToShow, [=] {
        ui->menuAll_Auxiliary_Panes->clear();

        QList<AuxiliaryPaneCapabilities> panes = plugins->auxiliaryPanes();
        if (panes.count() == 0) {
            QAction* action = ui->menuAll_Auxiliary_Panes->addAction(tr("No auxiliary panes"));
            action->setEnabled(false);
        } else {
            for (AuxiliaryPaneCapabilities pane : panes) {
                ui->menuAll_Auxiliary_Panes->addAction(pane.name, [=] {
                    currentDocument()->openAuxPane(pane.makePane());
                });
            }
        }
    });

    QShortcut* pasteHistory = new QShortcut(QKeySequence(tr("Ctrl+Shift+V")), this);
    connect(pasteHistory, &QShortcut::activated, [=] {
        if (currentEditor() != nullptr) {
            QPoint p = currentEditor()->mapToGlobal(currentEditor()->cursorRect().bottomLeft());
            ui->menuPaste_from_Clipboard_History->exec(p);
        }
    });

    #if defined(Q_OS_WIN) || defined(Q_OS_MAC)
        ui->menuHelp->insertAction(ui->actionAbout, updateManager->getCheckForUpdatesAction());
    #endif

    if (d->settings.contains("window/state")) {
        this->restoreState(d->settings.value("window/state").toByteArray());
    }

    ui->menuWindow->addAction(ui->filesDock->toggleViewAction());
    ui->menuWindow->addAction(ui->sourceControlDock->toggleViewAction());

    //Hide the project frame
    ui->projectFrame->setVisible(false);
    ui->actionStart->setVisible(false);
    ui->actionContinue->setVisible(false);
    ui->actionStep_Into->setVisible(false);
    ui->actionStep_Out->setVisible(false);
    ui->actionStep_Over->setVisible(false);
    ui->actionPause->setVisible(false);

    d->fileModel = new QFileSystemModel();
    d->fileModel->setRootPath(QDir::rootPath());
    d->fileModel->setReadOnly(false);
    ui->projectTree->setModel(d->fileModel);
    ui->projectTree->setHeaderHidden(true);
    ui->projectTree->hideColumn(1);
    ui->projectTree->hideColumn(2);
    ui->projectTree->hideColumn(3);
    ui->projectTree->setRootIndex(d->fileModel->index(QDir::rootPath()));
    ui->projectTree->scrollTo(d->fileModel->index(QDir::homePath()));
    ui->projectTree->expand(d->fileModel->index(QDir::homePath()));
    QScroller::grabGesture(ui->projectTree, QScroller::LeftMouseButtonGesture);

    updateRecentFiles();
    connect(recentFiles, &RecentFilesManager::filesUpdated, this, &MainWindow::updateRecentFiles);

    if (d->settings.value("files/showHidden").toBool()) {
        d->fileModel->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);
    }
}

MainWindow::~MainWindow()
{
    delete d;
    delete ui;
}

void MainWindow::show() {
    if (ui->tabs->count() == 0) {
        newTab();
    }
    QMainWindow::show();

#ifdef Q_OS_WIN
    //Force the palette on Windows
    this->setPalette(QApplication::palette());
#endif
}

TextWidget* MainWindow::newTab() {
    TextWidget* view = new TextWidget(this);
    ui->tabs->addWidget(view);
    ui->tabs->setCurrentWidget(view);

    connect(view->editor(), &TextEditor::editedChanged, this, &MainWindow::checkForEdits);
    connect(view->editor(), &TextEditor::backendChanged, [=] {
        if (currentEditor() == view->editor()) {
            QUrl url = view->editor()->fileUrl();
            if (url.isLocalFile()) {
                QString file = url.toLocalFile();
                this->setWindowFilePath(file);
                ui->projectTree->expand(d->fileModel->index(file));
                ui->projectTree->scrollTo(d->fileModel->index(file));

                d->previouslyOpenLocalFile = url;
            }
        }
    });

    #ifdef Q_OS_MAC
        int index = d->tabBar->addTab(view->editor()->getTabButton()->text());
        d->tabBar->setCurrentIndex(index);
        connect(view->editor(), &TextEditor::titleChanged, [=](QString title) {
            d->tabBar->setTabText(ui->tabs->indexOf(view), title);
        });
        connect(view->editor(), &TextEditor::primaryTopNotificationChanged, [=](TopNotification* topNotification) {
            d->primaryTopNotifications.insert(view, topNotification);

            if (currentDocument() == view) {
                emit changeTouchBarTopNotification(topNotification);
            }

            updateTouchBar();
        });
        /*connect(view->editor(), &TextEditor::destroyed, [=] {
            if (primaryTopNotifications.contains(view)) {
                primaryTopNotifications.remove(view);
            }
        });*/
        d->primaryTopNotifications.insert(view, nullptr);
    #else
        connect(view->editor()->getTabButton(), &QPushButton::clicked, [=]{
            ui->tabs->setCurrentWidget(view);
        });
        ui->tabButtons->addWidget(view->editor()->getTabButton());
    #endif

    updateDocumentDependantTabs();
    return view;
}

void MainWindow::newTab(QString filename) {
    TextEditor* w = currentEditor();
    if (currentEditor() == nullptr || currentEditor()->isEdited() || currentEditor()->title() != "") {
        w = newTab()->editor();
    }

    w->openFile(plugins->getLocalFileBackend()->openFromUrl(QUrl::fromLocalFile(filename)));
    updateGit();
}

void MainWindow::newTab(FileBackend* backend) {
    TextEditor* w = currentEditor();
    if (currentEditor() == nullptr || currentEditor()->isEdited() || currentEditor()->title() != "") {
        w = newTab()->editor();
    }

    w->openFile(backend);
    updateGit();
}

void MainWindow::newTab(QByteArray contents) {
    TextEditor* w = currentEditor();
    if (currentEditor() == nullptr || currentEditor()->isEdited() || currentEditor()->title() != "") {
        w = newTab()->editor();
    }

    w->loadText(contents);
    updateGit();
}

void MainWindow::on_actionNew_triggered()
{
    if (d->currentProjectFile == "") {
        newTab();
    } else {
        MainWindow* newWin = new MainWindow();
        newWin->show();
    }
}

void MainWindow::on_tabs_currentChanged(int arg1)
{
    for (int i = 0; i < ui->tabs->count(); i++) {
        TextEditor* item = static_cast<TextWidget*>(ui->tabs->widget(i))->editor();
        item->setActive(false);
    }

    TextWidget* current = static_cast<TextWidget*>(ui->tabs->widget(arg1));
    if (current != nullptr) {
        current->editor()->setActive(true);
        QUrl url = current->editor()->fileUrl();
        #ifdef Q_OS_MAC
            if (url.isLocalFile()) {
                QString file = url.toLocalFile();
                QFileIconProvider ic;
                QFileInfo fileInfo(file);
                this->setWindowIcon(ic.icon(fileInfo));
                this->setWindowFilePath(file);
                this->setWindowTitle(current->editor()->title());
            } else if (current->editor()->title() != "") {
                this->setWindowTitle(current->editor()->title());
                this->setWindowFilePath("");
            } else {
                this->setWindowTitle("theSlate");
                this->setWindowIcon(QIcon(":/icons/icon.svg"));
                this->setWindowFilePath("");
            }

            d->tabBar->setCurrentIndex(arg1);
        #endif

        if (url.isLocalFile()) {
            QString file = url.toLocalFile();
            ui->projectTree->scrollTo(d->fileModel->index(file));
            ui->projectTree->expand(d->fileModel->index(file));

            d->previouslyOpenLocalFile = url;
        }

        updateGit();
    }

#ifdef Q_OS_MAC
    if (current != nullptr) {
        emit changeTouchBarTopNotification(d->primaryTopNotifications.value(current));
    } else {
        emit changeTouchBarTopNotification(nullptr);
    }
    updateTouchBar();
#endif
}

void MainWindow::on_actionExit_triggered()
{
    QApplication::closeAllWindows();
    if (openWindows.count() == 0) {
        QApplication::exit();
    }
}

void MainWindow::on_actionOpen_triggered()
{
    this->menuBar()->setEnabled(false);
    QAction* action = plugins->getLocalFileBackend()->makeOpenAction(this, std::bind(&MainWindow::getOpenOption, this, std::placeholders::_1));
    action->trigger();
    action->deleteLater();
    this->menuBar()->setEnabled(true);
}

void MainWindow::on_actionSave_triggered()
{
    //Ensure there's actually a document to save
    if (ui->tabs->count() != 0) saveCurrentDocument();
}

TextWidget* MainWindow::currentDocument() {
    return static_cast<TextWidget*>(ui->tabs->widget(ui->tabs->currentIndex()));
}

TextEditor* MainWindow::currentEditor() {
    if (currentDocument() == nullptr) return nullptr;
    return currentDocument()->editor();
}

void MainWindow::checkForEdits() {
    for (int i = 0; i < ui->tabs->count(); i++) {
        TextEditor* item = static_cast<TextWidget*>(ui->tabs->widget(i))->editor();
        if (item->isEdited()) {
            this->setWindowModified(true);
            return;
        }
    }
    this->setWindowModified(false);
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (ui->tabs->count() > 0) {
        QList<TextWidget*> saveNeeded;
        for (int i = 0; i < ui->tabs->count(); i++) {
            TextWidget* tab = static_cast<TextWidget*>(ui->tabs->widget(i));
            if (tab->editor()->isEdited()) saveNeeded.append(tab);
        }

        if (saveNeeded.count() > 0) {
            QEventLoop* loop = new QEventLoop();
            ExitSaveDialog* dialog = new ExitSaveDialog(saveNeeded, this);
            //dialog->setWindowFlag(Qt::Sheet);
            dialog->setWindowModality(Qt::WindowModal);
            connect(dialog, &ExitSaveDialog::accepted, [=] {
                loop->exit(0);
            });
            connect(dialog, &ExitSaveDialog::rejected, [=] {
                loop->exit(1);
            });
            connect(dialog, &ExitSaveDialog::closeTab, [=](TextWidget* tab) {
                ui->tabButtons->removeWidget(tab->editor()->getTabButton());
                ui->tabs->removeWidget(tab);
                tab->deleteLater();

                updateDocumentDependantTabs();
            });
            dialog->show();

            if (loop->exec() == 1) {
                event->ignore();
                loop->deleteLater();
                return;
            }
            loop->deleteLater();
        }
    }

    d->settings.setValue("window/state", this->saveState());
    event->accept();
    this->deleteLater();
    openWindows.removeOne(this);
}

bool MainWindow::saveCurrentDocument(bool saveAs) {
    if (currentEditor()->saveFileAskForFilename(saveAs)) {
        updateGit();
        return true;
    } else {
        return false;
    }
}

bool MainWindow::closeCurrentTab() {
    if (currentEditor()->isEdited()) {
        tMessageBox* messageBox = new tMessageBox(this);
        messageBox->setWindowTitle(tr("Save Changes?"));
        messageBox->setText(tr("Do you want to save your changes to this document?"));
        messageBox->setIcon(tMessageBox::Warning);
        messageBox->setWindowFlags(Qt::Sheet);
        messageBox->setStandardButtons(tMessageBox::Discard | tMessageBox::Save | tMessageBox::Cancel);
        messageBox->setDefaultButton(tMessageBox::Save);
        int button = messageBox->exec();

        if (button == tMessageBox::Save) {
            if (!saveCurrentDocument()) {
                return false;
            }
        } else if (button == tMessageBox::Cancel) {
            return false;
        }
    }

    TextWidget* current = currentDocument();

    #ifdef Q_OS_MAC
        d->tabBar->removeTab(ui->tabs->indexOf(current));
    #else
        ui->tabButtons->removeWidget(current->editor()->getTabButton());
    #endif

    ui->tabs->removeWidget(current);
    current->deleteLater();

    updateDocumentDependantTabs();
    return true;
}


void MainWindow::on_closeButton_clicked()
{
    closeCurrentTab();
}

void MainWindow::on_actionCopy_triggered()
{
    currentEditor()->copy();
}

void MainWindow::on_actionCut_triggered()
{
    currentEditor()->cut();
}

void MainWindow::on_actionPaste_triggered()
{
    currentEditor()->paste();
}

void MainWindow::on_actionAbout_triggered()
{
    tAboutDialog aboutWindow;
    aboutWindow.exec();
}

void MainWindow::on_projectTree_clicked(const QModelIndex &index)
{
    if (!d->fileModel->isDir(index)) {
        for (int i = 0; i < ui->tabs->count(); i++) {
            QUrl url = static_cast<TextWidget*>(ui->tabs->widget(i))->editor()->fileUrl();
            if (url.isLocalFile()) {
                if (d->fileModel->filePath(index) == url.toLocalFile()) {
                    ui->tabs->setCurrentIndex(i);
                    return;
                }
            }
        }

        newTab();
        currentEditor()->openFile(plugins->getLocalFileBackend()->openFromUrl(QUrl::fromLocalFile(d->fileModel->filePath(index))));
        updateGit();
    }
}

void MainWindow::updateGit() {
    if (currentEditor() == nullptr) {
        ui->gitWidget->setCurrentDocument(QUrl());
    } else {
        ui->gitWidget->setCurrentDocument(currentEditor()->fileUrl());
    }
}

void MainWindow::on_actionSave_All_triggered()
{
    for (int i = 0; i < ui->tabs->count(); i++) {
        TextEditor* document = static_cast<TextWidget*>(ui->tabs->widget(i))->editor();
        if (document->title() != "") {
            document->saveFile();
        }
    }
}

void MainWindow::on_actionFind_and_Replace_triggered()
{
    currentDocument()->showFindReplace();
}

void MainWindow::on_actionSave_As_triggered()
{
    saveCurrentDocument(true);
}

void MainWindow::on_actionRevert_triggered()
{
    currentEditor()->revertFile();
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event) {
    const QMimeData* data = event->mimeData();
    if (data->hasUrls()) {
        event->setDropAction(Qt::CopyAction);
        event->acceptProposedAction();
        return;
    }
    event->setDropAction(Qt::IgnoreAction);
}

void MainWindow::dragLeaveEvent(QDragLeaveEvent *event) {

}

void MainWindow::dragMoveEvent(QDragMoveEvent *event) {

}

void MainWindow::dropEvent(QDropEvent *event) {
    const QMimeData* data = event->mimeData();
    if (data->hasUrls()) {
        for (QUrl url : data->urls()) {
            if (url.isLocalFile()) {
                newTab(url.toLocalFile());
            }
        }
    }
}

void MainWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        ui->retranslateUi(this);
    }
}

void MainWindow::on_actionPrint_triggered()
{
    PrintDialog* d = new PrintDialog(currentEditor(), this);
    d->setWindowModality(Qt::WindowModal);
    d->resize(this->width() - 20, this->height() - 50);
    d->show();
}

void MainWindow::on_actionFile_Bug_triggered()
{
    QDesktopServices::openUrl(QUrl("https://github.com/vicr123/theslate/issues"));
}

void MainWindow::on_actionSources_triggered()
{
    QDesktopServices::openUrl(QUrl("https://github.com/vicr123/theslate"));
}

void MainWindow::on_actionUndo_triggered()
{
    currentEditor()->undo();
}

void MainWindow::on_actionRedo_triggered()
{
    currentEditor()->redo();
}

void MainWindow::on_actionSettings_triggered()
{
    QEventLoop loop;

    SettingsDialog* d = new SettingsDialog(this);
    d->setWindowFlags(Qt::Sheet);
    d->setWindowModality(Qt::WindowModal);
    d->show();

    connect(d, &QDialog::finished, &loop, &QEventLoop::quit);

    loop.exec();

    for (int i = 0; i < ui->tabs->count(); i++) {
        TextEditor* item = static_cast<TextWidget*>(ui->tabs->widget(i))->editor();
        item->reloadSettings();
    }

    d->deleteLater();
}

void MainWindow::on_actionClose_triggered()
{
    closeCurrentTab();
}

void MainWindow::on_actionNew_Window_triggered()
{
    MainWindow* w = new MainWindow();
    w->show();
}

void MainWindow::on_projectTree_customContextMenuRequested(const QPoint &pos)
{
    QModelIndex index = ui->projectTree->indexAt(pos);
    if (index.isValid()) {
        QFileInfo info = d->fileModel->fileInfo(index);

        QMenu* menu = new QMenu();
        menu->addSection(tr("For %1").arg(info.baseName()));
        if (info.isFile()) {
            menu->addAction(tr("Edit in new tab"), [=] {
                newTab();
                currentEditor()->openFile(plugins->getLocalFileBackend()->openFromUrl(QUrl::fromLocalFile(d->fileModel->filePath(index))));
                updateGit();
            });
            menu->addAction(tr("Edit in new window"), [=] {
                MainWindow* w = new MainWindow();
                w->newTab(info.fileName());
                w->show();
            });
            menu->addSeparator();
            menu->addAction(QIcon::fromTheme("edit-rename", QIcon(":/icons/edit-rename.svg")), tr("Rename"), [=] {
                ui->projectTree->edit(ui->projectTree->currentIndex());
            });
            menu->addAction(QIcon::fromTheme("edit-delete", QIcon(":/icons/edit-delete.svg")), tr("Delete"), [=] {
                tMessageBox* messageBox = new tMessageBox(this->window());
                messageBox->setWindowTitle(tr("Delete this file?"));
                messageBox->setText(tr("%1 will be irrecoverably deleted.").arg(d->fileModel->fileName(index)));
                messageBox->setIcon(tMessageBox::Warning);
                messageBox->setWindowFlags(Qt::Sheet);
                QAbstractButton* button = messageBox->addButton(tr("Delete"), tMessageBox::DestructiveRole);
                messageBox->setStandardButtons(tMessageBox::Cancel);

                connect(button, &QAbstractButton::clicked, this, [=] {
                    QString fileName = d->fileModel->fileName(index);
                    tToast* toast = new tToast();
                    if (d->fileModel->remove(index)) {
                        toast->setTitle(tr("%n file(s) deleted", nullptr, 1));
                        toast->setText(tr("%1 was deleted from your device.").arg(fileName));
                    } else {
                        toast->setTitle(tr("Couldn't delete file"));
                        toast->setText(tr("There was a problem deleting %1.").arg(fileName));
                    }
                    connect(toast, &tToast::dismissed, toast, &tToast::deleteLater);
                    toast->show(this);
                });

                messageBox->exec();
                messageBox->deleteLater();
            });
        } else if (info.isDir()) {
            menu->addAction(QIcon::fromTheme("edit-rename", QIcon(":/icons/edit-rename.svg")), tr("Rename"), [=] {
                ui->projectTree->edit(ui->projectTree->currentIndex());
            });
        }
        menu->exec(ui->projectTree->mapToGlobal(pos));
    }
}

void MainWindow::on_actionSelect_All_triggered()
{
    if (currentEditor() != nullptr) {
        currentEditor()->selectAll();
    }
}

void MainWindow::updateRecentFiles() {
    QMenu* m = ui->menuOpen_Recent;
    m->clear();

    if (recentFiles->getFiles().count() == 0) {
        QAction* a = new QAction();
        a->setText(tr("No Recent Items"));
        a->setEnabled(false);
        m->addAction(a);
    } else {
        for (QString file : recentFiles->getFiles()) {
            QUrl url(file);
            QFileInfo f(url.toLocalFile());

            m->addAction(f.fileName(), [=] {
                newTab(plugins->getLocalFileBackend()->openFromUrl(url));
            });
        }
    }

    m->addSeparator();
    m->addAction(tr("Clear Recent Items"), [=] {
        recentFiles->clear();
    });
}

void MainWindow::on_actionUse_Menubar_toggled(bool arg1)
{
    d->settings.setValue("appearance/menubar", arg1);
    ui->menuBar->setVisible(arg1);
    d->menuAction->setVisible(!arg1);

    if (arg1) {
        ui->menuHelp->setIcon(QIcon());
    } else {
        ui->menuHelp->setIcon(QIcon::fromTheme("help-contents", QIcon(":/icons/help-contents.svg")));
    }
}

void MainWindow::updateDocumentDependantTabs() {
    bool enabled = (ui->tabs->count() != 0);
    ui->closeButton->setVisible(enabled);
    ui->actionSave->setEnabled(enabled);
    ui->actionSave_As->setEnabled(enabled);
    ui->actionChange_Syntax_Highlighting->setEnabled(enabled);
    ui->actionClose->setEnabled(enabled);
    ui->actionRevert->setEnabled(enabled);
    ui->actionPrint->setEnabled(enabled);
    ui->actionReload_Using_Encoding->setEnabled(enabled);
    ui->actionChange_File_Encoding->setEnabled(enabled);
    ui->actionUndo->setEnabled(enabled);
    ui->actionRedo->setEnabled(enabled);
    ui->actionCut->setEnabled(enabled);
    ui->actionCopy->setEnabled(enabled);
    ui->actionPaste->setEnabled(enabled);
    ui->menuPaste_from_Clipboard_History->setEnabled(enabled);
    ui->actionComment->setEnabled(enabled);
    ui->actionSelect_All->setEnabled(enabled);
    ui->actionFind_and_Replace->setEnabled(enabled);
    ui->actionUppercase->setEnabled(enabled);
    ui->actionLowercase->setEnabled(enabled);
    ui->actionTitle_Case->setEnabled(enabled);
    ui->menuOpen_Auxiliary_Pane->setEnabled(enabled);
    ui->actionChange_Syntax_Highlighting->setEnabled(enabled);
    ui->actionLine->setEnabled(enabled);
}

QVariant MainWindow::getOpenOption(QString option)
{
    if (option == "currentDirectory") {
        //The directory of the currently open file
        if (!d->previouslyOpenLocalFile.isValid()) return QVariant();
        return QFileInfo(d->previouslyOpenLocalFile.toLocalFile()).dir().absolutePath();
    }
    return QVariant();
}

void MainWindow::on_actionComment_triggered()
{
    currentEditor()->commentSelectedText();
}

void MainWindow::on_actionChange_Syntax_Highlighting_triggered()
{
    currentEditor()->chooseHighlighter();
}

void MainWindow::on_actionChange_File_Encoding_triggered()
{
    currentEditor()->chooseCodec();
}

void MainWindow::on_actionReload_Using_Encoding_triggered()
{
    currentEditor()->chooseCodec(true);
}

void MainWindow::on_actionLine_triggered()
{
    currentEditor()->gotoLine();
}

void MainWindow::on_actionUppercase_triggered()
{
    currentEditor()->setSelectedTextCasing(TextEditor::Uppercase);
}

void MainWindow::on_actionLowercase_triggered()
{
    currentEditor()->setSelectedTextCasing(TextEditor::Lowercase);
}

void MainWindow::on_actionTitle_Case_triggered()
{
    currentEditor()->setSelectedTextCasing(TextEditor::Titlecase);
}

void MainWindow::on_actionBeautify_triggered()
{
    currentEditor()->beautify();
}
