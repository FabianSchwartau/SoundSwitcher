#include "autostart_manager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QSettings>
#include <QStringList>
#include <QTextStream>

bool AutostartManager::isEnabled()
{
    const QString userFile = userAutostartFilePath();
    if (QFileInfo::exists(userFile)) {
        return !desktopFileIsHidden(userFile);
    }

    for (const QString &systemFile : systemAutostartFilePaths()) {
        if (QFileInfo::exists(systemFile)) {
            return !desktopFileIsHidden(systemFile);
        }
    }

    return false;
}

bool AutostartManager::setEnabled(bool enabled, QString *errorMessage)
{
    return writeDesktopFile(enabled, errorMessage);
}

QString AutostartManager::userAutostartFilePath()
{
    return QDir(autostartDirectory()).filePath(QStringLiteral(SOUNDSWITCHER_DESKTOP_ID ".desktop"));
}

QString AutostartManager::autostartDirectory()
{
    const QString configHome = qEnvironmentVariable("XDG_CONFIG_HOME");
    if (!configHome.isEmpty()) {
        return QDir(configHome).filePath(QStringLiteral("autostart"));
    }

    return QDir::home().filePath(QStringLiteral(".config/autostart"));
}

QStringList AutostartManager::systemAutostartFilePaths()
{
    QStringList configDirs = QString::fromLocal8Bit(qgetenv("XDG_CONFIG_DIRS")).split(QLatin1Char(':'), Qt::SkipEmptyParts);
    if (configDirs.isEmpty()) {
        configDirs.append(QStringLiteral("/etc/xdg"));
    }

    QStringList paths;
    for (const QString &configDir : configDirs) {
        paths.append(QDir(configDir).filePath(QStringLiteral("autostart/" SOUNDSWITCHER_DESKTOP_ID ".desktop")));
    }
    return paths;
}

bool AutostartManager::desktopFileIsHidden(const QString &path)
{
    QSettings settings(path, QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("Desktop Entry"));
    return settings.value(QStringLiteral("Hidden"), false).toBool();
}

bool AutostartManager::writeDesktopFile(bool enabled, QString *errorMessage)
{
    const QString directory = autostartDirectory();
    if (!QDir().mkpath(directory)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Could not create the autostart directory.");
        }
        return false;
    }

    QSaveFile file(userAutostartFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Could not write the autostart entry.");
        }
        return false;
    }

    QTextStream stream(&file);
    stream << "[Desktop Entry]\n";
    stream << "Type=Application\n";
    stream << "Name=SoundSwitcher\n";
    stream << "Comment=Switch audio outputs with global shortcuts\n";
    stream << "Exec=" << quotedExecutable() << " --hidden\n";
    stream << "Icon=" << SOUNDSWITCHER_DESKTOP_ID << "\n";
    stream << "Terminal=false\n";
    stream << "Categories=AudioVideo;Audio;Utility;Qt;KDE;\n";
    stream << "StartupNotify=false\n";
    stream << "X-GNOME-Autostart-enabled=true\n";
    stream << "X-KDE-autostart-after=panel\n";
    if (!enabled) {
        stream << "Hidden=true\n";
    }

    if (!file.commit()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Could not save the autostart entry.");
        }
        return false;
    }

    return true;
}

QString AutostartManager::quotedExecutable()
{
    QString path = QCoreApplication::applicationFilePath();
    path.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    path.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    path.replace(QLatin1Char('$'), QStringLiteral("\\$"));
    path.replace(QLatin1Char('`'), QStringLiteral("\\`"));
    return QStringLiteral("\"%1\"").arg(path);
}
