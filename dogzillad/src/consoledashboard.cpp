#include "consoledashboard.h"
#include <iostream>
#include <QByteArray>
#include <QDateTime>
#include <QDirListing>
#include <QFile>
#include <QLoggingCategory>
#include <QNetworkInterface>
#include <QTimerEvent>

Q_STATIC_LOGGING_CATEGORY(lcCon, "dogzilla.console")

ConsoleDashboard::ConsoleDashboard(QObject *parent)
    : QObject{parent}
{
    using F = QDirListing::IteratorFlag;
    const auto flags = F::FilesOnly | F::Recursive | F::ResolveSymlinks;
    for (const auto &dirEntry : QDirListing("/sys", flags)) {
        const QString fileName = dirEntry.fileName();
        if (fileName.contains("battery_voltage")) {
            m_batteryVoltageFile.setFileName(dirEntry.filePath());
            break;
        }
    }

    /*bool ok = */ QNetworkInformation::loadDefaultBackend();
    m_networkInfo = QNetworkInformation::instance();
    if (m_networkInfo) {
        QObject::connect(m_networkInfo, &QNetworkInformation::reachabilityChanged,
                         this, &ConsoleDashboard::onReachabilityChanged);
        onReachabilityChanged(m_networkInfo->reachability());
    } else {
        qWarning() << "no network info for you";
        onReachabilityChanged(QNetworkInformation::Reachability::Unknown);
    }

    m_timerId = startTimer(1000);
}

void ConsoleDashboard::timerEvent(QTimerEvent *ev)
{
    if (ev->timerId() == m_timerId)
        update();
}

// Read cumulative byte counters for a specific interface
void ConsoleDashboard::readInterfaceBytes(const QString &ifaceName, quint64 &rx, quint64 &tx)
{
    QFile rxFile(QString("/sys/class/net/%1/statistics/rx_bytes").arg(ifaceName));
    QFile txFile(QString("/sys/class/net/%1/statistics/tx_bytes").arg(ifaceName));

    rx = 0;
    tx = 0;

    if (rxFile.open(QFile::ReadOnly | QFile::Text)) {
        rx = rxFile.readLine().trimmed().toULongLong();
        rxFile.close();
    }
    if (txFile.open(QFile::ReadOnly | QFile::Text)) {
        tx = txFile.readLine().trimmed().toULongLong();
        txFile.close();
    }
}

// Calculate bandwidth from previous counter values, assuming this is called once per second
void ConsoleDashboard::calculateBandwidth(quint64 currentRx, quint64 currentTx,
                                          quint64 prevRx, quint64 prevTx,
                                          quint64 &rxBandwidth, quint64 &txBandwidth)
{
    // Handle counter wraparound: treat as new baseline
    if (currentRx < prevRx)
        currentRx = 0;
    if (currentTx < prevTx)
        currentTx = 0;

    rxBandwidth = currentRx - prevRx;
    txBandwidth = currentTx - prevTx;
}

void ConsoleDashboard::onReachabilityChanged(QNetworkInformation::Reachability r)
{
    qCDebug(lcCon) << r;
    update();
}

int ConsoleDashboard::cpuPercent()
{
    QFile f("/proc/stat");
    if (f.open(QFile::ReadOnly)) {
        QByteArray buf = f.readLine(128);
        auto fields = buf.split(' ');
        if (fields.size() < 9)
            return 0;
        // QList("cpu", "", "3957412", "137559", "3074696", "85788478", "19465", "0", "6445", "0", "0", "0\n")
        // 0cpuid: number of cpu
        // 1user: normal processes executing in user mode
        // 2nice: niced processes executing in user mode
        // 3system: processes executing in kernel mode
        // 4idle: twiddling thumbs
        // 5iowait: waiting for I/O to complete
        // 6irq: servicing interrupts
        // 7softirq: servicing softirqs
        const int user = fields.at(2).toInt();
        const int nice = fields.at(3).toInt();
        const int sys = fields.at(4).toInt();
        const int idle = fields.at(5).toInt() + fields.at(6).toInt(); // idle + iowait
        const int irq = fields.at(7).toInt();
        const int softirq = fields.at(8).toInt();

        int busy = user + nice + sys + irq + softirq;
        int total = busy + idle;
        float percent = ((total - m_prevCpuTotal) - (idle - m_prevCpuIdle)) * 100.0 / (total - m_prevCpuTotal);
        // qDebug() << fields;
        // qDebug() << "user" << user << "nice" << nice << "sys" << sys << "idle" << idle << "irq" << irq << "softirq" << softirq;
        // qDebug() << busy << m_prevCpuIdle << "->" << idle
        //          << m_prevCpuTotal << "->" << total
        //          << ((total - m_prevCpuTotal) - (idle - m_prevCpuIdle)) << (total - m_prevCpuTotal)
        //          << percent;

        m_prevCpuIdle = idle;
        m_prevCpuTotal = total;

        return roundf(percent);
    }
    return 0;
}

QString ConsoleDashboard::readBatteryVoltage()
{
    const bool opened = m_batteryVoltageFile.open(QFile::ReadOnly);
    if (!opened) {
        qCWarning(lcCon) << "failed to open" << m_batteryVoltageFile.fileName();
        return {};
    }
    QByteArray whole = m_batteryVoltageFile.readAll();
    m_batteryVoltageFile.close();
    if (whole.length() < 4)
        return {};
    whole.chop(1); // newline
    QByteArray fraction = whole.last(3);
    whole.chop(3);
    m_batteryVoltage = whole.toFloat() + fraction.toFloat() / 1000.0f;
    // qDebug() << whole << '.' << fraction << ret << 'V';
    return QString::number(m_batteryVoltage, 'f', 1);
}

std::string ConsoleDashboard::batteryBars()
{
    // the set of symbol combinations gives us 7 levels
    if (m_batteryLevel > 86)
        return "[║║}";
    if (m_batteryLevel > 71)
        return "[║│}";
    if (m_batteryLevel > 57)
        return "[║·}";
    if (m_batteryLevel > 43)
        return "[║ }";
    if (m_batteryLevel > 29)
        return "[│ }";
    if (m_batteryLevel > 14)
        return "[· }";
    return "[  }";
}

void ConsoleDashboard::update()
{
    const auto now = QDateTime::currentDateTime();
    // clear the screen every 10th update; otherwise overwrite to limit flicker
    out << (m_updateCount % 10 ? "\033[H" : "\033c") << now.toString("hh:mm:ss ").toStdString()
            << batteryBars() << m_batteryLevel << "%" << std::endl;

    int row = 1;
    for (const auto &iface : QNetworkInterface::allInterfaces()) {
        // qDebug() << iface << iface.type();
        if (iface.name().startsWith("docker") || iface.name() == "lo")
            continue;
        bool output = false;
        for (const auto &addr : iface.addressEntries()) {
            const auto ip = addr.ip();
            if (ip.isLoopback())
                continue;
            bool isIPV4 = false;
            quint32 ipv4 = ip.toIPv4Address(&isIPV4);
            if (isIPV4) {
                quint64 rxBps = 0, txBps = 0;

                // Read current byte counters
                quint64 curRx, curTx;
                readInterfaceBytes(iface.name(), curRx, curTx);

                // Calculate bandwidth using helper function
                if (m_prevRxBytes.contains(iface.name())) {
                    calculateBandwidth(curRx, curTx,
                                       m_prevRxBytes[iface.name()],
                                       m_prevTxBytes[iface.name()],
                                       rxBps, txBps);
                }

                // Update previous counters for next iteration
                m_prevRxBytes[iface.name()] = curRx;
                m_prevTxBytes[iface.name()] = curTx;

                // Format bandwidth (KB/s or MB/s)
                QString bandwidthStr;
                if (rxBps < 1048576 && txBps < 1048576) {
                    // unfortunately we don't have space for both RX and TX
                    //~ bandwidthStr = QString("R%1K T%2K").arg(rxBps / 1024).arg(txBps / 1024);
                    bandwidthStr = QString("K%1").arg(txBps / 1024);
                } else {
                    bandwidthStr = QString("M%1").arg(txBps / 1048576);
                }

                qCDebug(lcCon) << iface.name() << iface.type() << addr.ip() << "RX" << rxBps << "B/s, TX" << txBps;
                //          << "perm/lk/temp?" << addr.isPermanent() << addr.isLifetimeKnown() << addr.isTemporary()
                //          << "ip flags" << ip.isBroadcast() << ip.isGlobal() << ip.isLinkLocal() << ip.isLoopback()
                //             << ip.isMulticast() << ip.isPrivateUse() << ip.isSiteLocal() << ip.isUniqueLocalUnicast();
                out << iface.name().first(1).toStdString() << ip.toString().toStdString()
                    << bandwidthStr.toStdString() << std::endl;
                output = true;
                ++row;
            }
        }
        if (!output)
            out << iface.name().toStdString() << " ?  " << std::endl;
        if (row >= 4)
            break;
    }
    if (row < 4)
        out << readBatteryVoltage().toStdString() << "V   cpu " << cpuPercent() << "%";
    ++m_updateCount;
}

/*!
    The TTY device on which to output the dashboard.
    If not set, defaults to stdout.
*/
void ConsoleDashboard::setTty(const QString &tty)
{
    if (m_tty == tty)
        return;
    m_tty = tty;
    std::ofstream newStream(tty.toStdString());
    std::swap(out, newStream);
    emit ttyChanged();
}

/*!
    Current battery level to display, in percent of full charge.
    This is passed in rather than read directly, because
    it comes from the serial-connected motor controller.
*/
void ConsoleDashboard::setBatteryLevel(int v)
{
    if (m_batteryLevel == v)
        return;
    if (v > 99)
        v = 99;
    m_batteryLevel = v;
    emit batteryLevelChanged();
}
