// iOS compatibility stub for QtSerialPort / QSerialPort.
//
// iOS (iPadOS) provides no serial/USB backend and Qt ships no qtserialport
// module for the ios kit. wfview references QSerialPort from several files
// (commhandler, pttyhandler, kenwoodcommander, yaesucommander, ...). Rather
// than #ifdef every one of them, this header provides a minimal, API-compatible
// QSerialPort that is a QIODevice subclass. It compiles and links, but every
// port fails to open, so serial ("USB") connections are simply unavailable on
// iOS. Network (Icom LAN / wfserver) operation is unaffected.
//
// Only the API surface actually used by wfview is implemented.

#ifndef WFVIEW_IOS_QSERIALPORT_STUB_H
#define WFVIEW_IOS_QSERIALPORT_STUB_H

#include <QIODevice>
#include <QString>

class QSerialPort : public QIODevice
{
    Q_OBJECT
public:
    enum Direction { Input = 1, Output = 2, AllDirections = Input | Output };
    Q_DECLARE_FLAGS(Directions, Direction)

    enum BaudRate {
        Baud1200 = 1200, Baud2400 = 2400, Baud4800 = 4800, Baud9600 = 9600,
        Baud19200 = 19200, Baud38400 = 38400, Baud57600 = 57600,
        Baud115200 = 115200, UnknownBaud = -1
    };

    enum DataBits {
        Data5 = 5, Data6 = 6, Data7 = 7, Data8 = 8, UnknownDataBits = -1
    };

    enum Parity {
        NoParity = 0, EvenParity = 2, OddParity = 3, SpaceParity = 4,
        MarkParity = 5, UnknownParity = -1
    };

    enum StopBits {
        OneStop = 1, OneAndHalfStop = 3, TwoStop = 2, UnknownStopBits = -1
    };

    enum FlowControl {
        NoFlowControl = 0, HardwareControl, SoftwareControl, UnknownFlowControl = -1
    };

    enum PinoutSignal {
        NoSignal = 0x00, DataTerminalReadySignal = 0x04,
        DataCarrierDetectSignal = 0x08, DataSetReadySignal = 0x10,
        RingIndicatorSignal = 0x20, RequestToSendSignal = 0x40,
        ClearToSendSignal = 0x80
    };
    Q_DECLARE_FLAGS(PinoutSignals, PinoutSignal)

    enum SerialPortError {
        NoError = 0, DeviceNotFoundError, PermissionError, OpenError,
        ParityError, FramingError, BreakConditionError, WriteError, ReadError,
        ResourceError, UnsupportedOperationError, UnknownError, TimeoutError,
        NotOpenError
    };

    explicit QSerialPort(QObject *parent = nullptr) : QIODevice(parent) {}
    explicit QSerialPort(const QString &name, QObject *parent = nullptr)
        : QIODevice(parent), m_portName(name) {}
    ~QSerialPort() override { if (isOpen()) close(); }

    void setPortName(const QString &name) { m_portName = name; }
    QString portName() const { return m_portName; }

    bool setBaudRate(qint32 baudRate, Directions = AllDirections) { m_baudRate = baudRate; return true; }
    qint32 baudRate(Directions = AllDirections) const { return m_baudRate; }

    bool setDataBits(DataBits d) { m_dataBits = d; return true; }
    DataBits dataBits() const { return m_dataBits; }

    bool setParity(Parity p) { m_parity = p; return true; }
    Parity parity() const { return m_parity; }

    bool setStopBits(StopBits s) { m_stopBits = s; return true; }
    StopBits stopBits() const { return m_stopBits; }

    bool setFlowControl(FlowControl f) { m_flowControl = f; return true; }
    FlowControl flowControl() const { return m_flowControl; }

    bool setDataTerminalReady(bool set) { m_dtr = set; return false; }
    bool isDataTerminalReady() const { return m_dtr; }

    bool setRequestToSend(bool set) { m_rts = set; return false; }
    bool isRequestToSend() const { return m_rts; }

    PinoutSignals pinoutSignals() const { return NoSignal; }

    bool setReadBufferSize(qint64 size) { m_readBufferSize = size; return true; }
    qint64 readBufferSize() const { return m_readBufferSize; }

    SerialPortError error() const { return m_error; }
    void clearError() { m_error = NoError; }

    bool clear(Directions = AllDirections) { return true; }
    bool flush() { return false; }

    // QIODevice overrides: no serial hardware exists on iOS, so opening fails.
    bool open(OpenMode mode) override
    {
        Q_UNUSED(mode)
        m_error = OpenError;
        emit errorOccurred(m_error);
        emit error(m_error);
        return false;
    }
    void close() override { QIODevice::close(); }

    bool isSequential() const override { return true; }

signals:
    void errorOccurred(QSerialPort::SerialPortError error);
    // Deprecated Qt5-era signal name; some code connects via SIGNAL(error(...)).
    void error(QSerialPort::SerialPortError error);

protected:
    qint64 readData(char *, qint64) override { return -1; }
    qint64 writeData(const char *, qint64) override { return -1; }

private:
    QString m_portName;
    qint32 m_baudRate = Baud9600;
    DataBits m_dataBits = Data8;
    Parity m_parity = NoParity;
    StopBits m_stopBits = OneStop;
    FlowControl m_flowControl = NoFlowControl;
    bool m_dtr = false;
    bool m_rts = false;
    qint64 m_readBufferSize = 0;
    SerialPortError m_error = NoError;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QSerialPort::Directions)
Q_DECLARE_OPERATORS_FOR_FLAGS(QSerialPort::PinoutSignals)

#endif // WFVIEW_IOS_QSERIALPORT_STUB_H
