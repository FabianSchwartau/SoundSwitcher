#include "main_window.h"

#include "app_icon.h"
#include "autostart_manager.h"
#include "settings_store.h"
#include "shortcut_manager.h"

#include <QAbstractItemView>
#include <QAction>
#include <QCheckBox>
#include <QCloseEvent>
#include <QColor>
#include <QFont>
#include <QHeaderView>
#include <QIcon>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QHBoxLayout>
#include <QMap>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QSet>
#include <QSignalBlocker>
#include <QSize>
#include <QToolButton>
#include <QStatusBar>
#include <QTableWidget>
#include <QToolBar>
#include <QWidgetAction>

#include <algorithm>

namespace {

QIcon currentOutputMarkerIcon()
{
    QPixmap pixmap(28, 28);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor(QStringLiteral("#0f172a")), 2));
    painter.setBrush(QColor(QStringLiteral("#38d5c8")));
    painter.drawEllipse(QPointF(14, 14), 8, 8);

    painter.setPen(QPen(QColor(QStringLiteral("#ffffff")), 1.5));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(QPointF(14, 14), 4.5, 4.5);

    return QIcon(pixmap);
}

} // namespace

MainWindow::MainWindow(AudioManager *audioManager, ShortcutManager *shortcutManager, QWidget *parent)
    : QMainWindow(parent)
    , m_audioManager(audioManager)
    , m_shortcutManager(shortcutManager)
{
    setWindowTitle(QStringLiteral("SoundSwitcher"));
    setWindowIcon(soundSwitcherIcon());
    resize(920, 460);

    m_assignments = SettingsStore::loadShortcutAssignments();
    m_appliedAssignments = m_assignments;
    setupUi();
}

QVector<AudioOutput> MainWindow::outputs() const
{
    return m_outputs;
}

QHash<QString, QString> MainWindow::displayNames() const
{
    QHash<QString, QString> names;
    for (const auto &output : m_outputs) {
        names.insert(output.name, output.description);
    }

    for (const auto &assignment : m_assignments) {
        if (!names.contains(assignment.sinkName)) {
            names.insert(assignment.sinkName, assignment.sinkName);
        }
    }

    for (const auto &assignment : m_appliedAssignments) {
        if (!names.contains(assignment.sinkName)) {
            names.insert(assignment.sinkName, assignment.sinkName);
        }
    }

    return names;
}

void MainWindow::setCloseToTray(bool closeToTray)
{
    m_closeToTray = closeToTray;
}

void MainWindow::showAndRaise()
{
    show();
    raise();
    activateWindow();
}

void MainWindow::refreshOutputs()
{
    if (m_dirty) {
        m_assignments = collectAssignmentsFromTable();
    }

    QString errorMessage;
    m_outputs = m_audioManager->outputs(&errorMessage);

    std::sort(m_outputs.begin(), m_outputs.end(), [](const AudioOutput &left, const AudioOutput &right) {
        const int descriptionOrder = QString::localeAwareCompare(left.description, right.description);
        if (descriptionOrder != 0) {
            return descriptionOrder < 0;
        }
        return QString::localeAwareCompare(left.name, right.name) < 0;
    });
    makeOutputDescriptionsUnique();

    populateTable();
    m_shortcutManager->setAssignments(m_appliedAssignments, displayNames());

    if (m_dirty) {
        setStatus(QStringLiteral("Unsaved shortcut changes"));
    } else if (errorMessage.isEmpty()) {
        setStatus(QStringLiteral("%1 audio output(s)").arg(m_outputs.size()));
    } else {
        setStatus(errorMessage);
    }

    emit stateChanged();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_closeToTray) {
        hide();
        event->ignore();
        return;
    }

    QMainWindow::closeEvent(event);
}

void MainWindow::setupUi()
{
    auto *toolbar = addToolBar(QStringLiteral("Main Toolbar"));
    toolbar->setMovable(false);
    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    auto *refreshAction = toolbar->addAction(QIcon::fromTheme(QStringLiteral("view-refresh")), QStringLiteral("Refresh"));
    connect(refreshAction, &QAction::triggered, this, &MainWindow::refreshOutputs);

    m_applyAction = toolbar->addAction(QIcon::fromTheme(QStringLiteral("document-save")), QStringLiteral("Apply Shortcuts"));
    m_applyAction->setEnabled(false);
    connect(m_applyAction, &QAction::triggered, this, &MainWindow::applyShortcuts);

    toolbar->addSeparator();

    m_autostartCheckBox = new QCheckBox(QStringLiteral("Start on login"), toolbar);
    m_autostartCheckBox->setToolTip(QStringLiteral("Start SoundSwitcher automatically when you log in."));
    m_autostartCheckBox->setChecked(AutostartManager::isEnabled());
    auto *autostartWidgetAction = new QWidgetAction(toolbar);
    autostartWidgetAction->setDefaultWidget(m_autostartCheckBox);
    toolbar->addAction(autostartWidgetAction);
    connect(m_autostartCheckBox, &QCheckBox::toggled, this, &MainWindow::setAutostartEnabled);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(ColumnCount);
    m_table->setHorizontalHeaderLabels({
        QString(),
        QStringLiteral("Output"),
        QStringLiteral("Shortcut"),
        QString(),
    });
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(MarkerColumn, QHeaderView::Fixed);
    m_table->setColumnWidth(MarkerColumn, 42);
    m_table->horizontalHeader()->setSectionResizeMode(OutputColumn, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(ShortcutColumn, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(ActionsColumn, QHeaderView::Fixed);
    m_table->setColumnWidth(ActionsColumn, 52);

    setCentralWidget(m_table);

    m_statusLabel = new QLabel(this);
    statusBar()->addWidget(m_statusLabel, 1);
}

void MainWindow::populateTable()
{
    m_editorsBySink.clear();
    m_table->setRowCount(0);

    QSet<QString> seenSinks;
    for (const auto &output : m_outputs) {
        seenSinks.insert(output.name);
        addOutputRow(output, shortcutForSink(output.name), true);
    }

    QVector<ShortcutAssignment> unavailableAssignments;
    for (const auto &assignment : m_assignments) {
        if (!seenSinks.contains(assignment.sinkName)) {
            unavailableAssignments.append(assignment);
        }
    }

    std::sort(unavailableAssignments.begin(), unavailableAssignments.end(), [](const auto &left, const auto &right) {
        return QString::localeAwareCompare(left.sinkName, right.sinkName) < 0;
    });

    for (const auto &assignment : unavailableAssignments) {
        AudioOutput output;
        output.name = assignment.sinkName;
        output.description = QStringLiteral("(Unavailable) %1").arg(assignment.sinkName);
        addOutputRow(output, assignment.shortcut, false);
    }

    m_table->clearSelection();
}

void MainWindow::addOutputRow(const AudioOutput &output, const QKeySequence &shortcut, bool available)
{
    const int row = m_table->rowCount();
    m_table->insertRow(row);

    auto *markerItem = new QTableWidgetItem();
    markerItem->setTextAlignment(Qt::AlignCenter);
    markerItem->setFlags(Qt::ItemIsEnabled);
    if (output.isCurrent) {
        markerItem->setIcon(currentOutputMarkerIcon());
        markerItem->setToolTip(QStringLiteral("Current output"));
    }
    m_table->setItem(row, MarkerColumn, markerItem);

    auto *outputItem = new QTableWidgetItem(output.description);
    outputItem->setData(Qt::UserRole, output.name);
    outputItem->setToolTip(output.name);
    outputItem->setFlags(Qt::ItemIsEnabled);
    if (output.isCurrent) {
        QFont font = outputItem->font();
        font.setBold(true);
        outputItem->setFont(font);
    }
    if (!available) {
        QFont font = outputItem->font();
        font.setItalic(true);
        outputItem->setFont(font);
    }
    m_table->setItem(row, OutputColumn, outputItem);

    auto *shortcutWidget = new QWidget(m_table);
    auto *shortcutLayout = new QHBoxLayout(shortcutWidget);
    shortcutLayout->setContentsMargins(6, 2, 6, 2);
    shortcutLayout->setSpacing(4);

    auto *editor = new QKeySequenceEdit(shortcut, shortcutWidget);
    editor->setProperty("sinkName", output.name);
    editor->setMinimumWidth(210);
    connect(editor, &QKeySequenceEdit::keySequenceChanged, this, [this]() {
        markDirty();
    });
    m_editorsBySink.insert(output.name, editor);

    auto *clearButton = new QToolButton(shortcutWidget);
    clearButton->setIcon(QIcon::fromTheme(QStringLiteral("edit-clear")));
    clearButton->setIconSize(QSize(18, 18));
    clearButton->setAutoRaise(true);
    clearButton->setToolTip(QStringLiteral("Clear shortcut"));
    connect(clearButton, &QToolButton::clicked, this, [this, sinkName = output.name]() {
        clearShortcut(sinkName);
    });

    shortcutLayout->addWidget(editor);
    shortcutLayout->addWidget(clearButton);
    m_table->setCellWidget(row, ShortcutColumn, shortcutWidget);

    auto *actionsWidget = new QWidget(m_table);
    auto *actionsLayout = new QHBoxLayout(actionsWidget);
    actionsLayout->setContentsMargins(4, 2, 4, 2);
    actionsLayout->setAlignment(Qt::AlignCenter);

    auto *switchButton = new QToolButton(actionsWidget);
    switchButton->setIcon(QIcon::fromTheme(QStringLiteral("audio-volume-high")));
    switchButton->setIconSize(QSize(20, 20));
    switchButton->setAutoRaise(true);
    switchButton->setEnabled(available);
    switchButton->setToolTip(QStringLiteral("Switch to this output"));
    connect(switchButton, &QToolButton::clicked, this, [this, sinkName = output.name]() {
        switchOutput(sinkName);
    });

    actionsLayout->addWidget(switchButton);
    m_table->setCellWidget(row, ActionsColumn, actionsWidget);

    m_table->setRowHeight(row, 42);
}

void MainWindow::makeOutputDescriptionsUnique()
{
    QHash<QString, int> descriptionCounts;
    for (const auto &output : m_outputs) {
        descriptionCounts[output.description] += 1;
    }

    QHash<QString, int> descriptionIndexes;
    for (auto &output : m_outputs) {
        if (descriptionCounts.value(output.description) < 2) {
            continue;
        }

        const QString baseDescription = output.description;
        const int index = ++descriptionIndexes[baseDescription];
        output.description = QStringLiteral("%1 (%2)").arg(baseDescription).arg(index);
    }
}

void MainWindow::applyShortcuts()
{
    const QVector<ShortcutAssignment> assignments = collectAssignmentsFromTable();

    QMap<QString, QString> seenShortcuts;
    for (const auto &assignment : assignments) {
        const QString sequence = assignment.shortcut.toString(QKeySequence::PortableText);
        if (seenShortcuts.contains(sequence)) {
            QMessageBox::warning(
                this,
                QStringLiteral("Duplicate Shortcut"),
                QStringLiteral("%1 is already assigned to another output.")
                    .arg(assignment.shortcut.toString(QKeySequence::NativeText)));
            return;
        }
        seenShortcuts.insert(sequence, assignment.sinkName);
    }

    m_assignments = assignments;
    m_appliedAssignments = m_assignments;
    SettingsStore::saveShortcutAssignments(m_assignments);
    m_shortcutManager->setAssignments(m_appliedAssignments, displayNames());

    m_dirty = false;
    m_applyAction->setEnabled(false);
    setStatus(QStringLiteral("Shortcuts applied"));
    emit stateChanged();
}

void MainWindow::switchOutput(const QString &sinkName)
{
    if (sinkName.isEmpty()) {
        setStatus(QStringLiteral("No audio output selected"));
        return;
    }

    QString errorMessage;
    const bool switched = m_audioManager->switchToOutput(sinkName, &errorMessage);
    if (switched) {
        setStatus(QStringLiteral("Switched to %1").arg(descriptionForSink(sinkName)));
        refreshOutputs();
        if (!errorMessage.isEmpty()) {
            setStatus(errorMessage);
        }
    } else {
        const QString status = errorMessage.isEmpty()
            ? QStringLiteral("Could not switch audio output")
            : errorMessage;
        refreshOutputs();
        setStatus(status);
    }
}

void MainWindow::clearShortcut(const QString &sinkName)
{
    if (sinkName.isEmpty()) {
        return;
    }

    auto *editor = m_editorsBySink.value(sinkName, nullptr);
    if (editor != nullptr) {
        editor->clear();
        markDirty();
    }
}

void MainWindow::setAutostartEnabled(bool enabled)
{
    QString errorMessage;
    if (!AutostartManager::setEnabled(enabled, &errorMessage)) {
        const QSignalBlocker blocker(m_autostartCheckBox);
        m_autostartCheckBox->setChecked(!enabled);
        QMessageBox::warning(
            this,
            QStringLiteral("Autostart"),
            errorMessage.isEmpty() ? QStringLiteral("Could not update the autostart setting.") : errorMessage);
        return;
    }

    setStatus(enabled ? QStringLiteral("Autostart enabled") : QStringLiteral("Autostart disabled"));
}

void MainWindow::markDirty()
{
    m_dirty = true;
    if (m_applyAction != nullptr) {
        m_applyAction->setEnabled(true);
    }
    setStatus(QStringLiteral("Unsaved shortcut changes"));
}

void MainWindow::setStatus(const QString &message)
{
    if (m_statusLabel != nullptr) {
        m_statusLabel->setText(message);
    }
}

QVector<ShortcutAssignment> MainWindow::collectAssignmentsFromTable() const
{
    QVector<ShortcutAssignment> assignments;

    for (int row = 0; row < m_table->rowCount(); ++row) {
        const auto *outputItem = m_table->item(row, OutputColumn);
        const auto *shortcutWidget = m_table->cellWidget(row, ShortcutColumn);
        auto *editor = shortcutWidget == nullptr
            ? nullptr
            : shortcutWidget->findChild<QKeySequenceEdit *>();
        if (outputItem == nullptr || editor == nullptr) {
            continue;
        }

        ShortcutAssignment assignment;
        assignment.sinkName = outputItem->data(Qt::UserRole).toString();
        assignment.shortcut = editor->keySequence();

        if (!assignment.sinkName.isEmpty() && !assignment.shortcut.isEmpty()) {
            assignments.append(assignment);
        }
    }

    return assignments;
}

QKeySequence MainWindow::shortcutForSink(const QString &sinkName) const
{
    for (const auto &assignment : m_assignments) {
        if (assignment.sinkName == sinkName) {
            return assignment.shortcut;
        }
    }

    return {};
}

QString MainWindow::descriptionForSink(const QString &sinkName) const
{
    for (const auto &output : m_outputs) {
        if (output.name == sinkName) {
            return output.description;
        }
    }

    return sinkName;
}
