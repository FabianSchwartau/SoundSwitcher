#include "settings_store.h"

#include <QSettings>

QVector<ShortcutAssignment> SettingsStore::loadShortcutAssignments()
{
    QVector<ShortcutAssignment> assignments;
    QSettings settings;

    const int size = settings.beginReadArray(QStringLiteral("shortcuts"));
    assignments.reserve(size);
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);

        ShortcutAssignment assignment;
        assignment.sinkName = settings.value(QStringLiteral("sinkName")).toString();
        assignment.shortcut = QKeySequence::fromString(
            settings.value(QStringLiteral("sequence")).toString(),
            QKeySequence::PortableText);

        if (!assignment.sinkName.isEmpty() && !assignment.shortcut.isEmpty()) {
            assignments.append(assignment);
        }
    }
    settings.endArray();

    return assignments;
}

void SettingsStore::saveShortcutAssignments(const QVector<ShortcutAssignment> &assignments)
{
    QSettings settings;
    settings.remove(QStringLiteral("shortcuts"));
    settings.beginWriteArray(QStringLiteral("shortcuts"));

    int index = 0;
    for (const auto &assignment : assignments) {
        if (assignment.sinkName.isEmpty() || assignment.shortcut.isEmpty()) {
            continue;
        }

        settings.setArrayIndex(index++);
        settings.setValue(QStringLiteral("sinkName"), assignment.sinkName);
        settings.setValue(
            QStringLiteral("sequence"),
            assignment.shortcut.toString(QKeySequence::PortableText));
    }

    settings.endArray();
    settings.sync();
}
