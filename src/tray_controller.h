#pragma once

#include <QObject>

class AudioManager;
class QActionGroup;
class MainWindow;
class QMenu;
class QSystemTrayIcon;
class ShortcutManager;

class TrayController : public QObject {
    Q_OBJECT

public:
    TrayController(AudioManager *audioManager, MainWindow *mainWindow, ShortcutManager *shortcutManager, QObject *parent = nullptr);

    void show();
    bool isAvailable() const;

private:
    void rebuildMenu();
    void showSettings();
    void switchToOutput(const QString &sinkName);

    AudioManager *m_audioManager = nullptr;
    MainWindow *m_mainWindow = nullptr;
    QActionGroup *m_outputActionGroup = nullptr;
    QSystemTrayIcon *m_trayIcon = nullptr;
    QMenu *m_menu = nullptr;
};
