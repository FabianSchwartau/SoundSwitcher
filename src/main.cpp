#include "audio_manager.h"
#include "main_window.h"
#include "shortcut_manager.h"
#include "tray_controller.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QSystemTrayIcon>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);

    QCoreApplication::setOrganizationName(QStringLiteral("SoundSwitcher"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("soundswitcher.local"));
    QCoreApplication::setApplicationName(QStringLiteral("SoundSwitcher"));
    QCoreApplication::setApplicationVersion(QStringLiteral(SOUNDSWITCHER_VERSION));
    QApplication::setDesktopFileName(QStringLiteral(SOUNDSWITCHER_DESKTOP_ID));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Switch audio outputs with global shortcuts."));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption hiddenOption(
        QStringLiteral("hidden"),
        QStringLiteral("Start without opening the settings window."));
    parser.addOption(hiddenOption);
    parser.process(application);

    AudioManager audioManager;
    ShortcutManager shortcutManager(&audioManager);
    MainWindow mainWindow(&audioManager, &shortcutManager);
    TrayController trayController(&audioManager, &mainWindow, &shortcutManager);
    mainWindow.refreshOutputs();

    const bool trayAvailable = trayController.isAvailable();
    QApplication::setQuitOnLastWindowClosed(!trayAvailable);
    mainWindow.setCloseToTray(trayAvailable);

    if (trayAvailable) {
        trayController.show();
    }

    if (!parser.isSet(hiddenOption) || !trayAvailable) {
        mainWindow.showAndRaise();
    }

    return application.exec();
}
