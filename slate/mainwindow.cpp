#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "exitsavedialog.h"
#include <QMimeData>
#include <QFileIconProvider>
#include "messagebox.h"
#include <QLineEdit>
#include <ttoast.h>
#include <QDesktopServices>
#include "settingsdialog.h"
#include "picturetabbar.h"
#include <QInputDialog>
#include <QShortcut>
#include "plugins/pluginmanager.h"
#include "managers/recentfilesmanager.h"
#include "managers/updatemanager.h"

#include <Repository>
#include <SyntaxHighlighter>
#include <Definition>
#include <Theme>

#ifdef Q_OS_MAC
    extern QString bundlePath;
    extern void setToolbarItemWidget(QMacToolBarItem* item, QWidget* widget);
#endif

extern QLinkedList<QString> clipboardHistory;
extern PluginManager* plugins;
extern RecentFilesManager* recentFiles;
extern UpdateManager* updateManager;
extern KSyntaxHighlighting::Repository* highlightRepo;

extern QColor getSyntaxHighlighterColor(QString color);

QList<MainWindow*> MainWindow::openWindows = QList<MainWindow*>();



MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    openWindows.append(this);

    ui->mainToolBar->setIconSize(ui->mainToolBar->iconSize() * theLibsGlobal::getDPIScaling());
    
    //Load plugins
    /*for (SyntaxHighlighting* highlighter : plugins->syntaxHighlighters()) {
        for (QString name : highlighter->availableHighlighters()) {
            ui->menuCode->addAction(name, [=] {
                setCurrentDocumentHighlighting(highlighter->makeHighlighter(name, &getSyntaxHighlighterColor));
            });
        }
    }*/

    for (FileBackendFactory* factory : plugins->fileBackends()) {
        if (factory != plugins->getLocalFileBackend()) {
            QAction* openAction = factory->makeOpenAction(this);
            ui->menuOpenFrom->addAction(openAction);
        }
        connect(factory, &FileBackendFactory::openFile, [=](FileBackend* backend) {
            newTab(backend);
        });
    }

    //Load Syntax Highlighters
    QMenu* sectionMenu = nullptr;
    for (KSyntaxHighlighting::Definition d : highlightRepo->definitions()) {
        if (sectionMenu == nullptr || sectionMenu->property("sectionName") != d.section()) {
            sectionMenu = new QMenu();
            sectionMenu->setTitle(" " + d.translatedSection());
            sectionMenu->setProperty("sectionName", d.section());

            if (d.section() != "") {
                ui->menuCode->addMenu(sectionMenu);
            }
        }

        sectionMenu->addAction(d.translatedName(), [=] {
            setCurrentDocumentHighlighting(d);
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

        QMacToolBar* toolbar = new QMacToolBar();

        QList<QMacToolBarItem*> allowedItems;

        QMacToolBarItem* newItem = new QMacToolBarItem();
        newItem->setText(tr("New"));
        //newItem->setIcon(QIcon(":/icons/document-new.svg"));
        newItem->setIcon(QApplication::style()->standardIcon(QStyle::SP_FileIcon));
        newItem->setProperty("name", "new");
        connect(newItem, SIGNAL(activated()), this, SLOT(on_actionNew_triggered()));
        allowedItems.append(newItem);

        QMacToolBarItem* openItem = new QMacToolBarItem();
        openItem->setText(tr("Open"));
        //openItem->setIcon(QIcon(":/icons/document-open.svg"));
        openItem->setIcon(QApplication::style()->standardIcon(QStyle::SP_DialogOpenButton));
        openItem->setProperty("name", "open");
        connect(openItem, SIGNAL(activated()), this, SLOT(on_actionOpen_triggered()));
        allowedItems.append(openItem);

        QMacToolBarItem* saveItem = new QMacToolBarItem();
        saveItem->setText(tr("Save"));
        //saveItem->setIcon(QIcon(":/icons/document-save.svg"));
        saveItem->setIcon(QApplication::style()->standardIcon(QStyle::SP_DialogSaveButton));
        saveItem->setProperty("name", "save");
        connect(saveItem, SIGNAL(activated()), this, SLOT(on_actionSave_triggered()));
        allowedItems.append(saveItem);

        ui->tabFrame->setVisible(false);

        tabBar = new QTabBar(this);
        tabBar->setDocumentMode(true);
        tabBar->setTabsClosable(true);
        tabBar->setMovable(true);
        connect(tabBar, &QTabBar::currentChanged, [=](int index) {
            ui->tabs->setCurrentIndex(index);
        });
        connect(tabBar, &QTabBar::tabCloseRequested, [=](int index) {
            ui->tabs->setCurrentIndex(index);
            closeCurrentTab();
        });
        connect(tabBar, &QTabBar::tabMoved, [=](int from, int to) {
            QWidget* w = ui->tabs->widget(from);
            ui->tabs->removeWidget(w);
            ui->tabs->insertWidget(to, w);
        });
        ((QBoxLayout*) ui->centralWidget->layout())->insertWidget(0, tabBar);

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
        singleMenu->addSeparator();
        singleMenu->addAction(ui->actionCut);
        singleMenu->addAction(ui->actionCopy);
        singleMenu->addAction(ui->actionPaste);
        singleMenu->addMenu(ui->menuPaste_from_Clipboard_History);
        singleMenu->addSeparator();
        singleMenu->addAction(ui->actionPrint);
        singleMenu->addAction(ui->actionFind_and_Replace);
        singleMenu->addAction(ui->actionSelect_All);
        singleMenu->addSeparator();
        singleMenu->addMenu(ui->menuCode);
        singleMenu->addMenu(ui->menuWindow);
        singleMenu->addSeparator();
        singleMenu->addAction(ui->actionSettings);
        singleMenu->addMenu(ui->menuHelp);
        singleMenu->addAction(ui->actionExit);

        menuButton = new QToolButton();
        menuButton->setPopupMode(QToolButton::InstantPopup);
        menuButton->setMenu(singleMenu);
        menuButton->setArrowType(Qt::NoArrow);
        menuButton->setIcon(QIcon::fromTheme("theslate", QIcon(":/icons/icon.svg")));
        menuButton->setIconSize(ui->mainToolBar->iconSize());
        menuAction = ui->mainToolBar->insertWidget(ui->actionNew, menuButton);
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

                menuButton->setIcon(QIcon(pixmap));
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

        ui->actionUse_Menubar->setChecked(settings.value("appearance/menubar", false).toBool());
        on_actionUse_Menubar_toggled(settings.value("appearance/menubar", false).toBool());
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
                action->setEnabled(currentDocument() != nullptr);
                connect(action, &QAction::triggered, [=] {
                    currentDocument()->insertPlainText(*i);
                });
                ui->menuPaste_from_Clipboard_History->addAction(action);
            }
        }
    });
    QShortcut* pasteHistory = new QShortcut(QKeySequence(tr("Ctrl+Shift+V")), this);
    connect(pasteHistory, &QShortcut::activated, [=] {
        if (currentDocument() != nullptr) {
            QPoint p = currentDocument()->mapToGlobal(currentDocument()->cursorRect().bottomLeft());
            ui->menuPaste_from_Clipboard_History->exec(p);
        }
    });

    #if defined(Q_OS_WIN) || defined(Q_OS_MAC)
        ui->menuHelp->insertAction(ui->actionAbout, updateManager->getCheckForUpdatesAction());
    #endif

    for (QByteArray codec : QTextCodec::availableCodecs()) {
        ui->menuReload_Using_Encoding->addAction(codec, [=] {
            if (currentDocument() != nullptr) currentDocument()->revertFile(QTextCodec::codecForName(codec));
        });
        ui->menuChange_File_Encoding->addAction(codec, [=] {
            if (currentDocument() != nullptr) currentDocument()->setTextCodec(QTextCodec::codecForName(codec));
        });
    }

    if (settings.contains("window/state")) {
        this->restoreState(settings.value("window/state").toByteArray());
    }

    ui->menuWindow->addAction(ui->filesDock->toggleViewAction());
    ui->menuWindow->addAction(ui->sourceControlDock->toggleViewAction());

    //Hide the project frame
    ui->projectFrame->setVisible(false);
    ui->menuSource_Control->setEnabled(false);
    ui->actionStart->setVisible(false);
    ui->actionContinue->setVisible(false);
    ui->actionStep_Into->setVisible(false);
    ui->actionStep_Out->setVisible(false);
    ui->actionStep_Over->setVisible(false);
    ui->actionPause->setVisible(false);

    ui->sourceControlOptionsButton->setMenu(ui->menuSource_Control);
    ui->menuSource_Control->setEnabled(true);
    ui->gitProgressFrame->setVisible(false);

    fileModel = new QFileSystemModel();
    fileModel->setRootPath(QDir::rootPath());
    fileModel->setReadOnly(false);
    ui->projectTree->setModel(fileModel);
    ui->projectTree->hideColumn(1);
    ui->projectTree->hideColumn(2);
    ui->projectTree->hideColumn(3);
    ui->projectTree->setRootIndex(fileModel->index(QDir::rootPath()));
    ui->projectTree->scrollTo(fileModel->index(QDir::homePath()));
    ui->projectTree->expand(fileModel->index(QDir::homePath()));

    updateRecentFiles();
    connect(recentFiles, &RecentFilesManager::filesUpdated, this, &MainWindow::updateRecentFiles);

    if (settings.value("files/showHidden").toBool()) {
        fileModel->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);
    }
}

MainWindow::~MainWindow()
{
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

void MainWindow::newTab() {
    TextEditor* view = new TextEditor(this);
    ui->tabs->addWidget(view);
    ui->tabs->setCurrentWidget(view);

    connect(view, SIGNAL(editedChanged()), this, SLOT(checkForEdits()));
    connect(view, &TextEditor::backendChanged, [=] {
        if (currentDocument() == view) {
            QUrl url = view->fileUrl();
            if (url.isLocalFile()) {
                QString file = url.toLocalFile();
                this->setWindowFilePath(file);
                ui->projectTree->scrollTo(fileModel->index(file));
                ui->projectTree->expand(fileModel->index(file));
            }
        }
    });

    #ifdef Q_OS_MAC
        int index = tabBar->addTab(view->getTabButton()->text());
        tabBar->setCurrentIndex(index);
        connect(view, &TextEditor::titleChanged, [=](QString title) {
            tabBar->setTabText(ui->tabs->indexOf(view), title);
        });
        connect(view, &TextEditor::primaryTopNotificationChanged, [=](TopNotification* topNotification) {
            primaryTopNotifications.insert(view, topNotification);

            if (currentDocument() == view) {
                emit changeTouchBarTopNotification(topNotification);
            }

            updateTouchBar();
        });
        /*connect(view, &TextEditor::destroyed, [=] {
            if (primaryTopNotifications.contains(view)) {
                primaryTopNotifications.remove(view);
            }
        });*/
        primaryTopNotifications.insert(view, nullptr);
    #else
        connect(view->getTabButton(), &QPushButton::clicked, [=]{
            ui->tabs->setCurrentWidget(view);
        });
        ui->tabButtons->addWidget(view->getTabButton());
    #endif

    ui->closeButton->setVisible(true);
    ui->actionSave->setEnabled(true);
    ui->menuCode->setEnabled(true);
    ui->actionClose->setEnabled(true);
}

void MainWindow::newTab(QString filename) {
    if (currentDocument() == nullptr || currentDocument()->isEdited() || currentDocument()->title() != "") {
        newTab();
    }

    currentDocument()->openFile(plugins->getLocalFileBackend()->openFromUrl(QUrl::fromLocalFile(filename)));
    updateGit();
}

void MainWindow::newTab(FileBackend* backend) {
    if (currentDocument() == nullptr || currentDocument()->isEdited() || currentDocument()->title() != "") {
        newTab();
    }

    currentDocument()->openFile(backend);
    updateGit();
}

void MainWindow::newTab(QByteArray contents) {
    if (currentDocument() == nullptr || currentDocument()->isEdited() || currentDocument()->title() != "") {
        newTab();
    }

    currentDocument()->loadText(contents);
    updateGit();
}

void MainWindow::on_actionNew_triggered()
{
    if (currentProjectFile == "") {
        newTab();
    } else {
        MainWindow* newWin = new MainWindow();
        newWin->show();
    }
}

void MainWindow::on_tabs_currentChanged(int arg1)
{
    for (int i = 0; i < ui->tabs->count(); i++) {
        TextEditor* item = (TextEditor*) ui->tabs->widget(i);
        item->setActive(false);
    }

    TextEditor* current = (TextEditor*) ui->tabs->widget(arg1);
    if (current != nullptr) {
        current->setActive(true);
        QUrl url = current->fileUrl();
        #ifdef Q_OS_MAC
            if (url.isLocalFile()) {
                QString file = url.toLocalFile();
                QFileIconProvider ic;
                QFileInfo fileInfo(file);
                this->setWindowIcon(ic.icon(fileInfo));
                this->setWindowFilePath(file);
                this->setWindowTitle(current->title());
            } else if (current->title() != "") {
                this->setWindowTitle(current->title());
                this->setWindowFilePath("");
            } else {
                this->setWindowTitle("theSlate");
                this->setWindowIcon(QIcon(":/icons/icon.svg"));
                this->setWindowFilePath("");
            }

            tabBar->setCurrentIndex(arg1);
        #endif

        if (url.isLocalFile()) {
            QString file = url.toLocalFile();
            ui->projectTree->scrollTo(fileModel->index(file));
            ui->projectTree->expand(fileModel->index(file));
        }

        updateGit();
    }

#ifdef Q_OS_MAC
    if (current != nullptr) {
        emit changeTouchBarTopNotification(primaryTopNotifications.value(current));
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
    QAction* action = plugins->getLocalFileBackend()->makeOpenAction(this);
    action->trigger();
    action->deleteLater();
    this->menuBar()->setEnabled(true);
}

void MainWindow::on_actionSave_triggered()
{
    saveCurrentDocument();
}

TextEditor* MainWindow::currentDocument() {
    return (TextEditor*) ui->tabs->widget(ui->tabs->currentIndex());
}

void MainWindow::checkForEdits() {
    for (int i = 0; i < ui->tabs->count(); i++) {
        TextEditor* item = (TextEditor*) ui->tabs->widget(i);
        if (item->isEdited()) {
            this->setWindowModified(true);
            return;
        }
    }
    this->setWindowModified(false);
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (ui->tabs->count() > 0) {
        QList<TextEditor*> saveNeeded;
        for (int i = 0; i < ui->tabs->count(); i++) {
            TextEditor* tab = (TextEditor*) ui->tabs->widget(i);
            if (tab->isEdited()) saveNeeded.append(tab);
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
            connect(dialog, &ExitSaveDialog::closeTab, [=](TextEditor* tab) {
                ui->tabButtons->removeWidget(tab->getTabButton());
                ui->tabs->removeWidget(tab);
                tab->deleteLater();

                if (ui->tabs->count() == 0) {
                    ui->closeButton->setVisible(false);
                    ui->actionSave->setEnabled(false);
                    ui->menuCode->setEnabled(false);
                    ui->actionClose->setEnabled(false);
                }
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

    settings.setValue("window/state", this->saveState());
    event->accept();
    this->deleteLater();
    openWindows.removeOne(this);
}

bool MainWindow::saveCurrentDocument(bool saveAs) {
    if (currentDocument()->saveFileAskForFilename(saveAs)) {
        updateGit();
        return true;
    } else {
        return false;
    }
}

bool MainWindow::closeCurrentTab() {
    if (currentDocument()->isEdited()) {
        MessageBox* messageBox = new MessageBox(this);
        messageBox->setWindowTitle(tr("Save Changes?"));
        messageBox->setText(tr("Do you want to save your changes to this document?"));
        messageBox->setIcon(MessageBox::Warning);
        messageBox->setWindowFlags(Qt::Sheet);
        messageBox->setStandardButtons(MessageBox::Discard | MessageBox::Save | MessageBox::Cancel);
        messageBox->setDefaultButton(MessageBox::Save);
        int button = messageBox->exec();

        if (button == MessageBox::Save) {
            if (!saveCurrentDocument()) {
                return false;
            }
        } else if (button == MessageBox::Cancel) {
            return false;
        }
    }

    TextEditor* current = currentDocument();

    #ifdef Q_OS_MAC
        tabBar->removeTab(ui->tabs->indexOf(current));
    #else
        ui->tabButtons->removeWidget(current->getTabButton());
    #endif

    ui->tabs->removeWidget(current);
    current->deleteLater();

    if (ui->tabs->count() == 0) {
        ui->closeButton->setVisible(false);
        ui->actionSave->setEnabled(false);
        ui->menuCode->setEnabled(false);
        ui->actionClose->setEnabled(false);
    }
    return true;
}


void MainWindow::on_closeButton_clicked()
{
    closeCurrentTab();
}

void MainWindow::on_actionCopy_triggered()
{
    currentDocument()->copy();
}

void MainWindow::on_actionCut_triggered()
{
    currentDocument()->cut();
}

void MainWindow::on_actionPaste_triggered()
{
    currentDocument()->paste();
}

void MainWindow::on_actionAbout_triggered()
{
    AboutWindow aboutWindow;
    aboutWindow.exec();
}

void MainWindow::on_actionNo_Highlighting_triggered()
{
    setCurrentDocumentHighlighting(KSyntaxHighlighting::Definition());
}

void MainWindow::on_projectTree_clicked(const QModelIndex &index)
{
    if (!fileModel->isDir(index)) {
        for (int i = 0; i < ui->tabs->count(); i++) {
            QUrl url = ((TextEditor*) ui->tabs->widget(i))->fileUrl();
            if (url.isLocalFile()) {
                if (fileModel->filePath(index) == url.toLocalFile()) {
                    ui->tabs->setCurrentIndex(i);
                    return;
                }
            }
        }

        newTab();
        currentDocument()->openFile(plugins->getLocalFileBackend()->openFromUrl(QUrl::fromLocalFile(fileModel->filePath(index))));
        updateGit();
    }
}

void MainWindow::updateGit() {
    if (GitIntegration::findGit().count() == 0) {
        ui->sourceControlPanes->setCurrentIndex(3);
    } else if (currentDocument() == nullptr) {
        //Do nothing
    } else if (currentDocument()->git == nullptr) {
        ui->sourceControlPanes->setCurrentIndex(2);
    } else {
        if (currentDocument()->git->needsInit()) {
            ui->sourceControlPanes->setCurrentIndex(1);
        } else {
            ui->sourceControlPanes->setCurrentIndex(0);
            currentDocument()->git->reloadStatus()->then([=](QStringList changedFiles) {
                ui->modifiedChanges->clear();

                QVector<QListWidgetItem*> items;
                bool hasConflicts = false;
                for (QString changedFile : changedFiles) {
                    if (changedFile != "") {
                        QChar flag1 = changedFile.at(0);
                        QChar flag2 = changedFile.at(1);
                        QString fileLocation = changedFile.mid(2);

                        QListWidgetItem* item = new QListWidgetItem;
                        item->setText(fileLocation);
                        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                        if (flag1 == 'A' || flag1 == 'M') {
                            //Staged change
                            item->setCheckState(Qt::Checked);
                        } else {
                            //Unstaged change
                            item->setCheckState(Qt::Unchecked);
                        }

                        if (flag1 == 'U' && flag2 == 'U') { //Merge conflict
                            item->setText(fileLocation + " [CONFLICTING]");
                            hasConflicts = true;
                        }
                        item->setData(Qt::UserRole, fileLocation);

                        items.append(item);
                    }
                }

                std::sort(items.begin(), items.end(), [=](const QListWidgetItem* a, const QListWidgetItem* b) {
                    return a->text() < b->text();
                });
                for (QListWidgetItem* item : items) {
                    ui->modifiedChanges->addItem(item);
                }

                ui->gitMergeConflictsFrame->setVisible(hasConflicts);
            });
        }
    }
}

void MainWindow::on_modifiedChanges_itemChanged(QListWidgetItem *item)
{
    if (item->checkState() == Qt::Checked) {
        currentDocument()->git->add(item->data(Qt::UserRole).toString());
    } else {
        currentDocument()->git->unstage(item->data(Qt::UserRole).toString());
    }
}

void MainWindow::on_actionSave_All_triggered()
{
    for (int i = 0; i < ui->tabs->count(); i++) {
        TextEditor* document = (TextEditor*) ui->tabs->widget(i);
        if (document->title() != "") {
            document->saveFile();
        }
    }
}

void MainWindow::setCurrentDocumentHighlighting(KSyntaxHighlighting::Definition highlighting) {
    if (currentDocument() == nullptr) {

    } else {
        currentDocument()->setHighlighter(highlighting);
    }
}

void MainWindow::on_initGitButton_clicked()
{
    currentDocument()->git->init();
}

void MainWindow::on_actionFind_and_Replace_triggered()
{
    currentDocument()->toggleFindReplace();
}

void MainWindow::on_actionSave_As_triggered()
{
    saveCurrentDocument(true);
}

void MainWindow::on_actionRevert_triggered()
{
    currentDocument()->revertFile();
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

void MainWindow::on_gitAbortMergeButton_clicked()
{
    currentDocument()->git->abortMerge();
}

void MainWindow::on_actionPull_triggered()
{
    GitTask* task = currentDocument()->git->pull();
    ui->gitProgressFrame->setVisible(true);
    ui->gitProgressTitle->setText(tr("Git Pull"));
    ui->gitProgressOutput->setText(tr("Pulling from remote repository..."));
    connect(task, &GitTask::output, [=](QString message) {
        ui->gitProgressOutput->setText(message);
    });
    connect(task, &GitTask::finished, [=](QString message) {
        tToast* toast = new tToast();
        toast->setTitle(tr("Git Pull"));
        toast->setText(tr("Local repository updated"));
        toast->show(this);
        connect(toast, SIGNAL(dismissed()), toast, SLOT(deleteLater()));
        ui->gitProgressFrame->setVisible(false);
    });
    connect(task, &GitTask::failed, [=](QString message) {
        if (message == "CONFLICT") {
            tToast* toast = new tToast();
            toast->setTitle(tr("Automatic merging failed"));
            toast->setText(tr("Conflicting files in working directory need to be resolved."));
            toast->show(this);
            connect(toast, SIGNAL(dismissed()), toast, SLOT(deleteLater()));
        } else if (message == "UNCLEAN") {
            tToast* toast = new tToast();
            toast->setTitle(tr("Merging failed"));
            toast->setText(tr("Your working directory is not clean. Commit your changes before you pull."));
            toast->show(this);
            connect(toast, SIGNAL(dismissed()), toast, SLOT(deleteLater()));
        }
        ui->gitProgressFrame->setVisible(false);
    });
}

void MainWindow::on_actionPush_triggered()
{
    GitTask* task = currentDocument()->git->push();
    ui->gitProgressFrame->setVisible(true);
    ui->gitProgressTitle->setText(tr("Git Push"));
    ui->gitProgressOutput->setText(tr("Pushing to remote repository..."));
    connect(task, &GitTask::output, [=](QString message) {
        ui->gitProgressOutput->setText(message);
    });
    connect(task, &GitTask::finished, [=](QString message) {
        tToast* toast = new tToast();
        toast->setTitle(tr("Git Push"));
        toast->setText(tr("Files were pushed to the remote repository"));
        toast->show(this);
        connect(toast, SIGNAL(dismissed()), toast, SLOT(deleteLater()));
        ui->gitProgressFrame->setVisible(false);
    });
    connect(task, &GitTask::failed, [=](QString message) {
        if (message == "UPDATE") {
            QMap<QString, QString> actions;
            actions.insert("pull", "Git Pull");

            tToast* toast = new tToast();
            toast->setTitle(tr("Push Rejected"));
            toast->setText(tr("Your local Git repository is not up to date. You'll need to pull from the remote repository before you can push."));
            toast->setActions(actions);
            toast->show(this);
            connect(toast, SIGNAL(dismissed()), toast, SLOT(deleteLater()));
            connect(toast, &tToast::actionClicked, [=](QString key) {
                if (key == "pull") {
                    toast->announceAction("Pulling from remote repository");
                    on_actionPull_triggered();
                }
            });
        }
        ui->gitProgressFrame->setVisible(false);
    });
}

void MainWindow::on_commitButton_clicked()
{
    if (ui->commitMessage->text() == "") {
        tToast* toast = new tToast();
        toast->setTitle(tr("Git Commit"));
        toast->setText(tr("A commit message is required."));
        toast->show(this);
        connect(toast, SIGNAL(dismissed()), toast, SLOT(deleteLater()));
        ui->gitProgressFrame->setVisible(false);
    } else {
        QString commit = currentDocument()->git->commit(ui->commitMessage->text());

        QMap<QString, QString> actions;
        actions.insert("push", "Git Push");

        tToast* toast = new tToast();
        toast->setTitle(tr("Git Commit"));
        toast->setText(tr("Your local files have been committed. Your HEAD now points to %1").arg(commit + " " + ui->commitMessage->text()));
        toast->setActions(actions);
        toast->show(this);
        connect(toast, SIGNAL(dismissed()), toast, SLOT(deleteLater()));
        connect(toast, &tToast::actionClicked, [=](QString key) {
            if (key == "push") {
                toast->announceAction("Pushing to remote repository");
                on_actionPush_triggered();
            }
        });
        ui->commitMessage->setText("");
    }
}

void MainWindow::on_actionPrint_triggered()
{
    PrintDialog* d = new PrintDialog(currentDocument(), this);
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
    currentDocument()->undo();
}

void MainWindow::on_actionRedo_triggered()
{
    currentDocument()->redo();
}

void MainWindow::on_actionSettings_triggered()
{
    QEventLoop loop;

    SettingsDialog* d = new SettingsDialog(this);
    d->setWindowFlags(Qt::Sheet);
    d->setWindowModality(Qt::WindowModal);
    d->show();

    connect(d, SIGNAL(finished(int)), &loop, SLOT(quit()));

    loop.exec();

    for (int i = 0; i < ui->tabs->count(); i++) {
        TextEditor* item = (TextEditor*) ui->tabs->widget(i);
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
        QFileInfo info = fileModel->fileInfo(index);

        QMenu* menu = new QMenu();
        menu->addSection(tr("For %1").arg(info.baseName()));
        if (info.isFile()) {
            menu->addAction(tr("Edit in new tab"), [=] {
                newTab();
                currentDocument()->openFile(plugins->getLocalFileBackend()->openFromUrl(QUrl::fromLocalFile(fileModel->filePath(index))));
                updateGit();
            });
            menu->addAction(tr("Edit in new window"), [=] {
                MainWindow* w = new MainWindow();
                w->newTab(info.fileName());
                w->show();
            });
            menu->addSeparator();
            menu->addAction(tr("Rename"), [=] {
                ui->projectTree->edit(ui->projectTree->currentIndex());
            });
            menu->addAction(tr("Delete"), [=] {

            });
        } else if (info.isDir()) {
            menu->addAction(tr("Rename"), [=] {
                bool ok;
                QString newName = QInputDialog::getText(this, tr("Rename"), tr("What do you want to call this file?"), QLineEdit::Normal, info.baseName(), &ok, Qt::Sheet);
                if (ok) {
                    QFile f(info.filePath());
                    if (newName.contains(".")) {
                        f.rename(newName);
                    } else {
                        f.rename(newName + info.completeSuffix());
                    }
                }
            });
        }
        menu->exec(ui->projectTree->mapToGlobal(pos));
    }
}

void MainWindow::on_actionSelect_All_triggered()
{
    if (currentDocument() != nullptr) {
        currentDocument()->selectAll();
    }
}

void MainWindow::on_sourceControlPanes_currentChanged(int arg1)
{
    if (arg1 == 0) {
        ui->menuSource_Control->setEnabled(true);
    } else {
        ui->menuSource_Control->setEnabled(false);
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
    settings.setValue("appearance/menubar", arg1);
    ui->menuBar->setVisible(arg1);
    menuAction->setVisible(!arg1);

    if (arg1) {
        ui->menuCode->setIcon(QIcon());
        ui->menuHelp->setIcon(QIcon());
    } else {
        ui->menuCode->setIcon(QIcon::fromTheme("commit", QIcon(":/icons/commit.svg")));
        ui->menuHelp->setIcon(QIcon::fromTheme("help-contents", QIcon(":/icons/help-contents.svg")));
    }
}
