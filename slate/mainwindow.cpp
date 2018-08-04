#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "exitsavedialog.h"
#include <QMimeData>
#include <QFileIconProvider>
#include <QLineEdit>
#include <ttoast.h>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    #ifdef Q_OS_WIN
        //Set up special palette for Windows
        QPalette pal = this->palette();
        pal.setColor(QPalette::Window, QColor(255, 255, 255));
        this->setPalette(pal);
        //ui->tabFrame->setPalette(pal);
    #endif

    #ifdef Q_OS_MACOS
        //Set up Mac toolbar
        ui->toolBar->setVisible(false);
        ui->mainToolBar->setVisible(false);

        QMacToolBar* toolbar = new QMacToolBar();

        QList<QMacToolBarItem*> allowedItems;

        QMacToolBarItem* newItem = new QMacToolBarItem();
        newItem->setText(tr("New"));
        newItem->setIcon(QIcon(":/icons/document-new.svg"));
        newItem->setProperty("name", "new");
        connect(newItem, SIGNAL(activated()), this, SLOT(on_actionNew_triggered()));
        allowedItems.append(newItem);

        QMacToolBarItem* openItem = new QMacToolBarItem();
        openItem->setText(tr("Open"));
        openItem->setIcon(QIcon(":/icons/document-open.svg"));
        openItem->setProperty("name", "open");
        connect(openItem, SIGNAL(activated()), this, SLOT(on_actionOpen_triggered()));
        allowedItems.append(openItem);

        QMacToolBarItem* saveItem = new QMacToolBarItem();
        saveItem->setText(tr("Save"));
        saveItem->setIcon(QIcon(":/icons/document-save.svg"));
        saveItem->setProperty("name", "save");
        connect(saveItem, SIGNAL(activated()), this, SLOT(on_actionSave_triggered()));
        allowedItems.append(saveItem);

        toolbar->setAllowedItems(allowedItems);
        toolbar->setItems(allowedItems);

        toolbar->attachToWindow(this->windowHandle());
    #else
        //Set up single menu except on macOS
        QMenu* singleMenu = new QMenu();
        singleMenu->addAction(ui->actionNew);
        singleMenu->addSeparator();
        singleMenu->addAction(ui->actionOpen);
        singleMenu->addSeparator();
        singleMenu->addAction(ui->actionSave);
        singleMenu->addAction(ui->actionSave_As);
        singleMenu->addAction(ui->actionSave_All);
        singleMenu->addAction(ui->actionRevert);
        singleMenu->addSeparator();
        singleMenu->addAction(ui->actionCut);
        singleMenu->addAction(ui->actionCopy);
        singleMenu->addAction(ui->actionPaste);
        singleMenu->addSeparator();
        singleMenu->addAction(ui->actionFind_and_Replace);
        singleMenu->addSeparator();
        singleMenu->addMenu(ui->menuCode);
        singleMenu->addAction(ui->actionShowSourceControlWindow);
        singleMenu->addSeparator();
        singleMenu->addAction(ui->actionAbout);
        singleMenu->addAction(ui->actionExit);

        QToolButton* menuButton = new QToolButton();
        menuButton->setPopupMode(QToolButton::InstantPopup);
        menuButton->setMenu(singleMenu);
        menuButton->setArrowType(Qt::NoArrow);
        menuButton->setIcon(QIcon::fromTheme("theslate", QIcon(":/icons/icon.svg")));
        ui->mainToolBar->insertWidget(ui->actionNew, menuButton);
    #endif

    ui->menuBar->setVisible(false);

    if (settings.contains("window/state")) {
        this->restoreState(settings.value("window/state").toByteArray());
    }

    //Hide the project frame
    ui->projectFrame->setVisible(false);
    ui->actionFile_in_Project->setVisible(false);
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

    //Set up code highlighting options
    /*ui->menuCode->addAction("C++", [=] {setCurrentDocumentHighlighting(SyntaxHighlighter::cpp);});
    ui->menuCode->addAction("JavaScript", [=] {setCurrentDocumentHighlighting(SyntaxHighlighter::js);});
    ui->menuCode->addAction("Python", [=] {setCurrentDocumentHighlighting(SyntaxHighlighter::py);});
    ui->menuCode->addAction("XML", [=] {setCurrentDocumentHighlighting(SyntaxHighlighter::xml);});
    ui->menuCode->addAction("Markdown", [=] {setCurrentDocumentHighlighting(SyntaxHighlighter::md);});
    ui->menuCode->addAction("JavaScript Object Notation (JSON)", [=] {setCurrentDocumentHighlighting(SyntaxHighlighter::json);});*/

    QObjectList availablePlugins;
    availablePlugins.append(QPluginLoader::staticInstances());

    QStringList pluginSearchPaths;
    #if defined(Q_OS_WIN)
        pluginSearchPaths.append(QApplication::applicationDirPath() + "/../../SyntaxHighlightingPlugins/");
        pluginSearchPaths.append(QApplication::applicationDirPath() + "/syntaxhighlighting/");
    #elif defined(Q_OS_MAC)
        pluginSearchPaths.append(QApplication::applicationDirPath() + "../syntaxhighlighting/");
    #elif (defined Q_OS_UNIX)
        pluginSearchPaths.append("/usr/share/theslate/syntaxhighlighting/");
    #endif


    for (QString path : pluginSearchPaths) {
        QDirIterator it(path, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            QPluginLoader loader(it.filePath());
            QObject* plugin = loader.instance();
            if (plugin) {
                availablePlugins.append(plugin);
            }
        }
    }

    for (QObject* obj : availablePlugins) {
        SyntaxHighlighting* highlighter = qobject_cast<SyntaxHighlighting*>(obj);
        if (highlighter) {
            for (QString name : highlighter->availableHighlighters()) {
                ui->menuCode->addAction(name, [=] {
                    //highlighter->makeHighlighter(name);
                    setCurrentDocumentHighlighting(highlighter->makeHighlighter(name));
                });
            }
        }
    }

    fileModel = new QFileSystemModel();
    fileModel->setRootPath(QDir::rootPath());
    ui->projectTree->setModel(fileModel);
    ui->projectTree->hideColumn(1);
    ui->projectTree->hideColumn(2);
    ui->projectTree->hideColumn(3);
    ui->projectTree->setRootIndex(fileModel->index(QDir::rootPath()));
    ui->projectTree->scrollTo(fileModel->index(QDir::homePath()));
    ui->projectTree->expand(fileModel->index(QDir::homePath()));
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
}

void MainWindow::newTab() {
    TextEditor* view = new TextEditor(this);
    ui->tabs->addWidget(view);
    ui->tabs->setCurrentWidget(view);

    connect(view, SIGNAL(editedChanged()), this, SLOT(checkForEdits()));
    connect(view->getTabButton(), &QPushButton::clicked, [=]{
        ui->tabs->setCurrentWidget(view);
    });
    connect(view, &TextEditor::fileNameChanged, [=] {
        if (currentDocument() == view) {
            this->setWindowFilePath(view->filename());
            ui->projectTree->scrollTo(fileModel->index(view->filename()));
            ui->projectTree->expand(fileModel->index(view->filename()));
        }
    });
    ui->tabButtons->addWidget(view->getTabButton());

    ui->closeButton->setVisible(true);
    ui->actionSave->setEnabled(true);
    ui->menuCode->setEnabled(true);
}

void MainWindow::newTab(QString filename) {
    if (currentDocument() == nullptr || currentDocument()->isEdited() || currentDocument()->filename() != "") {
        newTab();
    }

    currentDocument()->openFile(filename);
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
        #ifdef Q_OS_MAC
            if (current->filename() == "") {
                this->setWindowTitle("theSlate");
                this->setWindowIcon(QIcon(":/icons/icon.svg"));
                this->setWindowFilePath("");
            } else {
                QFileIconProvider ic;
                QFileInfo file(current->filename());
                this->setWindowIcon(ic.icon(file));
                this->setWindowFilePath(current->filename());
                this->setWindowTitle(file.fileName());
            }
        #endif
        ui->projectTree->scrollTo(fileModel->index(current->filename()));
        ui->projectTree->expand(fileModel->index(current->filename()));

        updateGit();
    }
}

void MainWindow::on_actionExit_triggered()
{
    QApplication::closeAllWindows();
}

void MainWindow::on_actionOpen_triggered()
{
    this->menuBar()->setEnabled(false);
    QEventLoop* loop = new QEventLoop();
    QFileDialog* openDialog = new QFileDialog(this, Qt::Sheet);
    openDialog->setWindowModality(Qt::WindowModal);
    openDialog->setAcceptMode(QFileDialog::AcceptOpen);

    if (currentProjectFile == "") {
        openDialog->setDirectory(QDir::home());
    } else {
        openDialog->setDirectory(QFileInfo(currentProjectFile).path());
    }

    openDialog->setNameFilter("All Files (*)");
    connect(openDialog, SIGNAL(finished(int)), openDialog, SLOT(deleteLater()));
    connect(openDialog, SIGNAL(finished(int)), loop, SLOT(quit()));
    openDialog->show();

    //Block until dialog is finished
    loop->exec();
    loop->deleteLater();

    if (openDialog->result() == QDialog::Accepted) {
        newTab(openDialog->selectedFiles().first());
    }
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
            ExitSaveDialog* dialog = new ExitSaveDialog(saveNeeded, this);
            dialog->setWindowFlag(Qt::Sheet);
            dialog->setWindowModality(Qt::WindowModal);
            connect(dialog, &ExitSaveDialog::closeWindow, [=] {
                this->close();
            });
            connect(dialog, &ExitSaveDialog::closeTab, [=](TextEditor* tab) {
                ui->tabButtons->removeWidget(tab->getTabButton());
                ui->tabs->removeWidget(tab);
                tab->deleteLater();

                if (ui->tabs->count() == 0) {
                    ui->closeButton->setVisible(false);
                    ui->actionSave->setEnabled(false);
                    ui->menuCode->setEnabled(false);
                }
            });
            dialog->show();

            event->ignore();
            return;
        }
    }

    settings.setValue("window/state", this->saveState());
    event->accept();
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
        QMessageBox* messageBox = new QMessageBox(this);
        messageBox->setWindowTitle(tr("Save Changes?"));
        messageBox->setText(tr("Do you want to save your changes to this document?"));
        messageBox->setIcon(QMessageBox::Warning);
        messageBox->setWindowFlags(Qt::Sheet);
        messageBox->setStandardButtons(QMessageBox::Discard | QMessageBox::Save | QMessageBox::Cancel);
        messageBox->setDefaultButton(QMessageBox::Save);
        int button = messageBox->exec();

        if (button == QMessageBox::Save) {
            if (!saveCurrentDocument()) {
                return false;
            }
        } else if (button == QMessageBox::Cancel) {
            return false;
        }
    }

    TextEditor* current = currentDocument();
    ui->tabButtons->removeWidget(current->getTabButton());
    ui->tabs->removeWidget(current);
    current->deleteLater();

    if (ui->tabs->count() == 0) {
        ui->closeButton->setVisible(false);
        ui->actionSave->setEnabled(false);
        ui->menuCode->setEnabled(false);
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
    setCurrentDocumentHighlighting(nullptr);
}

void MainWindow::on_projectTree_clicked(const QModelIndex &index)
{
    if (!fileModel->isDir(index)) {
        for (int i = 0; i < ui->tabs->count(); i++) {
            if (fileModel->filePath(index) == ((TextEditor*) ui->tabs->widget(i))->filename()) {
                ui->tabs->setCurrentIndex(i);
                return;
            }
        }

        newTab();
        currentDocument()->openFile(fileModel->filePath(index));
        updateGit();
    }
}

void MainWindow::updateGit() {
    if (GitIntegration::findGit().count() == 0) {
        ui->sourceControlPanes->setCurrentIndex(3);
    } else if (currentDocument()->git == nullptr) {
        ui->sourceControlPanes->setCurrentIndex(2);
    } else {
        if (currentDocument()->git->needsInit()) {
            ui->sourceControlPanes->setCurrentIndex(1);
        } else {
            ui->sourceControlPanes->setCurrentIndex(0);
            QStringList changedFiles = currentDocument()->git->reloadStatus();
            ui->modifiedChanges->clear();

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

                    ui->modifiedChanges->addItem(item);
                }
            }

            ui->gitMergeConflictsFrame->setVisible(hasConflicts);
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
        if (document->filename() != "") {
            document->saveFile();
        }
    }
}

void MainWindow::switchToFile(QString file, QString fakeFileContents) {
    for (int i = 0; i < ui->tabs->count(); i++) {
        if (((TextEditor*) ui->tabs->widget(i))->filename() == file) {
            ui->tabs->setCurrentIndex(i);
            return;
        }
    }

    newTab();
    if (fakeFileContents == "") {
        currentDocument()->openFile(file);
        updateGit();
    } else {
        currentDocument()->openFileFake(file, fakeFileContents);
    }
}

void MainWindow::setCurrentDocumentHighlighting(QSyntaxHighlighter* highlighting) {
    if (currentDocument() == nullptr) {

    } else {
        currentDocument()->setHighlighter(highlighting);
    }
}

void MainWindow::on_initGitButton_clicked()
{
    currentDocument()->git->init();
}

void MainWindow::on_actionShowSourceControlWindow_triggered()
{
    ui->sourceControlDock->setVisible(!ui->sourceControlDock->isVisible());
}

void MainWindow::on_sourceControlDock_visibilityChanged(bool visible)
{
    ui->actionShowSourceControlWindow->setChecked(visible);
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