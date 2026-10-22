#ifndef KX3WINDOW_H
#define KX3WINDOW_H

// Dedicated control screen for the Elecraft KX3 (and KX2), connected through
// a USB-serial CAT cable (KXUSB or similar).
//
// The KX3 speaks the Elecraft K3/KX3 text CAT protocol ("FA;", "IF;" ...).
// It looks Kenwood-like, but the IF layout, meter and level scales differ
// from the Kenwood rig definitions, and the radio has no spectrum data on
// CAT.  As with the FT-817, wfmain hands the serial port to this
// self-contained window instead of starting the regular rig connection.
// The window is laid out after the radio's own front panel.

#include <QWidget>
#include <QByteArray>
#include <QString>
#include <QList>
#include <QVector>
#include <QRectF>
#include <QPair>

#include <QPushButton>

class QSerialPort;
class QTimer;
class QLineEdit;
class QLabel;
class QBoxLayout;

// Model ID stored in prefs.radioCIVAddr / the connection profile for the
// Elecraft manufacturer.  The radio answers "ID017;" like every Elecraft
// rig, so this is only a marker.
constexpr quint16 KX3_MODEL_ID = 3003;

// State shown on the LCD.
struct kx3LcdState
{
    bool connected = false;
    bool catOk = false;
    QString model = "KX3";
    qint64 vfoAHz = 0;
    qint64 vfoBHz = 0;
    int mode = 2;           // MD code: 1 LSB 2 USB 3 CW 4 FM 5 AM 6 DATA 7 CW-R 9 DATA-R
    int dataSub = -1;       // DT code in DATA modes, -1 unknown
    bool transmitting = false;
    int sMeter = 0;         // SM raw 0-15
    int txBar = 0;          // BG 0-10 while transmitting
    bool split = false;
    bool rit = false;
    bool xit = false;
    int offsetHz = 0;       // RIT/XIT offset
    bool locked = false;
    bool preamp = false;
    bool att = false;
    bool nb = false;
    bool vox = false;
    QString agc;            // "FST" / "SLO"
    int band = -1;          // BN code
    int afGain = -1;        // AG 0-60
    int rfGain = -1;        // RG 190-250
    int powerTenths = -1;   // PC in 0.1 W
    int bandwidthHz = -1;   // BW
    int ifShiftHz = -1;     // IS: passband centre
    int micGain = -1;       // MG
    int keyerWpm = -1;      // KS
    int squelch = -1;       // SQ 0-29
    bool apf = false;
    bool nr = false;
    bool notch = false;
    int step = 10;
    QString knobs;          // current function of the four small knobs, shown on the LCD
};

// Front panel push button: tap and hold (0.5 s) are separate functions, as
// on the radio.  The hold function is printed above the key.
class kx3Key : public QPushButton
{
    Q_OBJECT
public:
    kx3Key(const QString& tap, QWidget *parent = nullptr);

signals:
    void tapped();
    void held();

private:
    QTimer *holdTimer;
    bool holdFired = false;
};

class kx3Lcd : public QWidget
{
    Q_OBJECT
public:
    explicit kx3Lcd(QWidget *parent = nullptr);
    void setState(const kx3LcdState& s);
    QSize sizeHint() const override { return QSize(560, 220); }

signals:
    void digitClicked(int step);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    void drawSegmentDigit(QPainter& p, const QRectF& r, QChar c, bool lit);

    kx3LcdState st;
    QVector<QPair<QRectF, int>> digitRects;
};

class kx3Window : public QWidget
{
    Q_OBJECT
public:
    kx3Window(const QString& portName, quint32 baudRate, QWidget *parent = nullptr);
    ~kx3Window() override;

    bool openPort();

signals:
    void closed();
    void exitRequested();   // radio powered off with the 電源 OFF key: quit wfview

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void readSerial();
    void poll();
    void responseTimeout();
    void safetyTimeout();

private:
    struct catItem {
        QByteArray command;     // without the trailing ';'
        QByteArray reply;       // expected reply prefix; empty for set commands
        bool coalesce;          // a newer queued command with the same prefix replaces it
    };

    void buildUi();
    void updateUi();

    void enqueue(const QByteArray& command, const QByteArray& reply = QByteArray(),
                 bool front = false, bool coalesce = false);
    void set(const QByteArray& command, bool coalesce = false);
    void read(const QByteArray& command);
    void processQueue();
    void finishCurrent();
    void handleFrame(const QByteArray& frame);
    void parseIf(const QByteArray& frame);
    void refreshAll();
    void appendLog(const QString& text);
    bool isOpen() const;

    void setVfoA(qint64 hz);
    void setVfoB(qint64 hz);
    void moveOffset(int steps);
    void setAfGain(int value);
    void setRfGain(int value);
    void setSquelch(int value);
    void setPower(int tenths);
    void setBandwidth(int hz);
    void setIfShift(int hz);
    void setMicGain(int value);
    void setKeyerSpeed(int wpm);
    void pressSwitch(int code, bool hold, const QString& name);
    void afterSwitch();
    QWidget *makePanelKey(const QString& tap, const QString& hold, int code, const QString& tip);
    void turnKnob(int knob, int steps);
    void sendCw();
    void powerOff();
    void startTransmit();
    void stopTransmit();
    void sendNow(const QByteArray& command);

    static QString formatFrequency(qint64 hz);

    QString portName;
    quint32 baudRate;
    QSerialPort *serial = nullptr;
    QTimer *pollTimer = nullptr;
    QTimer *responseTimer = nullptr;
    QTimer *gapTimer = nullptr;
    QTimer *safetyTimer = nullptr;

    QList<catItem> queue;
    bool busy = false;
    catItem current;
    QByteArray rxBuffer;
    int pollPhase = 0;
    int slowIndex = 0;
    int consecutiveTimeouts = 0;
    qint64 lastControlMs = 0;
    int savedK2Level = -1;      // "K2n;" at connect, restored on close
    bool isKx2 = false;

    kx3LcdState st;

    kx3Lcd *lcd = nullptr;
    QPushButton *pttBtn = nullptr;
    QPushButton *disconnectBtn = nullptr;
    QLineEdit *freqInput = nullptr;
    QLineEdit *cwInput = nullptr;
    QLabel *modelLabel = nullptr;
    QList<QLabel*> knobLabels;  // value under each small knob

    // Small knob functions (selected by tapping the knob, as on the radio)
    enum { knobAf, knobPbt, knobKeyer, knobOfs };
    bool afIsRf = false;        // AF/RF-SQL: false AF gain, true RF gain / squelch
    bool pbtShift = false;      // PBT I/II: false I (width), true II (shift)
    bool keyerIsPwr = false;    // KEYER/MIC: true while in PWR (hold) function
    bool ofsIsVfoB = false;     // OFS/B: false RIT/XIT offset, true VFO B
};

#endif // KX3WINDOW_H
