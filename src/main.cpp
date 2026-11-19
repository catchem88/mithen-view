#include "mainwindow.h"
#include "qvapplication.h"
#ifdef Q_OS_WIN
#include "qvwindows11style.h"
#include <windows.h>
#endif

#include <QCommandLineParser>
#include <QFontDatabase>
#include <QLocalServer>
#include <QLocalSocket>
#include <QOperatingSystemVersion>

int main(int argc, char *argv[])
{
    QCoreApplication::setOrganizationName("mithen-view");
    QCoreApplication::setApplicationName("mithen-view-JDP");
    QCoreApplication::setApplicationVersion(QString::fromLatin1(MITHEINVIEW_VERSION));

    SettingsManager::migrateOldSettings();

    QString defaultStyleName;
#if defined Q_OS_WIN && QT_VERSION >= QT_VERSION_CHECK(6, 8, 1)
    // windows11 style works on Windows 10 too if the right font is available
    if (QOperatingSystemVersion::current() < QOperatingSystemVersion::Windows11)
        defaultStyleName = "windows11";
#endif
    // Convenient way to set a default style but still allow the user to customize it
    if (!defaultStyleName.isEmpty() && qEnvironmentVariableIsEmpty("QT_STYLE_OVERRIDE"))
        qputenv("QT_STYLE_OVERRIDE", defaultStyleName.toLocal8Bit());

    QVApplication app(argc, argv);

#if defined Q_OS_WIN && QT_VERSION >= QT_VERSION_CHECK(6, 8, 1)
    // For windows11 style on Windows 10, make sure we have the font it needs, otherwise change style
    if (QOperatingSystemVersion::current() < QOperatingSystemVersion::Windows11 &&
        QApplication::style()->name() == "windows11" &&
        !QFontDatabase::families().contains("Segoe Fluent Icons"))
    {
        const QString fontPath = QDir(QApplication::applicationDirPath()).filePath("fonts/Segoe Fluent Icons.ttf");
        if (QFile::exists(fontPath))
            QFontDatabase::addApplicationFont(fontPath);
        else
            QApplication::setStyle("windowsvista");
    }
#endif

#ifdef Q_OS_WIN
    if (QApplication::style()->name() == "windows11")
        QApplication::setStyle(new QvWindows11Style(QApplication::style()));
#endif

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QObject::tr("file"), QObject::tr("The file to open."));
    parser.process(app);

    // Single-instance support: check if another MithenView instance is already running
    const QString serverName = "mithen-view-single-instance-" + QCoreApplication::organizationName();
    QLocalSocket instanceSocket;
    instanceSocket.connectToServer(serverName);

    bool forwarded = false;
    if(instanceSocket.waitForConnected(500)) {
        //Another instance is running - send file path(s) to it
        const QStringList &files = parser.positionalArguments();
        if(!files.isEmpty()) {
            instanceSocket.write(files.join('\n').toUtf8());
            instanceSocket.flush();
            instanceSocket.waitForBytesWritten(1000);
            //Wait for the running instance to acknowledge. A closing or hung instance accepts
            //the connection but never processes it, so start our own instance when it does
            //not answer instead of silently exiting without showing the image.
            forwarded = instanceSocket.waitForReadyRead(1500) && instanceSocket.readAll().contains('1');
            if(!forwarded)
                instanceSocket.abort();
        }
        else {
            //Nothing to forward (plain launch while another instance runs): keep the old behavior
            forwarded = true;
        }
#ifdef Q_OS_WIN
        AllowSetForegroundWindow(ASFW_ANY);
#endif
        if(forwarded) {
            instanceSocket.disconnectFromServer();
            return 0;
        }
    }

    //We are the first instance - start the local server to receive file paths from new instances
    QLocalServer::removeServer(serverName);
    QLocalServer *instanceServer = new QLocalServer(&app);
    instanceServer->listen(serverName);

    //Stop accepting forwarded paths as soon as the app starts closing: the event loop no
    //longer handles them during shutdown, so a quick relaunch that connected to a dying
    //instance would be swallowed and the image would never open.
    const auto closeInstanceServer = [instanceServer]{ instanceServer->close(); };
    QObject::connect(&app, &QGuiApplication::lastWindowClosed, instanceServer, closeInstanceServer);
    QObject::connect(&app, &QCoreApplication::aboutToQuit, instanceServer, closeInstanceServer);

    QObject::connect(instanceServer,&QLocalServer::newConnection,[instanceServer]() {
        QLocalSocket *clientConnection = instanceServer->nextPendingConnection();
        if(clientConnection) {
            QObject::connect(clientConnection,&QLocalSocket::readyRead,[clientConnection]() {
                const QString data = QString::fromUtf8(clientConnection->readAll());
                //Acknowledge right away so the launching instance knows the paths were
                //accepted and does not start a second instance
                clientConnection->write("1");
                clientConnection->flush();
                const QStringList files = data.split('\n',Qt::SkipEmptyParts);
                MainWindow *targetWindow = nullptr;
                for(const QString &file : files) {
                    if(!file.isEmpty() && QFile::exists(file)) {
                        const bool allowMultiple = qvApp->getSettingsManager().getBoolean("allowmultiplewindows");
                        targetWindow = qvApp->getMainWindow(!allowMultiple ? false : true);
                        QVApplication::openFile(targetWindow,file,true);
                    }
                }
                //Bring the window to the foreground and active focus
                if(targetWindow) {
                    targetWindow->setWindowState((targetWindow->windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
                    targetWindow->show();
                    targetWindow->raise();
                    targetWindow->activateWindow();
#ifdef Q_OS_WIN
                    HWND hwnd = reinterpret_cast<HWND>(targetWindow->winId());
                    DWORD currentThreadId = GetCurrentThreadId();
                    DWORD foregroundThreadId = GetWindowThreadProcessId(GetForegroundWindow(),NULL);
                    if(foregroundThreadId != currentThreadId) {
                        AttachThreadInput(foregroundThreadId,currentThreadId,TRUE);
                        BringWindowToTop(hwnd);
                        SetForegroundWindow(hwnd);
                        SetActiveWindow(hwnd);
                        SetFocus(hwnd);
                        AttachThreadInput(foregroundThreadId,currentThreadId,FALSE);
                    }
                    else {
                        BringWindowToTop(hwnd);
                        SetForegroundWindow(hwnd);
                        SetActiveWindow(hwnd);
                        SetFocus(hwnd);
                    }
#endif
                    targetWindow->setFocus();
                }
                clientConnection->disconnectFromServer();
                clientConnection->deleteLater();
            });
        }
    });

    if (!parser.positionalArguments().isEmpty())
    {
        const bool allowMultiple = app.getSettingsManager().getBoolean("allowmultiplewindows");
        // Respect the "Allow multiple window open" setting: reuse existing window when unchecked
        QVApplication::openFile(app.getMainWindow(!allowMultiple ? false : true), parser.positionalArguments().constFirst(), true);
    }
    else if (!QVApplication::tryRestoreLastSession())
    {
        QVApplication::newWindow();
    }

    return QApplication::exec();
}
