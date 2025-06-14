#include "processcategorizer.h"
#include <QDebug>
#include <tlhelp32.h>
#include <processthreadsapi.h>
#include <winsvc.h>

ProcessCategorizer& ProcessCategorizer::getInstance()
{
    static ProcessCategorizer instance;
    return instance;
}

ProcessCategorizer::ProcessCategorizer()
{
    // Initialize type descriptions
    typeDescriptions[ProcessType::System] = "System Process";
    typeDescriptions[ProcessType::Background] = "Background Service";
    typeDescriptions[ProcessType::Application] = "Application";
    typeDescriptions[ProcessType::Unknown] = "Unknown Process";
    
    // Initialize styles for each process type
    typeStyles[ProcessType::System] = R"(
        background-color: #2d2d2d;
        border-left: 4px solid #d32f2f;
        color: #ff6b6b;
    )";
    
    typeStyles[ProcessType::Background] = R"(
        background-color: #2d2d2d;
        border-left: 4px solid #1976d2;
        color: #64b5f6;
    )";
    
    typeStyles[ProcessType::Application] = R"(
        background-color: #2d2d2d;
        border-left: 4px solid #388e3c;
        color: #81c784;
    )";
    
    typeStyles[ProcessType::Unknown] = R"(
        background-color: #2d2d2d;
        border-left: 4px solid #757575;
        color: #bdbdbd;
    )";
    
    initializeSystemProcesses();
}

void ProcessCategorizer::initializeSystemProcesses()
{
    // Common Windows system processes
    knownProcesses["System"] = ProcessType::System;
    knownProcesses["System Idle Process"] = ProcessType::System;
    knownProcesses["smss.exe"] = ProcessType::System;
    knownProcesses["csrss.exe"] = ProcessType::System;
    knownProcesses["wininit.exe"] = ProcessType::System;
    knownProcesses["services.exe"] = ProcessType::System;
    knownProcesses["lsass.exe"] = ProcessType::System;
    knownProcesses["winlogon.exe"] = ProcessType::System;
    knownProcesses["explorer.exe"] = ProcessType::System;
    knownProcesses["svchost.exe"] = ProcessType::System;
    knownProcesses["spoolsv.exe"] = ProcessType::System;
    knownProcesses["taskmgr.exe"] = ProcessType::System;
    knownProcesses["dwm.exe"] = ProcessType::System;
    knownProcesses["fontdrvhost.exe"] = ProcessType::System;
    knownProcesses["RuntimeBroker.exe"] = ProcessType::System;
    knownProcesses["SearchHost.exe"] = ProcessType::System;
    knownProcesses["ShellExperienceHost.exe"] = ProcessType::System;
    knownProcesses["StartMenuExperienceHost.exe"] = ProcessType::System;
    knownProcesses["TextInputHost.exe"] = ProcessType::System;
    knownProcesses["WmiPrvSE.exe"] = ProcessType::System;
}

bool ProcessCategorizer::isSystemProcess(const QString& name, DWORD pid) const
{
    // Check if it's a known system process
    if (knownProcesses.contains(name) && knownProcesses[name] == ProcessType::System) {
        return true;
    }
    
    // Check if process is running from system directories
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (hProcess) {
        wchar_t path[MAX_PATH];
        if (GetModuleFileNameExW(hProcess, nullptr, path, MAX_PATH)) {
            QString processPath = QString::fromWCharArray(path).toLower();
            // Check if process is running from Windows system directories
            if (processPath.contains("\\windows\\system32\\") ||
                processPath.contains("\\windows\\syswow64\\") ||
                processPath.contains("\\program files\\") ||
                processPath.contains("\\program files (x86)\\")) {
                CloseHandle(hProcess);
                return true;
            }
        }
        CloseHandle(hProcess);
    }
    
    return false;
}

bool ProcessCategorizer::isBackgroundService(DWORD pid) const
{
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (hProcess) {
        // Check if process is a service
        SC_HANDLE scm = OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT);
        if (scm) {
            DWORD bytesNeeded = 0;
            DWORD servicesReturned = 0;
            
            // First call to get required buffer size
            EnumServicesStatusExW(scm, SC_ENUM_PROCESS_INFO, SERVICE_TYPE_ALL,
                               SERVICE_STATE_ALL, nullptr, 0, &bytesNeeded,
                               &servicesReturned, nullptr, nullptr);
            
            if (bytesNeeded > 0) {
                QVector<BYTE> buffer(bytesNeeded);
                ENUM_SERVICE_STATUS_PROCESSW* services = 
                    reinterpret_cast<ENUM_SERVICE_STATUS_PROCESSW*>(buffer.data());
                
                if (EnumServicesStatusExW(scm, SC_ENUM_PROCESS_INFO, SERVICE_TYPE_ALL,
                                       SERVICE_STATE_ALL, buffer.data(), buffer.size(),
                                       &bytesNeeded, &servicesReturned, nullptr, nullptr)) {
                    for (DWORD i = 0; i < servicesReturned; i++) {
                        if (services[i].ServiceStatusProcess.dwProcessId == pid) {
                            CloseServiceHandle(scm);
                            CloseHandle(hProcess);
                            return true;
                        }
                    }
                }
            }
            CloseServiceHandle(scm);
        }
        CloseHandle(hProcess);
    }
    return false;
}

ProcessCategory ProcessCategorizer::categorizeProcess(const QString& name, DWORD pid)
{
    ProcessCategory category;
    
    if (isSystemProcess(name, pid)) {
        category.type = ProcessType::System;
    } else if (isBackgroundService(pid)) {
        category.type = ProcessType::Background;
    } else {
        category.type = ProcessType::Application;
    }
    
    category.description = typeDescriptions[category.type];
    category.style = typeStyles[category.type];
    
    return category;
}

QString ProcessCategorizer::getProcessStyle(ProcessType type) const
{
    return typeStyles.value(type, typeStyles[ProcessType::Unknown]);
}

QString ProcessCategorizer::getProcessDescription(ProcessType type) const
{
    return typeDescriptions.value(type, typeDescriptions[ProcessType::Unknown]);
} 