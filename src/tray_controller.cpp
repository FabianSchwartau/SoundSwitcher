#include "tray_controller.h"

#include "app_icon.h"
#include "audio_manager.h"
#include "main_window.h"
#include "shortcut_manager.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QIcon>
#include <QMenu>
#include <QSystemTrayIcon>

TrayController::TrayController(
    AudioManager *audioManager,
    MainWindow *mainWindow,
    ShortcutManager *shortcutManager,
    QObject *parent)
    : QObject(parent)
    , m_audioManager(audioManager)
    , m_mainWindow(mainWindow)
    , m_trayIcon(new QSystemTrayIcon(this))
    , m_menu(new QMenu())
{
    m_menu->setObjectName(QStringLiteral("trayMenu"));
    m_menu->setTitle(QStringLiteral("SoundSwitcher"));

    m_trayIcon->setIcon(soundSwitcherIcon());
    m_trayIcon->setToolTip(QStringLiteral("SoundSwitcher"));
    m_trayIcon->setContextMenu(m_menu);

    connect(m_trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
            showSettings();
        }
    });
    connect(m_mainWindow, &MainWindow::stateChanged, this, &TrayController::rebuildMenu);
    connect(shortcutManager, &ShortcutManager::outputChanged, m_mainWindow, &MainWindow::refreshOutputs);

    rebuildMenu();
}

void TrayController::show()
{
    m_trayIcon->show();
}

bool TrayController::isAvailable() const
{
    return QSystemTrayIcon::isSystemTrayAvailable();
}

void TrayController::rebuildMenu()
{
    delete m_outputActionGroup;
    m_outputActionGroup = nullptr;
    m_menu->clear();
    m_outputActionGroup = new QActionGroup(m_menu);
    m_outputActionGroup->setExclusive(true);

    auto *settingsAction = m_menu->addAction(QIcon::fromTheme(QStringLiteral("configure")), QStringLiteral("Settings"));
    connect(settingsAction, &QAction::triggered, this, &TrayController::showSettings);

    m_menu->addSeparator();

    const QVector<AudioOutput> outputs = m_mainWindow->outputs();
    if (outputs.isEmpty()) {
        auto *emptyAction = m_menu->addAction(QStringLiteral("No audio outputs"));
        emptyAction->setEnabled(false);
    } else {
        for (const auto &output : outputs) {
            auto *outputAction = m_menu->addAction(QIcon::fromTheme(QStringLiteral("audio-card")), output.description);
            outputAction->setCheckable(true);
            outputAction->setChecked(output.isCurrent);
            outputAction->setData(output.name);
            m_outputActionGroup->addAction(outputAction);
            connect(outputAction, &QAction::triggered, this, [this, sinkName = output.name]() {
                switchToOutput(sinkName);
            });
        }
    }

    m_menu->addSeparator();

    auto *refreshAction = m_menu->addAction(QIcon::fromTheme(QStringLiteral("view-refresh")), QStringLiteral("Refresh Devices"));
    connect(refreshAction, &QAction::triggered, m_mainWindow, &MainWindow::refreshOutputs);

    auto *quitAction = m_menu->addAction(QIcon::fromTheme(QStringLiteral("application-exit")), QStringLiteral("Quit"));
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);
}

void TrayController::showSettings()
{
    m_mainWindow->showAndRaise();
}

void TrayController::switchToOutput(const QString &sinkName)
{
    m_audioManager->switchToOutput(sinkName);
    m_mainWindow->refreshOutputs();
}
