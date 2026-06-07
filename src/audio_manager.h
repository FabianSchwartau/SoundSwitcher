#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QVector>

struct pa_context;
struct pa_threaded_mainloop;

struct AudioOutput {
    QString name;
    QString description;
    bool isCurrent = false;
};

class AudioManager : public QObject {
    Q_OBJECT

public:
    explicit AudioManager(QObject *parent = nullptr);
    ~AudioManager() override;

    QVector<AudioOutput> outputs(QString *errorMessage = nullptr);
    bool switchToOutput(const QString &sinkName, QString *errorMessage = nullptr);
    void signalMainloop();

private:
    bool currentOutputHintIsValid() const;
    bool readDefaultSinkNameLocked(QString *sinkName, QString *errorMessage);
    bool ensureConnected(QString *errorMessage);
    void disconnectFromServer();

    static void contextStateCallback(pa_context *context, void *userdata);

    pa_threaded_mainloop *m_mainloop = nullptr;
    pa_context *m_context = nullptr;
    QString m_currentOutputHint;
    QElapsedTimer m_currentOutputHintTimer;
};
