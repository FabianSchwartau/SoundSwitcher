#include "shortcut_manager.h"

#include "audio_manager.h"

#include <KGlobalAccel>

#include <QAction>
#include <QCryptographicHash>

ShortcutManager::ShortcutManager(AudioManager *audioManager, QObject *parent)
    : QObject(parent)
    , m_audioManager(audioManager)
{
}

ShortcutManager::~ShortcutManager()
{
    clearActions();
}

void ShortcutManager::setAssignments(
    const QVector<ShortcutAssignment> &assignments,
    const QHash<QString, QString> &displayNames)
{
    clearActions();

    m_assignments = assignments;
    m_displayNames = displayNames;

    for (const auto &assignment : m_assignments) {
        if (!assignment.sinkName.isEmpty() && !assignment.shortcut.isEmpty()) {
            registerAction(assignment);
        }
    }
}

void ShortcutManager::clearActions()
{
    for (QAction *action : std::as_const(m_actions)) {
        KGlobalAccel::self()->removeAllShortcuts(action);
        action->deleteLater();
    }
    m_actions.clear();
}

void ShortcutManager::registerAction(const ShortcutAssignment &assignment)
{
    auto *action = new QAction(this);
    action->setObjectName(actionObjectName(assignment.sinkName));
    action->setText(QStringLiteral("Switch audio output to %1").arg(displayName(assignment.sinkName)));

    connect(action, &QAction::triggered, this, [this, sinkName = assignment.sinkName]() {
        QString errorMessage;
        const bool switched = m_audioManager->switchToOutput(sinkName, &errorMessage);

        if (switched) {
            emit outputChanged();
        }
    });

    const QList<QKeySequence> desiredShortcuts = {assignment.shortcut};
    KGlobalAccel::self()->setShortcut(
        action,
        desiredShortcuts,
        KGlobalAccel::NoAutoloading);

    m_actions.insert(assignment.sinkName, action);
}

QString ShortcutManager::displayName(const QString &sinkName) const
{
    const QString name = m_displayNames.value(sinkName);
    return name.isEmpty() ? sinkName : name;
}

QString ShortcutManager::actionObjectName(const QString &sinkName)
{
    const QByteArray digest = QCryptographicHash::hash(sinkName.toUtf8(), QCryptographicHash::Sha1).toHex();
    return QStringLiteral("switch-output-%1").arg(QString::fromLatin1(digest.left(16)));
}
