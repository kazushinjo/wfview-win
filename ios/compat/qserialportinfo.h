// iOS compatibility stub for QSerialPortInfo.
// Reports zero available ports (there are none on iOS).

#ifndef WFVIEW_IOS_QSERIALPORTINFO_STUB_H
#define WFVIEW_IOS_QSERIALPORTINFO_STUB_H

#include <QList>
#include <QString>
#include "qserialport.h"

class QSerialPortInfo
{
public:
    QSerialPortInfo() = default;
    explicit QSerialPortInfo(const QString &name) : m_portName(name) {}
    explicit QSerialPortInfo(const QSerialPort &) {}

    QString portName() const { return m_portName; }
    QString systemLocation() const { return QString(); }
    QString description() const { return QString(); }
    QString manufacturer() const { return QString(); }
    QString serialNumber() const { return QString(); }

    quint16 vendorIdentifier() const { return 0; }
    quint16 productIdentifier() const { return 0; }
    bool hasVendorIdentifier() const { return false; }
    bool hasProductIdentifier() const { return false; }

    bool isNull() const { return m_portName.isEmpty(); }
    bool isBusy() const { return false; }

    static QList<QSerialPortInfo> availablePorts() { return QList<QSerialPortInfo>(); }
    static QList<qint32> standardBaudRates() { return QList<qint32>(); }

private:
    QString m_portName;
};

#endif // WFVIEW_IOS_QSERIALPORTINFO_STUB_H
