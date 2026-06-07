#pragma once

#include "settings_store.h"

#include <QHash>
#include <QObject>
#include <QString>

class QAction;
class AudioManager;

class ShortcutManager : public QObject {
    Q_OBJECT

public:
    explicit ShortcutManager(AudioManager *audioManager, QObject *parent = nullptr);
    ~ShortcutManager() override;

    void setAssignments(
        const QVector<ShortcutAssignment> &assignments,
        const QHash<QString, QString> &displayNames);

signals:
    void outputChanged();

private:
    void clearActions();
    void registerAction(const ShortcutAssignment &assignment);
    QString displayName(const QString &sinkName) const;
    static QString actionObjectName(const QString &sinkName);

    AudioManager *m_audioManager = nullptr;
    QVector<ShortcutAssignment> m_assignments;
    QHash<QString, QString> m_displayNames;
    QHash<QString, QAction *> m_actions;
};
