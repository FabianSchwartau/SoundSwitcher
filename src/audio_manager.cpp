#include "audio_manager.h"

#include <pulse/pulseaudio.h>

#include <QStringList>

namespace {

QString pulseError(pa_context *context, const QString &fallback)
{
    if (context == nullptr) {
        return fallback;
    }

    const auto errorCode = pa_context_errno(context);
    const char *message = pa_strerror(errorCode);
    if (message == nullptr || message[0] == '\0') {
        return fallback;
    }

    return QString::fromUtf8(message);
}

struct ServerInfoQuery {
    AudioManager *manager = nullptr;
    QString defaultSinkName;
    QString error;
    bool done = false;
};

struct SinkListQuery {
    AudioManager *manager = nullptr;
    QVector<AudioOutput> outputs;
    QString error;
    bool done = false;
};

struct SinkInputListQuery {
    AudioManager *manager = nullptr;
    QVector<std::uint32_t> inputIndexes;
    QString error;
    bool done = false;
};

struct SuccessQuery {
    AudioManager *manager = nullptr;
    bool success = false;
    bool done = false;
};

void serverInfoCallback(pa_context *, const pa_server_info *info, void *userdata)
{
    auto *query = static_cast<ServerInfoQuery *>(userdata);
    if (info == nullptr) {
        query->error = QStringLiteral("Could not read PulseAudio server information.");
    } else if (info->default_sink_name != nullptr) {
        query->defaultSinkName = QString::fromUtf8(info->default_sink_name);
    }

    query->done = true;
    query->manager->signalMainloop();
}

void sinkInfoListCallback(pa_context *context, const pa_sink_info *info, int eol, void *userdata)
{
    auto *query = static_cast<SinkListQuery *>(userdata);
    if (eol < 0) {
        query->error = pulseError(context, QStringLiteral("Could not read audio outputs."));
        query->done = true;
        query->manager->signalMainloop();
        return;
    }

    if (eol > 0) {
        query->done = true;
        query->manager->signalMainloop();
        return;
    }

    if (info == nullptr || info->name == nullptr) {
        return;
    }

    AudioOutput output;
    output.name = QString::fromUtf8(info->name);
    output.description = info->description != nullptr
        ? QString::fromUtf8(info->description)
        : output.name;
    query->outputs.append(output);
}

void sinkInputListCallback(pa_context *context, const pa_sink_input_info *info, int eol, void *userdata)
{
    auto *query = static_cast<SinkInputListQuery *>(userdata);
    if (eol < 0) {
        query->error = pulseError(context, QStringLiteral("Could not read active audio streams."));
        query->done = true;
        query->manager->signalMainloop();
        return;
    }

    if (eol > 0) {
        query->done = true;
        query->manager->signalMainloop();
        return;
    }

    if (info != nullptr) {
        query->inputIndexes.append(info->index);
    }
}

void successCallback(pa_context *, int success, void *userdata)
{
    auto *query = static_cast<SuccessQuery *>(userdata);
    query->success = success != 0;
    query->done = true;
    query->manager->signalMainloop();
}

} // namespace

AudioManager::AudioManager(QObject *parent)
    : QObject(parent)
{
}

AudioManager::~AudioManager()
{
    disconnectFromServer();
}

QVector<AudioOutput> AudioManager::outputs(QString *errorMessage)
{
    if (!ensureConnected(errorMessage)) {
        return {};
    }

    pa_threaded_mainloop_lock(m_mainloop);

    QString defaultSinkName;
    if (!readDefaultSinkNameLocked(&defaultSinkName, errorMessage)) {
        pa_threaded_mainloop_unlock(m_mainloop);
        return {};
    }

    SinkListQuery sinkQuery;
    sinkQuery.manager = this;
    pa_operation *operation = pa_context_get_sink_info_list(m_context, sinkInfoListCallback, &sinkQuery);
    if (operation == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = pulseError(m_context, QStringLiteral("Could not request audio outputs."));
        }
        pa_threaded_mainloop_unlock(m_mainloop);
        return {};
    }

    while (!sinkQuery.done) {
        pa_threaded_mainloop_wait(m_mainloop);
    }
    pa_operation_unref(operation);
    pa_threaded_mainloop_unlock(m_mainloop);

    if (!sinkQuery.error.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = sinkQuery.error;
        }
        return {};
    }

    bool useCurrentOutputHint = currentOutputHintIsValid();
    if (useCurrentOutputHint) {
        useCurrentOutputHint = false;
        for (const auto &output : std::as_const(sinkQuery.outputs)) {
            if (output.name == m_currentOutputHint) {
                useCurrentOutputHint = true;
                break;
            }
        }
    }

    for (auto &output : sinkQuery.outputs) {
        if (useCurrentOutputHint) {
            output.isCurrent = output.name == m_currentOutputHint;
        } else {
            output.isCurrent = output.name == defaultSinkName;
        }
    }

    return sinkQuery.outputs;
}

bool AudioManager::switchToOutput(const QString &sinkName, QString *errorMessage)
{
    if (sinkName.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("No audio output selected.");
        }
        return false;
    }

    if (!ensureConnected(errorMessage)) {
        return false;
    }

    const QByteArray sinkNameUtf8 = sinkName.toUtf8();

    pa_threaded_mainloop_lock(m_mainloop);

    SuccessQuery defaultQuery;
    defaultQuery.manager = this;
    pa_operation *operation = pa_context_set_default_sink(
        m_context,
        sinkNameUtf8.constData(),
        successCallback,
        &defaultQuery);

    if (operation == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = pulseError(m_context, QStringLiteral("Could not request output switch."));
        }
        pa_threaded_mainloop_unlock(m_mainloop);
        return false;
    }

    while (!defaultQuery.done) {
        pa_threaded_mainloop_wait(m_mainloop);
    }
    pa_operation_unref(operation);

    if (!defaultQuery.success) {
        if (errorMessage != nullptr) {
            *errorMessage = pulseError(m_context, QStringLiteral("Could not switch the default audio output."));
        }
        pa_threaded_mainloop_unlock(m_mainloop);
        return false;
    }

    SinkInputListQuery inputQuery;
    inputQuery.manager = this;
    operation = pa_context_get_sink_input_info_list(m_context, sinkInputListCallback, &inputQuery);
    if (operation != nullptr) {
        while (!inputQuery.done) {
            pa_threaded_mainloop_wait(m_mainloop);
        }
        pa_operation_unref(operation);
    }

    QStringList failedMoves;
    for (const auto inputIndex : inputQuery.inputIndexes) {
        SuccessQuery moveQuery;
        moveQuery.manager = this;
        operation = pa_context_move_sink_input_by_name(
            m_context,
            inputIndex,
            sinkNameUtf8.constData(),
            successCallback,
            &moveQuery);

        if (operation == nullptr) {
            failedMoves.append(QString::number(inputIndex));
            continue;
        }

        while (!moveQuery.done) {
            pa_threaded_mainloop_wait(m_mainloop);
        }
        pa_operation_unref(operation);

        if (!moveQuery.success) {
            failedMoves.append(QString::number(inputIndex));
        }
    }

    pa_threaded_mainloop_unlock(m_mainloop);

    if (!failedMoves.isEmpty() && errorMessage != nullptr) {
        *errorMessage = QStringLiteral("The default output changed, but some active streams could not be moved.");
    }

    m_currentOutputHint = sinkName;
    m_currentOutputHintTimer.restart();

    return true;
}

bool AudioManager::currentOutputHintIsValid() const
{
    return !m_currentOutputHint.isEmpty()
        && m_currentOutputHintTimer.isValid()
        && m_currentOutputHintTimer.elapsed() < 5000;
}

bool AudioManager::readDefaultSinkNameLocked(QString *sinkName, QString *errorMessage)
{
    ServerInfoQuery query;
    query.manager = this;

    pa_operation *operation = pa_context_get_server_info(m_context, serverInfoCallback, &query);
    if (operation == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = pulseError(m_context, QStringLiteral("Could not request PulseAudio server information."));
        }
        return false;
    }

    while (!query.done) {
        pa_threaded_mainloop_wait(m_mainloop);
    }
    pa_operation_unref(operation);

    if (!query.error.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = query.error;
        }
        return false;
    }

    if (sinkName != nullptr) {
        *sinkName = query.defaultSinkName;
    }
    return true;
}

bool AudioManager::ensureConnected(QString *errorMessage)
{
    if (m_context != nullptr && pa_context_get_state(m_context) == PA_CONTEXT_READY) {
        return true;
    }

    disconnectFromServer();

    m_mainloop = pa_threaded_mainloop_new();
    if (m_mainloop == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Could not create the PulseAudio main loop.");
        }
        return false;
    }

    m_context = pa_context_new(pa_threaded_mainloop_get_api(m_mainloop), "SoundSwitcher");
    if (m_context == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Could not create the PulseAudio context.");
        }
        disconnectFromServer();
        return false;
    }

    pa_context_set_state_callback(m_context, contextStateCallback, this);

    pa_threaded_mainloop_lock(m_mainloop);

    if (pa_context_connect(m_context, nullptr, PA_CONTEXT_NOFLAGS, nullptr) < 0) {
        if (errorMessage != nullptr) {
            *errorMessage = pulseError(m_context, QStringLiteral("Could not connect to PulseAudio."));
        }
        pa_threaded_mainloop_unlock(m_mainloop);
        disconnectFromServer();
        return false;
    }

    if (pa_threaded_mainloop_start(m_mainloop) < 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Could not start the PulseAudio main loop.");
        }
        pa_threaded_mainloop_unlock(m_mainloop);
        disconnectFromServer();
        return false;
    }

    while (true) {
        const pa_context_state_t state = pa_context_get_state(m_context);
        if (state == PA_CONTEXT_READY) {
            pa_threaded_mainloop_unlock(m_mainloop);
            return true;
        }

        if (!PA_CONTEXT_IS_GOOD(state)) {
            if (errorMessage != nullptr) {
                *errorMessage = pulseError(m_context, QStringLiteral("PulseAudio connection failed."));
            }
            pa_threaded_mainloop_unlock(m_mainloop);
            disconnectFromServer();
            return false;
        }

        pa_threaded_mainloop_wait(m_mainloop);
    }
}

void AudioManager::disconnectFromServer()
{
    if (m_mainloop == nullptr) {
        return;
    }

    pa_threaded_mainloop_lock(m_mainloop);
    if (m_context != nullptr) {
        pa_context_set_state_callback(m_context, nullptr, nullptr);
        pa_context_disconnect(m_context);
        pa_context_unref(m_context);
        m_context = nullptr;
    }
    pa_threaded_mainloop_unlock(m_mainloop);

    pa_threaded_mainloop_stop(m_mainloop);
    pa_threaded_mainloop_free(m_mainloop);
    m_mainloop = nullptr;
}

void AudioManager::signalMainloop()
{
    if (m_mainloop != nullptr) {
        pa_threaded_mainloop_signal(m_mainloop, 0);
    }
}

void AudioManager::contextStateCallback(pa_context *, void *userdata)
{
    auto *manager = static_cast<AudioManager *>(userdata);
    manager->signalMainloop();
}
