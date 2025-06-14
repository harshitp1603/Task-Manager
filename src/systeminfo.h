#ifndef SYSTEMINFO_H
#define SYSTEMINFO_H

#include <QObject>
#include <QTimer>
#include <QVector>
#include <QMap>
#include <QDateTime>
#include <windows.h>
#include <psapi.h>
#include <pdh.h>
#include "processcategorizer.h"
#include <map>

struct ProcessCpuTimes {
    qint64 lastSystemTime = 0;
    qint64 lastKernelTime = 0;
    qint64 lastUserTime = 0;
};

struct ProcessInfo {
    QString name;
    qint64 pid;
    double cpuUsage;  // CPU usage percentage for this process
    qint64 memoryUsage;
    double diskUsage;  // Disk I/O in MB/s
    double networkUsage;  // Network I/O in MB/s
    QString status;
    QString path;     // Process executable path
    qint64 startTime; // Process start time
    ProcessType type;        // Process type (System, Background, Application)
    QString typeDescription; // Human-readable type description
    QString style;          // CSS style for visual differentiation
    // Rolling average buffers
    QVector<double> cpuUsageHistory;
    QVector<double> diskUsageHistory;
    QVector<double> networkUsageHistory;
    double cpuUsageAvg = 0.0;
    double diskUsageAvg = 0.0;
    double networkUsageAvg = 0.0;
};

struct ProcessDiskIo {
    ULONGLONG lastReadBytes = 0;
    ULONGLONG lastWriteBytes = 0;
    qint64 lastUpdateTime = 0;
};

class SystemInfo : public QObject {
    Q_OBJECT

public:
    explicit SystemInfo(QObject *parent = nullptr);
    ~SystemInfo();

    QVector<ProcessInfo> getProcessList() const;
    double getCpuUsage() const;
    double getMemoryUsage() const;
    double getDiskUsage() const;
    double getNetworkUsage() const;
    double getProcessCpuUsage(qint64 pid) const;
    bool terminateProcess(qint64 pid);
    bool forceTerminateProcess(qint64 pid);
    bool killProcessTree(qint64 pid);
    bool isProcessRunning(qint64 pid) const;
    bool hasProcessAccess(qint64 pid) const;

    // Performance optimization
    void setUpdateInterval(int milliseconds);

    // Efficiency mode methods
    bool setProcessPriority(qint64 pid, int priority);
    bool optimizeBackgroundProcesses();
    bool optimizeMemoryUsage();
    bool throttleNonEssentialProcesses();
    QVector<ProcessInfo> getHighResourceProcesses() const;
    bool isEfficiencyModeEnabled() const { return efficiencyModeEnabled; }
    void setEfficiencyMode(bool enabled);

signals:
    void dataUpdated();
    void efficiencyModeChanged(bool enabled);

private slots:
    void updateSystemInfo();

private:
    QTimer *updateTimer;
    QVector<ProcessInfo> processList;
    double cpuUsage;
    double memoryUsage;
    double diskUsage;
    double networkUsage;
    int numProcessors;
    qint64 lastSystemTime;
    QMap<qint64, ProcessCpuTimes> processCpuTimesMap;
    std::map<qint64, ProcessDiskIo> diskIoMap;

    // CPU monitoring
    PDH_HQUERY cpuQuery;
    PDH_HCOUNTER cpuCounter;

    // Network monitoring
    PDH_HQUERY networkQuery;
    PDH_HCOUNTER bytesReceivedCounter;
    PDH_HCOUNTER bytesSentCounter;
    double lastBytesReceived;
    double lastBytesSent;
    qint64 lastNetworkUpdateTime;
    
    // Efficiency mode members
    bool efficiencyModeEnabled;
    QVector<qint64> throttledProcesses;
    QMap<qint64, int> originalPriorities;
    
    void initializeCpuCounter();
    void updateProcessList();
    void updateCpuUsage();
    void updateMemoryUsage();
    void updateDiskUsage();
    void updateNetworkUsage();
    void updateProcessCpuUsage();
    void initializeProcessCpuCounter(qint64 pid);
    void initCpuCounter();
    void initNetworkCounter();
    void removeProcessFromList(qint64 pid);

    // Efficiency mode helper methods
    void restoreOriginalPriorities();
    bool isProcessEssential(const ProcessInfo& process) const;
    int getProcessPriorityClass(qint64 pid) const;
    void applyEfficiencyModeSettings();
    void removeEfficiencyModeSettings();

    // Helper methods for process termination
    bool enableDebugPrivilege();
    bool terminateProcessWithPrivilege(qint64 pid);
    QVector<qint64> getChildProcesses(qint64 parentPid) const;
    bool killProcessWithHandle(HANDLE hProcess);
};

#endif // SYSTEMINFO_H 