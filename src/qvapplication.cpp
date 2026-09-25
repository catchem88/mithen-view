#include "qvapplication.h"
#include "qvoptionsdialog.h"
#include "simplefonticonengine.h"

#include <QFileOpenEvent>
#include <QJsonArray>
#include <QOperatingSystemVersion>
#include <QSettings>
#include <QTimer>
#include <QFileDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QFontDatabase>
#include <QStyleHints>

QVApplication::QVApplication(int &argc, char **argv) : QApplication(argc, argv)
{
    // Connections
    connect(this, &QGuiApplication::commitDataRequest, this, &QVApplication::onCommitDataRequest, Qt::DirectConnection);
    connect(this, &QCoreApplication::aboutToQuit, this, &QVApplication::onAboutToQuit);
    connect(&settingsManager, &SettingsManager::settingsUpdated, this, &QVApplication::settingsUpdated);
    connect(&actionManager, &ActionManager::recentsMenuUpdated, this, &QVApplication::recentsMenuUpdated);

    settingsUpdated();

    showMainMenuIcons = getSettingsManager().getBoolean("mainmenuicons");
    showContextMenuIcons = getSettingsManager().getBoolean("contextmenuicons");
    showSubmenuIcons = getSettingsManager().getBoolean("submenuicons");
#ifdef Q_OS_WIN
    // Workaround for ugly menu shadows in windows11 style (QTBUG-133116)
    useCustomMenuShadow =
        (QT_VERSION < QT_VERSION_CHECK(6, 11, 0) || QOperatingSystemVersion::current() < QOperatingSystemVersion::Windows11) &&
        style()->objectName().compare("windows11", Qt::CaseInsensitive) == 0;
#endif

    // Ask Qt to show menu icons - the action clone logic decides whether to actually set icons
    setAttribute(Qt::AA_DontShowIconsInMenus, false);

    actionManager.loadRecentsList();

    // Build menu bar
    menuBar = actionManager.buildMenuBar();
    connect(menuBar, &QMenuBar::triggered, this, [](QAction *triggeredAction){
        ActionManager::actionTriggered(triggeredAction);
    });

    hideIncompatibleActions();
}

QVApplication::~QVApplication()
{
    menuBar->deleteLater();
}

bool QVApplication::event(QEvent *event)
{
    if (event->type() == QEvent::FileOpen)
    {
        auto *openEvent = static_cast<QFileOpenEvent *>(event);
        bool allowMultiple = qvApp->getSettingsManager().getBoolean("allowmultiplewindows");
        openFile(getMainWindow(!allowMultiple ? false : true), openEvent->file());
    }
    else if (event->type() == QEvent::ApplicationStateChange)
    {
        auto *stateEvent = static_cast<QApplicationStateChangeEvent*>(event);
        if (stateEvent->applicationState() == Qt::ApplicationActive)
            settingsManager.loadSettings();
        else if (stateEvent->applicationState() == Qt::ApplicationInactive)
            invalidateFolderListings();
    }
    else if (event->type() == QEvent::Quit)
    {
        SessionSaveDecision result = getSessionSaveDecision();
        if (result == SessionSaveDecision::Cancel)
        {
            event->ignore();
            return true;
        }
        isSessionStateSaveRequested = result == SessionSaveDecision::Yes;
    }
    return QApplication::event(event);
}

void QVApplication::openFile(MainWindow *window, const QString &file, bool resize)
{
    window->setJustLaunchedWithImage(resize);
    window->openFile(file);
}

void QVApplication::openFile(const QString &file, bool resize)
{
    const bool allowMultiple = qvApp->getSettingsManager().getBoolean("allowmultiplewindows");
    // When multiple windows are NOT allowed, always reuse the most recent window
    // When multiple windows ARE allowed, find an empty window or create a new one
    auto *window = qvApp->getMainWindow(!allowMultiple ? false : true);

    QVApplication::openFile(window, file, resize);
}

void QVApplication::pickFile(MainWindow *parent)
{
    QSettings settings;
    settings.beginGroup("recents");

    auto *fileDialog = new QFileDialog(parent, tr("Open..."));
    fileDialog->setDirectory(settings.value("lastFileDialogDir", QDir::homePath()).toString());
    fileDialog->setFileMode(QFileDialog::ExistingFiles);
    fileDialog->setNameFilters(qvApp->getNameFilterList());
    if (parent)
        fileDialog->setWindowModality(Qt::WindowModal);

    connect(fileDialog, &QFileDialog::filesSelected, fileDialog, [parent](const QStringList &selected){
        bool isFirstLoop = true;
        for (const auto &file : selected)
        {
            if (isFirstLoop && parent)
                parent->openFile(file);
            else
                QVApplication::openFile(file);

            isFirstLoop = false;
        }

        // Set lastFileDialogDir
        QSettings settings;
        settings.beginGroup("recents");
        settings.setValue("lastFileDialogDir", QFileInfo(selected.constFirst()).path());
    });
    fileDialog->show();
}

MainWindow *QVApplication::newWindow(const QJsonObject &windowSessionState)
{
    auto *w = new MainWindow(nullptr, windowSessionState);
    w->show();
    w->raise();

    return w;
}

MainWindow *QVApplication::getMainWindow(bool shouldBeEmpty)
{
    MainWindow *foundWindow = nullptr;

    for (MainWindow *window : std::as_const(activeWindows))
    {
        // Don't reuse a window that is displaying a file or waiting for one to load.
        if (shouldBeEmpty && window->hasFileOrPendingLoad())
            continue;

        if (foundWindow && foundWindow->getLastActivatedTimestamp() >= window->getLastActivatedTimestamp())
            continue;

        foundWindow = window;
    }

    return foundWindow ? foundWindow : newWindow();
}

void QVApplication::recentsMenuUpdated()
{
}

void QVApplication::addToActiveWindows(MainWindow *window)
{
    if (!window)
        return;

    activeWindows.insert(window);
}

void QVApplication::deleteFromActiveWindows(MainWindow *window)
{
    if (!window)
        return;

    activeWindows.remove(window);
}

bool QVApplication::foundLoadedImage() const
{
    for (const MainWindow *window : activeWindows)
    {
        if (window->getIsPixmapLoaded())
            return true;
    }
    return false;
}

bool QVApplication::foundOnTopWindow() const
{
    for (const MainWindow *window : activeWindows)
    {
        if (window->getWindowOnTop())
            return true;
    }
    return false;
}

void QVApplication::openOptionsDialog(QWidget *parent)
{
    if (optionsDialog)
    {
        optionsDialog->raise();
        optionsDialog->activateWindow();
        return;
    }

    optionsDialog = new QVOptionsDialog(parent);
    optionsDialog->show();
}

void QVApplication::openAboutDialog(QWidget *parent)
{
    if (aboutDialog)
    {
        aboutDialog->raise();
        aboutDialog->activateWindow();
        return;
    }

    aboutDialog = new QVAboutDialog(parent);
    aboutDialog->show();
}

void QVApplication::hideIncompatibleActions()
{
}

void QVApplication::settingsUpdated()
{
    auto &settingsManager = getSettingsManager();

    QString disabledFileExtensionsStr = settingsManager.getString("disabledfileextensions");
    disabledFileExtensions = Qv::listToSet(!disabledFileExtensionsStr.isEmpty() ? disabledFileExtensionsStr.split(';') : QStringList());

    defineFilterLists();
}

void QVApplication::defineFilterLists()
{
    allFileExtensionSet.clear();
    fileExtensionSet.clear();
    mimeTypeNameSet.clear();
    nameFilterList.clear();

    const auto addExtension = [&](const QString &extension) {
        if (allFileExtensionSet.contains(extension))
            return;
        allFileExtensionSet << extension;
        if (disabledFileExtensions.contains(extension))
            return;
        fileExtensionSet << extension;
    };

    // Build extension list
    const auto &byteArrayFormats = QImageReader::supportedImageFormats();
    for (const auto &byteArray : byteArrayFormats)
    {
        const auto fileExtension = "." + QString::fromUtf8(byteArray);

        // Qt 5.15 seems to have added pdf support for QImageReader but it is super broken in MithenView
        if (fileExtension == ".pdf")
            continue;

        addExtension(fileExtension);

        // Register additional file extensions that decoders support but don't advertise
        if (fileExtension == ".jpg")
        {
            addExtension(".jpe");
            addExtension(".jfi");
            addExtension(".jfif");
        }
        else if (fileExtension == ".heic")
        {
            addExtension(".heics");
        }
        else if (fileExtension == ".heif")
        {
            addExtension(".heifs");
            addExtension(".hif");
        }
        else if (fileExtension == ".j2k")
        {
            addExtension(".j2c");
        }
    }

    // Build mime type list
    const auto &byteArrayMimeTypes = QImageReader::supportedMimeTypes();
    for (const auto &byteArray : byteArrayMimeTypes)
    {
        const QString mimeType = QString::fromUtf8(byteArray);

        // Qt 5.15 seems to have added pdf support for QImageReader but it is super broken in MithenView
        if (mimeType == "application/pdf")
            continue;

        mimeTypeNameSet << mimeType;
    }

    // Build name filter list for file dialogs
    const auto extensions = Qv::setToSortedList(fileExtensionSet);
    auto filterString = tr("Supported Images") + " (";
    for (const auto &extension : extensions)
    {
        filterString += "*" + extension + " ";
    }
    filterString.chop(1);
    filterString += ")";
    nameFilterList << filterString;
    nameFilterList << tr("All Files") + " (*)";
}

void QVApplication::ensureFontLoaded(const QString &path)
{
    static QSet<QString> loadedFontPaths;

    if (loadedFontPaths.contains(path))
        return;

    QFontDatabase::addApplicationFont(path);
    loadedFontPaths.insert(path);
}

QIcon QVApplication::iconFromFont(const Qv::MaterialIcon iconName)
{
    static std::optional<QFont> materialIconFont;
    if (!materialIconFont.has_value())
    {
        ensureFontLoaded(":/fonts/MaterialIconsOutlined-Regular.otf");
        materialIconFont = QFont("Material Icons Outlined");
        materialIconFont->setStyleStrategy(QFont::NoFontMerging);
    }
    return QIcon(new SimpleFontIconEngine(QChar(static_cast<quint16>(iconName)), materialIconFont.value()));
}

qreal QVApplication::keyboardAutoRepeatInterval()
{
    return 1.0 / qGuiApp->styleHints()->keyboardAutoRepeatRateF();
}

bool QVApplication::isMouseEventSynthesized(const QMouseEvent *event)
{
    return event->deviceType() != QInputDevice::DeviceType::Mouse;
}

bool QVApplication::supportsSessionPersistence()
{
    return false;
}

bool QVApplication::tryRestoreLastSession()
{
    if (!supportsSessionPersistence())
        return false;

    QSettings settings;

    if (!settings.value("options/persistsession").toBool())
        return false;

    const QJsonObject sessionState = settings.value("sessionstate").toJsonObject();

    if (sessionState.isEmpty() || sessionState["version"].toInt() != Qv::SessionStateVersion)
        return false;

    const QJsonArray windowArray = sessionState["windows"].toArray();
    for (const QJsonValue &item : windowArray)
    {
        QVApplication::newWindow(item.toObject());
    }

    settings.remove("sessionstate");

    return true;
}

void QVApplication::onSystemInitiatedQuit()
{
    isQuitSystemInitiated = true;
}

QVApplication::SessionSaveDecision QVApplication::getSessionSaveDecision() const
{
    if (!supportsSessionPersistence())
        return SessionSaveDecision::No;
    if (isQuitSystemInitiated)
        return SessionSaveDecision::Yes;
    if (!getSettingsManager().getBoolean("persistsession") || !foundLoadedImage())
        return SessionSaveDecision::No;

    QMessageBox msgBox;
    msgBox.setWindowModality(Qt::ApplicationModal);
    msgBox.setWindowTitle(tr("Remember Session?"));
    msgBox.setText(tr("Would you like to remember your opened images and re-open them at next launch?"));
    QPushButton *yesButton = msgBox.addButton(tr("&Remember"), QMessageBox::YesRole);
    msgBox.addButton(tr("&End Session"), QMessageBox::NoRole);
    msgBox.setStandardButtons(QMessageBox::Cancel);
    msgBox.setDefaultButton(yesButton);
    msgBox.setEscapeButton(QMessageBox::Cancel);
    msgBox.exec();
    if (msgBox.standardButton(msgBox.clickedButton()) == QMessageBox::Cancel)
        return SessionSaveDecision::Cancel;
    return msgBox.clickedButton() == yesButton ? SessionSaveDecision::Yes : SessionSaveDecision::No;
}

void QVApplication::addClosedWindowSessionState(const QJsonObject &state, const qint64 lastActivatedTimestamp)
{
    closedWindowData.append({state, lastActivatedTimestamp});
}

void QVApplication::onCommitDataRequest(QSessionManager &manager)
{
    Q_UNUSED(manager)

    isApplicationQuitting = true;
}

void QVApplication::onAboutToQuit()
{
    isApplicationQuitting = true;

    if (isSessionStateSaveRequested)
    {
        QSettings settings;
        if (!closedWindowData.isEmpty())
        {
            QJsonObject state;

            state["version"] = Qv::SessionStateVersion;

            std::sort(
                closedWindowData.begin(), closedWindowData.end(),
                [](const ClosedWindowData& a, const ClosedWindowData& b) {
                    return a.lastActivatedTimestamp < b.lastActivatedTimestamp;
                });
            QJsonArray windows;
            for (const ClosedWindowData& item : std::as_const(closedWindowData))
                windows.append(item.sessionState);
            state["windows"] = windows;

            settings.setValue("sessionstate", state);
        }
        else
        {
            settings.remove("sessionstate");
        }
    }

    // Delay destroying application until thread pool threads have finished
    QThreadPool::globalInstance()->waitForDone();
}
