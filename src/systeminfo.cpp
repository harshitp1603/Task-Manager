#include "systeminfo.h"
#include "processcategorizer.h"
#include <QDebug>
#include <QDateTime>
#include <QTimer>
#include <tlhelp32.h>
#include <psapi.h>
#include <pdh.h>
#pragma comment(lib, "pdh.lib")

static PDH_HQUERY g_hQuery = NULL;
static PDH_HCOUNTER g_hCounter = NULL;

SystemInfo::SystemInfo(QObject *parent) : QObject(parent),
    cpuUsage(0.0),
    memoryUsage(0.0),
    diskUsage(0.0),
    networkUsage(0.0),
    numProcessors(1),
    lastSystemTime(0),
    lastBytesReceived(0.0),
    lastBytesSent(0.0),
    lastNetworkUpdateTime(0),
    cpuQuery(NULL),
    cpuCounter(NULL),
    networkQuery(NULL),
    bytesReceivedCounter(NULL),
    bytesSentCounter(NULL),
    efficiencyModeEnabled(false)
{
    updateTimer = new QTimer(this);
    connect(updateTimer, &QTimer::timeout, this, &SystemInfo::updateSystemInfo);
    updateTimer->start(1000);

    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    numProcessors = sysInfo.dwNumberOfProcessors;

    FILETIME idleTime, kernelTime, userTime;
    if (GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
        ULARGE_INTEGER k, u;
        k.LowPart = kernelTime.dwLowDateTime;
        k.HighPart = kernelTime.dwHighDateTime;
        u.LowPart = userTime.dwLowDateTime;
        u.HighPart = userTime.dwHighDateTime;
        lastSystemTime = k.QuadPart + u.QuadPart;
    } else {
        qWarning() << "Failed to get initial system times";
    }

    initCpuCounter();
    initNetworkCounter();
    updateSystemInfo();
}

SystemInfo::~SystemInfo() {
    if (g_hQuery != NULL) {
        PdhCloseQuery(g_hQuery);
        g_hQuery = NULL;
        g_hCounter = NULL;
    }
    PdhCloseQuery(cpuQuery);
    PdhCloseQuery(networkQuery);
}



QVector<ProcessInfo> SystemInfo::getProcessList() const {
    return processList;
}

double SystemInfo::getCpuUsage() const { return cpuUsage; }
double SystemInfo::getMemoryUsage() const { return memoryUsage; }
double SystemInfo::getDiskUsage() const { return diskUsage; }
double SystemInfo::getNetworkUsage() const { return networkUsage; }

double SystemInfo::getProcessCpuUsage(qint64 pid) const {
    for (const auto &process : processList) {
        if (process.pid == pid) {
            return process.cpuUsage;
        }
    }
    return 0.0;
}

void SystemInfo::updateSystemInfo() {
    updateProcessList();
    updateProcessCpuUsage();
    updateCpuUsage();
    updateMemoryUsage();
    updateDiskUsage();
    updateNetworkUsage();
    emit dataUpdated();
}

void SystemInfo::updateProcessCpuUsage() {
    FILETIME idleTime, kernelTime, userTime;
    if (!GetSystemTimes(&idleTime, &kernelTime, &userTime)) return;
    ULARGE_INTEGER k, u;
    k.LowPart = kernelTime.dwLowDateTime;
    k.HighPart = kernelTime.dwHighDateTime;
    u.LowPart = userTime.dwLowDateTime;
    u.HighPart = userTime.dwHighDateTime;
    qint64 currentSystemTime = k.QuadPart + u.QuadPart;
    qint64 systemTimeDelta = currentSystemTime - lastSystemTime;
    if (systemTimeDelta <= 0) systemTimeDelta = 1;

    for (ProcessInfo &proc : processList) {
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, (DWORD)proc.pid);
        if (!hProcess) {
            proc.cpuUsage = 0.0;
            continue;
        }
        FILETIME createTime, exitTime, kernelTime, userTime;
        if (GetProcessTimes(hProcess, &createTime, &exitTime, &kernelTime, &userTime)) {
            ULARGE_INTEGER k, u;
            k.LowPart = kernelTime.dwLowDateTime;
            k.HighPart = kernelTime.dwHighDateTime;
            u.LowPart = userTime.dwLowDateTime;
            u.HighPart = userTime.dwHighDateTime;
            qint64 processKernel = k.QuadPart;
            qint64 processUser = u.QuadPart;
            qint64 processTotal = processKernel + processUser;

            ProcessCpuTimes &prev = processCpuTimesMap[proc.pid];
            if (prev.lastKernelTime == 0 && prev.lastUserTime == 0) {
                proc.cpuUsage = 0.0;
            } else {
                qint64 processTimeDelta = processTotal - (prev.lastKernelTime + prev.lastUserTime);
                double usage = (double)processTimeDelta / (double)systemTimeDelta / numProcessors * 100.0;
                proc.cpuUsage = usage < 0.0 ? 0.0 : usage;
            }

            // Rolling average for CPU usage
            proc.cpuUsageHistory.append(proc.cpuUsage);
            if (proc.cpuUsageHistory.size() > 5) proc.cpuUsageHistory.removeFirst();
            double cpuSum = 0.0;
            for (double v : proc.cpuUsageHistory) cpuSum += v;
            proc.cpuUsageAvg = cpuSum / proc.cpuUsageHistory.size();
            proc.cpuUsage = proc.cpuUsageAvg;

            prev.lastKernelTime = processKernel;
            prev.lastUserTime = processUser;
            prev.lastSystemTime = currentSystemTime;
        } else {
            proc.cpuUsage = 0.0;
        }
        CloseHandle(hProcess);
    }
    lastSystemTime = currentSystemTime;
}

void SystemInfo::updateCpuUsage() {
    if (g_hQuery && g_hCounter) {
        PDH_FMT_COUNTERVALUE counterVal;
        PdhCollectQueryData(g_hQuery);
        PdhGetFormattedCounterValue(g_hCounter, PDH_FMT_DOUBLE, nullptr, &counterVal);
        cpuUsage = counterVal.doubleValue;
        qDebug() << "System CPU usage (PDH):" << cpuUsage;
    } else {
        cpuUsage = 0.0;
    }
}

void SystemInfo::updateMemoryUsage() {
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&memInfo);
    memoryUsage = memInfo.dwMemoryLoad;
}

void SystemInfo::updateDiskUsage() {
    ULARGE_INTEGER freeBytesAvailable, totalBytes, totalFreeBytes;
    if (GetDiskFreeSpaceExW(L"C:\\", &freeBytesAvailable, &totalBytes, &totalFreeBytes)) {
        double totalSpace = static_cast<double>(totalBytes.QuadPart);
        double freeSpace = static_cast<double>(totalFreeBytes.QuadPart);
        diskUsage = ((totalSpace - freeSpace) / totalSpace) * 100.0;
    }

    // Rolling average for Disk usage
    for (ProcessInfo &proc : processList) {
        proc.diskUsageHistory.append(proc.diskUsage);
        if (proc.diskUsageHistory.size() > 5) proc.diskUsageHistory.removeFirst();
        double diskSum = 0.0;
        for (double v : proc.diskUsageHistory) diskSum += v;
        proc.diskUsageAvg = diskSum / proc.diskUsageHistory.size();
        proc.diskUsage = proc.diskUsageAvg;
    }
}

void SystemInfo::updateNetworkUsage()
{
    if (networkQuery && bytesReceivedCounter && bytesSentCounter) {
        PDH_FMT_COUNTERVALUE receivedVal, sentVal;
        PDH_STATUS status = PdhCollectQueryData(networkQuery);
        
        if (status == ERROR_SUCCESS) {
            qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
            qint64 timeDelta = currentTime - lastNetworkUpdateTime;
            
            if (timeDelta > 0) {  // Ensure we have a valid time delta
                status = PdhGetFormattedCounterValue(bytesReceivedCounter, 
                    PDH_FMT_DOUBLE, NULL, &receivedVal);
                if (status == ERROR_SUCCESS) {
                    status = PdhGetFormattedCounterValue(bytesSentCounter, 
                        PDH_FMT_DOUBLE, NULL, &sentVal);
                    if (status == ERROR_SUCCESS) {
                        // Calculate the difference in bytes since last update
                        double currentBytesReceived = receivedVal.doubleValue;
                        double currentBytesSent = sentVal.doubleValue;
                        
                        // Calculate bytes per second (difference divided by time delta in seconds)
                        double bytesPerSecond = ((currentBytesReceived - lastBytesReceived) + 
                                               (currentBytesSent - lastBytesSent)) / 
                                               (timeDelta / 1000.0);  // Convert ms to seconds
                        
                        // Store current values for next calculation
                        lastBytesReceived = currentBytesReceived;
                        lastBytesSent = currentBytesSent;
                        
                        // Convert to KB/s (divide by 1024)
                        networkUsage = bytesPerSecond / 1024.0;
                        
                        qDebug() << "Network usage:" << networkUsage << "KB/s"
                                << "(Received:" << ((currentBytesReceived - lastBytesReceived) / (timeDelta / 1000.0) / 1024.0) << "KB/s,"
                                << "Sent:" << ((currentBytesSent - lastBytesSent) / (timeDelta / 1000.0) / 1024.0) << "KB/s)";
                    }
                }
                lastNetworkUpdateTime = currentTime;
            }
        }
    } else {
        networkUsage = 0.0;
    }

    // Sum per-process network usage for total
    double totalProcessNetwork = 0.0;
    for (const ProcessInfo &proc : processList) {
        totalProcessNetwork += proc.networkUsage;
    }
    networkUsage = totalProcessNetwork;
}

bool SystemInfo::enableDebugPrivilege()
{
    HANDLE hToken;
    TOKEN_PRIVILEGES tkp;
    
    // Get a token for this process
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
        return false;

    // Get the LUID for the debug privilege
    if (!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &tkp.Privileges[0].Luid)) {
        CloseHandle(hToken);
        return false;
    }

    tkp.PrivilegeCount = 1;
    tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    // Enable the debug privilege
    bool result = AdjustTokenPrivileges(hToken, FALSE, &tkp, sizeof(tkp), NULL, NULL) != 0;
    CloseHandle(hToken);
    
    return result && (GetLastError() == ERROR_SUCCESS);
}

bool SystemInfo::terminateProcessWithPrivilege(qint64 pid)
{
    if (!enableDebugPrivilege())
        return false;

    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, static_cast<DWORD>(pid));
    if (!hProcess)
        return false;

    bool result = killProcessWithHandle(hProcess);
    CloseHandle(hProcess);
    return result;
}

bool SystemInfo::killProcessWithHandle(HANDLE hProcess)
{
    // Try different termination methods in order of increasing severity
    if (TerminateProcess(hProcess, 1))
        return true;

    // If normal termination fails, try to suspend all threads first
    DWORD processId = GetProcessId(hProcess);
    HANDLE hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hThreadSnap != INVALID_HANDLE_VALUE) {
        THREADENTRY32 te32;
        te32.dwSize = sizeof(THREADENTRY32);
        
        if (Thread32First(hThreadSnap, &te32)) {
            do {
                if (te32.th32OwnerProcessID == processId) {
                    HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te32.th32ThreadID);
                    if (hThread) {
                        SuspendThread(hThread);
                        CloseHandle(hThread);
                    }
                }
            } while (Thread32Next(hThreadSnap, &te32));
        }
        CloseHandle(hThreadSnap);
    }

    // Try termination again after suspending threads
    return TerminateProcess(hProcess, 1) != 0;
}

QVector<qint64> SystemInfo::getChildProcesses(qint64 parentPid) const
{
    QVector<qint64> childPids;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE)
        return childPids;

    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);

    if (Process32FirstW(hSnapshot, &pe32)) {
        do {
            if (pe32.th32ParentProcessID == static_cast<DWORD>(parentPid)) {
                childPids.append(pe32.th32ProcessID);
            }
        } while (Process32NextW(hSnapshot, &pe32));
    }
    CloseHandle(hSnapshot);
    return childPids;
}

bool SystemInfo::isProcessRunning(qint64 pid) const
{
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (!hProcess)
        return false;

    DWORD exitCode;
    bool running = GetExitCodeProcess(hProcess, &exitCode) && exitCode == STILL_ACTIVE;
    CloseHandle(hProcess);
    return running;
}

bool SystemInfo::hasProcessAccess(qint64 pid) const
{
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid));
    if (!hProcess)
        return false;
    CloseHandle(hProcess);
    return true;
}

bool SystemInfo::terminateProcess(qint64 pid)
{
    // First try with maximum privileges
    if (!enableDebugPrivilege()) {
        qWarning() << "Failed to enable debug privilege for process" << pid;
        // Continue anyway, as some processes might be terminable without debug privilege
    }

    // Try to get a handle with maximum possible access rights
    HANDLE hProcess = OpenProcess(
        PROCESS_TERMINATE | PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | 
        PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_CREATE_THREAD | 
        PROCESS_SUSPEND_RESUME | SYNCHRONIZE,
        FALSE,
        static_cast<DWORD>(pid)
    );

    if (!hProcess) {
        // If we can't get a handle with full access, try with minimal access
        hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid));
        if (!hProcess) {
            qWarning() << "Failed to open process" << pid << "with error:" << GetLastError();
            return false;
        }
    }

    bool terminated = false;
    DWORD exitCode;

    // Method 1: Try to suspend all threads first
    DWORD processId = GetProcessId(hProcess);
    HANDLE hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hThreadSnap != INVALID_HANDLE_VALUE) {
        THREADENTRY32 te32;
        te32.dwSize = sizeof(THREADENTRY32);
        
        if (Thread32First(hThreadSnap, &te32)) {
            do {
                if (te32.th32OwnerProcessID == processId) {
                    HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME | THREAD_TERMINATE, FALSE, te32.th32ThreadID);
                    if (hThread) {
                        SuspendThread(hThread);
                        TerminateThread(hThread, 0);
                        CloseHandle(hThread);
                    }
                }
            } while (Thread32Next(hThreadSnap, &te32));
        }
        CloseHandle(hThreadSnap);
    }

    // Method 2: Try to terminate the process with different exit codes
    for (int exitCode : {1, 0, -1, 0xDEAD, 0xBEEF}) {
        if (TerminateProcess(hProcess, exitCode)) {
            if (WaitForSingleObject(hProcess, 1000) == WAIT_OBJECT_0) {
                terminated = true;
                break;
            }
        }
    }

    // Method 3: If still not terminated, try to crash the process
    if (!terminated) {
        // Try to allocate memory in the process and write a crash instruction
        LPVOID remoteMem = VirtualAllocEx(hProcess, NULL, 1024, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (remoteMem) {
            // Write a simple crash instruction (int 3)
            BYTE crashCode[] = {0xCC};
            if (WriteProcessMemory(hProcess, remoteMem, crashCode, sizeof(crashCode), NULL)) {
                // Create a remote thread to execute the crash code
                HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)remoteMem, NULL, 0, NULL);
                if (hThread) {
                    WaitForSingleObject(hThread, 1000);
                    CloseHandle(hThread);
                    terminated = true;
                }
            }
            VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
        }
    }

    // Method 4: If still not terminated, try to kill the process tree
    if (!terminated) {
        CloseHandle(hProcess);
        if (killProcessTree(pid)) {
            terminated = true;
        } else {
            // Try one more time with a fresh handle
            hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid));
            if (hProcess) {
                if (TerminateProcess(hProcess, 1)) {
                    if (WaitForSingleObject(hProcess, 1000) == WAIT_OBJECT_0) {
                        terminated = true;
                    }
                }
            }
        }
    }

    // Method 5: Last resort - try to crash the process by corrupting its memory
    if (!terminated && hProcess) {
        const void* baseAddresses[] = {
            (void*)0x400000,    // Common base address
            (void*)0x1000000,   // Alternative base address
            (void*)0x2000000,   // Another alternative
            (void*)0x140000000, // 64-bit process base
            (void*)0x180000000  // Another 64-bit base
        };
        
        for (const void* baseAddr : baseAddresses) {
            SIZE_T bytesWritten;
            if (WriteProcessMemory(hProcess, (LPVOID)baseAddr, "CRASH", 5, &bytesWritten)) {
                if (TerminateProcess(hProcess, 1)) {
                    if (WaitForSingleObject(hProcess, 1000) == WAIT_OBJECT_0) {
                        terminated = true;
                        break;
                    }
                }
            }
        }
    }

    if (hProcess) {
        CloseHandle(hProcess);
    }

    // Final verification
    if (terminated) {
        // Double check that the process is really gone
        HANDLE verifyHandle = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, static_cast<DWORD>(pid));
        if (!verifyHandle) {
            // Process is definitely gone
            removeProcessFromList(pid);
            return true;
        } else {
            // Check exit code
            if (GetExitCodeProcess(verifyHandle, &exitCode) && exitCode != STILL_ACTIVE) {
                CloseHandle(verifyHandle);
                removeProcessFromList(pid);
                return true;
            }
            CloseHandle(verifyHandle);
        }
    }

    // If all methods failed, try one last time with system-level termination
    return forceTerminateProcess(pid);
}

bool SystemInfo::forceTerminateProcess(qint64 pid)
{
    // Try to get a handle with maximum possible access rights
    HANDLE hProcess = OpenProcess(
        PROCESS_TERMINATE | PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | 
        PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_CREATE_THREAD | 
        PROCESS_SUSPEND_RESUME | SYNCHRONIZE,
        FALSE,
        static_cast<DWORD>(pid)
    );

    if (!hProcess) {
        return false;
    }

    bool terminated = false;

    // Method 1: Try to crash the process by corrupting its memory at multiple locations
    const void* baseAddresses[] = {
        (void*)0x400000,    // Common base address
        (void*)0x1000000,   // Alternative base address
        (void*)0x2000000,   // Another alternative
        (void*)0x140000000, // 64-bit process base
        (void*)0x180000000  // Another 64-bit base
    };
    
    for (const void* baseAddr : baseAddresses) {
        SIZE_T bytesWritten;
        if (WriteProcessMemory(hProcess, (LPVOID)baseAddr, "CRASH", 5, &bytesWritten)) {
            if (TerminateProcess(hProcess, 1)) {
                if (WaitForSingleObject(hProcess, 1000) == WAIT_OBJECT_0) {
                    terminated = true;
                    break;
                }
            }
        }
    }

    // Method 2: If still not terminated, try to inject a crash instruction
    if (!terminated) {
        LPVOID remoteMem = VirtualAllocEx(hProcess, NULL, 1024, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (remoteMem) {
            BYTE crashCode[] = {0xCC}; // int 3 instruction
            if (WriteProcessMemory(hProcess, remoteMem, crashCode, sizeof(crashCode), NULL)) {
                HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)remoteMem, NULL, 0, NULL);
                if (hThread) {
                    WaitForSingleObject(hThread, 1000);
                    CloseHandle(hThread);
                    terminated = true;
                }
            }
            VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
        }
    }

    // Method 3: If still not terminated, try to corrupt the process's stack
    if (!terminated) {
        CONTEXT ctx;
        ctx.ContextFlags = CONTEXT_ALL;
        HANDLE hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (hThreadSnap != INVALID_HANDLE_VALUE) {
            THREADENTRY32 te32;
            te32.dwSize = sizeof(THREADENTRY32);
            if (Thread32First(hThreadSnap, &te32)) {
                do {
                    if (te32.th32OwnerProcessID == GetProcessId(hProcess)) {
                        HANDLE hThread = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, FALSE, te32.th32ThreadID);
                        if (hThread) {
                            if (GetThreadContext(hThread, &ctx)) {
                                // Corrupt the stack pointer
                                ctx.Rsp = 0;
                                SetThreadContext(hThread, &ctx);
                            }
                            CloseHandle(hThread);
                        }
                    }
                } while (Thread32Next(hThreadSnap, &te32));
            }
            CloseHandle(hThreadSnap);
        }
    }

    CloseHandle(hProcess);

    // Final verification
    if (terminated) {
        HANDLE verifyHandle = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, static_cast<DWORD>(pid));
        if (!verifyHandle) {
            removeProcessFromList(pid);
            return true;
        }
        DWORD exitCode;
        if (GetExitCodeProcess(verifyHandle, &exitCode) && exitCode != STILL_ACTIVE) {
            CloseHandle(verifyHandle);
            removeProcessFromList(pid);
            return true;
        }
        CloseHandle(verifyHandle);
    }

    return false;
}

bool SystemInfo::killProcessTree(qint64 pid)
{
    // Get all child processes first
    QVector<qint64> childPids = getChildProcesses(pid);
    
    // Kill all children first
    for (qint64 childPid : childPids) {
        killProcessTree(childPid);
    }

    // Now kill the parent process
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_INFORMATION | SYNCHRONIZE, FALSE, static_cast<DWORD>(pid));
    if (!hProcess) {
        return false;
    }

    bool terminated = false;
    if (TerminateProcess(hProcess, 1)) {
        // Wait for the process to actually terminate
        if (WaitForSingleObject(hProcess, 1000) == WAIT_OBJECT_0) {
            terminated = true;
        }
    }

    CloseHandle(hProcess);
    return terminated;
}

void SystemInfo::setUpdateInterval(int milliseconds)
{
    if (updateTimer) {
        updateTimer->setInterval(milliseconds);
    }
}

void SystemInfo::initCpuCounter() {
    if (g_hQuery == NULL) {
        PDH_STATUS status = PdhOpenQuery(NULL, 0, &g_hQuery);
        if (status == ERROR_SUCCESS) {
            status = PdhAddCounter(g_hQuery, L"\\Processor(_Total)\\% Processor Time", 0, &g_hCounter);
            if (status != ERROR_SUCCESS) {
                PdhCloseQuery(g_hQuery);
                g_hQuery = NULL;
                g_hCounter = NULL;
            }
        }
    }
}

bool SystemInfo::setProcessPriority(qint64 pid, int priority)
{
    HANDLE hProcess = OpenProcess(PROCESS_SET_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (hProcess == NULL) {
        return false;
    }

    bool success = SetPriorityClass(hProcess, static_cast<DWORD>(priority)) != 0;
    CloseHandle(hProcess);
    return success;
}

bool SystemInfo::optimizeBackgroundProcesses()
{
    bool success = true;
    for (const ProcessInfo &proc : processList) {
        if (proc.type == ProcessType::Background && !isProcessEssential(proc)) {
            // Store original priority if not already stored
            if (!originalPriorities.contains(proc.pid)) {
                originalPriorities[proc.pid] = getProcessPriorityClass(proc.pid);
            }
            // Set background processes to below normal priority
            if (!setProcessPriority(proc.pid, BELOW_NORMAL_PRIORITY_CLASS)) {
                success = false;
            }
        }
    }
    return success;
}

bool SystemInfo::optimizeMemoryUsage()
{
    bool success = true;
    for (const ProcessInfo &proc : processList) {
        if (!isProcessEssential(proc) && proc.memoryUsage > 100 * 1024) { // More than 100MB
            HANDLE hProcess = OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE, static_cast<DWORD>(proc.pid));
            if (hProcess) {
                // Set process working set size limits
                SIZE_T minWS = 1024 * 1024;  // 1MB minimum
                SIZE_T maxWS = 50 * 1024 * 1024;  // 50MB maximum
                if (!SetProcessWorkingSetSize(hProcess, minWS, maxWS)) {
                    success = false;
                }
                CloseHandle(hProcess);
            }
        }
    }
    return success;
}

bool SystemInfo::throttleNonEssentialProcesses()
{
    bool success = true;
    for (const ProcessInfo &proc : processList) {
        if (!isProcessEssential(proc) && proc.cpuUsage > 5.0) { // CPU usage > 5%
            // Store original priority if not already stored
            if (!originalPriorities.contains(proc.pid)) {
                originalPriorities[proc.pid] = getProcessPriorityClass(proc.pid);
            }
            // Throttle CPU usage by setting to below normal priority
            if (!setProcessPriority(proc.pid, BELOW_NORMAL_PRIORITY_CLASS)) {
                success = false;
            } else {
                throttledProcesses.append(proc.pid);
            }
        }
    }
    return success;
}

QVector<ProcessInfo> SystemInfo::getHighResourceProcesses() const
{
    QVector<ProcessInfo> highResourceProcesses;
    for (const ProcessInfo &proc : processList) {
        if (proc.cpuUsage > 10.0 || proc.memoryUsage > 200 * 1024) { // CPU > 10% or Memory > 200MB
            highResourceProcesses.append(proc);
        }
    }
    return highResourceProcesses;
}

void SystemInfo::setEfficiencyMode(bool enabled)
{
    if (efficiencyModeEnabled == enabled) {
        return;
    }

    efficiencyModeEnabled = enabled;
    if (enabled) {
        // Store original priorities before applying changes
        for (const ProcessInfo &proc : processList) {
            if (!originalPriorities.contains(proc.pid)) {
                originalPriorities[proc.pid] = getProcessPriorityClass(proc.pid);
            }
        }
        applyEfficiencyModeSettings();
    } else {
        removeEfficiencyModeSettings();
    }
    emit efficiencyModeChanged(enabled);
}

void SystemInfo::applyEfficiencyModeSettings()
{
    optimizeBackgroundProcesses();
    optimizeMemoryUsage();
    throttleNonEssentialProcesses();
}

void SystemInfo::removeEfficiencyModeSettings()
{
    restoreOriginalPriorities();
    throttledProcesses.clear();
}

void SystemInfo::restoreOriginalPriorities()
{
    for (auto it = originalPriorities.begin(); it != originalPriorities.end(); ++it) {
        setProcessPriority(it.key(), it.value());
    }
    originalPriorities.clear();
}

bool SystemInfo::isProcessEssential(const ProcessInfo& process) const
{
    // List of essential system processes that should not be modified
    static const QStringList essentialProcesses = {
        "System", "Registry", "smss.exe", "csrss.exe", "wininit.exe",
        "services.exe", "lsass.exe", "svchost.exe", "explorer.exe",
        "Taskmgr.exe", "ProcManager.exe"  // Our own process
    };

    return process.type == ProcessType::System ||
           essentialProcesses.contains(process.name, Qt::CaseInsensitive);
}

int SystemInfo::getProcessPriorityClass(qint64 pid) const
{
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (hProcess == NULL) {
        return NORMAL_PRIORITY_CLASS;
    }

    DWORD priority = GetPriorityClass(hProcess);
    CloseHandle(hProcess);
    return priority ? priority : NORMAL_PRIORITY_CLASS;
}

void SystemInfo::removeProcessFromList(qint64 pid)
{
    // Remove from process list
    for (int i = 0; i < processList.size(); ++i) {
        if (processList[i].pid == pid) {
            processList.removeAt(i);
            break;
        }
    }
    
    // Clean up associated data
    processCpuTimesMap.remove(pid);
    originalPriorities.remove(pid);
    throttledProcesses.removeAll(pid);
    
    // Force an immediate update
    QMetaObject::invokeMethod(this, "updateSystemInfo", Qt::QueuedConnection);
}

void SystemInfo::updateProcessList()
{
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) {
        qWarning() << "Failed to create process snapshot";
        return;
    }

    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);

    // Clear existing process list
    processList.clear();

    if (Process32FirstW(hSnap, &pe32)) {
        do {
            ProcessInfo proc;
            proc.pid = pe32.th32ProcessID;
            proc.name = QString::fromWCharArray(pe32.szExeFile);
            
            // Get process handle for additional information
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pe32.th32ProcessID);
            if (hProcess) {
                // Get process path
                WCHAR path[MAX_PATH];
                if (GetModuleFileNameExW(hProcess, NULL, path, MAX_PATH)) {
                    proc.path = QString::fromWCharArray(path);
                }

                // Get process memory usage
                PROCESS_MEMORY_COUNTERS_EX pmc;
                if (GetProcessMemoryInfo(hProcess, (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
                    proc.memoryUsage = pmc.WorkingSetSize / 1024; // Convert to KB
                }

                // Get process start time
                FILETIME createTime, exitTime, kernelTime, userTime;
                if (GetProcessTimes(hProcess, &createTime, &exitTime, &kernelTime, &userTime)) {
                    ULARGE_INTEGER createTimeUL;
                    createTimeUL.LowPart = createTime.dwLowDateTime;
                    createTimeUL.HighPart = createTime.dwHighDateTime;
                    proc.startTime = createTimeUL.QuadPart;
                }

                // Determine process status
                DWORD exitCode;
                if (GetExitCodeProcess(hProcess, &exitCode)) {
                    proc.status = (exitCode == STILL_ACTIVE) ? "Running" : "Not Responding";
                }

                // Get disk I/O information
                double diskUsageMBps = 0.0;
                IO_COUNTERS ioCounters;
                qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
                if (GetProcessIoCounters(hProcess, &ioCounters)) {
                    qint64 readBytes = ioCounters.ReadTransferCount;
                    qint64 writeBytes = ioCounters.WriteTransferCount;
                    qint64 totalBytes = readBytes + writeBytes;

                    ProcessDiskIo &diskIo = diskIoMap[proc.pid];
                    if (diskIo.lastUpdateTime > 0) {
                        qint64 timeDelta = currentTime - diskIo.lastUpdateTime;
                        if (timeDelta > 0) {
                            qint64 bytesDelta = (readBytes - diskIo.lastReadBytes) + (writeBytes - diskIo.lastWriteBytes);
                            diskUsageMBps = (bytesDelta / 1048576.0) / (timeDelta / 1000.0); // MB/s
                        }
                    }
                    // Update last values
                    diskIo.lastReadBytes = readBytes;
                    diskIo.lastWriteBytes = writeBytes;
                    diskIo.lastUpdateTime = currentTime;
                }
                proc.diskUsage = diskUsageMBps;

                // Rolling average for Disk usage
                proc.diskUsageHistory.append(proc.diskUsage);
                if (proc.diskUsageHistory.size() > 5) proc.diskUsageHistory.removeFirst();
                double diskSum = 0.0;
                for (double v : proc.diskUsageHistory) diskSum += v;
                proc.diskUsageAvg = diskSum / proc.diskUsageHistory.size();
                proc.diskUsage = proc.diskUsageAvg;

                // Get network I/O information (approximate: sum of Read/Write bytes for network handles)
                double networkUsageMBps = 0.0;
                IO_COUNTERS netIoCounters;
                qint64 netCurrentTime = QDateTime::currentMSecsSinceEpoch();
                if (GetProcessIoCounters(hProcess, &netIoCounters)) {
                    // On Windows, IO_COUNTERS includes all I/O (disk + network), but for most user processes, network is a small part
                    // We'll approximate network usage as the sum of Read/Write bytes minus the disk bytes (not perfect, but gives a value)
                    // For now, just use the same as disk (for demo), but you can refine with ETW or other APIs for true network I/O
                    // Here, we use the same bytes as disk for demonstration
                    // If you want to distinguish, you need a more advanced method (not available in standard Win32 API)
                    // We'll just show the same as disk for now
                    networkUsageMBps = 0.0; // Placeholder, see note above
                }
                proc.networkUsage = networkUsageMBps;
                // Rolling average for Network usage
                if (proc.networkUsageHistory.size() > 5) proc.networkUsageHistory.removeFirst();
                proc.networkUsageHistory.append(proc.networkUsage);
                double netSum = 0.0;
                for (double v : proc.networkUsageHistory) netSum += v;
                proc.networkUsage = netSum / proc.networkUsageHistory.size();

                // Categorize process type
                ProcessCategory category = ProcessCategorizer::getInstance().categorizeProcess(proc.name, static_cast<DWORD>(proc.pid));
                proc.type = category.type;
                proc.typeDescription = ProcessCategorizer::getInstance().getProcessDescription(category.type);
                proc.style = ProcessCategorizer::getInstance().getProcessStyle(category.type);

                CloseHandle(hProcess);
            }

            // Add process to list
            processList.append(proc);

        } while (Process32NextW(hSnap, &pe32));
    }

    CloseHandle(hSnap);
}

void SystemInfo::initNetworkCounter()
{
    PDH_STATUS status = PdhOpenQuery(NULL, 0, &networkQuery);
    if (status != ERROR_SUCCESS) {
        qWarning() << "Failed to open network query";
        return;
    }

    // Add counters for network bytes received and sent
    status = PdhAddCounter(networkQuery, L"\\Network Interface(*)\\Bytes Received/sec", 0, &bytesReceivedCounter);
    if (status != ERROR_SUCCESS) {
        qWarning() << "Failed to add bytes received counter";
        return;
    }

    status = PdhAddCounter(networkQuery, L"\\Network Interface(*)\\Bytes Sent/sec", 0, &bytesSentCounter);
    if (status != ERROR_SUCCESS) {
        qWarning() << "Failed to add bytes sent counter";
        return;
    }

    // Collect initial data
    PdhCollectQueryData(networkQuery);
    lastNetworkUpdateTime = QDateTime::currentMSecsSinceEpoch();
} 