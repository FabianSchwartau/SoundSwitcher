#pragma once

#include <QString>
#include <QStringList>

class AutostartManager {
public:
    static bool isEnabled();
    static bool setEnabled(bool enabled, QString *errorMessage = nullptr);
    static QString userAutostartFilePath();

private:
    static QString autostartDirectory();
    static QStringList systemAutostartFilePaths();
    static bool desktopFileIsHidden(const QString &path);
    static bool writeDesktopFile(bool enabled, QString *errorMessage);
    static QString quotedExecutable();
};
