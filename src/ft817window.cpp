#include "ft817window.h"
#include "logcategories.h"

#include <QSerialPort>
#include <QTimer>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QDialog>
#include <QDialogButtonBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QSettings>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QCloseEvent>
#include <QDateTime>
#include <QtMath>
#include <QHash>
#include <cmath>

// FT-817/818 CAT opcodes (5-byte commands, opcode in the last byte).
// Verified against the hamlib ft817 driver.
namespace {
constexpr quint8 opLockOn = 0x00;
constexpr quint8 opLockOff = 0x80;
constexpr quint8 opSetFreq = 0x01;
constexpr quint8 opSplitOn = 0x02;
constexpr quint8 opSplitOff = 0x82;
constexpr quint8 opReadFreqMode = 0x03;
constexpr quint8 opClarOn = 0x05;
constexpr quint8 opClarOff = 0x85;
constexpr quint8 opSetMode = 0x07;
constexpr quint8 opPttOn = 0x08;
constexpr quint8 opPttOff = 0x88;
constexpr quint8 opRptShift = 0x09;     // P1: 0x09 minus, 0x49 plus, 0x89 simplex
constexpr quint8 opToneMode = 0x0A;     // P1: 0x0A DCS, 0x2A CTCSS (TSQ), 0x4A encoder, 0x8A off
constexpr quint8 opCtcssTone = 0x0B;
constexpr quint8 opDcsCode = 0x0C;
constexpr quint8 opPowerOn = 0x0F;
constexpr quint8 opPowerOff = 0x8F;
constexpr quint8 opToggleVfo = 0x81;
constexpr quint8 opEepromRead = 0xBB;   // read only; this window never writes EEPROM
constexpr quint8 opReadTxMeter = 0xBD;
constexpr quint8 opReadRx = 0xE7;
constexpr quint8 opClarFreq = 0xF5;
constexpr quint8 opReadTx = 0xF7;
constexpr quint8 opRptOffset = 0xF9;

// Read-only EEPROM status locations.  Sources: hamlib ft817.c (0x55, 0x59,
// 0x65, 0x7A) and the Ham::Device::FT817COMM Perl module (the rest).
constexpr quint16 eepromVfo = 0x55;     // bit0 VFO B, bit4 HOME, bit6 memory mode
constexpr quint16 eepromFlags1 = 0x57;  // bits1-0 AGC, bit5 NB, bit6 LOCK (0 = on), bit7 FAST (0 = on)
constexpr quint16 eepromFlags2 = 0x58;  // bits1-0 meter, bit4 KYR, bit5 BK, bit6 VLT, bit7 VOX
constexpr quint16 eepromBand = 0x59;    // low nibble VFO A band, high nibble VFO B band
constexpr quint16 eepromDigMode = 0x65; // top 3 bits: DIG sub mode
constexpr quint16 eepromFlags3 = 0x79;  // bits1-0 TX power, bit3 DW, bit4 PRI, bits6-5 scan, bit7 ARTS
constexpr quint16 eepromSplit = 0x7A;   // bits0-6 rear antenna per band group, bit7 split
constexpr quint16 eepromCharger = 0x7B; // bit4 charging
constexpr quint16 eepromMemCh = 0x44F;  // current memory channel - 1

// FT-817 VFO band records (26 bytes each, band order 160 m ... PHAN).  The
// FT-818 adds 60 m and shifts this layout, so these are read on the FT-817 only.
constexpr quint16 vfoARecordBase = 0x7D;
constexpr quint16 vfoBRecordBase = 0x203;
constexpr quint16 vfoRecordSize = 0x1A;
constexpr int recRpt = 1;               // bits7-6: 00 simplex, 01 minus, 10 plus, 11 non-standard
constexpr int recAttIpo = 2;            // bit4 ATT, bit5 IPO
constexpr int recTone = 4;              // bits1-0: 00 off, 01 TONE, 10 TSQ, 11 DCS

const qint64 bandFrequencies[] = {
    1900000, 3550000, 7100000, 10120000, 14200000, 18100000,
    21200000, 24930000, 28500000, 50200000, 145000000, 433000000
};

const int ctcssTones[] = {
    670, 693, 719, 744, 770, 797, 825, 854, 885, 915, 948, 974, 1000, 1035, 1072,
    1109, 1148, 1188, 1230, 1273, 1318, 1365, 1413, 1462, 1514, 1567, 1598, 1622,
    1655, 1679, 1713, 1738, 1773, 1799, 1835, 1862, 1899, 1928, 1966, 1995, 2035,
    2065, 2107, 2181, 2257, 2291, 2336, 2418, 2503, 2541
};

const int dcsCodes[] = {
    23, 25, 26, 31, 32, 36, 43, 47, 51, 53, 54, 65, 71, 72, 73, 74, 114, 115, 116,
    122, 125, 131, 132, 134, 143, 145, 152, 155, 156, 162, 165, 172, 174, 205, 212,
    223, 225, 226, 243, 244, 245, 246, 251, 252, 255, 261, 263, 265, 266, 271, 274,
    306, 311, 315, 325, 331, 332, 343, 346, 351, 356, 364, 365, 371, 411, 412, 413,
    423, 431, 432, 445, 446, 452, 454, 455, 462, 464, 465, 466, 503, 506, 516, 523,
    526, 532, 546, 565, 606, 612, 624, 627, 631, 632, 654, 662, 664, 703, 712, 723,
    731, 732, 734, 743, 754
};

const char *digModeNames[] = { "RTTY", "PSK31-L", "PSK31-U", "USER-L", "USER-U" };

// Soft key rows shown on the LCD above the A/B/C keys; F selects the row.
const char *softKeyRows[][3] = {
    { "A/B", "SPL", "CLAR" },
    { "RPT", "TON", "SET" },
    { "NAR", "STEP", "" },
};
constexpr int softKeyPages = 3;

const char *panelStyle =
    "QPushButton { color: #f2f2f2; font-weight: bold; font-size: 11px;"
    " background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #5c5c5c, stop:1 #2a2a2a);"
    " border: 1px solid #0d0d0d; border-radius: 10px; padding: 3px 6px; min-height: 26px; }"
    "QPushButton:pressed { background: #1c1c1c; }"
    "QPushButton:disabled { color: #777; }"
    "QLabel { color: #dcdcdc; }";
}

// ---------------------------------------------------------------------------
// ft817Knob
// ---------------------------------------------------------------------------

ft817Knob::ft817Knob(bool large, QWidget *parent) : QWidget(parent), large(large)
{
    setMinimumSize(large ? 230 : 84, large ? 230 : 84);
    setCursor(Qt::OpenHandCursor);
    setToolTip(large ? tr("DIAL: ドラッグで回転 / 右半分クリックで＋1ステップ・左半分で−1 / ホイール")
                     : tr("SEL: ドラッグ・クリック・ホイールで操作"));
}

QSize ft817Knob::sizeHint() const
{
    return large ? QSize(270, 270) : QSize(96, 96);
}

void ft817Knob::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const double side = qMin(width(), height()) - 6;
    const QPointF c(width() / 2.0, height() / 2.0);
    const double r = side / 2.0;

    // Shadow and knurled rubber skirt
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 120));
    p.drawEllipse(c + QPointF(2, 3), r, r);
    QRadialGradient skirt(c, r);
    skirt.setColorAt(0.0, QColor(40, 40, 40));
    skirt.setColorAt(0.92, QColor(22, 22, 22));
    skirt.setColorAt(1.0, QColor(8, 8, 8));
    p.setBrush(skirt);
    p.drawEllipse(c, r, r);

    const int ridges = large ? 90 : 36;
    p.setPen(QPen(QColor(70, 70, 70), large ? 2 : 1.5));
    for (int i = 0; i < ridges; ++i)
    {
        const double a = qDegreesToRadians(i * 360.0 / ridges + indicatorAngle);
        const QPointF dir(qSin(a), -qCos(a));
        p.drawLine(c + dir * (r - 1), c + dir * (r * (large ? 0.88 : 0.80)));
    }

    // Brushed face
    const double face = r * (large ? 0.84 : 0.74);
    QConicalGradient brushed(c, 45);
    brushed.setColorAt(0.00, QColor(95, 95, 95));
    brushed.setColorAt(0.25, QColor(50, 50, 50));
    brushed.setColorAt(0.50, QColor(90, 90, 90));
    brushed.setColorAt(0.75, QColor(45, 45, 45));
    brushed.setColorAt(1.00, QColor(95, 95, 95));
    p.setPen(QPen(QColor(15, 15, 15), 2));
    p.setBrush(brushed);
    p.drawEllipse(c, face, face);

    const double a = qDegreesToRadians(indicatorAngle);
    const QPointF dir(qSin(a), -qCos(a));
    if (large)
    {
        // Finger dimple, like the FT-817 main dial
        const QPointF dimple = c + dir * (face * 0.58);
        QRadialGradient dg(dimple + QPointF(-4, -4), face * 0.22);
        dg.setColorAt(0.0, QColor(25, 25, 25));
        dg.setColorAt(1.0, QColor(70, 70, 70));
        p.setPen(QPen(QColor(20, 20, 20), 1.5));
        p.setBrush(dg);
        p.drawEllipse(dimple, face * 0.18, face * 0.18);
    }
    else
    {
        p.setPen(QPen(QColor(240, 240, 240), 3, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(c + dir * (face * 0.25), c + dir * (face * 0.92));
    }
}

void ft817Knob::rotate(double degrees)
{
    indicatorAngle = std::fmod(indicatorAngle + degrees + 360.0, 360.0);
    update();
}

void ft817Knob::mousePressEvent(QMouseEvent *event)
{
    pressPos = event->position();
    const QPointF d = pressPos - QPointF(width() / 2.0, height() / 2.0);
    lastAngle = qRadiansToDegrees(qAtan2(d.x(), -d.y()));
    accumulated = 0.0;
    dragging = false;
    setCursor(Qt::ClosedHandCursor);
}

void ft817Knob::mouseMoveEvent(QMouseEvent *event)
{
    if (!(event->buttons() & Qt::LeftButton))
        return;
    if (!dragging && QLineF(pressPos, event->position()).length() < 5)
        return;
    dragging = true;

    const QPointF d = event->position() - QPointF(width() / 2.0, height() / 2.0);
    const double angle = qRadiansToDegrees(qAtan2(d.x(), -d.y()));
    double delta = angle - lastAngle;
    if (delta > 180) delta -= 360;
    if (delta < -180) delta += 360;
    lastAngle = angle;
    rotate(delta);
    accumulated += delta;

    // One step per 10 degrees (main dial) or 30 degrees (SEL detent).
    const double detent = large ? 10.0 : 30.0;
    const int steps = int(accumulated / detent);
    if (steps != 0)
    {
        accumulated -= steps * detent;
        emit stepsTurned(steps);
    }
}

void ft817Knob::mouseReleaseEvent(QMouseEvent *event)
{
    setCursor(Qt::OpenHandCursor);
    if (!dragging)
    {
        const int dir = event->position().x() >= width() / 2.0 ? 1 : -1;
        rotate(dir * (large ? 10.0 : 30.0));
        emit stepsTurned(dir);
    }
    dragging = false;
}

void ft817Knob::wheelEvent(QWheelEvent *event)
{
    const int notches = event->angleDelta().y() / 120;
    if (notches != 0)
    {
        rotate(notches * (large ? 10.0 : 30.0));
        emit stepsTurned(notches);
    }
    event->accept();
}

// ---------------------------------------------------------------------------
// ft817Lcd
// ---------------------------------------------------------------------------

ft817Lcd::ft817Lcd(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(480, 200);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);     // hand cursor over clickable digits and soft keys
}

void ft817Lcd::setState(const ft817LcdState& s)
{
    st = s;
    update();
}

void ft817Lcd::drawSegmentDigit(QPainter& p, const QRectF& r, QChar c, bool lit)
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
    const double w = r.width(), h = r.height(), t = w * 0.17, g = t * 0.12;

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
    p.translate(h * 0.08, 0);
    p.shear(-0.08, 0);
    p.setPen(Qt::NoPen);
    for (auto it = segs.cbegin(); it != segs.cend(); ++it)
    {
        const bool segOn = lit && on.contains(it.key());
        p.setBrush(segOn ? QColor(30, 20, 8) : QColor(30, 20, 8, 22));
        p.drawPolygon(it.value());
    }
    p.restore();
}

void ft817Lcd::drawIndicator(QPainter& p, const QPointF& pos, const QString& text, bool on)
{
    p.setPen(on ? QColor(30, 20, 8) : QColor(30, 20, 8, 28));
    p.drawText(pos, text);
}

void ft817Lcd::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const double W = width(), H = height();

    // Bezel and amber backlit glass
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(12, 12, 12));
    p.drawRoundedRect(rect(), 8, 8);
    const QRectF glass = QRectF(rect()).adjusted(7, 7, -7, -7);
    QLinearGradient amber(glass.topLeft(), glass.bottomLeft());
    const bool backlight = st.connected && !st.powerOff;
    amber.setColorAt(0.0, backlight ? QColor(250, 186, 80) : QColor(150, 135, 100));
    amber.setColorAt(1.0, backlight ? QColor(232, 150, 44) : QColor(125, 112, 85));
    p.setBrush(amber);
    p.drawRoundedRect(glass, 4, 4);

    const bool lit = st.connected && st.catOk;
    const double gx = glass.left() + 10, gw = glass.width() - 20;
    QFont f = font();
    f.setBold(true);

    // Top indicator row
    f.setPixelSize(int(qMax(11.0, H * 0.065)));
    p.setFont(f);
    const double topY = glass.top() + H * 0.10;
    struct ind { QString text; bool on; };
    QString tone = st.toneMode == 1 ? "T" : st.toneMode == 2 ? "TSQ" : st.toneMode == 3 ? "DCS" : "T";
    const QString vfoText = st.memMode ? (st.memChannel.isEmpty() ? QString("MEM") : st.memChannel)
                                       : QString(st.vfoB ? "VFO b" : "VFO a");
    const QString rptText = st.rptShift < 0 ? "−" : st.rptShift == 2 ? "±" : "+";
    const QList<ind> inds = {
        {vfoText, lit},
        {"HOME", lit && st.home},
        {"LOCK", lit && st.locked},
        {"SPL", lit && st.split},
        {"CLAR", lit && st.clar},
        {rptText, lit && st.rptShift != 0},
        {tone, lit && st.toneMode != 0},
        {"BUSY", lit && st.busy && !st.transmitting},
        {"HI SWR", lit && st.transmitting && st.hiSwr},
        {"TX", lit && st.transmitting},
    };
    double x = gx;
    for (const ind& i : inds)
    {
        drawIndicator(p, QPointF(x, topY), i.text, i.on);
        x += p.fontMetrics().horizontalAdvance(i.text) + W * 0.022;
    }

    // Second indicator row: receiver / keyer / scan status read from EEPROM
    f.setPixelSize(int(qMax(10.0, H * 0.042)));
    p.setFont(f);
    const bool ee = lit && st.eepromKnown;
    const QString scanText = st.scan < 0 ? "SCN▼" : "SCN▲";
    const QList<ind> inds2 = {
        {QString("AGC-%1").arg(st.agc.isEmpty() ? QString("A") : st.agc.left(st.agc == "OFF" ? 3 : 1)), ee},
        {"NB", ee && st.nb},
        {"IPO", lit && st.recordKnown && st.ipo},
        {"ATT", lit && st.recordKnown && st.att},
        {"FAST", ee && st.fastTune},
        {"VOX", ee && st.vox},
        {"KYR", ee && st.kyr},
        {"BK", ee && st.bk},
        {"VLT", ee && st.vlt},
        {"DW", ee && st.dw},
        {"PRI", ee && st.pri},
        {scanText, ee && st.scan != 0},
        {"ARTS", ee && st.arts},
        {"CHG", ee && st.charger},
    };
    const double row2Y = glass.top() + H * 0.165;
    x = gx;
    for (const ind& i : inds2)
    {
        drawIndicator(p, QPointF(x, row2Y), i.text, i.on);
        x += p.fontMetrics().horizontalAdvance(i.text) + W * 0.018;
    }

    // Mode block on the left of the frequency
    const double rowTop = glass.top() + H * 0.20;
    const double rowH = H * 0.37;
    f.setPixelSize(int(H * 0.11));
    p.setFont(f);
    p.setPen(lit ? QColor(30, 20, 8) : QColor(30, 20, 8, 28));
    p.drawText(QRectF(gx, rowTop, gw * 0.17, rowH * 0.55), Qt::AlignLeft | Qt::AlignBottom,
               lit ? st.mode : QString("USB"));
    f.setPixelSize(int(H * 0.06));
    p.setFont(f);
    if (lit && !st.digMode.isEmpty())
        p.drawText(QRectF(gx, rowTop + rowH * 0.58, gw * 0.18, rowH * 0.3), Qt::AlignLeft | Qt::AlignTop, st.digMode);

    // Frequency: 3 MHz digits . 3 kHz digits . 2 digits (10 Hz resolution)
    const qint64 hz = st.frequencyHz;
    QString mhz = QString::number(hz / 1000000).rightJustified(3, ' ');
    if (mhz.size() > 3) mhz = mhz.right(3);
    const QString khz = QString("%1").arg((hz / 1000) % 1000, 3, 10, QChar('0'));
    const QString tens = QString("%1").arg((hz / 10) % 100, 2, 10, QChar('0'));
    const QString digits = mhz + khz + tens;

    const double freqLeft = gx + gw * 0.19;
    const double freqWidth = gw * 0.80;
    const double digitW = freqWidth / (8 + 2 * 0.35);
    const double dw = digitW * 0.82;
    const double dh = rowH * 0.92;
    digitRects.clear();
    double dx = freqLeft;
    for (int i = 0; i < 8; ++i)
    {
        const bool small = i >= 6;  // 10 Hz digits are drawn smaller, as on the radio
        const double scale = small ? 0.72 : 1.0;
        const QRectF dr(dx, rowTop + dh * (1.0 - scale), dw * scale, dh * scale);
        drawSegmentDigit(p, dr, lit ? digits.at(i) : QChar('8'), lit && digits.at(i) != ' ');
        const int stepForDigit = int(qPow(10, 8 - i));
        digitRects.append(qMakePair(QRectF(dx, rowTop, digitW, dh + 8), stepForDigit));
        if (lit && stepForDigit == st.step)
        {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(30, 20, 8));
            p.drawRect(QRectF(dx + dw * scale * 0.1, rowTop + dh + 4, dw * scale * 0.8, 3));
        }
        dx += digitW;
        if (i == 2 || i == 5)
        {
            p.setPen(Qt::NoPen);
            p.setBrush(lit ? QColor(30, 20, 8) : QColor(30, 20, 8, 22));
            const double dot = dw * 0.17;
            p.drawRect(QRectF(dx + digitW * 0.05, rowTop + dh - dot, dot, dot));
            dx += digitW * 0.35;
        }
    }

    // Clarifier offset, under the mode (below the DIG sub mode if shown)
    if (lit && st.clar)
    {
        f.setPixelSize(int(H * 0.055));
        p.setFont(f);
        p.setPen(QColor(30, 20, 8));
        p.drawText(QRectF(gx, rowTop + rowH * (st.digMode.isEmpty() ? 0.62 : 0.86), gw * 0.18, rowH * 0.3),
                   Qt::AlignLeft | Qt::AlignTop,
                   QString("%1%2k").arg(st.clarOffsetHz < 0 ? "−" : "+")
                       .arg(std::abs(st.clarOffsetHz) / 1000.0, 0, 'f', 2));
    }

    // Bar meter: S in RX, PO in TX
    const double meterTop = rowTop + rowH + H * 0.06;
    const double meterH = H * 0.07;
    f.setPixelSize(int(H * 0.06));
    p.setFont(f);
    p.setPen(lit ? QColor(30, 20, 8) : QColor(30, 20, 8, 28));
    p.drawText(QRectF(gx, meterTop - 2, gw * 0.06, meterH + 4), Qt::AlignLeft | Qt::AlignVCenter,
               st.transmitting ? "PO" : "S");
    const double barLeft = gx + gw * 0.07;
    const double barW = gw * 0.62 / 15.0;
    const int level = st.transmitting ? st.po : st.sMeter;
    for (int i = 0; i < 15; ++i)
    {
        p.setPen(Qt::NoPen);
        p.setBrush(lit && i < level ? QColor(30, 20, 8) : QColor(30, 20, 8, 25));
        const double bh = meterH * (0.55 + 0.45 * i / 14.0);
        p.drawRect(QRectF(barLeft + i * barW, meterTop + meterH - bh, barW * 0.72, bh));
    }
    f.setPixelSize(int(H * 0.045));
    p.setFont(f);
    p.setPen(lit ? QColor(30, 20, 8) : QColor(30, 20, 8, 28));
    const QStringList scale = {"1", "3", "5", "7", "9", "+20", "+40", "+60"};
    for (int i = 0; i < scale.size(); ++i)
        p.drawText(QPointF(barLeft + (i * 2) * barW, meterTop + meterH + H * 0.055), scale.at(i));

    // TX metering readout / FM discriminator to the right of the bar
    f.setPixelSize(int(H * 0.05));
    p.setFont(f);
    const QRectF side(barLeft + 15 * barW + gw * 0.02, meterTop - H * 0.03, gw * 0.29, meterH + H * 0.11);
    if (lit && st.transmitting)
        p.drawText(side, Qt::AlignLeft | Qt::AlignVCenter,
                   QString("ALC %1  SWR %2\nMOD %3").arg(st.alc).arg(st.swr).arg(st.mod));
    else if (lit)
    {
        QStringList lines;
        lines << QString("STEP %1%2").arg(st.step >= 1000 ? st.step / 1000.0 : st.step)
                     .arg(st.step >= 1000 ? " kHz" : " Hz");
        QStringList info;
        if (!st.txPower.isEmpty()) info << QString("PWR %1").arg(st.txPower);
        if (!st.antenna.isEmpty()) info << QString("ANT %1").arg(st.antenna);
        if (!info.isEmpty()) lines << info.join("  ");
        QStringList info2;
        if (!st.meterMode.isEmpty()) info2 << QString("MTR %1").arg(st.meterMode);
        if (st.mode.startsWith("FM") && st.discCentered) info2 << "◆CTR";
        if (!info2.isEmpty()) lines << info2.join("  ");
        f.setPixelSize(int(H * 0.042));
        p.setFont(f);
        p.drawText(side, Qt::AlignLeft | Qt::AlignVCenter, lines.join("\n"));
    }

    // Soft key row (labels for the A / B / C keys)
    const double keyTop = glass.bottom() - H * 0.15;
    f.setPixelSize(int(H * 0.07));
    p.setFont(f);
    const double cellW = gw / 4.0;
    softKeyRects.clear();
    for (int i = 0; i < 3; ++i)
    {
        const QRectF cell(gx + i * cellW, keyTop, cellW - 10, H * 0.11);
        softKeyRects.append(cell);
        p.setPen(QPen(lit ? QColor(30, 20, 8) : QColor(30, 20, 8, 28), 1.5));
        p.setBrush(Qt::NoBrush);
        p.drawRect(cell);
        p.drawText(cell, Qt::AlignCenter, i < st.softKeys.size() ? st.softKeys.at(i) : QString());
    }
    const QRectF pageCell(gx + 3 * cellW, keyTop, cellW, H * 0.11);
    softKeyRects.append(pageCell);
    p.drawText(pageCell, Qt::AlignCenter, QString("F-%1").arg(st.page + 1));

    if (!st.connected || !st.catOk)
    {
        f.setPixelSize(int(H * 0.08));
        p.setFont(f);
        p.setPen(QColor(120, 20, 10));
        p.drawText(QRectF(glass.left(), rowTop + rowH * 0.25, glass.width(), rowH * 0.5), Qt::AlignCenter,
                   !st.connected ? tr("ポート未接続") : tr("CAT 応答待ち（電源・ボーレート・ケーブルを確認）"));
    }
}

void ft817Lcd::mousePressEvent(QMouseEvent *event)
{
    for (const auto& d : digitRects)
    {
        if (d.first.contains(event->position()))
        {
            emit digitClicked(d.second);
            return;
        }
    }
    for (int i = 0; i < softKeyRects.size(); ++i)
    {
        if (softKeyRects.at(i).contains(event->position()))
        {
            emit softKeyClicked(i);
            return;
        }
    }
    QWidget::mousePressEvent(event);
}

void ft817Lcd::mouseMoveEvent(QMouseEvent *event)
{
    bool clickable = false;
    for (const auto& d : digitRects)
        clickable = clickable || d.first.contains(event->position());
    for (const QRectF& r : softKeyRects)
        clickable = clickable || r.contains(event->position());
    setCursor(clickable ? Qt::PointingHandCursor : Qt::ArrowCursor);
    QWidget::mouseMoveEvent(event);
}

// ---------------------------------------------------------------------------
// ft817Window
// ---------------------------------------------------------------------------

const QList<ft817Window::modeInfo>& ft817Window::setModes()
{
    static const QList<modeInfo> list = {
        {0x00, "LSB"}, {0x01, "USB"}, {0x02, "CW"}, {0x03, "CWR"},
        {0x04, "AM"}, {0x08, "FM"}, {0x0A, "DIG"}, {0x0C, "PKT"}
    };
    return list;
}

QString ft817Window::modeTitle(quint8 code)
{
    QString base;
    switch (code & 0x7F)
    {
    case 0x00: base = "LSB"; break;
    case 0x01: base = "USB"; break;
    case 0x02: base = "CW"; break;
    case 0x03: base = "CWR"; break;
    case 0x04: base = "AM"; break;
    case 0x06: base = "WFM"; break;
    case 0x08: base = "FM"; break;
    case 0x0A: base = "DIG"; break;
    case 0x0C: base = "PKT"; break;
    default: base = QString("%1").arg(code & 0x7F, 2, 16, QChar('0')).toUpper(); break;
    }
    if (code & 0x80)
        base += "-N";
    return base;
}

ft817Window::ft817Window(const QString& portName, quint32 baudRate, bool ft818, QWidget *parent)
    : QWidget(parent, Qt::Window), portName(portName), baudRate(baudRate), ft818(ft818)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(QString("wfview - %1 (%2 @ %3 bps)").arg(ft818 ? "FT-818" : "FT-817", portName).arg(baudRate));

    serial = new QSerialPort(this);
    connect(serial, &QSerialPort::readyRead, this, &ft817Window::readSerial);
    connect(serial, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError err) {
        if (err == QSerialPort::NoError || err == QSerialPort::TimeoutError)
            return;
        appendLog(QString("Serial error: %1").arg(serial->errorString()));
        if (err == QSerialPort::ResourceError)
        {
            // USB-serial adapter unplugged
            stopScan(tr("シリアル接続が切れたためスキャンを停止しました"));
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
    pollTimer->setInterval(120);
    connect(pollTimer, &QTimer::timeout, this, &ft817Window::poll);

    responseTimer = new QTimer(this);
    responseTimer->setSingleShot(true);
    connect(responseTimer, &QTimer::timeout, this, &ft817Window::responseTimeout);

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
    connect(safetyTimer, &QTimer::timeout, this, &ft817Window::safetyTimeout);

    scanTimer = new QTimer(this);
    scanTimer->setSingleShot(true);
    connect(scanTimer, &QTimer::timeout, this, &ft817Window::scanReadRx);

    st.frequencyHz = 7100000;
    st.mode = "USB";
    buildUi();
    updateUi();
}

ft817Window::~ft817Window()
{
    sendPttOffNow();
}

bool ft817Window::isOpen() const
{
    return serial != nullptr && serial->isOpen();
}

bool ft817Window::openPort()
{
    serial->setPortName(portName);
    serial->setBaudRate(baudRate);
    // Yaesu CAT: 8 data bits, no parity, 2 stop bits.
    serial->setDataBits(QSerialPort::Data8);
    serial->setParity(QSerialPort::NoParity);
    serial->setStopBits(QSerialPort::TwoStop);
    serial->setFlowControl(QSerialPort::NoFlowControl);
    if (!serial->open(QIODevice::ReadWrite))
    {
        appendLog(QString("Could not open %1: %2").arg(portName, serial->errorString()));
        updateUi();
        return false;
    }
    // Many USB CAT cables take their interface power from DTR.
    serial->setDataTerminalReady(true);
    serial->setRequestToSend(false);

    appendLog(QString("Connected: %1 @ %2 bps (8N2)").arg(portName).arg(baudRate));
    qInfo(logSystem()) << "FT-817/818: opened" << portName << "at" << baudRate;
    st.connected = true;
    pollTimer->start();
    refreshAll();
    updateUi();
    return true;
}

void ft817Window::closeEvent(QCloseEvent *event)
{
    stopScan(QString());
    pollTimer->stop();
    responseTimer->stop();
    gapTimer->stop();
    safetyTimer->stop();
    queue.clear();
    sendPttOffNow();
    if (serial->isOpen())
        serial->close();
    emit closed();
    event->accept();
}

void ft817Window::buildUi()
{
    setObjectName("ft817Body");
    setStyleSheet(QString("#ft817Body { background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
                          " stop:0 #3b3b3b, stop:1 #1b1b1b); }") + panelStyle);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 10, 16, 12);
    root->setSpacing(8);

    // Brand strip
    auto *brand = new QHBoxLayout();
    auto *model = new QLabel(ft818 ? "FT-818" : "FT-817");
    model->setStyleSheet("color: #f5f5f5; font-size: 20px; font-weight: 900;");
    auto *sub = new QLabel("HF/50/144/430 MHz ALL MODE TRANSCEIVER");
    sub->setStyleSheet("color: #a8a8a8; font-size: 11px; font-weight: bold;");
    brand->addStretch();
    brand->addWidget(model, 0, Qt::AlignBottom);
    brand->addSpacing(10);
    brand->addWidget(sub, 0, Qt::AlignBottom);
    root->addLayout(brand);

    auto *body = new QHBoxLayout();
    body->setSpacing(14);
    root->addLayout(body, 1);

    auto makeKey = [this](const QString& text, const QString& tip = QString()) {
        auto *b = new QPushButton(text, this);
        b->setFocusPolicy(Qt::NoFocus);
        b->setMinimumWidth(64);
        if (!tip.isEmpty())
            b->setToolTip(tip);
        return b;
    };
    auto caption = [](const QString& text) {
        auto *l = new QLabel(text);
        l->setAlignment(Qt::AlignCenter);
        l->setStyleSheet("color: #cfcfcf; font-size: 10px; font-weight: bold;");
        return l;
    };

    // Left key block: MODE, BAND, power / LOCK
    auto *left = new QGridLayout();
    left->setVerticalSpacing(4);
    left->setHorizontalSpacing(6);
    left->addWidget(caption("MODE"), 0, 0, 1, 2);
    auto *modeDown = makeKey("◀", tr("モード（前）"));
    auto *modeUp = makeKey("▶", tr("モード（次）"));
    left->addWidget(modeDown, 1, 0);
    left->addWidget(modeUp, 1, 1);
    left->addWidget(caption("BAND"), 2, 0, 1, 2);
    auto *bandDown = makeKey("DWN", tr("下のバンドへ"));
    auto *bandUp = makeKey("UP", tr("上のバンドへ"));
    left->addWidget(bandDown, 3, 0);
    left->addWidget(bandUp, 3, 1);
    left->setRowMinimumHeight(4, 14);
    auto *pwrBtn = makeKey(tr("電源"), tr("CATで電源 ON / OFF"));
    auto *lockBtn = makeKey("LOCK", tr("パネルロック ON / OFF"));
    left->addWidget(pwrBtn, 5, 0);
    left->addWidget(lockBtn, 5, 1);
    left->setRowStretch(6, 1);
    connect(modeDown, &QPushButton::clicked, this, [this]() { selectAdjacentMode(-1); });
    connect(modeUp, &QPushButton::clicked, this, [this]() { selectAdjacentMode(1); });
    connect(bandDown, &QPushButton::clicked, this, [this]() { stepBand(-1); });
    connect(bandUp, &QPushButton::clicked, this, [this]() { stepBand(1); });
    connect(pwrBtn, &QPushButton::clicked, this, [this]() {
        if (st.catOk) powerOff(); else powerOn();
    });
    connect(lockBtn, &QPushButton::clicked, this, &ft817Window::toggleLock);
    body->addLayout(left);

    // Centre: LCD with A / B / C / F keys underneath
    auto *centre = new QVBoxLayout();
    centre->setSpacing(6);
    lcd = new ft817Lcd(this);
    connect(lcd, &ft817Lcd::digitClicked, this, [this](int step) {
        st.step = step;
        updateUi();
    });
    centre->addWidget(lcd, 1);
    auto *keys = new QHBoxLayout();
    keys->setSpacing(10);
    for (int i = 0; i < 3; ++i)
    {
        auto *b = makeKey(QString(QChar('A' + i)));
        connect(b, &QPushButton::clicked, this, [this, i]() { applySoftKey(i); });
        softButtons.append(b);
        keys->addWidget(b);
    }
    auto *fBtn = makeKey("F", tr("A / B / C キーの機能列を切替"));
    connect(fBtn, &QPushButton::clicked, this, [this]() {
        st.page = (st.page + 1) % softKeyPages;
        updateUi();
    });
    keys->addWidget(fBtn);
    centre->addLayout(keys);
    // The LCD soft key labels act like the A / B / C / F keys below them
    // (click() does nothing while the keys are disabled).
    connect(lcd, &ft817Lcd::softKeyClicked, this, [this, fBtn](int key) {
        if (key < softButtons.size())
            softButtons.at(key)->click();
        else
            fBtn->click();
    });
    body->addLayout(centre, 1);

    // Right: SEL knob above the large main dial
    auto *right = new QVBoxLayout();
    right->setSpacing(2);
    auto *selRow = new QHBoxLayout();
    selRow->addStretch();
    auto *selBox = new QVBoxLayout();
    selBox->setSpacing(0);
    selBox->addWidget(caption("SEL"));
    auto *sel = new ft817Knob(false, this);
    sel->setToolTip(tr("SEL: CLAR ON 中はクラリファイアを ±10 Hz、OFF 中は周波数を ±100 kHz"));
    connect(sel, &ft817Knob::stepsTurned, this, [this](int steps) {
        if (st.clar)
            setClarOffset(st.clarOffsetHz + steps * 10);
        else
            setFrequency(st.frequencyHz + qint64(steps) * 100000);
    });
    selBox->addWidget(sel, 0, Qt::AlignCenter);
    selRow->addLayout(selBox);
    right->addLayout(selRow);
    auto *dial = new ft817Knob(true, this);
    connect(dial, &ft817Knob::stepsTurned, this, [this](int steps) {
        setFrequency(st.frequencyHz + qint64(steps) * st.step);
    });
    right->addWidget(dial, 1);
    right->addWidget(caption("DIAL"));
    body->addLayout(right);

    // Bottom strip: PTT, direct frequency entry, log and disconnect
    auto *bottom = new QHBoxLayout();
    bottom->setSpacing(8);
    pttBtn = makeKey(tr("送信 (PTT)"));
    pttBtn->setMinimumWidth(150);
    connect(pttBtn, &QPushButton::clicked, this, [this]() {
        if (st.transmitting || awaitingTransmit)
        {
            stopTransmit();
            return;
        }
        startTransmit();
    });
    bottom->addWidget(pttBtn);
    bottom->addSpacing(10);
    bottom->addWidget(new QLabel(tr("周波数入力")));
    freqInput = new QLineEdit(this);
    freqInput->setPlaceholderText(tr("MHz（例 7.0745）"));
    freqInput->setFixedWidth(180);
    freqInput->setStyleSheet("QLineEdit { background: #f0f0f0; color: black; border: 1px solid #8a8a8a; border-radius: 8px; padding: 2px 6px; }");
    connect(freqInput, &QLineEdit::returnPressed, this, [this]() {
        QString text = freqInput->text().trimmed();
        text.replace(',', '.');
        bool ok = false;
        const double mhz = text.toDouble(&ok);
        if (!ok || mhz <= 0)
        {
            QMessageBox::warning(this, tr("FT-817/818"), tr("周波数をMHz単位の数値で入力してください。"));
            return;
        }
        setFrequency(qRound64(mhz * 1000000.0));
        freqInput->clear();
    });
    bottom->addWidget(freqInput);
    bottom->addSpacing(10);
    refreshBtn = makeKey(tr("更新"), tr("本体側で変更した設定を読み直して表示を更新"));
    refreshBtn->setMinimumWidth(90);
    connect(refreshBtn, &QPushButton::clicked, this, &ft817Window::refreshAll);
    bottom->addWidget(refreshBtn);
    bottom->addStretch();
    disconnectBtn = makeKey(tr("切断して戻る"));
    connect(disconnectBtn, &QPushButton::clicked, this, &QWidget::close);
    bottom->addWidget(disconnectBtn);

    // Range scan strip, above the bottom strip
    auto *scanRow = new QHBoxLayout();
    scanRow->setSpacing(6);
    const QString editStyle = "QLineEdit { background: #f0f0f0; color: black; border: 1px solid #8a8a8a; border-radius: 8px; padding: 2px 6px; }";
    auto makeScanEdit = [this, &editStyle](const QString& placeholder, int width) {
        auto *e = new QLineEdit(this);
        e->setPlaceholderText(placeholder);
        e->setFixedWidth(width);
        e->setStyleSheet(editStyle);
        return e;
    };
    scanRow->addWidget(new QLabel(tr("スキャン  開始")));
    scanStartEdit = makeScanEdit(tr("MHz"), 110);
    scanRow->addWidget(scanStartEdit);
    scanRow->addWidget(new QLabel(tr("終了")));
    scanEndEdit = makeScanEdit(tr("MHz"), 110);
    scanRow->addWidget(scanEndEdit);
    scanRow->addWidget(new QLabel(tr("ステップ")));
    scanStepEdit = makeScanEdit(tr("kHz"), 80);
    scanRow->addWidget(scanStepEdit);
    scanRow->addWidget(new QLabel(tr("kHz")));
    scanRow->addSpacing(8);
    scanRow->addWidget(new QLabel(tr("信号で停止後")));
    scanResumeCombo = new QComboBox(this);
    scanResumeCombo->addItem(tr("指定秒数後に再開"), int(ScanResume::afterTime));
    scanResumeCombo->addItem(tr("信号が消えて指定秒数後に再開"), int(ScanResume::afterSignalGone));
    scanResumeCombo->addItem(tr("停止したまま"), int(ScanResume::stay));
    scanRow->addWidget(scanResumeCombo);
    scanWaitSpin = new QSpinBox(this);
    scanWaitSpin->setRange(1, 60);
    scanWaitSpin->setSuffix(tr(" 秒"));
    // Match the line edits; otherwise the dark application style draws it smaller.
    scanWaitSpin->setFixedWidth(100);
    scanWaitSpin->setStyleSheet("QSpinBox { background: #f0f0f0; color: black; border: 1px solid #8a8a8a; border-radius: 8px;"
                                " padding: 2px; selection-background-color: #3874d8; selection-color: white; }");
    scanRow->addWidget(scanWaitSpin);
    scanRow->addSpacing(8);
    scanBtn = makeKey(tr("スキャン開始"), tr("指定範囲をステップごとに受信し、スケルチが開いたら止まります"));
    scanBtn->setMinimumWidth(140);
    connect(scanBtn, &QPushButton::clicked, this, [this]() {
        if (scanning)
            stopScan(tr("スキャンを停止しました"));
        else
            startScan();
    });
    scanRow->addWidget(scanBtn);
    scanRow->addStretch();

    QSettings settings;
    settings.beginGroup("FT817Scan");
    scanStartEdit->setText(settings.value("StartMHz").toString());
    scanEndEdit->setText(settings.value("EndMHz").toString());
    scanStepEdit->setText(settings.value("StepKHz").toString());
    scanResumeCombo->setCurrentIndex(qBound(0, settings.value("Resume", 1).toInt(), 2));
    scanWaitSpin->setValue(settings.value("WaitSec", 5).toInt());
    settings.endGroup();

    // Scan and bottom strips share larger text and controls (nearer ancestor
    // style sheet, so it wins over panelStyle's 11px buttons).
    auto *controls = new QWidget(this);
    controls->setObjectName("ft817Controls");
    controls->setStyleSheet(
        "#ft817Controls QLabel { font-size: 15px; }"
        "#ft817Controls QLineEdit, #ft817Controls QComboBox, #ft817Controls QSpinBox"
        " { font-size: 15px; min-height: 32px; }"
        "#ft817Controls QComboBox { border-radius: 8px; }"
        "#ft817Controls QPushButton { font-size: 15px; min-height: 36px; }");
    auto *controlsLayout = new QVBoxLayout(controls);
    controlsLayout->setContentsMargins(0, 4, 0, 0);
    controlsLayout->setSpacing(10);
    controlsLayout->addLayout(scanRow);
    controlsLayout->addLayout(bottom);
    root->addWidget(controls);


    resize(1180, 600);
}

void ft817Window::updateUi()
{
    st.softKeys.clear();
    for (int i = 0; i < 3; ++i)
        st.softKeys << QString::fromLatin1(softKeyRows[st.page][i]);
    lcd->setState(st);

    const bool open = isOpen();
    for (QPushButton *b : findChildren<QPushButton*>())
        if (b != disconnectBtn)
            b->setEnabled(open);
    freqInput->setEnabled(open);

    scanBtn->setText(scanning ? tr("スキャン停止") : tr("スキャン開始"));
    scanBtn->setStyleSheet(scanning ? "QPushButton { background: #c08000; color: black; }" : QString());
    for (QWidget *w : {static_cast<QWidget*>(scanStartEdit), static_cast<QWidget*>(scanEndEdit),
                       static_cast<QWidget*>(scanStepEdit), static_cast<QWidget*>(scanResumeCombo),
                       static_cast<QWidget*>(scanWaitSpin)})
        w->setEnabled(open && !scanning);

    pttBtn->setText(st.transmitting ? tr("受信に戻す") : tr("送信 (PTT)"));
    pttBtn->setStyleSheet(st.transmitting
        ? "QPushButton { background: #b01010; color: white; }"
        : "QPushButton { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #cfe9cf, stop:1 #8fc48f); color: black; }");
}

void ft817Window::applySoftKey(int key)
{
    const QString name = QString::fromLatin1(softKeyRows[st.page][key]);
    if (name == "A/B")
    {
        enqueue(Cat::set, cmd(opToggleVfo));
        // Read the new VFO back once the frequency readback guard (400 ms) has passed.
        QTimer::singleShot(450, this, [this]() {
            enqueue(Cat::readFreqMode, cmd(opReadFreqMode));
            enqueueEeprom(eepromVfo);
            enqueueEeprom(eepromBand);
        });
    }
    else if (name == "SPL")
        toggleSplit();
    else if (name == "CLAR")
        toggleClar();
    else if (name == "RPT")
        cycleRptShift();
    else if (name == "TON")
        cycleToneMode();
    else if (name == "SET")
        showSetDialog();
    else if (name == "NAR")
    {
        // Narrow toggle exists as a separate mode code for FM only (0x88).
        const quint8 base = modeCode & 0x7F;
        if (base == 0x08)
            setMode((modeCode & 0x80) ? 0x08 : 0x88);
        else
            appendLog(tr("NAR は FM モードでのみ CAT から切替できます"));
    }
    else if (name == "STEP")
    {
        static const int values[] = {10, 100, 1000, 2500, 5000, 10000, 12500, 25000, 100000};
        const int n = int(sizeof(values) / sizeof(values[0]));
        int next = values[0];
        for (int i = 0; i < n; ++i)
            if (values[i] == st.step) { next = values[(i + 1) % n]; break; }
        st.step = next;
        updateUi();
    }
}

void ft817Window::showSetDialog()
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("FT-817/818 設定 (CAT)"));
    auto *form = new QFormLayout(&dlg);

    auto *ctcss = new QComboBox(&dlg);
    for (int t : ctcssTones)
        ctcss->addItem(QString::number(t / 10.0, 'f', 1) + " Hz", t);
    ctcss->setCurrentIndex(qMax(0, ctcss->findData(ctcssTenths)));
    form->addRow(tr("CTCSS トーン"), ctcss);

    auto *dcs = new QComboBox(&dlg);
    for (int c : dcsCodes)
        dcs->addItem(QString("%1").arg(c, 3, 10, QChar('0')), c);
    dcs->setCurrentIndex(qMax(0, dcs->findData(dcsCode)));
    form->addRow(tr("DCS コード"), dcs);

    auto *offset = new QDoubleSpinBox(&dlg);
    offset->setRange(0.0, 99.99);
    offset->setDecimals(3);
    offset->setSuffix(" MHz");
    offset->setValue(rptOffsetHz / 1000000.0);
    form->addRow(tr("レピータ シフト幅"), offset);

    auto *clar = new QDoubleSpinBox(&dlg);
    clar->setRange(-9.99, 9.99);
    clar->setDecimals(2);
    clar->setSingleStep(0.01);
    clar->setSuffix(" kHz");
    clar->setValue(st.clarOffsetHz / 1000.0);
    form->addRow(tr("CLAR オフセット"), clar);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(buttons);

    if (dlg.exec() != QDialog::Accepted)
        return;
    if (ctcss->currentData().toInt() != ctcssTenths)
        setCtcssTone(ctcss->currentData().toInt());
    if (dcs->currentData().toInt() != dcsCode)
        setDcsCode(dcs->currentData().toInt());
    const qint64 newOffset = qRound64(offset->value() * 1000000.0);
    if (newOffset != rptOffsetHz)
        setRptOffset(newOffset);
    const int newClar = int(std::lround(clar->value() * 1000.0));
    if (newClar != st.clarOffsetHz)
        setClarOffset(newClar);
}

// --- CAT queue ------------------------------------------------------------------

QByteArray ft817Window::cmd(quint8 opcode, quint8 p1, quint8 p2, quint8 p3, quint8 p4)
{
    QByteArray c(5, '\0');
    c[0] = char(p1);
    c[1] = char(p2);
    c[2] = char(p3);
    c[3] = char(p4);
    c[4] = char(opcode);
    return c;
}

// Big-endian packed BCD, two digits per byte.
QByteArray ft817Window::bcd(qint64 value, int digits)
{
    const QString s = QString("%1").arg(value, digits, 10, QChar('0')).right(digits);
    QByteArray out;
    for (int i = 0; i < digits; i += 2)
        out.append(char((s.at(i).digitValue() << 4) | s.at(i + 1).digitValue()));
    return out;
}

QString ft817Window::formatFrequency(qint64 hz)
{
    return QString("%1.%2.%3")
        .arg(hz / 1000000)
        .arg((hz / 1000) % 1000, 3, 10, QChar('0'))
        .arg((hz / 10) % 100, 2, 10, QChar('0'));
}

QString ft817Window::stepTitle(int value)
{
    if (value >= 1000000) return QString("%1 MHz").arg(value / 1000000);
    if (value >= 1000) return QString("%1 kHz").arg(value / 1000.0);
    return QString("%1 Hz").arg(value);
}

void ft817Window::enqueue(Cat kind, const QByteArray& bytes, bool front, quint16 eepromAddr,
                          int recordOffset)
{
    if (!isOpen())
        return;

    catItem item;
    item.kind = kind;
    item.bytes = bytes;
    item.eepromAddr = eepromAddr;
    item.recordOffset = recordOffset;
    item.postDelayMs = 0;
    switch (kind)
    {
    case Cat::readFreqMode: item.expect = 5; item.timeoutMs = 1000; break;
    case Cat::readRx:
    case Cat::readTx: item.expect = 1; item.timeoutMs = 1000; break;
    case Cat::readTxMeter:
    case Cat::readEeprom: item.expect = 2; item.timeoutMs = 1000; break;
    case Cat::wake: item.expect = 0; item.timeoutMs = 0; item.postDelayMs = 200; break;
    default:
        // Set commands are answered with a one-byte ACK.  Not every firmware
        // sends it, so a missing ACK is not an error.
        item.expect = 1; item.timeoutMs = 250; break;
    }

    if (kind == Cat::set || kind == Cat::ptt || kind == Cat::wake)
        lastControlMs = QDateTime::currentMSecsSinceEpoch();

    // Coalesce knob/dial bursts: replace a still-queued setter of the same kind.
    if (kind == Cat::setFrequency || kind == Cat::setMode || kind == Cat::setClarOffset)
    {
        lastControlMs = QDateTime::currentMSecsSinceEpoch();
        for (catItem& q : queue)
        {
            if (q.kind == kind)
            {
                q.bytes = bytes;
                return;
            }
        }
    }

    if (front)
        queue.prepend(item);
    else
        queue.append(item);
    processQueue();
}

void ft817Window::processQueue()
{
    if (busy || queue.isEmpty() || !isOpen())
        return;
    current = queue.takeFirst();
    busy = true;
    rxBuffer.clear();
    serial->clear(QSerialPort::Input);

    if (current.kind != Cat::readFreqMode && current.kind != Cat::readRx && current.kind != Cat::readTx
        && current.kind != Cat::readTxMeter && current.kind != Cat::readEeprom)
        qDebug(logSystem()) << "FT-817/818: TX" << current.bytes.toHex(' ').toUpper();

    if (serial->write(current.bytes) != current.bytes.size())
    {
        appendLog(tr("CATコマンドを送信できませんでした。"));
        busy = false;
        return;
    }

    if (current.expect == 0)
        gapTimer->start(current.postDelayMs);
    else
        responseTimer->start(current.timeoutMs);
}

void ft817Window::readSerial()
{
    const QByteArray data = serial->readAll();
    if (!busy || current.expect == 0)
        return;     // unsolicited bytes (late ACKs) are dropped
    rxBuffer.append(data);
    if (rxBuffer.size() < current.expect)
        return;

    responseTimer->stop();
    const QByteArray packet = rxBuffer.left(current.expect);
    rxBuffer.clear();
    const catItem item = current;
    consecutiveTimeouts = 0;
    if (!st.catOk)
    {
        st.catOk = true;
        st.powerOff = false;
        appendLog(tr("CAT 応答を確認しました"));
    }
    handleResponse(item, packet);
    // Small gap so the radio is ready for the next command.
    gapTimer->start(5);
}

void ft817Window::responseTimeout()
{
    if (!busy)
        return;
    const bool isRead = current.kind == Cat::readFreqMode || current.kind == Cat::readRx
                        || current.kind == Cat::readTx || current.kind == Cat::readTxMeter
                        || current.kind == Cat::readEeprom;
    if (isRead)
    {
        if (++consecutiveTimeouts == 3)
        {
            appendLog(tr("CAT 応答なし（電源 OFF・ボーレート・ケーブルを確認してください）"));
            st.catOk = false;
            updateUi();
        }
        awaitingTransmit = false;
    }
    if (current.kind == Cat::readRx && scanAwaitingRx)
    {
        // Retry the scan's status read instead of stalling the scan.
        scanAwaitingRx = false;
        if (scanning)
            scanTimer->start(200);
    }
    busy = false;
    rxBuffer.clear();
    processQueue();
}

void ft817Window::handleResponse(const catItem& item, const QByteArray& data)
{
    const auto byte = [&data](int i) { return quint8(data.at(i)); };

    switch (item.kind)
    {
    case Cat::readFreqMode:
    {
        // Ignore readback while the user is still turning the dial.
        if (QDateTime::currentMSecsSinceEpoch() - lastControlMs < 400)
            break;
        qint64 value = 0;
        for (int i = 0; i < 4; ++i)
        {
            const int hi = byte(i) >> 4, lo = byte(i) & 0x0F;
            if (hi > 9 || lo > 9)
                return;
            value = value * 100 + hi * 10 + lo;
        }
        st.frequencyHz = value * 10;
        modeCode = byte(4);
        st.mode = modeTitle(modeCode);
        if ((modeCode & 0x7F) != 0x0A)
            st.digMode.clear();
        break;
    }
    case Cat::readRx:
        st.sMeter = qMin(15, int(byte(0) & 0x0F));
        st.busy = (byte(0) & 0x80) == 0;            // bit7 set = squelch closed
        st.codeMatched = (byte(0) & 0x40) == 0;
        st.discCentered = (byte(0) & 0x20) == 0;
        if (scanAwaitingRx)
        {
            scanAwaitingRx = false;
            scanHandleRx(st.busy);
        }
        break;
    case Cat::readTx:
    {
        const bool isTx = (byte(0) & 0x80) == 0;    // reads 0xFF while receiving
        st.transmitting = isTx;
        if (isTx)
        {
            st.po = byte(0) & 0x0F;
            st.hiSwr = (byte(0) & 0x40) != 0;
            awaitingTransmit = false;
            if (awaitingReceive)
                enqueue(Cat::ptt, cmd(opPttOff), true);
            else if (!safetyTimer->isActive())
                safetyTimer->start();
        }
        else
        {
            st.po = 0;
            st.hiSwr = false;
            awaitingTransmit = false;
            awaitingReceive = false;
            safetyTimer->stop();
        }
        break;
    }
    case Cat::readTxMeter:
        st.po = byte(0) >> 4;
        st.alc = byte(0) & 0x0F;
        st.swr = byte(1) >> 4;
        st.mod = byte(1) & 0x0F;
        break;
    case Cat::readEeprom:
    {
        const quint8 b = byte(0);
        if (item.recordOffset >= 0)
        {
            st.recordKnown = true;
            if (item.recordOffset == recRpt)
            {
                const int r = b >> 6;
                st.rptShift = r == 1 ? -1 : r == 2 ? 1 : r == 3 ? 2 : 0;
            }
            else if (item.recordOffset == recAttIpo)
            {
                st.att = (b & 0x10) != 0;
                st.ipo = (b & 0x20) != 0;
            }
            else if (item.recordOffset == recTone)
            {
                st.toneMode = b & 0x03;
            }
            break;
        }
        switch (item.eepromAddr)
        {
        case eepromVfo:
            st.vfoB = (b & 0x01) != 0;
            st.home = (b & 0x10) != 0;
            st.memMode = (b & 0x40) != 0;
            if (!st.memMode)
                st.memChannel.clear();
            break;
        case eepromFlags1:
        {
            static const char *agcNames[] = {"AUTO", "FAST", "SLOW", "OFF"};
            st.eepromKnown = true;
            st.agc = QString::fromLatin1(agcNames[b & 0x03]);
            st.nb = (b & 0x20) != 0;
            st.locked = (b & 0x40) == 0;
            st.fastTune = (b & 0x80) == 0;
            break;
        }
        case eepromFlags2:
        {
            static const char *meterNames[] = {"PWR", "ALC", "SWR", "MOD"};
            st.meterMode = QString::fromLatin1(meterNames[b & 0x03]);
            st.kyr = (b & 0x10) != 0;
            st.bk = (b & 0x20) != 0;
            st.vlt = (b & 0x40) != 0;
            st.vox = (b & 0x80) != 0;
            break;
        }
        case eepromBand:
            bandA = b & 0x0F;
            bandB = b >> 4;
            if (refreshRecordPending)
            {
                refreshRecordPending = false;
                enqueueRecordReads();
            }
            break;
        case eepromDigMode:
        {
            const int sub = b >> 5;
            st.digMode = sub < 5 ? QString::fromLatin1(digModeNames[sub]) : QString();
            break;
        }
        case eepromFlags3:
        {
            static const char *powerNames[] = {"HIGH", "L3", "L2", "L1"};
            st.txPower = QString::fromLatin1(powerNames[b & 0x03]);
            st.dw = (b & 0x08) != 0;
            st.pri = (b & 0x10) != 0;
            const int scan = (b >> 5) & 0x03;
            st.scan = scan == 2 ? 1 : scan == 3 ? -1 : 0;
            st.arts = (b & 0x80) != 0;
            break;
        }
        case eepromSplit:
        {
            if (!st.transmitting)
                st.split = (b & 0x80) != 0;
            // Antenna (front/rear) is stored per band group; normalise the
            // band number to the FT-818 numbering as hamlib does.
            int band = st.vfoB ? bandB : bandA;
            if (band >= 0)
            {
                if (!ft818 && band >= 2)
                    ++band;
                const int bit = band <= 9 ? 0 : qMin(6, band - 9);
                st.antenna = (b & (1 << bit)) ? "REAR" : "FRONT";
            }
            break;
        }
        case eepromCharger:
            st.charger = (b & 0x10) != 0;
            break;
        case eepromMemCh:
        {
            const int ch = b + 1;
            st.memChannel = ch == 201 ? "M-PL" : ch == 202 ? "M-PU" : QString("M-%1").arg(ch, 3, 10, QChar('0'));
            break;
        }
        default:
            break;
        }
        break;
    }
    case Cat::wake:
    case Cat::set:
    case Cat::setFrequency:
    case Cat::setMode:
    case Cat::setClarOffset:
    case Cat::ptt:
        break;
    }
    updateUi();
}

void ft817Window::poll()
{
    // The scan does its own status reads; regular polling would slow it down.
    if (scanning || busy || !queue.isEmpty() || !isOpen())
        return;
    if (QDateTime::currentMSecsSinceEpoch() - lastControlMs < 300)
        return;

    if (st.transmitting || awaitingTransmit || awaitingReceive)
    {
        if (pollPhase++ % 2 == 0)
            enqueue(Cat::readTx, cmd(opReadTx));
        else
            enqueue(Cat::readTxMeter, cmd(opReadTxMeter));
        return;
    }

    switch (pollPhase++ % 6)
    {
    case 0:
        enqueue(Cat::readFreqMode, cmd(opReadFreqMode)); break;
    case 1: case 3:
        enqueue(Cat::readRx, cmd(opReadRx)); break;
    case 4:
        enqueue(Cat::readTx, cmd(opReadTx)); break;
    case 2:
        // VFO A/B and memory mode every cycle, so a change on the radio shows quickly.
        enqueueEeprom(eepromVfo);
        break;
    case 5:
    {
        const auto reads = eepromReads();
        const auto& r = reads.at(eepromIndex++ % reads.size());
        enqueueEeprom(r.first, r.second);
        break;
    }
    }
}

// All read-only EEPROM locations that make up the status display.
QList<QPair<quint16, int>> ft817Window::eepromReads() const
{
    QList<QPair<quint16, int>> list = {
        {eepromVfo, -1}, {eepromFlags1, -1}, {eepromFlags2, -1}, {eepromBand, -1},
        {eepromFlags3, -1}, {eepromSplit, -1}, {eepromCharger, -1}
    };
    if ((modeCode & 0x7F) == 0x0A)
        list.append({eepromDigMode, -1});
    if (st.memMode)
        list.append({eepromMemCh, -1});
    const int band = st.vfoB ? bandB : bandA;
    if (!ft818 && !st.memMode && band >= 0 && band <= 14)
    {
        const quint16 base = (st.vfoB ? vfoBRecordBase : vfoARecordBase) + band * vfoRecordSize;
        list.append({quint16(base + recRpt), recRpt});
        list.append({quint16(base + recAttIpo), recAttIpo});
        list.append({quint16(base + recTone), recTone});
    }
    return list;
}

void ft817Window::enqueueEeprom(quint16 addr, int recordOffset)
{
    enqueue(Cat::readEeprom, cmd(opEepromRead, quint8(addr >> 8), quint8(addr & 0xFF)), false, addr,
            recordOffset);
}

void ft817Window::enqueueRecordReads()
{
    for (const auto& r : eepromReads())
        if (r.second >= 0)
            enqueueEeprom(r.first, r.second);
}

// Re-read everything at once, e.g. after settings were changed on the radio.
void ft817Window::refreshAll()
{
    if (!isOpen())
        return;
    appendLog(tr("表示を更新（全項目を読み直し）"));
    enqueue(Cat::readFreqMode, cmd(opReadFreqMode));
    enqueue(Cat::readRx, cmd(opReadRx));
    enqueue(Cat::readTx, cmd(opReadTx));
    // VFO / band first: the band-record locations depend on them.
    for (const auto& r : eepromReads())
        if (r.second < 0)
            enqueueEeprom(r.first);
    if ((modeCode & 0x7F) != 0x0A)
        enqueueEeprom(eepromDigMode);
    if (!st.memMode)
        enqueueEeprom(eepromMemCh);
    refreshRecordPending = true;
}

void ft817Window::appendLog(const QString& text)
{
    // Status and error messages go to the wfview log file.
    qInfo(logSystem()) << "FT-817/818:" << text;
}

// --- Operations --------------------------------------------------------------

void ft817Window::setFrequency(qint64 hz)
{
    hz = (hz / 10) * 10;
    if (hz < 100000 || hz > 470000000)
    {
        appendLog(tr("周波数範囲外です: %1").arg(formatFrequency(hz)));
        return;
    }
    if (scanning && !scanSetting)
        stopScan(tr("手動で周波数を変更したためスキャンを停止しました"));
    if (st.locked)
        appendLog(tr("LOCK 中のため本体側で周波数変更が拒否される場合があります"));
    st.frequencyHz = hz;
    enqueue(Cat::setFrequency, bcd(hz / 10, 8) + char(opSetFreq));
    updateUi();
}

void ft817Window::setMode(quint8 code)
{
    modeCode = code;
    st.mode = modeTitle(code);
    enqueue(Cat::setMode, cmd(opSetMode, code));
    updateUi();
}

void ft817Window::selectAdjacentMode(int direction)
{
    const quint8 current = modeCode & 0x7F;
    const int n = setModes().size();
    int index = 0;
    for (int i = 0; i < n; ++i)
        if (setModes().at(i).code == current) { index = i; break; }
    setMode(setModes().at((index + direction + n) % n).code);
}

void ft817Window::stepBand(int direction)
{
    const int n = int(sizeof(bandFrequencies) / sizeof(bandFrequencies[0]));
    // Cyclic: past the top band wraps to the bottom one and vice versa.
    qint64 target = direction > 0 ? bandFrequencies[0] : bandFrequencies[n - 1];
    if (direction > 0)
    {
        for (int i = 0; i < n; ++i)
            if (bandFrequencies[i] > st.frequencyHz) { target = bandFrequencies[i]; break; }
    }
    else
    {
        for (int i = n - 1; i >= 0; --i)
            if (bandFrequencies[i] < st.frequencyHz) { target = bandFrequencies[i]; break; }
    }
    setFrequency(target);
}

void ft817Window::setClarOffset(int hz)
{
    hz = qBound(-9990, (hz / 10) * 10, 9990);
    st.clarOffsetHz = hz;
    const QByteArray offs = bcd(std::abs(hz) / 10, 4);
    enqueue(Cat::setClarOffset, cmd(opClarFreq, hz < 0 ? 0xFF : 0x00, 0x00,
                                    quint8(offs.at(0)), quint8(offs.at(1))));
    updateUi();
}

void ft817Window::toggleClar()
{
    st.clar = !st.clar;
    enqueue(Cat::set, cmd(st.clar ? opClarOn : opClarOff));
    updateUi();
}

void ft817Window::toggleSplit()
{
    st.split = !st.split;
    enqueue(Cat::set, cmd(st.split ? opSplitOn : opSplitOff));
    updateUi();
}

void ft817Window::toggleLock()
{
    st.locked = !st.locked;
    enqueue(Cat::set, cmd(st.locked ? opLockOn : opLockOff));
    updateUi();
}

void ft817Window::cycleRptShift()
{
    // simplex -> minus -> plus -> simplex
    st.rptShift = st.rptShift == 0 ? -1 : st.rptShift == -1 ? 1 : 0;
    const quint8 p1 = st.rptShift < 0 ? 0x09 : st.rptShift > 0 ? 0x49 : 0x89;
    enqueue(Cat::set, cmd(opRptShift, p1));
    updateUi();
}

void ft817Window::cycleToneMode()
{
    // off -> TONE (encoder) -> TSQ (CTCSS) -> DCS -> off
    st.toneMode = (st.toneMode + 1) % 4;
    static const quint8 p1[] = {0x8A, 0x4A, 0x2A, 0x0A};
    enqueue(Cat::set, cmd(opToneMode, p1[st.toneMode]));
    updateUi();
}

void ft817Window::setCtcssTone(int tenthsHz)
{
    ctcssTenths = tenthsHz;
    // The radio uses one tone for TX and RX; both fields carry the same value.
    const QByteArray t = bcd(tenthsHz, 4);
    enqueue(Cat::set, cmd(opCtcssTone, quint8(t.at(0)), quint8(t.at(1)), quint8(t.at(0)), quint8(t.at(1))));
}

void ft817Window::setDcsCode(int code)
{
    dcsCode = code;
    const QByteArray c = bcd(code, 4);
    enqueue(Cat::set, cmd(opDcsCode, quint8(c.at(0)), quint8(c.at(1)), quint8(c.at(0)), quint8(c.at(1))));
}

void ft817Window::setRptOffset(qint64 hz)
{
    rptOffsetHz = hz;
    const QByteArray o = bcd(hz / 10, 8);
    enqueue(Cat::set, o + char(opRptOffset));
}

void ft817Window::powerOn()
{
    // The radio needs a dummy 5-byte wake-up before the power-on command.
    appendLog(tr("電源 ON を送信"));
    enqueue(Cat::wake, QByteArray(5, char(0xFF)));
    enqueue(Cat::set, cmd(opPowerOn));
    consecutiveTimeouts = 0;
}

void ft817Window::powerOff()
{
    const auto ret = QMessageBox::question(this, tr("FT-817/818"), tr("無線機の電源を OFF にしますか？"),
                                           QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (ret != QMessageBox::Yes)
        return;
    enqueue(Cat::set, cmd(opPowerOff));
    st.catOk = false;
    st.powerOff = true;
    updateUi();
}

void ft817Window::startTransmit()
{
    if (st.transmitting || awaitingTransmit || !isOpen())
        return;
    stopScan(tr("送信のためスキャンを停止しました"));
    awaitingTransmit = true;
    awaitingReceive = false;
    st.transmitting = true;
    enqueue(Cat::ptt, cmd(opPttOn), true);
    if (!safetyTimer->isActive())
        safetyTimer->start();
    updateUi();
}

void ft817Window::stopTransmit()
{
    if (!isOpen())
        return;
    awaitingTransmit = false;
    awaitingReceive = true;
    st.transmitting = false;
    safetyTimer->stop();
    enqueue(Cat::ptt, cmd(opPttOff), true);
    updateUi();
}

void ft817Window::safetyTimeout()
{
    stopTransmit();
    QMessageBox::warning(this, tr("FT-817/818"), tr("安全のため連続送信を3分で停止しました。"));
}

void ft817Window::sendPttOffNow()
{
    if (isOpen())
    {
        serial->write(cmd(opPttOff));
        serial->waitForBytesWritten(100);
    }
}

void ft817Window::startScan()
{
    const auto parse = [](QLineEdit *e, double scale, bool *ok) {
        QString text = e->text().trimmed();
        text.replace(',', '.');
        return qRound64(text.toDouble(ok) * scale);
    };
    bool okStart = false, okEnd = false, okStep = false;
    const qint64 startHz = (parse(scanStartEdit, 1000000.0, &okStart) / 10) * 10;
    const qint64 endHz = (parse(scanEndEdit, 1000000.0, &okEnd) / 10) * 10;
    const qint64 stepHz = parse(scanStepEdit, 1000.0, &okStep);
    if (!okStart || !okEnd || startHz < 100000 || endHz > 470000000 || startHz >= endHz)
    {
        QMessageBox::warning(this, tr("FT-817/818"),
                             tr("開始と終了の周波数をMHz単位で入力してください（開始 < 終了）。"));
        return;
    }
    if (!okStep || stepHz < 10 || stepHz % 10 != 0 || stepHz > endHz - startHz)
    {
        QMessageBox::warning(this, tr("FT-817/818"),
                             tr("ステップをkHz単位で入力してください（0.01 kHz 単位、範囲の幅以下）。"));
        return;
    }

    QSettings settings;
    settings.beginGroup("FT817Scan");
    settings.setValue("StartMHz", scanStartEdit->text().trimmed());
    settings.setValue("EndMHz", scanEndEdit->text().trimmed());
    settings.setValue("StepKHz", scanStepEdit->text().trimmed());
    settings.setValue("Resume", scanResumeCombo->currentIndex());
    settings.setValue("WaitSec", scanWaitSpin->value());
    settings.endGroup();

    scanStartHz = startHz;
    scanEndHz = endHz;
    scanStepHz = stepHz;
    scanning = true;
    scanHolding = false;
    scanAwaitingRx = false;
    appendLog(tr("スキャン開始: %1 - %2, ステップ %3 kHz")
                  .arg(formatFrequency(startHz), formatFrequency(endHz))
                  .arg(double(stepHz) / 1000.0));

    // Continue after the channel the scan last stopped on, if still tuned there.
    if (scanStoppedAtHz >= startHz && scanStoppedAtHz <= endHz && st.frequencyHz == scanStoppedAtHz)
    {
        scanFreqHz = scanStoppedAtHz;
        scanAdvance();
    }
    else
    {
        scanGoTo(startHz);
    }
    updateUi();
}

void ft817Window::stopScan(const QString& reason)
{
    if (!scanning)
        return;
    scanning = false;
    scanHolding = false;
    scanAwaitingRx = false;
    scanTimer->stop();
    scanStoppedAtHz = scanFreqHz;
    if (!reason.isEmpty())
        appendLog(reason);
    updateUi();
}

void ft817Window::scanGoTo(qint64 hz)
{
    scanFreqHz = hz;
    scanSetting = true;
    setFrequency(hz);
    scanSetting = false;
    // Let the receiver and squelch settle before reading the status.
    scanTimer->start(100);
}

void ft817Window::scanAdvance()
{
    qint64 next = scanFreqHz + scanStepHz;
    if (next > scanEndHz)
        next = scanStartHz;
    scanGoTo(next);
}

void ft817Window::scanReadRx()
{
    if (!scanning || !isOpen())
        return;
    scanAwaitingRx = true;
    enqueue(Cat::readRx, cmd(opReadRx));
}

void ft817Window::scanHandleRx(bool signal)
{
    if (!scanning)
        return;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const auto resume = ScanResume(scanResumeCombo->currentData().toInt());

    if (!scanHolding)
    {
        if (!signal)
        {
            scanAdvance();
            return;
        }
        appendLog(tr("信号を検出: %1 (S%2)").arg(formatFrequency(scanFreqHz)).arg(st.sMeter));
        if (resume == ScanResume::stay)
        {
            stopScan(tr("信号を検出したためスキャンを停止しました"));
            return;
        }
        scanHolding = true;
        scanHoldSinceMs = now;
        scanLastSignalMs = now;
        scanTimer->start(200);
        return;
    }

    if (signal)
        scanLastSignalMs = now;
    const qint64 waitMs = qint64(scanWaitSpin->value()) * 1000;
    const bool resumeNow = resume == ScanResume::afterTime
        ? now - scanHoldSinceMs >= waitMs
        : !signal && now - scanLastSignalMs >= waitMs;
    if (resumeNow)
    {
        scanHolding = false;
        scanAdvance();
    }
    else
    {
        scanTimer->start(200);
    }
}
