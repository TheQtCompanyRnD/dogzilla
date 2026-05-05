// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
#include "lidar.h"
#include <QDateTime>
#include <QDebug>
#include <QJsonArray>
#include <QLoggingCategory>

Q_STATIC_LOGGING_CATEGORY(lcLdr, "dogzilla.lidar")
Q_STATIC_LOGGING_CATEGORY(lcLdrd, "dogzilla.lidar.data")

namespace {
// CRC-8 table from MS200 user manual, page 11
constexpr uint8_t CrcTable[256] = {
    0x00, 0x4d, 0x9a, 0xd7, 0x79, 0x34, 0xe3, 0xae, 0xf2, 0xbf, 0x68, 0x25,
    0x8b, 0xc6, 0x11, 0x5c, 0xa9, 0xe4, 0x33, 0x7e, 0xd0, 0x9d, 0x4a, 0x07,
    0x5b, 0x16, 0xc1, 0x8c, 0x22, 0x6f, 0xb8, 0xf5, 0x1f, 0x52, 0x85, 0xc8,
    0x66, 0x2b, 0xfc, 0xb1, 0xed, 0xa0, 0x77, 0x3a, 0x94, 0xd9, 0x0e, 0x43,
    0xb6, 0xfb, 0x2c, 0x61, 0xcf, 0x82, 0x55, 0x18, 0x44, 0x09, 0xde, 0x93,
    0x3d, 0x70, 0xa7, 0xea, 0x3e, 0x73, 0xa4, 0xe9, 0x47, 0x0a, 0xdd, 0x90,
    0xcc, 0x81, 0x56, 0x1b, 0xb5, 0xf8, 0x2f, 0x62, 0x97, 0xda, 0x0d, 0x40,
    0xee, 0xa3, 0x74, 0x39, 0x65, 0x28, 0xff, 0xb2, 0x1c, 0x51, 0x86, 0xcb,
    0x21, 0x6c, 0xbb, 0xf6, 0x58, 0x15, 0xc2, 0x8f, 0xd3, 0x9e, 0x49, 0x04,
    0xaa, 0xe7, 0x30, 0x7d, 0x88, 0xc5, 0x12, 0x5f, 0xf1, 0xbc, 0x6b, 0x26,
    0x7a, 0x37, 0xe0, 0xad, 0x03, 0x4e, 0x99, 0xd4, 0x7c, 0x31, 0xe6, 0xab,
    0x05, 0x48, 0x9f, 0xd2, 0x8e, 0xc3, 0x14, 0x59, 0xf7, 0xba, 0x6d, 0x20,
    0xd5, 0x98, 0x4f, 0x02, 0xac, 0xe1, 0x36, 0x7b, 0x27, 0x6a, 0xbd, 0xf0,
    0x5e, 0x13, 0xc4, 0x89, 0x63, 0x2e, 0xf9, 0xb4, 0x1a, 0x57, 0x80, 0xcd,
    0x91, 0xdc, 0x0b, 0x46, 0xe8, 0xa5, 0x72, 0x3f, 0xca, 0x87, 0x50, 0x1d,
    0xb3, 0xfe, 0x29, 0x64, 0x38, 0x75, 0xa2, 0xef, 0x41, 0x0c, 0xdb, 0x96,
    0x42, 0x0f, 0xd8, 0x95, 0x3b, 0x76, 0xa1, 0xec, 0xb0, 0xfd, 0x2a, 0x67,
    0xc9, 0x84, 0x53, 0x1e, 0xeb, 0xa6, 0x71, 0x3c, 0x92, 0xdf, 0x08, 0x45,
    0x19, 0x54, 0x83, 0xce, 0x60, 0x2d, 0xfa, 0xb7, 0x5d, 0x10, 0xc7, 0x8a,
    0x24, 0x69, 0xbe, 0xf3, 0xaf, 0xe2, 0x35, 0x78, 0xd6, 0x9b, 0x4c, 0x01,
    0xf4, 0xb9, 0x6e, 0x23, 0x8d, 0xc0, 0x17, 0x5a, 0x06, 0x4b, 0x9c, 0xd1,
    0x7f, 0x32, 0xe5, 0xa8,
};

uint8_t crc8(const uint8_t *data, int len)
{
    uint8_t crc = 0;
    for (int i = 0; i < len; ++i)
        crc = CrcTable[(crc ^ data[i]) & 0xff];
    return crc;
}

using DistanceAndIntensity = struct __attribute__((packed)) {
    uint16_t distance;
    uint8_t intensity;
};
static_assert(sizeof(DistanceAndIntensity) == 3);

qreal degreeHundredthsToRadians(int hundredths)
{
    return M_PI * hundredths / 18000;
}
} // namespace

Lidar::Lidar(QObject * parent)
  : QObject(parent)
{
    connect(&m_port, &QSerialPort::errorOccurred, this, &Lidar::onError);
    connect(&m_port, &QIODevice::readyRead, this, &Lidar::readAndHandle);
}

Lidar::~Lidar() { }

void Lidar::setSerialPort(const QString &path)
{
    m_serialPort = path;
    // maybeOpenSerialPort();
    setRunning(true); // start sending data
    emit serialPortChanged();
}

bool Lidar::maybeOpenSerialPort()
{
    if (!m_port.isWritable() && !m_serialPort.isEmpty() && m_baudRate > 0) {
        m_port.setPortName(m_serialPort);
        m_port.setBaudRate(m_baudRate);
        const bool success = m_port.open(QIODevice::ReadWrite);
        qCDebug(lcLdr) << m_port.portName() << m_port.baudRate() << "opened successfully?" << success;
        return success;
    }
    return m_port.isWritable();
}

void Lidar::emitScanData()
{
    QJsonObject data;
    {
        qint64 msecs = QDateTime::currentMSecsSinceEpoch();
        QJsonObject header;
        QJsonObject stamp;
        stamp.insert("sec", msecs / qint64(1000));
        stamp.insert("nanosec", msecs % qint64(1000) * 1000000);
        header.insert("stamp", stamp);
        header.insert("frameId", "laser_frame");
        data.insert("header", header);
    }
    data.insert("angleMin", 0); // m_minAngle); // avoid jitter?
    data.insert("angleMax", 2 * M_PI); //m_maxAngle);
    data.insert("angleIncrement", m_datumAngleDelta);
    data.insert("timeIncrement", m_datumTimeDelta);
    data.insert("scanTime", 360 / m_speed); // full rev
    data.insert("rangeMin", m_rangeMin);
    data.insert("rangeMax", m_rangeMax);
    {
        QJsonArray ja;
        std::copy(m_scanRanges.begin(), m_scanRanges.end(), std::back_inserter(ja));
        data.insert("ranges", ja);
    }
    {
        QJsonArray ja;
        std::copy(m_scanIntensities.begin(), m_scanIntensities.end(), std::back_inserter(ja));
        data.insert("intensities", ja);
    }
    emit sectorScanned(data);
}

void Lidar::onError(QSerialPort::SerialPortError err)
{
    qCWarning(lcLdr) << err;
}

void Lidar::readAndHandle()
{
    while (m_port.bytesAvailable() > 0) {
        QByteArray prefix = m_port.peek(4);
        if (prefix.size() < 4)
            return; // wait for more

        switch (uint8_t(prefix.at(0))) {
        case 0x54: { // point cloud data, fixed 47 bytes
            if (m_port.bytesAvailable() < 47)
                return;
            QByteArray buf = m_port.peek(47);
            const uint8_t len = uint8_t(buf.at(1)) & 0x1F;
            if (len != 12) {
                qCWarning(lcLdr) << "point cloud unexpected count" << len << "resyncing";
                m_port.read(1);
                continue;
            }
            const uint8_t expectedCrc = crc8(reinterpret_cast<const uint8_t *>(buf.constData()), 10 + 3 * len);
            const uint8_t actualCrc = uint8_t(buf.at(10 + 3 * len));
            if (expectedCrc != actualCrc) {
                qCWarning(lcLdr) << "point cloud CRC mismatch: got" << Qt::hex << actualCrc << "expected" << expectedCrc << buf.toHex();
                m_port.read(1);
                continue;
            }
            m_port.read(47);
            const uint16_t *nums = reinterpret_cast<const uint16_t *>(buf.constData());
            m_speed = nums[1];                    // deg / sec
            const uint16_t startAngle = nums[2];  // * 0.01 deg
            const uint16_t endAngle = nums[(6 + 3 * len) / 2];
            int angleRange = endAngle - startAngle;
            if (angleRange < 0)
                angleRange += 36000;
            const int datumAngleDelta = angleRange / (len - 1); // end angle is inclusive
            m_datumAngleDelta = degreeHundredthsToRadians(datumAngleDelta);
            // time increment: angle sweep (deg) / speed (deg/sec)
            m_datumTimeDelta = datumAngleDelta / qreal(100) / m_speed;
            const uint16_t timestamp = nums[(8 + 3 * len) / 2];
            qCDebug(lcLdrd) << buf.toHex() << "len" << buf.size() << len << "speed" << m_speed
                            << "startAngle" << startAngle << "endAngle" << endAngle << "timestamp" << timestamp;
            bool angleWraparound = false;
            const int newSize = m_insertIndex + len;
            if (m_scanAngles.size() < newSize)
                m_scanAngles.resize(newSize);
            if (m_scanRanges.size() < newSize)
                m_scanRanges.resize(newSize);
            if (m_scanIntensities.size() < newSize)
                m_scanIntensities.resize(newSize);
            for (int i = 0; i < len; ++i) {
                const int byteOffset = (6 + 3 * i);
                const DistanceAndIntensity *di = reinterpret_cast<const DistanceAndIntensity *>(buf.constData() + byteOffset);
                int interpAngle = startAngle + datumAngleDelta * i;
                if (interpAngle < 0)
                    interpAngle += 36000;
                else if (interpAngle > 36000) {
                    qCDebug(lcLdrd) << "angle wraparound" << interpAngle;
                    interpAngle -= 36000;
                    if (!angleWraparound) {
                        if (m_insertIndex > 0)
                            m_maxAngle = m_scanAngles.at(m_insertIndex - 1);
                        m_insertIndex = 0;
                        m_minAngle = degreeHundredthsToRadians(interpAngle);
                        angleWraparound = true;
                    }
                }

                // if (di[i].intensity < 16) {
                //     // called a "reserved value" in the MS200 manual : invalid data point
                //     ranges.append(std::numeric_limits<float>::quiet_NaN());
                //     intensities.append(std::numeric_limits<float>::quiet_NaN());
                // } else {
                //     ranges.append(di[i].distance / 1000.0f);   // mm to meters
                //     intensities.append(float(di[i].intensity));
                // }

                m_scanAngles[m_insertIndex] = degreeHundredthsToRadians(interpAngle);
                m_scanRanges[m_insertIndex] = di->distance / 1000.0f; // mm to meters
                m_scanIntensities[m_insertIndex] = di->intensity;
                qCDebug(lcLdrd) << i << "boff" << byteOffset << m_insertIndex << "angle" << interpAngle
                                << "dist" << di->distance << "intens" << di->intensity;
                // intensity range 0 - 255; < 16 is a reserved value; distance in mm
                ++m_insertIndex;
            }
            if (angleWraparound)
                emitScanData();
            break;
        }
        case 0xAA: { // SN code or info packet (header 0x55AA on wire is little-endian: aa 55)
            if (uint8_t(prefix.at(1)) != 0x55) {
                m_port.read(1);
                continue;
            }
            const uint8_t flag = uint8_t(prefix.at(2));
            const uint8_t len = uint8_t(prefix.at(3));
            const int total = 7 + len; // 2 header + 1 flag + 1 len + N data + 1 crc + 2 tail
            if (m_port.bytesAvailable() < total)
                return;
            QByteArray buf = m_port.peek(total);
            if (uint8_t(buf.at(total - 2)) != 0x31 || uint8_t(buf.at(total - 1)) != 0xF2) {
                qCWarning(lcLdr) << "SN packet bad tail" << buf.toHex();
                m_port.read(1);
                continue;
            }
            const uint8_t expectedCrc = crc8(reinterpret_cast<const uint8_t *>(buf.constData()), 4 + len);
            const uint8_t actualCrc = uint8_t(buf.at(4 + len));
            if (expectedCrc != actualCrc) {
                qCWarning(lcLdr) << "SN packet CRC mismatch: got" << Qt::hex << actualCrc << "expected" << expectedCrc << buf.toHex();
                m_port.read(1);
                continue;
            }
            m_port.read(total);
            switch (flag) {
            case 0x01:
                m_serialNumber = QString::fromLatin1(buf.mid(4, len));
                qCDebug(lcLdr) << "got serial number" << m_serialNumber;
                emit serialNumberChanged();
                break;
            case 0x02:
            case 0x03: {
                // payload is a sequence of length-prefixed Latin1 strings:
                // <len> <model> <len> <firmware> [opaque trailing bytes]
                QStringList parts;
                int pos = 4;
                const int end = 4 + len;
                for (int field = 0; field < 2 && pos < end; ++field) {
                    const uint8_t fieldLen = uint8_t(buf.at(pos++));
                    if (pos + fieldLen > end)
                        break;
                    parts << QString::fromLatin1(buf.mid(pos, fieldLen));
                    pos += fieldLen;
                }
                m_hardwareModel = parts.join(QLatin1Char(' '));
                qCDebug(lcLdr) << "got hardware model" << m_hardwareModel;
                emit hardwareModelChanged();
                break;
            }
            default:
                qCDebug(lcLdr) << "unhandled info packet flag" << Qt::hex << flag << buf.toHex();
                break;
            }
            break; // done with aa 55 packet
        }
        case 0xA5: { // WRITE_PARAM response: a5 f5 a2 c2 01 <data> <xor> 31 f2 (9 bytes)
            if (uint8_t(prefix.at(1)) != 0xF5) {
                m_port.read(1);
                continue;
            }
            if (m_port.bytesAvailable() < 9)
                return;
            QByteArray buf = m_port.peek(9);
            if (uint8_t(buf.at(7)) != 0x31 || uint8_t(buf.at(8)) != 0xF2) {
                m_port.read(1);
                continue;
            }
            m_port.read(9);
            qCDebug(lcLdr) << "command response" << buf.toHex();
            break;
        }
        default:
            m_port.read(1); // unknown byte; resync
            break;
        }
    }
}

void Lidar::setRunning(bool r)
{
    if (m_running == r)
        return;
    qCDebug(lcLdr) << m_running << "->" << r;

    if (maybeOpenSerialPort()) {
        const QByteArray data = r ? "\xa5\xf5\xa2\xc1\x01\x81\xb3\x31\xf2"
                                  : "\xa5\xf5\xa2\xc1\x01\x80\xb2\x31\xf2";
        qCDebug(lcLdrd) << m_port.portName() << m_port.baudRate() << data.toHex();
        const auto len = m_port.write(data);
        if (len == 9) {
            m_running = r;
            emit runningChanged();
        } else {
            qCWarning(lcLdr) << "short write" << len;
        }
    }
    qCDebug(lcLdr) << m_running;
}
