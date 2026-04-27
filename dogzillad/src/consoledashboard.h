#ifndef CONSOLEDASHBOARD_H
#define CONSOLEDASHBOARD_H

#include <QFile>
#include <QQmlEngine>
#include <QNetworkInformation>
#include <QObject>
#include <fstream>
#include <iostream>

class ConsoleDashboard : public QObject
{
    Q_PROPERTY(QString tty READ tty WRITE setTty NOTIFY ttyChanged FINAL)
    Q_PROPERTY(int batteryLevel READ batteryLevel WRITE setBatteryLevel NOTIFY batteryLevelChanged FINAL)
    Q_OBJECT
    QML_ELEMENT
public:
    explicit ConsoleDashboard(QObject *parent = nullptr);

    QString tty() const { return m_tty; }
    void setTty(const QString &tty);

    int batteryLevel() const { return m_batteryLevel; }
    void setBatteryLevel(int v);

signals:

    void ttyChanged();

    void batteryLevelChanged();

protected:
    void timerEvent(QTimerEvent *ev);

private:
    int cpuPercent();
    QString readBatteryVoltage();
    std::string batteryBars();
    void onReachabilityChanged(QNetworkInformation::Reachability r);
    void update();

private:
    QString m_tty;
    std::ofstream out;
    QNetworkInformation *m_networkInfo = nullptr;
    QFile m_batteryVoltageFile;
    int m_timerId = -1;
    int m_prevCpuIdle = 0;
    int m_prevCpuTotal = 0;
    int m_batteryLevel = 0; // percent, 0 - 99
    int m_updateCount = 0;
    float m_batteryVoltage = 0.0f;
};

#endif // CONSOLEDASHBOARD_H
