#ifndef FILEBACKEND_H
#define FILEBACKEND_H

#include <QObject>
#include <tpromise.h>
#include <functional>
#define THESLATE_FILEBACKEND_IID "org.thesuite.theSlate.FileBackend"

class FileBackend : public QObject {
    Q_OBJECT

    public:
        FileBackend(QObject* parent = nullptr) : QObject(parent) {}

        virtual QString documentTitle() = 0;
        virtual QUrl url() = 0;
        virtual bool readOnly() = 0;

    public slots:
        virtual tPromise<void>* save(QByteArray fileContents) = 0;
        virtual tPromise<QByteArray>* load() = 0;

    signals:
        void remoteFileEdited();
        void remoteFileRemoved();
};

class FileBackendFactory : public QObject {
    Q_OBJECT

    public:
        FileBackendFactory(QObject* parent = nullptr) : QObject(parent) {}

        virtual QAction* makeOpenAction(QWidget* parent, std::function<QVariant(QString)> getOption) = 0;
        virtual QString name() = 0;

    public slots:
        virtual FileBackend* openFromUrl(QUrl url) = 0;
        virtual QUrl askForUrl(QWidget* parent, std::function<QVariant(QString)> getOption, bool* ok = nullptr) = 0;

    signals:
        void openFile(FileBackend* backend);
};

class FileBackendPlugin {
    public:
        virtual ~FileBackendPlugin() {}

        virtual QList<FileBackendFactory*> getFactories() = 0;
};

Q_DECLARE_INTERFACE(FileBackendPlugin, THESLATE_FILEBACKEND_IID)

#endif // FILEBACKEND_H
