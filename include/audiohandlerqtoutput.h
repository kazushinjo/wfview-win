#ifndef AUDIOHANDLERQTOUTPUT_H
#define AUDIOHANDLERQTOUTPUT_H
#include "audiohandlerbase.h"

class audioHandlerQtOutput : public audioHandlerBase
{
    Q_OBJECT

public:
    explicit audioHandlerQtOutput(QObject* parent = nullptr) : audioHandlerBase(parent) {}
    ~audioHandlerQtOutput() override { dispose(); } // ensure close on destruction
    QString role() const override { return QStringLiteral("Output"); }

public slots:
    void incomingAudio(audioPacket packet);

protected:
    bool openDevice() noexcept override;
    void closeDevice() noexcept override;
    virtual QAudioFormat getNativeFormat() override;
    virtual bool isFormatSupported(QAudioFormat f) override;

private:
    void writeToOutputDevice(QByteArray data, quint32 seq, float amplitudePeak, float amplitudeRms);
    void drainPendingAudio();

#if (QT_VERSION < QT_VERSION_CHECK(6,0,0))
    QAudioOutput*    audioOutput {nullptr};
#else
    QAudioSink*      audioOutput {nullptr};
#endif

    QIODevice*       audioDevice {nullptr};
    QByteArray        pendingAudio;
    QTimer*           drainTimer {nullptr};

    // Reorder-by-sequence gate: converted packets arrive from the network in
    // whatever order UDP delivered/retransmitted them, not necessarily
    // temporal order. Writing them straight to the device in arrival order
    // splices late/reordered audio into the wrong spot in the stream, which
    // is heard as bursts of static/crackle (worst on cellular). Hold each
    // packet briefly until its predecessor shows up, or give up and skip
    // ahead if it never does (a genuine loss, not just reordering).
    bool             haveSeq {false};
    quint32          nextSeq {0};
    QMap<quint32, audioPacket> reorderBuf;
    static constexpr int REORDER_WAIT_MS = 60;
    static constexpr int REORDER_MAX_PACKETS = 10;

private slots:
    void onConverted(audioPacket pkt);

};

#endif // AUDIOHANDLERQTOUTPUT_H
