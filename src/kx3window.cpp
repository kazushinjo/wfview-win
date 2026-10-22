#include "kx3window.h"
#include "ft817window.h"    // ft817Knob: the same round knob widget
#include "buttonstyle.h"
#include "logcategories.h"

#include <QSerialPort>
#include <QTimer>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QBoxLayout>
#include <QGridLayout>
#include <QMessageBox>
#include <QPainter>
#include <QMouseEvent>
#include <QCloseEvent>
#include <QDateTime>
#include <QtMath>
#include <QHash>
#include <QRegularExpression>
#include <cmath>

// Elecraft K3/KX3 text CAT.  Commands and scales checked against the hamlib
// elecraft / k3 drivers (kx3_caps, kx3_set_level, kx3_get_bar_graph_level).
namespace {
// BN band numbers.
const char *bandNames[] = {"160", "80", "60", "40", "30", "20", "17", "15", "12", "10", "6"};

// SM raw 0-15 -> dB relative to S9 (hamlib K3_SM_CAL).
const int smCal[] = {-54, -42, -36, -24, -12, -6, 0, 10, 15, 20, 30, 35, 40, 50, 55, 60};

const char *dataSubNames[] = {"DATA A", "AFSK A", "FSK D", "PSK D"};

const char *panelStyle =
    "QPushButton { color: #f2f2f2; font-weight: bold; font-size: 11px;"
    " background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #4a4a4a, stop:1 #1e1e1e);"
    " border: 1px solid #0d0d0d; border-radius: 5px; padding: 3px 6px; min-height: 26px; }"
    "QPushButton:pressed { background: #111; }"
    "QPushButton:disabled { color: #777; }"
    "QLabel { color: #dcdcdc; }";

const QColor ink(20, 24, 22);

QString sMeterText(int raw)
{
    const int db = smCal[qBound(0, raw, 15)];
    if (db > 0)
        return QString("S9+%1").arg(db);
    return QString("S%1").arg(qMax(0, 9 + db / 6));
}
}

// ---------------------------------------------------------------------------
// kx3Lcd
// ---------------------------------------------------------------------------

kx3Lcd::kx3Lcd(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(480, 190);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);     // hand cursor over clickable digits
}

void kx3Lcd::setState(const kx3LcdState& s)
{
    st = s;
    update();
}

void kx3Lcd::drawSegmentDigit(QPainter& p, const QRectF& r, QChar c, bool lit)
{
    //  aaa
    // f   b
    //  ggg
    // e   c
    //  ddd
    static const QHash<QChar, QString> map = {
        {'0', "abcdef"}, {'1', "bc"}, {'2', "abdeg"}, {'3', "abcdg"}, {'4', "bcfg"},
        {'5', "acdfg"}, {'6', "acdefg"}, {'7', "abc"}, {'8', "abcdefg"}, {'9', "abcdfg"},
        {'-', "g"}, {' ', ""}
    };
    const QString on = map.value(c);
    const double w = r.width(), h = r.height(), t = w * 0.16, g = t * 0.12;

    auto hSeg = [&](double y) {
        QPolygonF poly;
        poly << QPointF(t / 2 + g, y) << QPointF(t + g, y - t / 2) << QPointF(w - t - g, y - t / 2)
             << QPointF(w - t / 2 - g, y) << QPointF(w - t - g, y + t / 2) << QPointF(t + g, y + t / 2);
        return poly;
    };
    auto vSeg = [&](double x, double y0, double y1) {
        QPolygonF poly;
        poly << QPointF(x, y0 + g) << QPointF(x + t / 2, y0 + t / 2 + g) << QPointF(x + t / 2, y1 - t / 2 - g)
             << QPointF(x, y1 - g) << QPointF(x - t / 2, y1 - t / 2 - g) << QPointF(x - t / 2, y0 + t / 2 + g);
        return poly;
    };

    const QHash<QChar, QPolygonF> segs = {
        {'a', hSeg(t / 2)}, {'g', hSeg(h / 2)}, {'d', hSeg(h - t / 2)},
        {'f', vSeg(t / 2, t / 2, h / 2)}, {'b', vSeg(w - t / 2, t / 2, h / 2)},
        {'e', vSeg(t / 2, h / 2, h - t / 2)}, {'c', vSeg(w - t / 2, h / 2, h - t / 2)},
    };

    p.save();
    p.translate(r.topLeft());
    p.translate(h * 0.06, 0);
    p.shear(-0.06, 0);
    p.setPen(Qt::NoPen);
    for (auto it = segs.cbegin(); it != segs.cend(); ++it)
    {
        const bool segOn = lit && on.contains(it.key());
        QColor col = ink;
        col.setAlpha(segOn ? 255 : 18);
        p.setBrush(col);
        p.drawPolygon(it.value());
    }
    p.restore();
}

void kx3Lcd::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const double W = width(), H = height();

    // Bezel and the KX3's grey-green transflective glass
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(10, 10, 10));
    p.drawRoundedRect(rect(), 6, 6);
    const QRectF glass = QRectF(rect()).adjusted(7, 7, -7, -7);
    QLinearGradient lcdBg(glass.topLeft(), glass.bottomLeft());
    const bool backlight = st.connected;
    lcdBg.setColorAt(0.0, backlight ? QColor(214, 226, 196) : QColor(150, 156, 142));
    lcdBg.setColorAt(1.0, backlight ? QColor(182, 198, 160) : QColor(128, 134, 120));
    p.setBrush(lcdBg);
    p.drawRoundedRect(glass, 3, 3);

    const bool lit = st.connected && st.catOk;
    const double gx = glass.left() + 10, gw = glass.width() - 20;
    QColor dim = ink;
    dim.setAlpha(26);
    QFont f = font();
    f.setBold(true);

    // Bar graph across the top: S meter in RX, power in TX (as on the radio)
    const double barTop = glass.top() + H * 0.06;
    const double barH = H * 0.06;
    f.setPixelSize(int(H * 0.055));
    p.setFont(f);
    p.setPen(lit ? ink : dim);
    p.drawText(QRectF(gx, barTop - 2, gw * 0.08, barH + 4), Qt::AlignLeft | Qt::AlignVCenter,
               st.transmitting ? "PWR" : "S");
    const double barLeft = gx + gw * 0.08;
    const int segments = st.transmitting ? 10 : 15;
    const double segW = gw * 0.60 / segments;
    const int level = st.transmitting ? st.txBar : st.sMeter;
    for (int i = 0; i < segments; ++i)
    {
        p.setPen(Qt::NoPen);
        p.setBrush(lit && i < level ? ink : dim);
        p.drawRect(QRectF(barLeft + i * segW, barTop, segW * 0.78, barH));
    }
    f.setPixelSize(int(H * 0.042));
    p.setFont(f);
    p.setPen(lit ? ink : dim);
    if (!st.transmitting)
    {
        // SM raw value -> S unit (hamlib calibration table)
        const QList<QPair<int, QString>> scale = {
            {2, "3"}, {3, "5"}, {4, "7"}, {6, "9"}, {9, "+20"}, {12, "+40"}, {15, "+60"}};
        for (const auto& s : scale)
            p.drawText(QPointF(barLeft + (s.first - 1) * segW, barTop + barH + H * 0.05), s.second);
    }
    f.setPixelSize(int(H * 0.06));
    p.setFont(f);
    p.drawText(QRectF(barLeft + gw * 0.62, barTop - 4, gw * 0.30, barH + 8), Qt::AlignLeft | Qt::AlignVCenter,
               !lit ? QString() : st.transmitting ? QString("TX") : sMeterText(st.sMeter));

    // Indicator row
    f.setPixelSize(int(qMax(10.0, H * 0.055)));
    p.setFont(f);
    struct ind { QString text; bool on; };
    const QList<ind> inds = {
        {"TX", lit && st.transmitting},
        {"SPLT", lit && st.split},
        {"RIT", lit && st.rit},
        {"XIT", lit && st.xit},
        {"PRE", lit && st.preamp},
        {"ATT", lit && st.att},
        {"NB", lit && st.nb},
        {"VOX", lit && st.vox},
        {"LOCK", lit && st.locked},
        {st.agc.isEmpty() ? QString("AGC") : QString("AGC-%1").arg(st.agc.left(1)), lit && !st.agc.isEmpty()},
    };
    const double indY = glass.top() + H * 0.25;
    double x = gx;
    for (const ind& i : inds)
    {
        p.setPen(i.on ? ink : dim);
        p.drawText(QPointF(x, indY), i.text);
        x += p.fontMetrics().horizontalAdvance(i.text) + W * 0.024;
    }

    // VFO A: 3 MHz digits . 3 kHz digits . 2 digits (10 Hz resolution)
    const double rowTop = glass.top() + H * 0.30;
    const double rowH = H * 0.36;
    const qint64 hz = st.vfoAHz;
    QString mhz = QString::number(hz / 1000000).rightJustified(3, ' ');
    if (mhz.size() > 3) mhz = mhz.right(3);
    const QString digits = mhz + QString("%1").arg((hz / 1000) % 1000, 3, 10, QChar('0'))
                           + QString("%1").arg((hz / 10) % 100, 2, 10, QChar('0'));
    const double freqLeft = gx;
    const double freqWidth = gw * 0.74;
    const double digitW = freqWidth / (8 + 2 * 0.35);
    const double dw = digitW * 0.80;
    const double dh = rowH * 0.92;
    digitRects.clear();
    double dx = freqLeft;
    for (int i = 0; i < 8; ++i)
    {
        const bool small = i >= 6;
        const double scale = small ? 0.72 : 1.0;
        const QRectF dr(dx, rowTop + dh * (1.0 - scale), dw * scale, dh * scale);
        drawSegmentDigit(p, dr, lit ? digits.at(i) : QChar('8'), lit && digits.at(i) != ' ');
        const int stepForDigit = int(qPow(10, 8 - i));
        digitRects.append(qMakePair(QRectF(dx, rowTop, digitW, dh + 8), stepForDigit));
        if (lit && stepForDigit == st.step)
        {
            p.setPen(Qt::NoPen);
            p.setBrush(ink);
            p.drawRect(QRectF(dx + dw * scale * 0.1, rowTop + dh + 4, dw * scale * 0.8, 3));
        }
        dx += digitW;
        if (i == 2 || i == 5)
        {
            p.setPen(Qt::NoPen);
            p.setBrush(lit ? ink : dim);
            const double dot = dw * 0.16;
            p.drawRect(QRectF(dx + digitW * 0.05, rowTop + dh - dot, dot, dot));
            dx += digitW * 0.35;
        }
    }

    // Mode and data sub mode to the right of VFO A
    const double sideX = gx + gw * 0.78;
    f.setPixelSize(int(H * 0.10));
    p.setFont(f);
    p.setPen(lit ? ink : dim);
    QString modeText;
    switch (st.mode)
    {
    case 1: modeText = "LSB"; break;
    case 2: modeText = "USB"; break;
    case 3: modeText = "CW"; break;
    case 4: modeText = "FM"; break;
    case 5: modeText = "AM"; break;
    case 6: modeText = "DATA"; break;
    case 7: modeText = "CW-R"; break;
    case 9: modeText = "DATA-R"; break;
    default: modeText = "---"; break;
    }
    p.drawText(QRectF(sideX, rowTop, gw * 0.22, rowH * 0.45), Qt::AlignLeft | Qt::AlignVCenter,
               lit ? modeText : QString("USB"));
    f.setPixelSize(int(H * 0.055));
    p.setFont(f);
    if (lit && (st.mode == 6 || st.mode == 9) && st.dataSub >= 0 && st.dataSub <= 3)
        p.drawText(QRectF(sideX, rowTop + rowH * 0.45, gw * 0.22, rowH * 0.25), Qt::AlignLeft | Qt::AlignVCenter,
                   QString::fromLatin1(dataSubNames[st.dataSub]));
    if (lit && (st.rit || st.xit))
        p.drawText(QRectF(sideX, rowTop + rowH * 0.70, gw * 0.22, rowH * 0.25), Qt::AlignLeft | Qt::AlignVCenter,
                   QString("%1 %2%3k").arg(st.xit && !st.rit ? "XIT" : "RIT",
                                           st.offsetHz < 0 ? "−" : "+")
                       .arg(std::abs(st.offsetHz) / 1000.0, 0, 'f', 2));

    // VFO B and settings lines
    const double infoTop = rowTop + rowH + H * 0.03;
    f.setPixelSize(int(H * 0.075));
    p.setFont(f);
    p.setPen(lit ? ink : dim);
    const qint64 b = st.vfoBHz;
    const QString bFreq = QString("%1.%2.%3").arg(b / 1000000)
                              .arg((b / 1000) % 1000, 3, 10, QChar('0'))
                              .arg((b / 10) % 100, 2, 10, QChar('0'));
    QString bandText;
    if (st.band >= 0 && st.band <= 10)
        bandText = QString("%1m").arg(QString::fromLatin1(bandNames[st.band]));
    p.drawText(QRectF(gx, infoTop, gw * 0.55, H * 0.10), Qt::AlignLeft | Qt::AlignVCenter,
               lit ? QString("VFO B %1").arg(bFreq) : QString());
    p.drawText(QRectF(gx + gw * 0.55, infoTop, gw * 0.45, H * 0.10), Qt::AlignRight | Qt::AlignVCenter,
               lit ? bandText : QString());

    f.setPixelSize(int(H * 0.055));
    p.setFont(f);
    QStringList info;
    if (st.afGain >= 0) info << QString("AF %1").arg(st.afGain);
    if (st.rfGain >= 0) info << QString("RF %1").arg(st.rfGain - 250);
    if (st.powerTenths >= 0) info << QString("PWR %1W").arg(st.powerTenths / 10.0, 0, 'f', 1);
    if (st.bandwidthHz >= 0) info << QString("BW %1k").arg(st.bandwidthHz / 1000.0, 0, 'f', 2);
    info << QString("STEP %1").arg(st.step >= 1000 ? QString("%1 kHz").arg(st.step / 1000.0)
                                                   : QString("%1 Hz").arg(st.step));
    QStringList flags;
    if (st.nr) flags << "NR";
    if (st.apf) flags << "APF";
    p.drawText(QRectF(gx, infoTop + H * 0.10, gw, H * 0.09), Qt::AlignLeft | Qt::AlignVCenter,
               lit ? info.join("   ") : QString());
    p.drawText(QRectF(gx, infoTop + H * 0.18, gw, H * 0.08), Qt::AlignLeft | Qt::AlignVCenter,
               lit ? (flags + QStringList{QString("KNOB %1").arg(st.knobs)}).join("   ") : QString());

    if (!st.connected || !st.catOk)
    {
        f.setPixelSize(int(H * 0.075));
        p.setFont(f);
        p.setPen(QColor(120, 20, 10));
        p.drawText(QRectF(glass.left(), rowTop + rowH * 0.25, glass.width(), rowH * 0.5), Qt::AlignCenter,
                   !st.connected ? tr("ポート未接続") : tr("CAT 応答待ち（電源・ボーレート・ケーブルを確認）"));
    }
}

void kx3Lcd::mousePressEvent(QMouseEvent *event)
{
    for (const auto& d : digitRects)
    {
        if (d.first.contains(event->position()))
        {
            emit digitClicked(d.second);
            return;
        }
    }
    QWidget::mousePressEvent(event);
}

void kx3Lcd::mouseMoveEvent(QMouseEvent *event)
{
    bool clickable = false;
    for (const auto& d : digitRects)
        clickable = clickable || d.first.contains(event->position());
    setCursor(clickable ? Qt::PointingHandCursor : Qt::ArrowCursor);
    QWidget::mouseMoveEvent(event);
}

// ---------------------------------------------------------------------------
// kx3Window
// ---------------------------------------------------------------------------

kx3Window::kx3Window(const QString& portName, quint32 baudRate, QWidget *parent)
    : QWidget(parent, Qt::Window), portName(portName), baudRate(baudRate)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(QString("wfview - KX3 (%1 @ %2 bps)").arg(portName).arg(baudRate));

    serial = new QSerialPort(this);
    connect(serial, &QSerialPort::readyRead, this, &kx3Window::readSerial);
    connect(serial, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError err) {
        if (err == QSerialPort::NoError || err == QSerialPort::TimeoutError)
            return;
        appendLog(QString("Serial error: %1").arg(serial->errorString()));
        if (err == QSerialPort::ResourceError)
        {
            // USB-serial cable unplugged
            pollTimer->stop();
            responseTimer->stop();
            queue.clear();
            busy = false;
            serial->close();
            st.catOk = false;
            st.transmitting = false;
            updateUi();
        }
    });

    pollTimer = new QTimer(this);
    pollTimer->setInterval(100);
    connect(pollTimer, &QTimer::timeout, this, &kx3Window::poll);

    responseTimer = new QTimer(this);
    responseTimer->setSingleShot(true);
    connect(responseTimer, &QTimer::timeout, this, &kx3Window::responseTimeout);

    gapTimer = new QTimer(this);
    gapTimer->setSingleShot(true);
    connect(gapTimer, &QTimer::timeout, this, [this]() {
        busy = false;
        processQueue();
    });

    // Safety: never transmit continuously for more than 3 minutes.
    safetyTimer = new QTimer(this);
    safetyTimer->setSingleShot(true);
    safetyTimer->setInterval(180000);
    connect(safetyTimer, &QTimer::timeout, this, &kx3Window::safetyTimeout);

    st.vfoAHz = 7100000;
    st.vfoBHz = 7100000;
    buildUi();
    updateUi();
}

kx3Window::~kx3Window()
{
    if (st.transmitting)
        sendNow("RX");
}

bool kx3Window::isOpen() const
{
    return serial != nullptr && serial->isOpen();
}

bool kx3Window::openPort()
{
    serial->setPortName(portName);
    serial->setBaudRate(baudRate);
    // Elecraft CAT: 8 data bits, no parity, 1 stop bit.
    serial->setDataBits(QSerialPort::Data8);
    serial->setParity(QSerialPort::NoParity);
    serial->setStopBits(QSerialPort::OneStop);
    serial->setFlowControl(QSerialPort::NoFlowControl);
    if (!serial->open(QIODevice::ReadWrite))
    {
        appendLog(QString("Could not open %1: %2").arg(portName, serial->errorString()));
        updateUi();
        return false;
    }
    // RTS / DTR can key the radio (PTT / KEY on the KX3 RS232 menu); keep them off.
    serial->setDataTerminalReady(false);
    serial->setRequestToSend(false);

    appendLog(QString("Connected: %1 @ %2 bps (8N1)").arg(portName).arg(baudRate));
    qInfo(logSystem()) << "KX3: opened" << portName << "at" << baudRate;
    st.connected = true;

    // No unsolicited auto-info; identify the radio; K22 for PC in 0.1 W
    // (the previous K2 level is restored when the window closes).
    set("AI0");
    read("ID");
    read("OM");
    read("K2");
    refreshAll();
    pollTimer->start();
    updateUi();
    return true;
}

void kx3Window::closeEvent(QCloseEvent *event)
{
    pollTimer->stop();
    responseTimer->stop();
    gapTimer->stop();
    safetyTimer->stop();
    queue.clear();
    if (st.transmitting)
        sendNow("RX");
    if (isOpen() && savedK2Level >= 0 && savedK2Level != 2)
        sendNow(QByteArray("K2") + QByteArray::number(savedK2Level));
    if (serial->isOpen())
        serial->close();
    emit closed();
    event->accept();
}

// ---------------------------------------------------------------------------
// kx3Key
// ---------------------------------------------------------------------------

kx3Key::kx3Key(const QString& tap, QWidget *parent) : QPushButton(tap, parent)
{
    setFocusPolicy(Qt::NoFocus);
    holdTimer = new QTimer(this);
    holdTimer->setSingleShot(true);
    holdTimer->setInterval(500);
    connect(holdTimer, &QTimer::timeout, this, [this]() {
        holdFired = true;
        emit held();
    });
    connect(this, &QPushButton::pressed, this, [this]() {
        holdFired = false;
        holdTimer->start();
    });
    connect(this, &QPushButton::released, this, [this]() {
        holdTimer->stop();
        if (!holdFired && isEnabled())
            emit tapped();
    });
}

// ---------------------------------------------------------------------------
// kx3Window: front panel
// ---------------------------------------------------------------------------

// One front panel key with its hold function printed above it.  Unless the
// caller connects its own handlers (code < 0), tap / hold are sent to the
// radio as SWT / SWH switch emulation (Programmer's Reference, Table 8).
QWidget *kx3Window::makePanelKey(const QString& tap, const QString& hold, int code, const QString& tip)
{
    auto *box = new QWidget(this);
    box->setObjectName("kx3KeyBox");
    auto *v = new QVBoxLayout(box);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(1);
    auto *holdLabel = new QLabel(hold, box);
    holdLabel->setAlignment(Qt::AlignCenter);
    holdLabel->setStyleSheet("color: #e0b040; font-size: 9px; font-weight: bold;");
    v->addWidget(holdLabel);
    auto *key = new kx3Key(tap, box);
    key->setObjectName(tap);
    key->setMinimumWidth(62);
    key->setToolTip(tip + (hold.isEmpty() ? QString() : tr("\n長押し: %1").arg(hold)));
    v->addWidget(key);
    if (code >= 0)
    {
        connect(key, &kx3Key::tapped, this, [this, code, tap]() { pressSwitch(code, false, tap); });
        if (!hold.isEmpty())
            connect(key, &kx3Key::held, this, [this, code, hold]() { pressSwitch(code, true, hold); });
    }
    return box;
}

void kx3Window::buildUi()
{
    setObjectName("kx3Body");
    setStyleSheet(QString("#kx3Body { background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
                          " stop:0 #2f2f2f, stop:1 #161616); }") + panelStyle +
                  // The application style sheet paints a dark box behind each
                  // label and each key's box.
                  "#kx3Body QLabel, #kx3Body #kx3KeyBox, #kx3Body #kx3Controls { background: transparent; }");

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 10, 16, 12);
    root->setSpacing(8);

    // Brand strip
    auto *brand = new QHBoxLayout();
    modelLabel = new QLabel("KX3");
    modelLabel->setStyleSheet("color: #f5f5f5; font-size: 22px; font-weight: 900;");
    brand->addStretch();
    brand->addWidget(modelLabel, 0, Qt::AlignBottom);
    root->addLayout(brand);

    auto *body = new QHBoxLayout();
    body->setSpacing(14);
    root->addLayout(body, 1);

    auto caption = [](const QString& text) {
        auto *l = new QLabel(text);
        l->setAlignment(Qt::AlignCenter);
        l->setStyleSheet("color: #cfcfcf; font-size: 10px; font-weight: bold;");
        return l;
    };
    auto keyOf = [](QWidget *box) { return box->findChild<kx3Key*>(); };

    // Left of the display: BAND / FREQ ENT / MSG / ATU / XMIT keys
    auto *left = new QGridLayout();
    left->setHorizontalSpacing(6);
    left->setVerticalSpacing(6);
    left->addWidget(makePanelKey("BAND+", "RCL", 8, tr("上のバンドへ")), 0, 0);
    left->addWidget(makePanelKey("BAND−", "STORE", 41, tr("下のバンドへ")), 0, 1);
    left->addWidget(makePanelKey("FREQ ENT", "SCAN", 10, tr("キーパッドで周波数入力")), 1, 0);
    left->addWidget(makePanelKey("MSG", "REC", 11, tr("メッセージ再生")), 1, 1);
    left->addWidget(makePanelKey("ATU TUNE", "ANT", 44, tr("内蔵 ATU でチューニング（短時間送信します）")), 2, 0);
    // XMIT tap goes through the PTT handling (3-minute safety stop).
    QWidget *xmitBox = makePanelKey("XMIT", "TUNE", -1, tr("送信 / 受信の切替"));
    connect(keyOf(xmitBox), &kx3Key::tapped, this, [this]() {
        if (st.transmitting) stopTransmit(); else startTransmit();
    });
    connect(keyOf(xmitBox), &kx3Key::held, this, [this]() { pressSwitch(16, true, "TUNE"); });
    left->addWidget(xmitBox, 2, 1);
    left->setRowStretch(3, 1);
    body->addLayout(left);

    // Centre: LCD, the six keypad keys and the three small knobs
    auto *centre = new QVBoxLayout();
    centre->setSpacing(6);
    lcd = new kx3Lcd(this);
    connect(lcd, &kx3Lcd::digitClicked, this, [this](int step) {
        st.step = step;
        updateUi();
    });
    centre->addWidget(lcd, 1);

    auto *keypad = new QHBoxLayout();
    keypad->setSpacing(6);
    keypad->addWidget(makePanelKey("PRE  1", "NR", 19, tr("プリアンプ ON / OFF")));
    keypad->addWidget(makePanelKey("ATTN  2", "NB", 27, tr("アッテネーター ON / OFF")));
    keypad->addWidget(makePanelKey("APF  3", "NTCH", 20, tr("CW オーディオピークフィルタ")));
    keypad->addWidget(makePanelKey("SPOT  4", "CWT", 28, tr("CW スポット")));
    keypad->addWidget(makePanelKey("CMP  5", "PITCH", 21, tr("スピーチコンプレッサ")));
    keypad->addWidget(makePanelKey("DLY  6", "VOX", 29, tr("送受切替ディレイ（QSK / VOX）")));
    centre->addLayout(keypad);

    // Small knobs: rotate = current function, tap = switch function, hold = second function
    auto *knobRow = new QHBoxLayout();
    knobRow->setSpacing(18);
    struct knobDef { const char *name; const char *hold; int code; const char *tip; };
    const knobDef knobDefs[] = {
        {"AF / RF-SQL  7", "MON", 32, "回す: AF ゲイン / タップ: RF ゲイン・スケルチに切替"},
        {"PBT I/II  8", "NORM", 33, "回す: 帯域幅（I） / タップ: シフト（II）に切替 / 長押し: 標準に戻す"},
        {"KEYER / MIC  9", "PWR", 34, "回す: CW はキーヤー速度、それ以外はマイクゲイン / 長押し: 送信出力"},
    };
    for (int i = 0; i < 3; ++i)
    {
        const knobDef& d = knobDefs[i];
        auto *box = new QVBoxLayout();
        box->setSpacing(1);
        auto *k = new ft817Knob(false, this);
        k->setToolTip(tr(d.tip));
        connect(k, &ft817Knob::stepsTurned, this, [this, i](int steps) { turnKnob(i, steps); });
        box->addWidget(k, 0, Qt::AlignCenter);
        auto *value = new QLabel("--");
        value->setAlignment(Qt::AlignCenter);
        value->setStyleSheet("color: #9fd39f; font-size: 11px; font-weight: bold;");
        knobLabels.append(value);
        box->addWidget(value);
        QWidget *push = makePanelKey(d.name, d.hold, -1, tr(d.tip));
        box->addWidget(push);
        knobRow->addLayout(box);
        const int code = d.code;
        connect(keyOf(push), &kx3Key::tapped, this, [this, i]() {
            if (i == knobAf) afIsRf = !afIsRf;
            else if (i == knobPbt) pbtShift = !pbtShift;
            else keyerIsPwr = false;
            if (i == knobPbt && pbtShift) read("IS");
            updateUi();
        });
        connect(keyOf(push), &kx3Key::held, this, [this, i, code]() {
            if (i == knobKeyer)
            {
                keyerIsPwr = true;      // PWR: power is set from this window
                read("PC");
                updateUi();
            }
            else if (i == knobPbt)
            {
                set("IS 9999");         // centre the passband
                read("IS");
                read("BW");
            }
            else
                pressSwitch(code, true, "MON");
        });
    }
    centre->addLayout(knobRow);
    body->addLayout(centre, 1);

    // Right: MODE / DATA / RIT / RATE and A/B / A>B / XIT / DISP, OFS/B knob, VFO A dial
    auto *right = new QVBoxLayout();
    right->setSpacing(6);
    auto *keys = new QGridLayout();
    keys->setHorizontalSpacing(6);
    keys->setVerticalSpacing(6);
    keys->addWidget(makePanelKey("MODE", "ALT", 14, tr("モード切替")), 0, 0);
    keys->addWidget(makePanelKey("DATA", "TEXT", 17, tr("DATA モード")), 1, 0);
    keys->addWidget(makePanelKey("RIT", "PF1", 18, tr("RIT ON / OFF")), 2, 0);
    keys->addWidget(makePanelKey("RATE", "KHZ", 12, tr("VFO の変化量")), 3, 0);
    keys->addWidget(makePanelKey("A / B", "REV", 24, tr("VFO A と B を入れ替え")), 0, 1);
    keys->addWidget(makePanelKey("A → B", "SPLIT", 25, tr("VFO A を B へコピー")), 1, 1);
    keys->addWidget(makePanelKey("XIT", "PF2", 26, tr("XIT ON / OFF")), 2, 1);
    keys->addWidget(makePanelKey("DISP", "MENU", 9, tr("表示切替")), 3, 1);
    right->addLayout(keys);

    auto *dials = new QHBoxLayout();
    auto *ofsBox = new QVBoxLayout();
    ofsBox->setSpacing(1);
    auto *ofs = new ft817Knob(false, this);
    ofs->setToolTip(tr("回す: RIT / XIT オフセット / タップ: VFO B に切替 / 長押し: オフセットを 0 に"));
    connect(ofs, &ft817Knob::stepsTurned, this, [this](int steps) { turnKnob(knobOfs, steps); });
    ofsBox->addWidget(ofs, 0, Qt::AlignCenter);
    auto *ofsValue = new QLabel("--");
    ofsValue->setAlignment(Qt::AlignCenter);
    ofsValue->setStyleSheet("color: #9fd39f; font-size: 11px; font-weight: bold;");
    knobLabels.append(ofsValue);
    ofsBox->addWidget(ofsValue);
    QWidget *ofsPush = makePanelKey("OFS / B", "CLR", -1, tr("オフセットと VFO B の切替"));
    connect(keyOf(ofsPush), &kx3Key::tapped, this, [this]() {
        ofsIsVfoB = !ofsIsVfoB;
        updateUi();
    });
    connect(keyOf(ofsPush), &kx3Key::held, this, [this]() {
        st.offsetHz = 0;
        set("RC");
        read("IF");
        updateUi();
    });
    ofsBox->addWidget(ofsPush);
    ofsBox->addStretch();
    dials->addLayout(ofsBox);
    auto *vfoBox = new QVBoxLayout();
    auto *dial = new ft817Knob(true, this);
    dial->setToolTip(tr("VFO A: ドラッグで回転 / 右半分クリックで＋1ステップ・左半分で−1 / ホイール"));
    connect(dial, &ft817Knob::stepsTurned, this, [this](int steps) {
        setVfoA(st.vfoAHz + qint64(steps) * st.step);
    });
    vfoBox->addWidget(dial, 1);
    vfoBox->addWidget(caption("VFO A"));
    dials->addLayout(vfoBox, 1);
    right->addLayout(dials, 1);
    body->addLayout(right);

    // Bottom strip: PTT, direct frequency entry, CW text, power off and disconnect
    auto *bottom = new QHBoxLayout();
    bottom->setSpacing(8);
    auto makeKey = [this](const QString& text, const QString& tip = QString()) {
        auto *b = new QPushButton(text, this);
        b->setFocusPolicy(Qt::NoFocus);
        if (!tip.isEmpty())
            b->setToolTip(tip);
        return b;
    };
    pttBtn = makeKey(tr("送信 (PTT)"));
    pttBtn->setMinimumWidth(150);
    connect(pttBtn, &QPushButton::clicked, this, [this]() {
        if (st.transmitting)
            stopTransmit();
        else
            startTransmit();
    });
    bottom->addWidget(pttBtn);
    bottom->addSpacing(10);
    const QString editStyle = "QLineEdit { background: #f0f0f0; color: black; border: 1px solid #8a8a8a;"
                              " border-radius: 8px; padding: 2px 6px; }";
    bottom->addWidget(new QLabel(tr("周波数入力")));
    freqInput = new QLineEdit(this);
    freqInput->setPlaceholderText(tr("MHz（例 7.0745）"));
    freqInput->setFixedWidth(160);
    freqInput->setStyleSheet(editStyle);
    connect(freqInput, &QLineEdit::returnPressed, this, [this]() {
        QString text = freqInput->text().trimmed();
        text.replace(',', '.');
        bool ok = false;
        const double mhz = text.toDouble(&ok);
        if (!ok || mhz <= 0)
        {
            QMessageBox::warning(this, tr("KX3"), tr("周波数をMHz単位の数値で入力してください。"));
            return;
        }
        setVfoA(qRound64(mhz * 1000000.0));
        freqInput->clear();
    });
    bottom->addWidget(freqInput);
    bottom->addSpacing(10);
    bottom->addWidget(new QLabel(tr("CW送信")));
    cwInput = new QLineEdit(this);
    cwInput->setPlaceholderText(tr("文字を入力して Enter（CW モード）"));
    cwInput->setMinimumWidth(220);
    cwInput->setStyleSheet(editStyle);
    connect(cwInput, &QLineEdit::returnPressed, this, &kx3Window::sendCw);
    bottom->addWidget(cwInput, 1);
    bottom->addSpacing(10);
    auto *refreshBtn = makeKey(tr("更新"), tr("本体側で変更した設定を読み直して表示を更新"));
    refreshBtn->setMinimumWidth(80);
    connect(refreshBtn, &QPushButton::clicked, this, &kx3Window::refreshAll);
    bottom->addWidget(refreshBtn);
    auto *pwrOffBtn = makeKey(tr("電源 OFF"), tr("CAT で無線機の電源を OFF にして終了（ON は本体で操作）"));
    pwrOffBtn->setMinimumWidth(100);
    connect(pwrOffBtn, &QPushButton::clicked, this, &kx3Window::powerOff);
    bottom->addWidget(pwrOffBtn);
    disconnectBtn = makeKey(tr("切断して戻る"));
    connect(disconnectBtn, &QPushButton::clicked, this, &QWidget::close);
    bottom->addWidget(disconnectBtn);

    // Bottom strip: larger text and controls (nearer ancestor style sheet, so
    // it wins over panelStyle's 11px buttons).
    auto *controls = new QWidget(this);
    controls->setObjectName("kx3Controls");
    controls->setStyleSheet(
        "#kx3Controls QLabel { font-size: 15px; }"
        "#kx3Controls QLineEdit { font-size: 15px; min-height: 32px; }"
        "#kx3Controls QPushButton { font-size: 15px; min-height: 36px; }");
    auto *controlsLayout = new QVBoxLayout(controls);
    controlsLayout->setContentsMargins(0, 4, 0, 0);
    controlsLayout->addLayout(bottom);
    root->addWidget(controls);

    resize(1320, 680);
}

void kx3Window::updateUi()
{
    const bool cw = st.mode == 3 || st.mode == 7;
    QStringList knobText;
    knobText << (afIsRf ? (st.mode == 4 ? "SQL" : "RF") : "AF")
             << (pbtShift ? "SHIFT" : "WIDTH")
             << (keyerIsPwr ? "PWR" : cw ? "WPM" : "MIC")
             << (ofsIsVfoB ? "VFO B" : "OFS");
    st.knobs = knobText.join(" / ");
    lcd->setState(st);

    const bool open = isOpen();
    for (QPushButton *b : findChildren<QPushButton*>())
        if (b != disconnectBtn)
            b->setEnabled(open);
    freqInput->setEnabled(open);
    cwInput->setEnabled(open);

    auto num = [](int v, const QString& text) { return v >= 0 ? text : QString("--"); };
    QString af;
    if (!afIsRf)
        af = num(st.afGain, QString("AF %1").arg(st.afGain));
    else if (st.mode == 4)
        af = num(st.squelch, QString("SQL %1").arg(st.squelch));
    else
        af = num(st.rfGain, QString("RF %1 dB").arg(st.rfGain - 250));
    knobLabels.at(knobAf)->setText(af);
    knobLabels.at(knobPbt)->setText(pbtShift
        ? num(st.ifShiftHz, QString("SHIFT %1 Hz").arg(st.ifShiftHz))
        : num(st.bandwidthHz, QString("BW %1 kHz").arg(st.bandwidthHz / 1000.0, 0, 'f', 2)));
    if (keyerIsPwr)
        knobLabels.at(knobKeyer)->setText(num(st.powerTenths, QString("PWR %1 W").arg(st.powerTenths / 10.0, 0, 'f', 1)));
    else if (cw)
        knobLabels.at(knobKeyer)->setText(num(st.keyerWpm, QString("%1 WPM").arg(st.keyerWpm)));
    else
        knobLabels.at(knobKeyer)->setText(num(st.micGain, QString("MIC %1").arg(st.micGain)));
    knobLabels.at(knobOfs)->setText(ofsIsVfoB ? QString("VFO B") : QString("OFS %1%2 Hz")
                                    .arg(st.offsetHz < 0 ? "−" : "+").arg(std::abs(st.offsetHz)));

    pttBtn->setText(st.transmitting ? tr("受信に戻す") : tr("送信 (PTT)"));
    pttBtn->setStyleSheet(st.transmitting
        ? "QPushButton { background: #b01010; color: white; }"
        : "QPushButton { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #cfe9cf, stop:1 #8fc48f); color: black; }");
}

void kx3Window::pressSwitch(int code, bool hold, const QString& name)
{
    if (isKx2)
    {
        // The KX2 front panel and SWT/SWH codes differ (Table 8A).
        appendLog(tr("KX2 ではこのキーは未対応です: %1").arg(name));
        return;
    }
    appendLog(tr("%1: %2").arg(hold ? tr("長押し") : tr("タップ"), name));
    set(QString("%1%2").arg(hold ? "SWH" : "SWT").arg(code, 2, 10, QChar('0')).toLatin1());
    afterSwitch();
}

// A panel key may change almost anything: read the state back once the
// radio has acted on it (band changes take a while).
void kx3Window::afterSwitch()
{
    QTimer::singleShot(400, this, [this]() {
        for (const char *c : {"IF", "FB", "BN", "PA", "RA", "NB", "NR", "AP", "GT", "LK", "VX",
                              "BW", "PC", "DT", "TQ"})
            read(c);
    });
}

void kx3Window::turnKnob(int knob, int steps)
{
    const bool cw = st.mode == 3 || st.mode == 7;
    switch (knob)
    {
    case knobAf:
        if (!afIsRf)
            setAfGain(qMax(0, st.afGain) + steps);
        else if (st.mode == 4)
            setSquelch(qMax(0, st.squelch) + steps);
        else
            setRfGain((st.rfGain < 0 ? 250 : st.rfGain) + steps);
        break;
    case knobPbt:
        if (pbtShift)
            setIfShift((st.ifShiftHz < 0 ? 1500 : st.ifShiftHz) + steps * 10);
        else
            setBandwidth((st.bandwidthHz < 0 ? 2700 : st.bandwidthHz) + steps * 50);
        break;
    case knobKeyer:
        if (keyerIsPwr)
            setPower(qMax(0, st.powerTenths) + steps);
        else if (cw)
            setKeyerSpeed((st.keyerWpm < 0 ? 20 : st.keyerWpm) + steps);
        else
            setMicGain(qMax(0, st.micGain) + steps);
        break;
    case knobOfs:
        if (ofsIsVfoB)
            setVfoB(st.vfoBHz + qint64(steps) * st.step);
        else
            moveOffset(steps);
        break;
    }
}

// --- CAT queue ------------------------------------------------------------------

void kx3Window::enqueue(const QByteArray& command, const QByteArray& reply, bool front, bool coalesce)
{
    if (!isOpen())
        return;

    // Coalesce knob/dial bursts: replace a still-queued setter of the same kind.
    if (coalesce)
    {
        lastControlMs = QDateTime::currentMSecsSinceEpoch();
        for (catItem& q : queue)
        {
            if (q.coalesce && q.command.left(2) == command.left(2))
            {
                q.command = command;
                return;
            }
        }
    }

    catItem item { command, reply, coalesce };
    if (front)
        queue.prepend(item);
    else
        queue.append(item);
    processQueue();
}

void kx3Window::set(const QByteArray& command, bool coalesce)
{
    lastControlMs = QDateTime::currentMSecsSinceEpoch();
    enqueue(command, QByteArray(), false, coalesce);
}

void kx3Window::read(const QByteArray& command)
{
    // Do not queue the same read twice.
    for (const catItem& q : queue)
        if (q.command == command)
            return;
    enqueue(command, command);
}

void kx3Window::processQueue()
{
    if (busy || queue.isEmpty() || !isOpen())
        return;
    current = queue.takeFirst();
    busy = true;

    const QByteArray bytes = current.command + ';';
    if (current.reply.isEmpty())
        qDebug(logSystem()) << "KX3: TX" << bytes;
    if (serial->write(bytes) != bytes.size())
    {
        appendLog(tr("CATコマンドを送信できませんでした。"));
        busy = false;
        return;
    }

    if (current.reply.isEmpty())
    {
        // Set commands have no reply.  A frequency or band change can keep
        // the radio busy for a while, so give it more time.
        const QByteArray c = current.command.left(2);
        gapTimer->start(c == "FA" || c == "FB" || c == "BN" || c == "MD" ? 60 : 20);
    }
    else
    {
        responseTimer->start(800);
    }
}

void kx3Window::finishCurrent()
{
    responseTimer->stop();
    consecutiveTimeouts = 0;
    if (!st.catOk)
    {
        st.catOk = true;
        appendLog(tr("CAT 応答を確認しました"));
    }
    gapTimer->start(5);
}

void kx3Window::readSerial()
{
    rxBuffer.append(serial->readAll());
    int end;
    while ((end = rxBuffer.indexOf(';')) >= 0)
    {
        // Only line breaks are dropped: IF ends with a meaningful space.
        QByteArray frame = rxBuffer.left(end);
        frame.replace('\r', QByteArray()).replace('\n', QByteArray());
        rxBuffer.remove(0, end + 1);
        if (frame.isEmpty())
            continue;
        const bool waiting = busy && !current.reply.isEmpty() && responseTimer->isActive();
        if (frame == "?")
        {
            // Busy or not understood (e.g. a command the model lacks).
            if (waiting)
            {
                appendLog(QString("Command not accepted: %1").arg(QString::fromLatin1(current.command)));
                finishCurrent();
            }
            continue;
        }
        handleFrame(frame);
        if (waiting && frame.startsWith(current.reply))
            finishCurrent();
    }
    if (rxBuffer.size() > 512)
        rxBuffer.clear();   // garbage without terminators (wrong baud rate)
}

void kx3Window::responseTimeout()
{
    if (!busy)
        return;
    if (++consecutiveTimeouts == 3)
    {
        appendLog(tr("CAT 応答なし（電源 OFF・ボーレート・ケーブルを確認してください）"));
        st.catOk = false;
        updateUi();
    }
    busy = false;
    processQueue();
}

void kx3Window::handleFrame(const QByteArray& frame)
{
    const QByteArray cmd = frame.left(2);
    const QByteArray arg = frame.mid(2);
    bool ok = false;
    const int value = arg.toInt(&ok);

    if (cmd == "IF")
        parseIf(frame);
    else if (cmd == "FA" && ok)
        st.vfoAHz = arg.toLongLong();
    else if (cmd == "FB" && ok)
        st.vfoBHz = arg.toLongLong();
    else if (cmd == "MD" && ok)
        st.mode = value;
    else if (cmd == "DT" && ok)
        st.dataSub = value;
    else if (cmd == "TQ" && ok)
    {
        const bool tx = value == 1;
        if (tx != st.transmitting)
        {
            st.transmitting = tx;
            // Also covers TX started on the radio or by TUNE / ATU keys.
            if (!tx)
                safetyTimer->stop();
            else if (!safetyTimer->isActive())
                safetyTimer->start();
        }
    }
    else if (cmd == "SM" && ok)
        st.sMeter = qBound(0, value, 15);
    else if (cmd == "BG" && arg.size() >= 2)
    {
        // 00-10 or 12-22 (hamlib kx3_get_bar_graph_level)
        const int bg = arg.left(2).toInt();
        st.txBar = bg >= 12 ? bg - 12 : bg;
    }
    else if (cmd == "BN" && ok)
        st.band = value;
    else if (cmd == "AG" && ok)
        st.afGain = value;
    else if (cmd == "RG" && ok)
        st.rfGain = value;
    else if (cmd == "PC" && arg.size() >= 3)
    {
        // K22: PCnnnr, r = 0 -> 0.1 W units, 1 -> watts (KXPA100).  Basic: PCnnn watts.
        const int n = arg.left(3).toInt();
        st.powerTenths = (arg.size() >= 4 && arg.at(3) == '0') ? n : n * 10;
    }
    else if (cmd == "BW" && ok)
        st.bandwidthHz = value * 10;
    else if (cmd == "IS" && arg.size() >= 5)
        st.ifShiftHz = arg.mid(1).toInt();
    else if (cmd == "MG" && ok)
        st.micGain = value;
    else if (cmd == "KS" && ok)
        st.keyerWpm = value;
    else if (cmd == "SQ" && ok)
        st.squelch = value;
    else if (cmd == "AP" && ok)
        st.apf = value != 0;
    else if (cmd == "NR" && ok)
        st.nr = value != 0;
    else if (cmd == "PA" && ok)
        st.preamp = value != 0;
    else if (cmd == "RA" && ok)
        st.att = value != 0;
    else if (cmd == "NB" && ok)
        st.nb = value != 0;
    else if (cmd == "GT" && ok)
        st.agc = value == 0 ? QString("OFF") : value <= 2 ? QString("FST") : QString("SLO");
    else if (cmd == "LK" && ok)
        st.locked = value != 0;
    else if (cmd == "VX" && ok)
        st.vox = value != 0;
    else if (cmd == "ID")
        appendLog(QString("ID: %1").arg(QString::fromLatin1(frame)));
    else if (cmd == "OM")
    {
        // OM response: position 13 '0' marks a KX-series radio, 14 is the model
        // ('1' KX2, '2' KX3) - hamlib elecraft_open().
        appendLog(QString("Options: %1").arg(QString::fromLatin1(frame)));
        if (frame.size() >= 15 && frame.at(13) == '0' && frame.at(14) == '1')
        {
            isKx2 = true;
            st.model = "KX2";
            modelLabel->setText("KX2");
            setWindowTitle(QString("wfview - KX2 (%1 @ %2 bps)").arg(portName).arg(baudRate));
        }
    }
    else if (cmd == "K2" && ok)
    {
        if (savedK2Level < 0)
        {
            savedK2Level = value;
            if (value != 2)
            {
                set("K22");
                read("PC");
            }
        }
    }
    else
        return;

    updateUi();
}

// IF[f]*****+yyyyrx*00tmvspbd1*  (K3/KX3 layout, 37 characters before ';')
void kx3Window::parseIf(const QByteArray& frame)
{
    if (frame.size() < 35)
        return;
    const bool tx = frame.at(28) == '1';
    st.split = frame.at(32) == '1';
    // While transmitting split, IF reports the TX (VFO B) frequency.
    if (!(tx && st.split))
        st.vfoAHz = frame.mid(2, 11).toLongLong();
    const int offset = frame.mid(19, 4).toInt();
    st.offsetHz = frame.at(18) == '-' ? -offset : offset;
    st.rit = frame.at(23) == '1';
    st.xit = frame.at(24) == '1';
    const char m = frame.at(29);
    if (m >= '1' && m <= '9')
        st.mode = m - '0';
    const char d = frame.at(34);
    if ((st.mode == 6 || st.mode == 9) && d >= '0' && d <= '3')
        st.dataSub = d - '0';
}

void kx3Window::poll()
{
    if (busy || !queue.isEmpty() || !isOpen())
        return;
    if (QDateTime::currentMSecsSinceEpoch() - lastControlMs < 250)
        return;

    const int phase = pollPhase++;
    if (st.transmitting)
    {
        read(phase % 2 == 0 ? QByteArray("TQ") : QByteArray("BG"));
        return;
    }
    if (phase % 2 == 1)
    {
        read("SM");
        return;
    }
    switch ((phase / 2) % 4)
    {
    case 0: case 2:
        read("IF");
        break;
    case 1:
        read("TQ");
        break;
    case 3:
    {
        // Slow status, one item per cycle
        static const char *slow[] = {"FB", "BN", "AG", "RG", "PC", "BW", "PA", "RA", "NB", "GT", "LK", "VX", "DT",
                                     "MG", "KS", "SQ", "IS", "AP", "NR"};
        const int n = int(sizeof(slow) / sizeof(slow[0]));
        QByteArray c = slow[slowIndex++ % n];
        if (c == "DT" && st.mode != 6 && st.mode != 9)
            c = "FB";
        read(c);
        break;
    }
    }
}

// Re-read everything at once, e.g. after settings were changed on the radio.
void kx3Window::refreshAll()
{
    if (!isOpen())
        return;
    appendLog(tr("表示を更新（全項目を読み直し）"));
    for (const char *c : {"IF", "FA", "FB", "MD", "BN", "TQ", "SM", "AG", "RG", "PC", "BW",
                          "PA", "RA", "NB", "GT", "LK", "VX", "DT", "MG", "KS", "SQ", "IS", "AP", "NR"})
        read(c);
}

void kx3Window::appendLog(const QString& text)
{
    // Status and error messages go to the wfview log file.
    qInfo(logSystem()) << "KX3:" << text;
}

void kx3Window::sendNow(const QByteArray& command)
{
    if (!isOpen())
        return;
    qDebug(logSystem()) << "KX3: TX" << command + ';';
    serial->write(command + ';');
    serial->waitForBytesWritten(100);
}

// --- Operations --------------------------------------------------------------

QString kx3Window::formatFrequency(qint64 hz)
{
    return QString("%1.%2.%3")
        .arg(hz / 1000000)
        .arg((hz / 1000) % 1000, 3, 10, QChar('0'))
        .arg((hz / 10) % 100, 2, 10, QChar('0'));
}

void kx3Window::setVfoA(qint64 hz)
{
    // KX3 receive coverage is roughly 0.5 - 54 MHz; the radio rejects gaps itself.
    if (hz < 500000 || hz > 54000000)
    {
        appendLog(tr("周波数範囲外です: %1").arg(formatFrequency(hz)));
        return;
    }
    if (st.locked)
        appendLog(tr("LOCK 中のため本体側で周波数変更が拒否される場合があります"));
    st.vfoAHz = hz;
    set(QString("FA%1").arg(hz, 11, 10, QChar('0')).toLatin1(), true);
    updateUi();
}

void kx3Window::setVfoB(qint64 hz)
{
    if (hz < 500000 || hz > 54000000)
        return;
    st.vfoBHz = hz;
    set(QString("FB%1").arg(hz, 11, 10, QChar('0')).toLatin1(), true);
    updateUi();
}

void kx3Window::moveOffset(int steps)
{
    if (!st.rit && !st.xit)
    {
        setVfoB(st.vfoBHz + qint64(steps) * st.step);
        return;
    }
    // RU / RD move the RIT / XIT offset by one step each.
    const int n = qMin(std::abs(steps), 20);
    for (int i = 0; i < n; ++i)
        set(steps > 0 ? "RU" : "RD");
    read("IF");
}

void kx3Window::setAfGain(int value)
{
    // KX3 AG range is 0-60 in practice (hamlib kx3_set_level).
    st.afGain = qBound(0, value, 60);
    set(QString("AG%1").arg(st.afGain, 3, 10, QChar('0')).toLatin1(), true);
    updateUi();
}

void kx3Window::setRfGain(int value)
{
    // KX3 RG range 190 (-60 dB) - 250 (full gain).
    st.rfGain = qBound(190, value, 250);
    set(QString("RG%1").arg(st.rfGain, 3, 10, QChar('0')).toLatin1(), true);
    updateUi();
}

void kx3Window::setSquelch(int value)
{
    st.squelch = qBound(0, value, 29);
    set(QString("SQ%1").arg(st.squelch, 3, 10, QChar('0')).toLatin1(), true);
    updateUi();
}

void kx3Window::setIfShift(int hz)
{
    st.ifShiftHz = qBound(300, hz, 3000);
    set(QString("IS %1").arg(st.ifShiftHz, 4, 10, QChar('0')).toLatin1(), true);
    updateUi();
}

void kx3Window::setMicGain(int value)
{
    st.micGain = qBound(0, value, 80);
    set(QString("MG%1").arg(st.micGain, 3, 10, QChar('0')).toLatin1(), true);
    updateUi();
}

void kx3Window::setKeyerSpeed(int wpm)
{
    st.keyerWpm = qBound(8, wpm, 50);
    set(QString("KS%1").arg(st.keyerWpm, 3, 10, QChar('0')).toLatin1(), true);
    updateUi();
}

void kx3Window::setPower(int tenths)
{
    // 0.1 W steps up to the KX3's 15 W (K22: trailing 0 = tenths).
    st.powerTenths = qBound(0, tenths, isKx2 ? 120 : 150);
    set(QString("PC%10").arg(st.powerTenths, 3, 10, QChar('0')).toLatin1(), true);
    updateUi();
}

void kx3Window::setBandwidth(int hz)
{
    st.bandwidthHz = qBound(50, hz, 4000);
    set(QString("BW%1").arg(st.bandwidthHz / 10, 4, 10, QChar('0')).toLatin1(), true);
    updateUi();
}

void kx3Window::sendCw()
{
    QString text = cwInput->text().trimmed().toUpper();
    if (text.isEmpty())
        return;
    if (st.mode != 3 && st.mode != 7)
    {
        QMessageBox::warning(this, tr("KX3"), tr("CW 送信は CW / CW-R モードで使用してください。"));
        return;
    }
    // KY takes up to 24 characters per command.
    text.remove(QRegularExpression("[^A-Z0-9 /?.,=+\\-]"));
    for (int i = 0; i < text.size(); i += 24)
        set(QByteArray("KY ") + text.mid(i, 24).toLatin1());
    appendLog(tr("CW 送信: %1").arg(text));
    cwInput->clear();
}

void kx3Window::powerOff()
{
    QMessageBox msgbox(this);
    msgbox.setWindowTitle(st.model);
    msgbox.setText(tr("無線機の電源を OFF にして、プログラムを終了しますか？\n"
                      "（電源 ON は本体の操作が必要です）"));
    msgbox.setIcon(QMessageBox::Icon::Question);
    msgbox.setStyleSheet(wfview_largeDialogStyleSheet());
    QPushButton *yesButton = msgbox.addButton(tr("はい"), QMessageBox::YesRole);
    QPushButton *cancelButton = msgbox.addButton(tr("キャンセル"), QMessageBox::RejectRole);
    msgbox.setDefaultButton(cancelButton);
    msgbox.exec();
    if (msgbox.clickedButton() != yesButton || !isOpen())
        return;
    // Send it right away instead of queueing: polling stops and the
    // program exits right after.
    pollTimer->stop();
    responseTimer->stop();
    gapTimer->stop();
    queue.clear();
    busy = false;
    if (st.transmitting)
        sendNow("RX");
    if (savedK2Level >= 0 && savedK2Level != 2)
        sendNow(QByteArray("K2") + QByteArray::number(savedK2Level));
    savedK2Level = -1;
    sendNow("PS0");
    appendLog(tr("無線機の電源を OFF にしました。プログラムを終了します"));
    st.catOk = false;
    updateUi();
    QTimer::singleShot(300, this, &kx3Window::exitRequested);
}

void kx3Window::startTransmit()
{
    if (st.transmitting || !isOpen())
        return;
    st.transmitting = true;
    lastControlMs = QDateTime::currentMSecsSinceEpoch();
    enqueue("TX", QByteArray(), true);
    if (!safetyTimer->isActive())
        safetyTimer->start();
    updateUi();
}

void kx3Window::stopTransmit()
{
    if (!isOpen())
        return;
    st.transmitting = false;
    safetyTimer->stop();
    lastControlMs = QDateTime::currentMSecsSinceEpoch();
    enqueue("RX", QByteArray(), true);
    updateUi();
}

void kx3Window::safetyTimeout()
{
    stopTransmit();
    QMessageBox::warning(this, st.model, tr("安全のため連続送信を3分で停止しました。"));
}
