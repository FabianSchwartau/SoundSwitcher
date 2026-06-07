#pragma once

#include "audio_manager.h"
#include "settings_store.h"

#include <QHash>
#include <QMainWindow>

class QCheckBox;
class QLabel;
class QKeySequenceEdit;
class QTableWidget;
class QAction;
class ShortcutManager;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(AudioManager *audioManager, ShortcutManager *shortcutManager, QWidget *parent = nullptr);

    QVector<AudioOutput> outputs() const;
    QHash<QString, QString> displayNames() const;

    void setCloseToTray(bool closeToTray);
    void showAndRaise();

public slots:
    void refreshOutputs();

signals:
    void stateChanged();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    enum Column {
        MarkerColumn = 0,
        OutputColumn,
        ShortcutColumn,
        ActionsColumn,
        ColumnCount
    };

    void setupUi();
    void populateTable();
    void addOutputRow(const AudioOutput &output, const QKeySequence &shortcut, bool available);
    void makeOutputDescriptionsUnique();
    void applyShortcuts();
    void switchOutput(const QString &sinkName);
    void clearShortcut(const QString &sinkName);
    void setAutostartEnabled(bool enabled);
    void markDirty();
    void setStatus(const QString &message);

    QVector<ShortcutAssignment> collectAssignmentsFromTable() const;
    QKeySequence shortcutForSink(const QString &sinkName) const;
    QString descriptionForSink(const QString &sinkName) const;

    AudioManager *m_audioManager = nullptr;
    ShortcutManager *m_shortcutManager = nullptr;

    QTableWidget *m_table = nullptr;
    QLabel *m_statusLabel = nullptr;
    QAction *m_applyAction = nullptr;
    QCheckBox *m_autostartCheckBox = nullptr;

    QVector<AudioOutput> m_outputs;
    QVector<ShortcutAssignment> m_assignments;
    QVector<ShortcutAssignment> m_appliedAssignments;
    QHash<QString, QKeySequenceEdit *> m_editorsBySink;

    bool m_dirty = false;
    bool m_closeToTray = true;
};
