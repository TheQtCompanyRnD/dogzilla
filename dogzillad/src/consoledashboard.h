#ifndef CONSOLEDASHBOARD_H
#define CONSOLEDASHBOARD_H

#include <QFile>
#include <QQmlEngine>
#include <QNetworkInformation>
#include <QObject>
#include <sstream>

class ConsoleDashboard : public QObject
{
    Q_PROPERTY(QString filePath READ filePath WRITE setFilePath NOTIFY filePathChanged FINAL)
    Q_PROPERTY(int batteryLevel READ batteryLevel WRITE setBatteryLevel NOTIFY batteryLevelChanged FINAL)
    Q_OBJECT
    QML_ELEMENT
public:
    explicit ConsoleDashboard(QObject *parent = nullptr);

    QString filePath() const { return m_filePath; }
    void setFilePath(const QString &path);

    int batteryLevel() const { return m_batteryLevel; }
    void setBatteryLevel(int v);

signals:

    void filePathChanged();

    void batteryLevelChanged();

protected:
    void timerEvent(QTimerEvent *ev);

private:
    int cpuPercent();
    QString readBatteryVoltage();
    void readInterfaceBytes(const QString &ifaceName, quint64 &rx, quint64 &tx);
    void calculateBandwidth(quint64 currentRx, quint64 currentTx,
                            quint64 prevRx,quint64 prevTx,
                            quint64 &rxBandwidth, quint64 &txBandwidth);
    std::string batteryBars();
    void writeFrame(const std::string &text);
    void onReachabilityChanged(QNetworkInformation::Reachability r);
    void update();

private:
    QString m_filePath;
    QFile m_out;
    bool m_outIsTty = false;
    QNetworkInformation *m_networkInfo = nullptr;
    QFile m_batteryVoltageFile;
    int m_timerId = -1;
    int m_prevCpuIdle = 0;
    int m_prevCpuTotal = 0;
    int m_batteryLevel = 0; // percent, 0 - 99
    int m_updateCount = 0;
    float m_batteryVoltage = 0.0f;
    // Bandwidth tracking
    QMap<QString, quint64> m_prevRxBytes;
    QMap<QString, quint64> m_prevTxBytes;
};

#endif // CONSOLEDASHBOARD_H
