#include "shortcutmanager.h"
#include "qvapplication.h"
#include "actionmanager.h"

#include <QSettings>

ShortcutManager::ShortcutManager(QObject *parent) : QObject(parent)
{
    initializeShortcutsList();
    updateShortcuts();
    hideShortcuts();
}

void ShortcutManager::updateShortcuts()
{
    QSettings settings;
    settings.beginGroup("shortcuts");

    // Set all shortcuts to the user-set shortcut or the default
    for (auto &shortcut : shortcutsList)
    {
        shortcut.shortcuts = settings.value(shortcut.name, shortcut.defaultShortcuts).toStringList();
    }

    // Set all action shortcuts now that the shortcuts have changed
    const auto &actionManager = qvApp->getActionManager();
    for (const auto &shortcut : getShortcutsList())
    {
        actionManager.setActionShortcuts(shortcut.name, stringListToKeySequenceList(shortcut.shortcuts));
    }

    hideShortcuts();

    emit shortcutsUpdated();
}

void ShortcutManager::initializeShortcutsList()
{
    shortcutsList.append({tr("Open"), "open", keyBindingsToStringList(QKeySequence::Open), {}});
    shortcutsList.append({tr("Open URL"), "openurl", QStringList(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O).toString()), {}});
    shortcutsList.append({tr("Reload File"), "reloadfile", keyBindingsToStringList(QKeySequence::Refresh), {}});
    shortcutsList.append({tr("Open Containing Folder"), "opencontainingfolder", {}, {}});
    shortcutsList.last().readableName = tr("Show in Explorer");
    shortcutsList.append({tr("Show File Info"), "showfileinfo", QStringList(QKeySequence(Qt::Key_I).toString()), {}});
    shortcutsList.append({tr("Restore from Trash"), "undo", keyBindingsToStringList(QKeySequence::Undo), {}});
    shortcutsList.last().readableName = tr("Undo Delete");
    shortcutsList.append({tr("Copy"), "copy", keyBindingsToStringList(QKeySequence::Copy), {}});
    shortcutsList.append({tr("Paste"), "paste", keyBindingsToStringList(QKeySequence::Paste), {}});
    shortcutsList.append({tr("Rename"), "rename", QStringList(QKeySequence(Qt::Key_F2).toString()), {}});
    // ctrl+r for renaming, unless it conflicts with refresh (i.e. reload file)
    if (!QKeySequence::keyBindings(QKeySequence::Refresh).contains(QKeySequence(Qt::CTRL | Qt::Key_R)))
        shortcutsList.last().defaultShortcuts << QKeySequence(Qt::CTRL | Qt::Key_R).toString();

    shortcutsList.append({tr("Move to Trash"), "delete", keyBindingsToStringList(QKeySequence::Delete), {}});
    shortcutsList.last().readableName = tr("Delete");
    shortcutsList.append({tr("Delete Permanently"), "deletepermanent", QStringList(QKeySequence(Qt::SHIFT | Qt::Key_Delete).toString()), {}});
    shortcutsList.append({tr("First File"), "firstfile", QStringList(QKeySequence(Qt::Key_Home).toString()), {}});
    shortcutsList.append({tr("Previous File"), "previousfile", QStringList(QKeySequence(Qt::Key_Left).toString()), {}});
    shortcutsList.append({tr("Next File"), "nextfile", QStringList(QKeySequence(Qt::Key_Right).toString()), {}});
    shortcutsList.append({tr("Last File"), "lastfile", QStringList(QKeySequence(Qt::Key_End).toString()), {}});
    shortcutsList.append({tr("Random File"), "randomfile", QStringList(QKeySequence(Qt::Key_R).toString()), {}});
    shortcutsList.append({tr("Zoom In"), "zoomin", keyBindingsToStringList(QKeySequence::ZoomIn), {}});
    // Allow zooming with Ctrl + plus like a regular person (without holding shift)
    if (!shortcutsList.last().defaultShortcuts.contains(QKeySequence(Qt::CTRL | Qt::Key_Equal).toString()))
        shortcutsList.last().defaultShortcuts << QKeySequence(Qt::CTRL | Qt::Key_Equal).toString();

    shortcutsList.append({tr("Zoom Out"), "zoomout", keyBindingsToStringList(QKeySequence::ZoomOut), {}});
    shortcutsList.append({tr("Set Zoom Level"), "zoomcustom", {}, {}});
    shortcutsList.append({tr("Original Size"), "originalsize", QStringList(QKeySequence(Qt::CTRL | Qt::Key_9).toString()), {}});
    shortcutsList.append({tr("Zoom to Fit"), "zoomtofit", QStringList(QKeySequence(Qt::CTRL | Qt::Key_0).toString()), {}});
    shortcutsList.append({tr("Fill Window"), "fillwindow", QStringList(QKeySequence(Qt::CTRL | Qt::Key_8).toString()), {}});
    shortcutsList.append({tr("Navigation Resets Zoom"), "navresetszoom", QStringList(QKeySequence(Qt::Key_Z).toString()), {}});
    shortcutsList.append({tr("Rotate Right"), "rotateright", {}, {}});
    shortcutsList.append({tr("Rotate Left"), "rotateleft", {}, {}});
    shortcutsList.append({tr("Mirror"), "mirror", {}, {}});
    shortcutsList.append({tr("Flip"), "flip", {}, {}});
    shortcutsList.append({tr("Reset Transformation"), "resettransformation", QStringList(QKeySequence(Qt::Key_T).toString()), {}});
    shortcutsList.append({tr("Match Image Size"), "matchimagesize", {}, {}});
    shortcutsList.append({tr("Scroll Up"), "scrollup", QStringList(QKeySequence(Qt::Key_Up).toString()), {}});
    shortcutsList.append({tr("Scroll Down"), "scrolldown", QStringList(QKeySequence(Qt::Key_Down).toString()), {}});
    shortcutsList.append({tr("Window On Top"), "windowontop", {}, {}});
    shortcutsList.append({tr("Toggle Titlebar Hidden"), "toggletitlebar", {}, {}});
    shortcutsList.append({tr("Full Screen"), "fullscreen", QStringList({QKeySequence(Qt::Key_F11).toString(), QKeySequence(Qt::ALT | Qt::Key_Return).toString(), QKeySequence(Qt::Key_F).toString()}), {}});
    shortcutsList.append({tr("Save Frame As"), "saveframeas", keyBindingsToStringList(QKeySequence::Save), {}});
    shortcutsList.append({tr("Pause"), "pause", QStringList(QKeySequence(Qt::Key_Space).toString()), {}});
    shortcutsList.append({tr("Next Frame"), "nextframe", QStringList(QKeySequence(Qt::Key_Period).toString()), {}});
    shortcutsList.append({tr("Previous Frame"), "previousframe", QStringList(QKeySequence(Qt::Key_Comma).toString()), {}});
    shortcutsList.append({tr("Decrease Speed"), "decreasespeed", QStringList(QKeySequence(Qt::Key_BracketLeft).toString()), {}});
    shortcutsList.append({tr("Reset Speed"), "resetspeed", QStringList(QKeySequence(Qt::Key_Backslash).toString()), {}});
    shortcutsList.append({tr("Increase Speed"), "increasespeed", QStringList(QKeySequence(Qt::Key_BracketRight).toString()), {}});
    shortcutsList.append({tr("Toggle Slideshow"), "slideshow", {}, {}});
    shortcutsList.append({tr("Settings"), "options", keyBindingsToStringList(QKeySequence::Preferences), {}});
    shortcutsList.append({tr("Close Window"), "closewindow", QStringList(QKeySequence(Qt::Key_Escape).toString()), {}});
    shortcutsList.append({tr("Quit"), "quit", keyBindingsToStringList(QKeySequence::Quit), {}});
    shortcutsList.last().defaultShortcuts << QKeySequence(Qt::CTRL | Qt::Key_W).toString();
    shortcutsList.last().readableName = tr("Exit");
}

void ShortcutManager::hideShortcuts()
{
    QMutableListIterator<SShortcut> i(shortcutsList);
    while (i.hasNext())
    {
        if (hiddenShortcuts.contains(i.next().name))
        {
            i.remove();
        }
    }
}

void ShortcutManager::setShortcutHidden(const QString &shortcut)
{
    hiddenShortcuts.append(shortcut);
    hideShortcuts();
}

void ShortcutManager::setShortcutsHidden(const QStringList &shortcuts)
{
    hiddenShortcuts.append(shortcuts);
    hideShortcuts();
}
