#ifndef ICOMUDPAUDIO_H
#define ICOMUDPAUDIO_H


#include <QObject>
#include <QUdpSocket>
#include <QNetworkDatagram>
#include <QHostInfo>
#include <QTimer>
#include <QMutex>
#include <QDateTime>
#include <QByteArray>
#include <QVector>
#include <QMap>
#include <QUuid>

// Allow easy endian-ness conversions
#include <QtEndian>

// Needed for audio
#include <QBuffer>
#include <QThread>

#include <QDebug>

#include "packettypes.h"

#include "icomudpbase.h"

#include "audiohandler.h"

// Class for all audio communications.
class icomUdpAudio : public icomUdpBase
{
	Q_OBJECT

public:
	icomUdpAudio(QHostAddress local, QHostAddress ip, quint16 aport, quint16 lport, audioSetup rxSetup, audioSetup txSetup);
	~icomUdpAudio();

	int audioLatency = 0;

	// Count of RX audio packets dropped in the last short window (since the
	// previous call), combining both drop points: whole packets skipped here
	// in dataReceived() for sustained excess latency, and packets discarded
	// downstream in the output handler (late frames, stale/reordered
	// duplicates, gaps given up on -- see audioHandlerBase::noteAudioDrop()).
	// The lifetime packetsLost/packetsSent ratio shown in the connection
	// status is a cumulative average since connect and hides short, sharp
	// glitches; this is meant to be sampled every second or two instead.
	int fetchAndResetRecentAudioDrops()
	{
		int n = recentExcessLatencyDrops;
		recentExcessLatencyDrops = 0;
		if (rxaudio != Q_NULLPTR)
			n += rxaudio->fetchAndResetRecentDrops();
		return n;
	}

signals:
    void haveAudioData(audioPacket data);

    void setupTxAudio(audioSetup setup);
    void setupRxAudio(audioSetup setup);

    void haveChangeLatency(quint16 value);
    void haveSetVolume(quint8 value);
    void haveRxLevels(quint16 amplitudePeak, quint16 amplitudeRMS, quint16 latency, quint16 current, bool under, bool over);
    void haveTxLevels(quint16 amplitudePeak, quint16 amplitudeRMS, quint16 latency, quint16 current, bool under, bool over);


public slots:
	void changeLatency(quint16 value);
	void setVolume(quint8 value);
    void setRxMuted(bool muted);
    void getRxLevels(quint16 amplitude, quint16 amplitudeRMS, quint16 latency, quint16 current, bool under, bool over);
    void getTxLevels(quint16 amplitude, quint16 amplitudeRMS, quint16 latency, quint16 current, bool under, bool over);
    void receiveAudioData(audioPacket audio);


private slots:
    void onRxAudioInitFailed();
    void onTxAudioInitFailed();

private:

	void sendTxAudio();
	void dataReceived();
	void watchdog();
	void startAudio();
	audioSetup rxSetup;
	audioSetup txSetup;

	uint16_t sendAudioSeq = 0;

    audioHandlerBase* rxaudio = Q_NULLPTR;
    QThread* rxAudioThread = Q_NULLPTR;

    audioHandlerBase* txaudio = Q_NULLPTR;
    QThread* txAudioThread = Q_NULLPTR;

    QTimer* txAudioTimer = Q_NULLPTR;
	bool enableTx = true;

	QMutex audioMutex;

    bool         m_rxMuted = false;

    QElapsedTimer audioClock;
    bool   audioHaveBase = false;
    quint32 audioBaseSeq = 0;       // extended seq
    qint64  audioBaseNs  = 0;       // base arrival time (monotonic)
    int     audioPktMs   = 20;      // TODO set to your actual framing (10/20/40ms etc)
    int     latencyCounter = 0;
    // Whole incoming packets skipped in dataReceived() because the network's
    // ping-measured lateness sustainedly exceeded the configured RX jitter
    // buffer (see the "Latency sustained -> flushing audio" qInfo). Read via
    // fetchAndResetRecentExcessLatencyDrops(); same thread as dataReceived()
    // so a plain int is fine.
    int     recentExcessLatencyDrops = 0;

    // Short-UDP-gap concealment (dataReceived()): repeats the last received
    // packet's audio under the missing extended seq numbers instead of
    // leaving a hole, preserving audio timing without a click. Members
    // rather than function-local statics so a second icomUdpAudio instance
    // in the same process (e.g. a second simultaneous rig connection) gets
    // its own concealment state instead of sharing one.
    quint32     lastEmittedSeq = 0;
    audioPacket lastEmittedAudio;

};

#endif
