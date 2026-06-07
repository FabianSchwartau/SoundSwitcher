#pragma once

#include <QKeySequence>
#include <QString>
#include <QVector>

struct ShortcutAssignment {
    QString sinkName;
    QKeySequence shortcut;
};

class SettingsStore {
public:
    static QVector<ShortcutAssignment> loadShortcutAssignments();
    static void saveShortcutAssignments(const QVector<ShortcutAssignment> &assignments);
};
