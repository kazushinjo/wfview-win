// =============================
// audiohandleroutput.cpp
// =============================
#include "audiohandlerqtoutput.h"

bool audioHandlerQtOutput::openDevice() noexcept
{
#if (QT_VERSION < QT_VERSION_CHECK(6,0,0))
    audioOutput = new QAudioOutput(deviceInfo, nativeFormat, this);
    connect(audioOutput, SIGNAL(stateChanged(QAudio::State)), this, SLOT(stateChanged(QAudio::State)));
#else
    audioOutput = new QAudioSink(deviceInfo, nativeFormat, this);
    connect(audioOutput, &QAudioSink::stateChanged, this, &audioHandlerQtOutput::stateChanged);
#endif

    emit setupConverter(radioFormat,  codec,
                        nativeFormat, codecType::LPCM,
                        7, setupData.resampleQuality);

    connect(this, &audioHandlerBase::sendToConverter, converter, &audioConverter::convert);
    connect(converter, SIGNAL(converted(audioPacket)), this, SLOT(onConverted(audioPacket)));

#if (defined(Q_OS_WIN) && (QT_VERSION < QT_VERSION_CHECK(6,0,0)))
    audioOutput->setBufferSize(nativeFormat.bytesForDuration(setupData.latency * 100));
#else
    audioOutput->setBufferSize(nativeFormat.bytesForDuration(setupData.latency * 1000));
#endif

    audioDevice = audioOutput->start();
    if (!audioDevice) return false;

    // QAudioSink may accept only part of a network packet.  Drain retained
    // bytes frequently rather than losing that remainder.
    drainTimer = new QTimer(this);
    drainTimer->setInterval(5);
    connect(drainTimer, &QTimer::timeout, this, &audioHandlerQtOutput::drainPendingAudio);
    drainTimer->start();

    // Pre-fill half the buffer with silence so ALSA has data to pull
    // before the first real audio packet arrives from the network.
    {
        const int prefillBytes = audioOutput->bufferSize() / 2;
        QByteArray silence(prefillBytes, '\0');
        audioDevice->write(silence.constData(), silence.size());
    }

    connect(audioOutput, SIGNAL(destroyed()), audioDevice, SLOT(deleteLater()), Qt::UniqueConnection);
#if (QT_VERSION < QT_VERSION_CHECK(6,0,0))
    qInfo(logAudio()) << "Connected to Qt audio output device" << deviceInfo.deviceName();
#else
    qInfo(logAudio()) << "Connected to Qt audio output device" << deviceInfo.description();
#endif
    return true;
}

void audioHandlerQtOutput::closeDevice() noexcept
{
    if (drainTimer) {
        drainTimer->stop();
        drainTimer->deleteLater();
        drainTimer = nullptr;
    }
    pendingAudio.clear();
    if (audioOutput) {
        if (audioDevice) {
            disconnect(audioDevice, nullptr, this, nullptr);
        }
        if (audioOutput->state() != QAudio::StoppedState) audioOutput->stop();
        disconnect(audioOutput, nullptr, nullptr, nullptr);
        delete audioOutput;
        audioOutput = nullptr;
    }
    audioDevice = nullptr;
}

void audioHandlerQtOutput::incomingAudio(audioPacket packet)
{
    if (!audioDevice || packet.data.isEmpty()) return;
    packet.volume = volume;
    emit sendToConverter(packet);
}

void audioHandlerQtOutput::onConverted(audioPacket pkt)
{
    if (!audioOutput || !audioDevice || pkt.data.isEmpty()) return;
    writeToOutputDevice(pkt.data, pkt.seq, pkt.amplitudePeak, pkt.amplitudeRMS);
}

void audioHandlerQtOutput::writeToOutputDevice(QByteArray data, quint32 seq, float ampPeak, float ampRms)
{
    Q_UNUSED(seq);
    if (!audioOutput || !audioDevice) return;

    qint64 buffered = audioOutput->bufferSize() - audioOutput->bytesFree();
    int devLatencyMs = static_cast<int>(nativeFormat.durationForBytes(buffered) / 1000);
    int pipelineMs   = lastReceived.isValid() ? static_cast<int>(lastReceived.elapsed()) : 0;
    int newLatency   = pipelineMs + devLatencyMs;
    int prev = currentLatency.load(std::memory_order_relaxed);
    currentLatency.store(static_cast<int>(prev * 0.8 + newLatency * 0.2), std::memory_order_relaxed);

    pendingAudio.append(data);
    // Absorb short network bursts but prevent unbounded accumulated latency.
    const int maxPending = qMax(audioOutput->bufferSize() * 8, data.size() * 8);
    if (pendingAudio.size() > maxPending)
        pendingAudio.remove(0, pendingAudio.size() - maxPending);
    drainPendingAudio();

    lastReceived.restart();
    amplitude.store(ampPeak, std::memory_order_relaxed);
    emit haveLevels(amplitudePeak(), static_cast<quint16>(ampRms * 255.0f), setupData.latency, currentLatency.load(), isUnderrun.load(), isOverrun.load());
}

void audioHandlerQtOutput::drainPendingAudio()
{
    if (!audioOutput || !audioDevice || pendingAudio.isEmpty()) return;
    while (!pendingAudio.isEmpty()) {
        const qint64 written = audioDevice->write(pendingAudio.constData(), pendingAudio.size());
        if (written <= 0) break;
        pendingAudio.remove(0, static_cast<int>(written));
    }
    if (pendingAudio.isEmpty()) isUnderrun.store(false, std::memory_order_relaxed);
}

QAudioFormat audioHandlerQtOutput::getNativeFormat()
{
    return setupData.port.preferredFormat();
}

bool audioHandlerQtOutput::isFormatSupported(QAudioFormat f)
{
    return setupData.port.isFormatSupported(f);
}
