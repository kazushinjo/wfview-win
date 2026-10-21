#ifndef FT817WINDOW_H
#define FT817WINDOW_H

// Dedicated control screen for the Yaesu FT-817/FT-817ND/FT-818
// (connected through a USB-serial CAT adapter).
//
// The FT-817 uses the legacy fixed 5-byte binary CAT protocol, which the
// normal wfview rig/commander pipeline (rig definition files, text CAT for
// newer Yaesu radios) cannot drive.  When a connection profile selects the
// FT-817, wfmain hands the serial port to this self-contained window instead
// of starting the regular rig connection.  The window is laid out after the
// radio's own front panel.

#include <QWidget>
#include <QByteArray>
#include <QString>
#include <QList>
#include <QVector>
#include <QRectF>
#include <QPair>

class QSerialPort;
class QTimer;
class QPushButton;
class QLineEdit;
class QDialog;
class QComboBox;
class QSpinBox;

// Model ID stored in prefs.radioCIVAddr / the connection profile to select
// the FT-817/818.  The real radio has no ID command, so this is only a marker.
constexpr quint16 FT817_MODEL_ID = 817;
constexpr quint16 FT818_MODEL_ID = 818;

// Round knob: the large main tuning dial and the small SEL knob.
class ft817Knob : public QWidget
{
    Q_OBJECT
public:
    explicit ft817Knob(bool large, QWidget *parent = nullptr);
    QSize sizeHint() const override;

signals:
    void stepsTurned(int steps);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    void rotate(double degrees);

    bool large;
    QPointF pressPos;
    double lastAngle = 0.0;
    double accumulated = 0.0;
    double indicatorAngle = 0.0;
    bool dragging = false;
};

// State shown on the LCD.
struct ft817LcdState
{
    bool connected = false;
    bool catOk = false;
    bool powerOff = false;
    qint64 frequencyHz = 0;
    QString mode;
    QString digMode;
    bool vfoB = false;
    bool transmitting = false;
    bool busy = false;
    bool codeMatched = false;
    bool discCentered = false;
    bool hiSwr = false;
    int sMeter = 0;
    int po = 0, alc = 0, swr = 0, mod = 0;
    bool split = false;
    bool clar = false;
    int clarOffsetHz = 0;
    bool locked = false;
    int rptShift = 0;       // 0 simplex, -1 minus, +1 plus, 2 non-standard
    int toneMode = 0;       // 0 off, 1 TONE (enc), 2 TSQ, 3 DCS

    // Read-only EEPROM status (empty / false until first read)
    bool eepromKnown = false;
    bool memMode = false;
    QString memChannel;
    bool home = false;
    QString agc;
    bool nb = false;
    bool fastTune = false;
    QString meterMode;
    bool kyr = false;
    bool bk = false;
    bool vlt = false;
    bool vox = false;
    QString txPower;
    bool pri = false;
    bool dw = false;
    int scan = 0;           // 0 off, 1 up, -1 down
    bool arts = false;
    bool charger = false;
    QString antenna;        // "FRONT" / "REAR"
    bool recordKnown = false;
    bool att = false;
    bool ipo = false;
    int step = 100;
    int page = 0;
    QStringList softKeys;
};

class ft817Lcd : public QWidget
{
    Q_OBJECT
public:
    explicit ft817Lcd(QWidget *parent = nullptr);
    void setState(const ft817LcdState& s);
    QSize sizeHint() const override { return QSize(560, 230); }

signals:
    void digitClicked(int step);
    void softKeyClicked(int key);   // 0-2: A / B / C labels, 3: F-n page label

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    void drawSegmentDigit(QPainter& p, const QRectF& r, QChar c, bool lit);
    void drawIndicator(QPainter& p, const QPointF& pos, const QString& text, bool on);

    ft817LcdState st;
    QVector<QPair<QRectF, int>> digitRects;
    QVector<QRectF> softKeyRects;
};

class ft817Window : public QWidget
{
    Q_OBJECT
public:
    ft817Window(const QString& portName, quint32 baudRate, bool ft818, QWidget *parent = nullptr);
    ~ft817Window() override;

    bool openPort();

signals:
    void closed();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void readSerial();
    void poll();
    void responseTimeout();
    void safetyTimeout();

private:
    // Kind of a queued CAT exchange; also used to coalesce repeated setters.
    enum class Cat {
        set, setFrequency, setMode, setClarOffset, ptt, wake,
        readFreqMode, readRx, readTx, readTxMeter, readEeprom
    };
    struct catItem {
        Cat kind;
        QByteArray bytes;
        int expect;         // response length; set commands answer with a 1-byte ACK
        int timeoutMs;
        int postDelayMs;
        quint16 eepromAddr;
        int recordOffset;   // >= 0: field offset inside the current VFO band record
    };

    struct modeInfo { quint8 code; const char *title; };
    static const QList<modeInfo>& setModes();
    static QString modeTitle(quint8 code);

    void buildUi();
    void updateUi();
    void applySoftKey(int key);
    void showSetDialog();

    void enqueue(Cat kind, const QByteArray& bytes, bool front = false, quint16 eepromAddr = 0,
                 int recordOffset = -1);
    QList<QPair<quint16, int>> eepromReads() const;
    void enqueueEeprom(quint16 addr, int recordOffset = -1);
    void enqueueRecordReads();
    void refreshAll();
    void processQueue();
    void handleResponse(const catItem& item, const QByteArray& data);
    void appendLog(const QString& text);
    bool isOpen() const;

    void setFrequency(qint64 hz);
    void setMode(quint8 code);
    void selectAdjacentMode(int direction);
    void stepBand(int direction);
    void setClarOffset(int hz);
    void toggleClar();
    void toggleSplit();
    void toggleLock();
    void cycleRptShift();
    void cycleToneMode();
    void setCtcssTone(int tenthsHz);
    void setDcsCode(int code);
    void setRptOffset(qint64 hz);
    void powerOn();
    void powerOff();
    void startTransmit();
    void stopTransmit();
    void sendPttOffNow();

    // Software range scan: the FT-817 CAT has no scan command, so step the
    // frequency and stop on an open squelch (RX status bit 7).
    enum class ScanResume { afterTime, afterSignalGone, stay };
    void showScanWindow();
    void startScan();
    void stopScan(const QString& reason);
    void scanGoTo(qint64 hz);
    void scanAdvance();
    void scanReadRx();
    void scanHandleRx(bool signal);

    static QByteArray cmd(quint8 opcode, quint8 p1 = 0, quint8 p2 = 0, quint8 p3 = 0, quint8 p4 = 0);
    static QByteArray bcd(qint64 value, int digits);
    static QString formatFrequency(qint64 hz);
    static QString stepTitle(int step);

    QString portName;
    quint32 baudRate;
    bool ft818;
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
    int consecutiveTimeouts = 0;
    qint64 lastControlMs = 0;
    int eepromIndex = 0;
    int bandA = -1, bandB = -1;     // band numbers from EEPROM 0x59
    bool refreshRecordPending = false;

    // Radio state
    ft817LcdState st;
    quint8 modeCode = 0x01;
    int ctcssTenths = 885;
    int dcsCode = 23;
    qint64 rptOffsetHz = 600000;
    bool awaitingTransmit = false;
    bool awaitingReceive = false;

    // Range scan
    QTimer *scanTimer = nullptr;
    bool scanning = false;
    bool scanSetting = false;       // setFrequency() called by the scan itself
    bool scanAwaitingRx = false;
    bool scanHolding = false;
    qint64 scanStartHz = 0, scanEndHz = 0, scanStepHz = 0, scanFreqHz = 0;
    qint64 scanHoldSinceMs = 0, scanLastSignalMs = 0;
    qint64 scanStoppedAtHz = -1;

    ft817Lcd *lcd = nullptr;
    QList<QPushButton*> softButtons;
    QPushButton *pttBtn = nullptr;
    QPushButton *refreshBtn = nullptr;
    QPushButton *disconnectBtn = nullptr;
    QLineEdit *freqInput = nullptr;
    QWidget *scanWindow = nullptr;
    QPushButton *scanWindowBtn = nullptr;
    QLineEdit *scanStartEdit = nullptr;
    QLineEdit *scanEndEdit = nullptr;
    QLineEdit *scanStepEdit = nullptr;
    QComboBox *scanResumeCombo = nullptr;
    QSpinBox *scanWaitSpin = nullptr;
    QPushButton *scanBtn = nullptr;
};

#endif // FT817WINDOW_H
