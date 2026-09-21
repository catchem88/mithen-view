#include "settingsmanager.h"
#include "qvnamespace.h"

#include <QSettings>
#include <QTranslator>
#include <QLocale>
#include <QCoreApplication>
#include <QDir>

#include <QDebug>

namespace
{
// Translations shipped by the installer live next to the executable
QString translationsPath()
{
    return QCoreApplication::applicationDirPath() + "/translations";
}
}

SettingsManager::SettingsManager(QObject *parent) : QObject(parent)
{
    initializeSettingsLibrary();
    loadSettings();
    loadTranslations();
}

QString SettingsManager::getInstalledLanguage() const
{
    // The installer ships a catalog for the language chosen during installation, so the language
    // is resolved from what is actually installed next to the executable.
    QStringList available;
    const auto entries = QDir(translationsPath()).entryList({"wView_*.qm"}, QDir::Files);
    for (const auto &entry : entries)
    {
        QString language = entry;
        language.remove(0, 6);
        language.remove(language.length() - 3, 3);
        available.append(language);
    }

    if (available.isEmpty())
        return "en";

    // Only one catalog installed means the install-time choice wins over the system locale
    if (available.size() == 1)
        return available.first();

    const auto languages = QLocale::system().uiLanguages();
    for (auto language : languages)
    {
        language.replace('-', '_');
        const auto countryless = language.left(2);

        for (const auto &entry : available)
        {
            if (entry.compare(language, Qt::CaseInsensitive) == 0)
                return entry;

            if (entry.compare(countryless, Qt::CaseInsensitive) == 0)
                return entry;
        }
    }
    return available.first();
}

void SettingsManager::loadTranslations()
{
    const QString language = getInstalledLanguage();
    if (language == "en")
        return;

    const QString path = translationsPath();

    if (qtTranslator.load(QLocale(language), "qtbase", "_", path))
        QCoreApplication::installTranslator(&qtTranslator);

    if (appTranslator.load("wView_" + language + ".qm", path))
        QCoreApplication::installTranslator(&appTranslator);
}

void SettingsManager::loadSettings()
{
    QSettings settings;
    settings.beginGroup("options");
    bool changed = false;

    const auto keys = settingsLibrary.keys();
    for (const auto &key : keys)
    {
        auto &setting = settingsLibrary[key];
        if (setting.value != settings.value(key, setting.defaultValue))
            changed = true;

        setting.value = settings.value(key, setting.defaultValue);
    }

    if (changed)
        emit settingsUpdated();
}

const QVariant SettingsManager::getSetting(const QString &key, bool defaults) const
{
    auto value = settingsLibrary.value(key);

    if (!defaults && !value.value.isNull())
        return value.value;

    if (!value.defaultValue.isNull())
        return value.defaultValue;

    qWarning() << "Error: Invalid settings key: " + key;
    return QVariant();
}

bool SettingsManager::getBoolean(const QString &key, bool defaults) const
{
    auto value = getSetting(key, defaults);

    if (value.canConvert<bool>())
        return value.value<bool>();

    qWarning() << "Error: Can't convert setting key " + key + " to bool";
    return false;
}

int SettingsManager::getInteger(const QString &key, bool defaults) const
{
    auto value = getSetting(key, defaults);

    if (value.canConvert<int>())
        return value.value<int>();

    qWarning() << "Error: Can't convert setting key " + key + " to int";
    return 0;
}

double SettingsManager::getDouble(const QString &key, bool defaults) const
{
    auto value = getSetting(key, defaults);

    if (value.canConvert<double>())
        return value.value<double>();

    qWarning() << "Error: Can't convert setting key " + key + " to double";
    return 0;
}

const QString SettingsManager::getString(const QString &key, bool defaults) const
{
    auto value = getSetting(key, defaults);

    if (value.canConvert<QString>())
        return value.value<QString>();

    qWarning() << "Error: Can't convert setting key " + key + " to string";
    return "";
}

bool SettingsManager::isDefault(const QString &key) const
{
    return getSetting(key) == getSetting(key, true);
}

void SettingsManager::migrateOldSettings()
{
    if (!QSettings().contains("firstlaunch"))
        copyFromOfficial();

    QSettings settings;
    settings.beginGroup("options");

    if (!settings.contains("smoothscalingmode") && settings.contains("filteringenabled"))
    {
        const auto value =
            settings.value("scalingenabled").toBool() ? Qv::SmoothScalingMode::Expensive :
            settings.value("filteringenabled").toBool() ? Qv::SmoothScalingMode::Bilinear :
            Qv::SmoothScalingMode::Disabled;
        settings.setValue("smoothscalingmode", static_cast<int>(value));
    }

    // Migration: Clear old Mirror shortcut "F" to avoid conflict with Fullscreen "F"
    QSettings shortcutSettings;
    shortcutSettings.beginGroup("shortcuts");
    const QString mirrorKey = "mirror";
    if (shortcutSettings.contains(mirrorKey))
    {
        QStringList currentShortcuts = shortcutSettings.value(mirrorKey).toStringList();
        if (currentShortcuts.contains(QKeySequence(Qt::Key_F).toString()))
        {
            currentShortcuts.removeAll(QKeySequence(Qt::Key_F).toString());
            if (currentShortcuts.isEmpty())
                shortcutSettings.remove(mirrorKey);
            else
                shortcutSettings.setValue(mirrorKey, currentShortcuts);
        }
    }

    // Migration: Update Full Screen shortcuts to new defaults
    const QString fullscreenKey = "fullscreen";
    if (shortcutSettings.contains(fullscreenKey))
    {
        QStringList currentShortcuts = shortcutSettings.value(fullscreenKey).toStringList();
        QStringList newShortcuts;
        newShortcuts << QKeySequence(Qt::Key_F11).toString();
        newShortcuts << QKeySequence(Qt::ALT | Qt::Key_Return).toString();
        newShortcuts << QKeySequence(Qt::Key_F).toString();
        // Only update if the shortcuts don't already match
        if (currentShortcuts != newShortcuts)
        {
            shortcutSettings.setValue(fullscreenKey, newShortcuts);
        }
    }

    shortcutSettings.endGroup();

    // Migration: Force skiphidden to true
    QSettings optionsSettings;
    optionsSettings.beginGroup("options");
    if (!optionsSettings.contains("skiphidden") || !optionsSettings.value("skiphidden").toBool())
    {
        optionsSettings.setValue("skiphidden", true);
    }

    optionsSettings.endGroup();

    // Migration: Update shortcut defaults
    // Remove "Up" from Rotate Right, "Down" from Rotate Left, "Ctrl+F" from Flip
    // Change "Pause" from "P" to "Space"
    const QString shortcutPrefix = "shortcuts/";
    QSettings shortcutMigrate;
    const QStringList removedShortcutKeys = {
        "rotateright", "rotateleft", "flip"
    };
    for (const QString &key : removedShortcutKeys)
    {
        if (shortcutMigrate.contains(shortcutPrefix + key))
        {
            QStringList currentShortcuts = shortcutMigrate.value(shortcutPrefix + key).toStringList();
            QStringList newShortcuts;
            for (const QString &shortcut : currentShortcuts)
            {
                if (QKeySequence(shortcut) != QKeySequence(Qt::Key_Up) &&
                    QKeySequence(shortcut) != QKeySequence(Qt::Key_Down) &&
                    QKeySequence(shortcut) != QKeySequence(Qt::CTRL | Qt::Key_F))
                {
                    newShortcuts << shortcut;
                }
            }
            if (newShortcuts.isEmpty())
                shortcutMigrate.remove(shortcutPrefix + key);
            else
                shortcutMigrate.setValue(shortcutPrefix + key, newShortcuts);
        }
    }

    // Change Pause from "P" to "Space"
    const QString pauseKey = "shortcuts/pause";
    if (shortcutMigrate.contains(pauseKey))
    {
        QStringList currentShortcuts = shortcutMigrate.value(pauseKey).toStringList();
        if (currentShortcuts.contains(QKeySequence(Qt::Key_P).toString()))
        {
            QStringList newShortcuts;
            for (const QString &shortcut : currentShortcuts)
            {
                if (QKeySequence(shortcut) == QKeySequence(Qt::Key_P))
                    newShortcuts << QKeySequence(Qt::Key_Space).toString();
                else
                    newShortcuts << shortcut;
            }
            shortcutMigrate.setValue(pauseKey, newShortcuts);
        }
    }
}

void SettingsManager::copyFromOfficial()
{
    const QSet<QString> keysToSkip;
    QSettings src{"wView", "wView"};
    QSettings dst{};

    for (const QString &key : src.allKeys())
    {
        if (keysToSkip.contains(key)) continue;
        dst.setValue(key, src.value(key));
    }
}

void SettingsManager::initializeSettingsLibrary()
{
    // Window
    settingsLibrary.insert("bgcolorenabled", {true, {}});
    settingsLibrary.insert("bgcolor", {"#212121", {}});
    settingsLibrary.insert("checkerboardbackground", {false, {}});
    settingsLibrary.insert("titlebarmode", {static_cast<int>(Qv::TitleBarText::Verbose), {}});
    settingsLibrary.insert("customtitlebartext", {"%z - %n", {}});
    settingsLibrary.insert("windowsizemode", {static_cast<int>(Qv::WindowSizeMode::Auto), {}});
    settingsLibrary.insert("windowpositionmode", {static_cast<int>(Qv::WindowPositionMode::Centered), {}});
    settingsLibrary.insert("menubarenabled", {true, {}});
    settingsLibrary.insert("allowmultiplewindows", {false, {}});
    settingsLibrary.insert("mainmenuicons", {true, {}});
    settingsLibrary.insert("contextmenuicons", {true, {}});
    settingsLibrary.insert("submenuicons", {true, {}});
    settingsLibrary.insert("slideshowkeepswindowontop", {false, {}});
    settingsLibrary.insert("minwindowresizedpercentage", {25, {}});
    settingsLibrary.insert("maxwindowresizedpercentage", {100, {}});
    // Image
    settingsLibrary.insert("smoothscalingmode", {static_cast<int>(Qv::SmoothScalingMode::Expensive), {}});
    settingsLibrary.insert("scalingtwoenabled", {true, {}});
    settingsLibrary.insert("smoothscalinglimitenabled", {true, {}});
    settingsLibrary.insert("smoothscalinglimitpercent", {400, {}});
    settingsLibrary.insert("scalefactor", {25, {}});
    settingsLibrary.insert("cursorzoom", {true, {}});
    settingsLibrary.insert("navresetszoom", {true, {}});
    settingsLibrary.insert("onetoonepixelsizing", {true, {}});
    settingsLibrary.insert("calculatedzoommode", {static_cast<int>(Qv::CalculatedZoomMode::FitWidth), {}});
    settingsLibrary.insert("initialviewmode", {static_cast<int>(Qv::InitialViewMode::Top), {}});
    settingsLibrary.insert("portraitpadding", {static_cast<int>(Qv::HorizontalPortraitPadding::Fifteen), {}});
    settingsLibrary.insert("constrainimageposition", {true, {}});
    settingsLibrary.insert("constraincentersmallimage", {true, {}});
    settingsLibrary.insert("disabledelayedconstraint", {false, {}});
    settingsLibrary.insert("colorspaceconversion", {static_cast<int>(Qv::ColorSpaceConversion::AutoDetect), {}});
    // Miscellaneous
    settingsLibrary.insert("sortmode", {static_cast<int>(Qv::SortMode::Name), {}});
    settingsLibrary.insert("sortdescending", {false, {}});
    settingsLibrary.insert("preloadingmode", {static_cast<int>(Qv::PreloadMode::Adjacent), {}});
    settingsLibrary.insert("navspeed", {50, {}});
    settingsLibrary.insert("loopfoldersenabled", {false, {}});
    settingsLibrary.insert("slideshowdirection", {static_cast<int>(Qv::SlideshowDirection::Forward), {}});
    settingsLibrary.insert("slideshowtimer", {5, {}});
    settingsLibrary.insert("afterdelete", {static_cast<int>(Qv::AfterDelete::MoveForward), {}});
    settingsLibrary.insert("askdelete", {true, {}});
    settingsLibrary.insert("allowmimecontentdetection", {false, {}});
    settingsLibrary.insert("skiphidden", {true, {}});
    // Formats
    settingsLibrary.insert("disabledfileextensions", {"", {}});
    // Mouse
    settingsLibrary.insert("navigationregionsenabled", {false, {}});
    settingsLibrary.insert("viewportdoubleclickaction", {static_cast<int>(Qv::ViewportClickAction::OriginalSize), {}});
    settingsLibrary.insert("viewportaltdoubleclickaction", {static_cast<int>(Qv::ViewportClickAction::ToggleTitlebarHidden), {}});
    settingsLibrary.insert("viewportdragaction", {static_cast<int>(Qv::ViewportDragAction::Pan), {}});
    settingsLibrary.insert("viewportaltdragaction", {static_cast<int>(Qv::ViewportDragAction::MoveWindow), {}});
    settingsLibrary.insert("viewportmiddlebuttonmode", {static_cast<int>(Qv::ClickOrDrag::Click), {}});
    settingsLibrary.insert("viewportmiddleclickaction", {static_cast<int>(Qv::ViewportClickAction::ZoomToFit), {}});
    settingsLibrary.insert("viewportaltmiddleclickaction", {static_cast<int>(Qv::ViewportClickAction::OriginalSize), {}});
    settingsLibrary.insert("viewportmiddledragaction", {static_cast<int>(Qv::ViewportDragAction::Pan), {}});
    settingsLibrary.insert("viewportaltmiddledragaction", {static_cast<int>(Qv::ViewportDragAction::MoveWindow), {}});

    //Mouse gestures (right-button drag)
    settingsLibrary.insert("gesturenavigationenabled", {true, {}});
    settingsLibrary.insert("gesturezoomenabled", {false, {}});
    settingsLibrary.insert("viewportverticalscrollaction", {static_cast<int>(Qv::ViewportScrollAction::Pan), {}});
    settingsLibrary.insert("viewporthorizontalscrollaction", {static_cast<int>(Qv::ViewportScrollAction::Navigate), {}});
    settingsLibrary.insert("viewportaltverticalscrollaction", {static_cast<int>(Qv::ViewportScrollAction::Zoom), {}});
    settingsLibrary.insert("viewportalthorizontalscrollaction", {static_cast<int>(Qv::ViewportScrollAction::Pan), {}});
    settingsLibrary.insert("scrollactioncooldown", {false, {}});
    settingsLibrary.insert("cursorautohidefullscreenenabled", {true, {}});
    settingsLibrary.insert("cursorautohidefullscreendelay", {2, {}});
}
