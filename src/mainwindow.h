#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTableWidget>
#include <QProgressBar>
#include <QLabel>
#include <QTabWidget>
#include <QPushButton>
#include <QGroupBox>
#include <QMessageBox>
#include <QLineEdit>
#include <QComboBox>
#include <QTimer>
#include <QDateTime>
#include <QTextEdit>
#include "systeminfo.h"

QT_BEGIN_NAMESPACE
class QVBoxLayout;
class QHBoxLayout;
class QHeaderView;
QT_END_NAMESPACE

// Cache structure for process data
struct ProcessCache {
    QVector<ProcessInfo> processes;
    qint64 timestamp;
    qint64 totalMemory;
};

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void updateUI();
    void onDataUpdated();
    void onSearchTextChanged(const QString &text);
    void onProcessTypeFilterChanged(int index);
    void onTableHeaderClicked(int column);
    void sortByMemory();
    void sortByCPU();
    void sortByPID();
    void runNewTask();
    void forceEndTask();
    void sortProcesses(int column, Qt::SortOrder order);
    void toggleEfficiencyMode();
    void onEfficiencyModeChanged(bool enabled);
    void updateEfficiencyButtonState();
    void updateProcessTable();
    void updateResourceUsage();
    void updateProcessComboBox(QComboBox *comboBox);
    void checkProcessHealth(const QString &processName, QTextEdit *statusDisplay, QTableWidget *diagnosticTable);

private:
    QTabWidget *tabWidget;
    QTableWidget *processTable;
    QProgressBar *cpuBar;
    QProgressBar *memoryBar;
    QProgressBar *diskBar;
    QLabel *cpuLabel;
    QComboBox *processSelect;
    QLabel *memoryLabel;
    QLabel *diskLabel;
    QLabel *cpuSumLabel;
    QLabel *memSumLabel;
    QLabel *diskSumLabel;
    QLabel *netSumLabel;
    QPushButton *sortMemoryButton;
    QPushButton *sortCPUButton;
    QPushButton *sortPIDButton;
    QPushButton *endTaskButton;
    QPushButton *efficiencyBtn;
    QLineEdit *searchBox;
    QComboBox *processTypeFilter;
    SystemInfo *systemInfo;
    QTimer *updateTimer;
    QTimer *comboBoxUpdateTimer;

    // Performance optimization members
    ProcessCache processCache;
    qint64 lastUpdateTime;
    int currentSortColumn;
    Qt::SortOrder currentSortOrder;
    bool isSortingEnabled;
    ProcessType currentProcessTypeFilter;
    bool efficiencyModeEnabled;

    void setupUI();
    void setupProcessTable();
    void setupSortingButtons();
    void setupSearchBox();
    void setupProcessTypeFilter();
    QGroupBox* createResourceGroup();
    void setApplicationStyle();
    void applySorting();
    void setupTableHeaders();
    QString formatTime(qint64 fileTime);
    QString formatMemorySize(qint64 bytes);
    void applyProcessStyle(QTableWidgetItem* item, const QString& style);

    // Process table update methods
    bool shouldUpdateProcessTable();
    void updateTableContents(const QVector<ProcessInfo> &processes, qint64 totalMemory);
    void updateTableRow(int row, const ProcessInfo &process, qint64 totalMemory);
    QTableWidgetItem* createTableItem(const QString &text, const QString &style, double sortValue = 0.0);
    bool shouldDisplayProcess(const ProcessInfo &process, const QString &searchText);
};

#endif // MAINWINDOW_H 