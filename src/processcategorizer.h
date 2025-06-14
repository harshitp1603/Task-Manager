#ifndef PROCESSCATEGORIZER_H
#define PROCESSCATEGORIZER_H

#include <QString>
#include <QMap>
#include <windows.h>
#include <psapi.h>

enum class ProcessType {
    System,         // Windows system processes
    Background,     // Background services
    Application,    // User applications
    Unknown        // Unclassified processes
};

struct ProcessCategory {
    ProcessType type;
    QString description;
    QString style;  // CSS style for visual differentiation
};

class ProcessCategorizer {
public:
    static ProcessCategorizer& getInstance();
    
    ProcessCategory categorizeProcess(const QString& name, DWORD pid);
    QString getProcessStyle(ProcessType type) const;
    QString getProcessDescription(ProcessType type) const;
    
private:
    ProcessCategorizer();  // Private constructor for singleton
    ~ProcessCategorizer() = default;
    
    // Prevent copying
    ProcessCategorizer(const ProcessCategorizer&) = delete;
    ProcessCategorizer& operator=(const ProcessCategorizer&) = delete;
    
    void initializeSystemProcesses();
    bool isSystemProcess(const QString& name, DWORD pid) const;
    bool isBackgroundService(DWORD pid) const;
    
    QMap<QString, ProcessType> knownProcesses;
    QMap<ProcessType, QString> typeDescriptions;
    QMap<ProcessType, QString> typeStyles;
};

#endif // PROCESSCATEGORIZER_H 