#ifndef TEXTEDITOR_H
#define TEXTEDITOR_H

#include <QWidget>
#include <QPlainTextEdit>
#include <QApplication>
#include <QFontDatabase>
#include <QFile>
#include <QFileInfo>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QDebug>
#include <QBoxLayout>
#include <QFileSystemWatcher>
#include <QSyntaxHighlighter>
#include <QTextCodec>
#include "tabbutton.h"
#include "SourceControl/gitintegration.h"
#include "textparts/topnotification.h"
#include "textparts/mergetool.h"

#include <Definition>

class TabButton;
class MainWindow;
class FileBackend;
struct MergeLines;

class TextEditorPrivate;
class TextStatusBar;
class OffscreenLinePopup;

class TextEditorLeftMargin : public QWidget
{
    Q_OBJECT

    public:
        TextEditorLeftMargin(TextEditor *editor);

        QSize sizeHint() const override;

    protected:
        void paintEvent(QPaintEvent *event) override;

    private:
        void mousePressEvent(QMouseEvent* event) override;

        TextEditor *editor;
};

class TextEditor : public QPlainTextEdit
{
    Q_OBJECT

    public:
        explicit TextEditor(QWidget *parent);
        ~TextEditor();

        enum Casing {
            Uppercase,
            Titlecase,
            Lowercase
        };
        enum OpenFileParameters {
            AddToRecentFiles
        };
        typedef QMap<OpenFileParameters, QVariant> OpenFileParametersMap;

        void setMainWindow(MainWindow* mainWindow);
        void setStatusBar(TextStatusBar* statusBar);

        QUrl fileUrl();
        QString title();
        bool isEdited();
        QSyntaxHighlighter* highlighter();

        void leftMarginPaintEvent(QPaintEvent *event);
        int leftMarginWidth();

    signals:
        void backendChanged();
        void editedChanged();
        void mergeDecision(MergeLines lines, bool on);
        void titleChanged(QString title);
        void primaryTopNotificationChanged(TopNotification* topNotification);

    public slots:
        TabButton* getTabButton();
        void setActive(bool active);

        void openFile(FileBackend* backend, OpenFileParametersMap parameters = OpenFileParametersMap());
        void openFileFake(QString contents);
        void loadText(QByteArray data);
        bool saveFile();
        bool saveFileAskForFilename(bool saveAs = false);
        void revertFile(QTextCodec* codec = nullptr);

        void setHighlighter(KSyntaxHighlighting::Definition hl);

        void setExtraSelectionGroup(int priority, QString extraSelectionGroup, QList<QTextEdit::ExtraSelection> selections);
        QList<QTextEdit::ExtraSelection> extraSelectionGroup(QString extraSelectionGroup);
        void clearExtraSelectionGroup(QString extraSelectionGroups);

        void lockScrolling(TextEditor* other);
        void setMergedLines(QList<MergeLines> mergedLines);
        bool mergedLineIsAccepted(MergeLines mergedLine);
        void toggleMergedLines(int line);
        void updateMergedLinesColour();

        void commentSelectedText();
        void setSelectedTextCasing(Casing casing);

        void chooseHighlighter();
        void chooseCodec(bool reload = false);
        void setTextCodec(QTextCodec* codec);

        void beautify(bool silent = false);

        void gotoLine();

        void reloadSettings();

    private slots:
        void updateLeftMarginAreaWidth();
        void highlightCurrentLine();
        void cursorLocationChanged();
        void updateLeftMarginArea(const QRect &, int);
        void reloadBlockHighlighting();

        void updateExtraSelections();
        void setExtraSelections(const QList<QTextEdit::ExtraSelection> &extraSelections);

        void addTopPanel(QWidget* panel);
        void removeTopPanel(QWidget* panel);

        void connectBackend();
        QByteArray formatForSaving(QString text);

        void setTextContents(QString text);

    protected:
        friend OffscreenLinePopup;

        QTextBlock lastVisibleBlock() const;

    private:
        TextEditorPrivate* d;

        void keyPressEvent(QKeyEvent* event);
        void resizeEvent(QResizeEvent* event);

        void dragEnterEvent(QDragEnterEvent* event);
        void dragLeaveEvent(QDragLeaveEvent* event);
        void dragMoveEvent(QDragMoveEvent* event);
        void dropEvent(QDropEvent* event);
};

#endif // TEXTEDITOR_H
