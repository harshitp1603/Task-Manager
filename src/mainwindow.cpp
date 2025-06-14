#include "mainwindow.h"
#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QHeaderView>
#include <QGroupBox>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QTableWidget>
#include <QMessageBox>
#include <QApplication>
#include <QDebug>
#include <QIcon>
#include <QLineEdit>
#include <QComboBox>
#include <QColor>
#include <QTimer>
#include <QDateTime>
#include <QMap>
#include <QStackedWidget>
#include <QProcess>
#include <QInputDialog>
#include <QFileDialog>
#include <QDialogButtonBox>
#include <QThread>
#include <windows.h>
#include <tlhelp32.h>
#include <QScrollArea>
#include <QTextEdit>

// Performance optimization constants
const int UPDATE_INTERVAL_MS = 1000;  // Update UI every 1 second
const int MAX_PROCESS_ROWS = 1000;    // Maximum number of processes to display
const int CACHE_DURATION_MS = 5000;   // Cache process data for 5 seconds

// Helper: Group names for process types
static QMap<ProcessType, QString> groupNames = {
    {ProcessType::Application, "Apps"},
    {ProcessType::Background, "Background processes"},
    {ProcessType::System, "System processes"},
    {ProcessType::Unknown, "Other"}
};

// Helper: Group order
static QList<ProcessType> groupOrder = {
    ProcessType::Application,
    ProcessType::Background,
    ProcessType::System,
    ProcessType::Unknown
};

// Helper: Store group expanded/collapsed state
QMap<ProcessType, bool> groupExpanded = {
    {ProcessType::Application, true},
    {ProcessType::Background, true},
    {ProcessType::System, true},
    {ProcessType::Unknown, true}
};

// Global (static) variable to hold the sorted process list (if sorting is active)
static QVector<ProcessInfo> sortedProcesses;

// Helper: Enable SeDebugPrivilege for the current process
bool enableDebugPrivilege() {
    HANDLE hToken;
    TOKEN_PRIVILEGES tkp;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
        return false;
    LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &tkp.Privileges[0].Luid);
    tkp.PrivilegeCount = 1;
    tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    BOOL result = AdjustTokenPrivileges(hToken, FALSE, &tkp, sizeof(tkp), NULL, NULL);
    CloseHandle(hToken);
    return (result && GetLastError() == ERROR_SUCCESS);
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent),
    tabWidget(nullptr),
    processTable(nullptr),
    cpuBar(nullptr),
    memoryBar(nullptr),
    diskBar(nullptr),
    cpuLabel(nullptr),
    memoryLabel(nullptr),
    diskLabel(nullptr),
    cpuSumLabel(nullptr),
    memSumLabel(nullptr),
    diskSumLabel(nullptr),
    netSumLabel(nullptr),
    sortMemoryButton(nullptr),
    sortCPUButton(nullptr),
    sortPIDButton(nullptr),
    endTaskButton(nullptr),
    searchBox(nullptr),
    processTypeFilter(nullptr),
    processSelect(nullptr),
    systemInfo(nullptr),
    updateTimer(nullptr),
    processCache{QVector<ProcessInfo>(), 0, 0},
    lastUpdateTime(0),
    currentSortColumn(-1),
    currentSortOrder(Qt::AscendingOrder),
    isSortingEnabled(true),
    currentProcessTypeFilter(ProcessType::Unknown),
    efficiencyBtn(nullptr),
    efficiencyModeEnabled(false)
{
    try {
        // Initialize system info with update interval
        systemInfo = new SystemInfo(this);
        systemInfo->setUpdateInterval(UPDATE_INTERVAL_MS);

        // Set application-wide style
        setApplicationStyle();

        // Set application icon
        QIcon appIcon(":/app_icon.png");
        if (!appIcon.isNull()) {
            setWindowIcon(appIcon);
            qApp->setWindowIcon(appIcon);  // Set for the entire application
        } else {
            qWarning() << "Failed to load application icon";
        }

        // Setup UI before connecting signals/timers
        setupUI();
        setWindowTitle("ProcManager");
        resize(1000, 700);

        // Now connect signals and start timers (after UI is ready)
        connect(systemInfo, &SystemInfo::dataUpdated, this, &MainWindow::onDataUpdated);
        updateTimer = new QTimer(this);
        connect(updateTimer, &QTimer::timeout, this, &MainWindow::updateUI);
        updateTimer->start(UPDATE_INTERVAL_MS);
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Error", QString("Failed to initialize: %1").arg(e.what()));
    }
}

MainWindow::~MainWindow()
{
    // Qt's parent-child relationship will handle memory cleanup
}

void MainWindow::setApplicationStyle()
{
    try {
        // Modern dark theme
        QString styleSheet = R"(
            QMainWindow {
                background-color: #1e1e1e;
            }
            
            QWidget {
                background-color: #1e1e1e;
                color: #ffffff;
                font-family: 'Segoe UI', Arial;
            }
            
            QTabWidget::pane {
                border: 1px solid #3a3a3a;
                background-color: #2d2d2d;
                border-radius: 5px;
            }
            
            QTabBar::tab {
                background-color: #2d2d2d;
                color: #ffffff;
                padding: 8px 20px;
                border: 1px solid #3a3a3a;
                border-bottom: none;
                border-top-left-radius: 4px;
                border-top-right-radius: 4px;
            }
            
            QTabBar::tab:selected {
                background-color: #3a3a3a;
                border-bottom: 2px solid #007acc;
            }
            
            QTabBar::tab:hover:!selected {
                background-color: #3a3a3a;
            }
            
            QTableWidget {
                background-color: #2d2d2d;
                alternate-background-color: #363636;
                border: 1px solid #3a3a3a;
                border-radius: 5px;
                gridline-color: #3a3a3a;
            }
            
            QTableWidget::item {
                padding: 5px;
                border-bottom: 1px solid #3a3a3a;
            }
            
            QTableWidget::item:selected {
                background-color: #007acc;
                color: white;
            }
            
            QHeaderView::section {
                background-color: #2d2d2d;
                color: #ffffff;
                padding: 8px;
                border: 1px solid #3a3a3a;
                font-weight: bold;
            }
            
            QProgressBar {
                border: 2px solid #3a3a3a;
                border-radius: 5px;
                text-align: center;
                background-color: #2d2d2d;
                color: white;
            }
            
            QProgressBar::chunk {
                background-color: #007acc;
                border-radius: 3px;
            }
            
            QGroupBox {
                border: 2px solid #3a3a3a;
                border-radius: 5px;
                margin-top: 1em;
                padding-top: 10px;
                background-color: #2d2d2d;
            }
            
            QGroupBox::title {
                subcontrol-origin: margin;
                subcontrol-position: top center;
                padding: 0 5px;
                color: #ffffff;
            }
            
            QPushButton {
                background-color: #007acc;
                color: white;
                border: none;
                padding: 8px 15px;
                border-radius: 4px;
                font-weight: bold;
            }
            
            QPushButton:hover {
                background-color: #0098ff;
            }
            
            QPushButton:pressed {
                background-color: #005999;
            }
            
            QLabel {
                color: #ffffff;
                font-size: 12px;
            }
        )";
        
        qApp->setStyleSheet(styleSheet);
    } catch (const std::exception& e) {
        qWarning() << "Failed to set application style:" << e.what();
    }
}

void MainWindow::setupUI()
{
    try {
        QWidget *centralWidget = new QWidget(this);
        setCentralWidget(centralWidget);

        // Main horizontal layout: Sidebar + Main content
        QHBoxLayout *mainHLayout = new QHBoxLayout(centralWidget);

        // --- Sidebar ---
        QWidget *sidebar = new QWidget();
        sidebar->setFixedWidth(160);
        QVBoxLayout *sidebarLayout = new QVBoxLayout(sidebar);
        sidebarLayout->setSpacing(10);
        sidebarLayout->setContentsMargins(0, 20, 0, 0);

        // Sidebar buttons: Only Processes and Performance
        QStringList sidebarItems = {"Processes", "Performance", "Troubleshoot"};
        QList<QPushButton*> sidebarButtons;
        for (const QString &item : sidebarItems) {
            QPushButton *btn = new QPushButton(item);
            btn->setCheckable(true);
            btn->setStyleSheet(R"(
                QPushButton {
                    background: transparent;
                    color: #fff;
                    text-align: left;
                    padding: 10px 20px;
                    border: none;
                    font-size: 15px;
                }
                QPushButton:checked {
                    background: #252525;
                    border-left: 4px solid #0078d4;
                    color: #0078d4;
                }
                QPushButton:hover {
                    background: #232323;
                }
            )");
            if (item == "Processes") btn->setChecked(true); // Default selection
            sidebarLayout->addWidget(btn);
            sidebarButtons.append(btn);
        }
        sidebarLayout->addStretch();

        // --- Main Content (Stacked) ---
        QWidget *mainContent = new QWidget();
        QVBoxLayout *mainVLayout = new QVBoxLayout(mainContent);
        mainVLayout->setSpacing(0);
        mainVLayout->setContentsMargins(0, 0, 0, 0);

        // Stacked widget for switching views
        QStackedWidget *stackedWidget = new QStackedWidget(mainContent);

        // --- Processes View ---
        QWidget *processesView = new QWidget();
        QVBoxLayout *processesLayout = new QVBoxLayout(processesView);
        processesLayout->setSpacing(0);
        processesLayout->setContentsMargins(0, 0, 0, 0);

        // Top Bar (reuse previous code)
        QWidget *topBar = new QWidget();
        topBar->setFixedHeight(48);
        QHBoxLayout *topBarLayout = new QHBoxLayout(topBar);
        topBarLayout->setContentsMargins(16, 8, 16, 8);
        topBarLayout->setSpacing(12);
        QLineEdit *searchBar = new QLineEdit();
        searchBar->setPlaceholderText("Type a name, publisher, or PID to search");
        searchBar->setMinimumWidth(320);
        searchBar->setStyleSheet(R"(
            QLineEdit {
                background: #232323;
                color: #fff;
                border-radius: 6px;
                border: 1px solid #333;
                padding: 6px 12px;
            }
            QLineEdit:focus {
                border: 1.5px solid #0078d4;
            }
        )");
        searchBox = searchBar; // assign to member for filtering
        connect(searchBar, &QLineEdit::textChanged, this, &MainWindow::onSearchTextChanged);
        topBarLayout->addWidget(searchBar);
        topBarLayout->addStretch();
        QPushButton *runTaskBtn = new QPushButton("Run new task");
        endTaskButton = new QPushButton("End task");
        efficiencyBtn = new QPushButton("Efficiency mode");
        efficiencyBtn->setCheckable(true);
        efficiencyBtn->setStyleSheet(R"(
            QPushButton {
                background: #333;
                color: #fff;
                border-radius: 4px;
                padding: 6px 16px;
                font-weight: bold;
            }
            QPushButton:checked {
                background: #4CAF50;
                color: white;
            }
            QPushButton:hover {
                background: #444;
            }
            QPushButton:checked:hover {
                background: #45a049;
            }
        )");
        connect(efficiencyBtn, &QPushButton::clicked, this, &MainWindow::toggleEfficiencyMode);
        connect(systemInfo, &SystemInfo::efficiencyModeChanged, this, &MainWindow::onEfficiencyModeChanged);
        topBarLayout->addWidget(runTaskBtn);
        topBarLayout->addWidget(endTaskButton);
        topBarLayout->addWidget(efficiencyBtn);

        // Resource Summary Row
        QWidget *resourceSummary = new QWidget();
        resourceSummary->setFixedHeight(48);
        QHBoxLayout *resourceLayout = new QHBoxLayout(resourceSummary);
        resourceLayout->setContentsMargins(16, 0, 16, 0);
        resourceLayout->setSpacing(32);
        // CPU
        cpuSumLabel = new QLabel("CPU: 0%");
        // Memory
        memSumLabel = new QLabel("Memory: 0%");
        // Disk
        diskSumLabel = new QLabel("Disk: 0%");
        // Network (stubbed)
        netSumLabel = new QLabel("Network: 0%");
        for (QLabel *lbl : {cpuSumLabel, memSumLabel, diskSumLabel, netSumLabel}) {
            lbl->setStyleSheet("color:#b0b0b0;font-weight:bold;font-size:14px;");
            resourceLayout->addWidget(lbl);
        }
        resourceLayout->addStretch();

        // Process Table
        processTable = new QTableWidget(this);
        processTable->setColumnCount(6);
        processTable->setHorizontalHeaderLabels({"Name", "Status", "CPU", "Memory (auto)", "Disk", "Network"});
        processTable->horizontalHeader()->setStyleSheet("QHeaderView::section{background:#232323;color:#fff;font-weight:bold;border:none;}");
        processTable->setStyleSheet(R"(
            QTableWidget {
                background: #181818;
                color: #fff;
                border: none;
                font-size: 14px;
                alternate-background-color: #232323;
            }
            QTableWidget::item:selected {
                background: #0078d4;
                color: #fff;
            }
        )");
        processTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        processTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        processTable->setAlternatingRowColors(true);
        processTable->setSortingEnabled(false);
        processTable->verticalHeader()->setVisible(false);
        processTable->horizontalHeader()->setStretchLastSection(true);

        processesLayout->addWidget(topBar);
        processesLayout->addWidget(resourceSummary);
        processesLayout->addWidget(processTable);

        // --- Performance View ---
        QWidget *performanceView = new QWidget();
        QVBoxLayout *perfLayout = new QVBoxLayout(performanceView);
        perfLayout->setContentsMargins(40, 40, 40, 40);
        perfLayout->setSpacing(32);
        // CPU
        QLabel *cpuPerfLabel = new QLabel("CPU Usage");
        QProgressBar *cpuPerfBarDetailed = new QProgressBar();
        cpuPerfBarDetailed->setRange(0, 100);
        cpuPerfBarDetailed->setStyleSheet("QProgressBar { border: 2px solid #3a3a3a; border-radius: 5px; text-align: center; background-color: #232323; color: white; } QProgressBar::chunk { background-color: #4CAF50; }");
        // Memory
        QLabel *memPerfLabel = new QLabel("Memory Usage");
        QProgressBar *memPerfBarDetailed = new QProgressBar();
        memPerfBarDetailed->setRange(0, 100);
        memPerfBarDetailed->setStyleSheet("QProgressBar { border: 2px solid #3a3a3a; border-radius: 5px; text-align: center; background-color: #232323; color: white; } QProgressBar::chunk { background-color: #2196F3; }");
        // Disk
        QLabel *diskPerfLabel = new QLabel("Disk Usage");
        QProgressBar *diskPerfBarDetailed = new QProgressBar();
        diskPerfBarDetailed->setRange(0, 100);
        diskPerfBarDetailed->setStyleSheet("QProgressBar { border: 2px solid #3a3a3a; border-radius: 5px; text-align: center; background-color: #232323; color: white; } QProgressBar::chunk { background-color: #FF9800; }");
        // Assign to members for real-time update
        cpuBar = cpuPerfBarDetailed;
        memoryBar = memPerfBarDetailed;
        diskBar = diskPerfBarDetailed;
        cpuLabel = cpuPerfLabel;
        memoryLabel = memPerfLabel;
        diskLabel = diskPerfLabel;
        // Add to layout
        perfLayout->addWidget(cpuPerfLabel);
        perfLayout->addWidget(cpuPerfBarDetailed);
        perfLayout->addWidget(memPerfLabel);
        perfLayout->addWidget(memPerfBarDetailed);
        perfLayout->addWidget(diskPerfLabel);
        perfLayout->addWidget(diskPerfBarDetailed);
        perfLayout->addStretch();

        // Add views to stacked widget
        stackedWidget->addWidget(processesView);    // index 0
        stackedWidget->addWidget(performanceView);  // index 1

        // --- Troubleshoot View ---
        QWidget *troubleshootView = new QWidget();
        QVBoxLayout *troubleshootLayout = new QVBoxLayout(troubleshootView);
        troubleshootLayout->setContentsMargins(40, 40, 40, 40);
        troubleshootLayout->setSpacing(32);

        // Process Health Check Section
        QGroupBox *healthCheckGroup = new QGroupBox("Process Health Check");
        QVBoxLayout *healthCheckLayout = new QVBoxLayout(healthCheckGroup);
        
        // Process Selection
        QHBoxLayout *processSelectLayout = new QHBoxLayout();
        processSelect = new QComboBox();
        processSelect->setMinimumWidth(300);
        processSelect->setMaxVisibleItems(15);  // Show 15 items at a time
        processSelect->setStyleSheet(R"(
            QComboBox {
                background-color: #232323;
                color: #ffffff;
                border: 1px solid #3a3a3a;
                border-radius: 4px;
                padding: 5px;
                min-height: 25px;
            }
            QComboBox::drop-down {
                border: none;
                width: 20px;
            }
            QComboBox::down-arrow {
                image: none;
                border-left: 5px solid transparent;
                border-right: 5px solid transparent;
                border-top: 5px solid #ffffff;
                margin-right: 5px;
            }
            QComboBox::drop-down {
                border: none;
                width: 20px;
            }
            QComboBox::down-arrow {
                image: none;
                border-left: 5px solid transparent;
                border-right: 5px solid transparent;
                border-top: 5px solid #ffffff;
                margin-right: 5px;
            }
            QScrollBar:vertical {
                background: #232323;
                width: 10px;
                margin: 0px;
            }
            QScrollBar::handle:vertical {
                background: #4a4a4a;
                min-height: 20px;
                border-radius: 5px;
            }
            QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
                height: 0px;
            }
        )");
        QPushButton *checkHealthBtn = new QPushButton("Check Health");
        QPushButton *endTaskBtn = new QPushButton("End Task");
        processSelectLayout->addWidget(new QLabel("Select Process:"));
        processSelectLayout->addWidget(processSelect);
        processSelectLayout->addWidget(checkHealthBtn);
        processSelectLayout->addWidget(endTaskBtn);
        processSelectLayout->addStretch();
        healthCheckLayout->addLayout(processSelectLayout);

        // Health Status Display
        QTextEdit *healthStatus = new QTextEdit();
        healthStatus->setReadOnly(true);
        healthStatus->setMinimumHeight(200);
        healthStatus->setStyleSheet(R"(
            QTextEdit {
                background-color: #232323;
                color: #ffffff;
                border: 1px solid #3a3a3a;
                border-radius: 4px;
                padding: 8px;
            }
        )");
        healthCheckLayout->addWidget(healthStatus);

        // Diagnostic Results
        QGroupBox *diagnosticGroup = new QGroupBox("Diagnostic Results");
        QVBoxLayout *diagnosticLayout = new QVBoxLayout(diagnosticGroup);
        QTableWidget *diagnosticTable = new QTableWidget();
        diagnosticTable->setColumnCount(3);
        diagnosticTable->setHorizontalHeaderLabels({"Issue", "Severity", "Recommendation"});
        diagnosticTable->setStyleSheet(R"(
            QTableWidget {
                background-color: #232323;
                color: #ffffff;
                border: 1px solid #3a3a3a;
                border-radius: 4px;
                gridline-color: #3a3a3a;
            }
            QHeaderView::section {
                background-color: #2d2d2d;
                color: #ffffff;
                padding: 8px;
                border: 1px solid #3a3a3a;
                font-weight: bold;
            }
        )");
        diagnosticLayout->addWidget(diagnosticTable);

        // Add groups to troubleshoot layout
        troubleshootLayout->addWidget(healthCheckGroup);
        troubleshootLayout->addWidget(diagnosticGroup);
        troubleshootLayout->addStretch();

        // Wrap the troubleshoot view in a scroll area
        QScrollArea *troubleshootScrollArea = new QScrollArea();
        troubleshootScrollArea->setWidget(troubleshootView);
        troubleshootScrollArea->setWidgetResizable(true);
        troubleshootScrollArea->setStyleSheet(R"(
            QScrollArea {
                border: none;
            }
            QScrollBar:vertical {
                background: #232323;
                width: 10px;
                margin: 0px;
            }
            QScrollBar::handle:vertical {
                background: #4a4a4a;
                min-height: 20px;
                border-radius: 5px;
            }
            QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
                height: 0px;
            }
        )");
        
        // Add scroll area to stacked widget
        stackedWidget->addWidget(troubleshootScrollArea);  // index 2

        mainVLayout->addWidget(stackedWidget);

        // Add sidebar and main content to main horizontal layout
        mainHLayout->addWidget(sidebar);
        mainHLayout->addWidget(mainContent);

        // --- Sidebar Navigation Logic ---
        // Connect all three buttons
        connect(sidebarButtons[0], &QPushButton::clicked, this, [=]() {
            sidebarButtons[0]->setChecked(true);
            sidebarButtons[1]->setChecked(false);
            sidebarButtons[2]->setChecked(false);
            stackedWidget->setCurrentIndex(0);
        });
        connect(sidebarButtons[1], &QPushButton::clicked, this, [=]() {
            sidebarButtons[0]->setChecked(false);
            sidebarButtons[1]->setChecked(true);
            sidebarButtons[2]->setChecked(false);
            stackedWidget->setCurrentIndex(1);
        });
        connect(sidebarButtons[2], &QPushButton::clicked, this, [=]() {
            sidebarButtons[0]->setChecked(false);
            sidebarButtons[1]->setChecked(false);
            sidebarButtons[2]->setChecked(true);
            stackedWidget->setCurrentIndex(2);
        });

        // Connect health check button
        connect(checkHealthBtn, &QPushButton::clicked, this, [=]() {
            QString selectedProcess = processSelect->currentText();
            if (selectedProcess.isEmpty()) {
                QMessageBox::warning(this, "Warning", "Please select a process to check.");
                return;
            }
            checkProcessHealth(selectedProcess, healthStatus, diagnosticTable);
        });

        // Connect end task button
        connect(endTaskBtn, &QPushButton::clicked, this, [=]() {
            QString selectedProcess = processSelect->currentText();
            if (selectedProcess.isEmpty()) {
                QMessageBox::warning(this, "Warning", "Please select a process to end.");
                return;
            }
            // Find the process in the process table
            for (int row = 0; row < processTable->rowCount(); ++row) {
                QTableWidgetItem* nameItem = processTable->item(row, 0);
                if (nameItem && nameItem->text() == selectedProcess) {
                    processTable->selectRow(row);
                    forceEndTask();
                    break;
                }
            }
        });

        // Update process list in combo box with scroll position preservation
        connect(systemInfo, &SystemInfo::dataUpdated, this, [this]() {
            // Get current selected index
            int currentIndex = processSelect->currentIndex();
            
            // Update combo box
            updateProcessComboBox(processSelect);
            
            // Try to restore the selected index
            if (currentIndex >= 0 && currentIndex < processSelect->count()) {
                processSelect->setCurrentIndex(currentIndex);
            }
        });

        // Connect End Task button
        connect(runTaskBtn, &QPushButton::clicked, this, &MainWindow::runNewTask);
        connect(endTaskButton, &QPushButton::clicked, this, &MainWindow::forceEndTask);
        connect(efficiencyBtn, &QPushButton::clicked, this, &MainWindow::toggleEfficiencyMode);
        connect(systemInfo, &SystemInfo::efficiencyModeChanged, this, &MainWindow::onEfficiencyModeChanged);
        // Enable/disable End Task button based on selection - modified to allow all processes
        connect(processTable, &QTableWidget::itemSelectionChanged, this, [=]() {
            QList<QTableWidgetItem*> selected = processTable->selectedItems();
            bool enable = false;
            if (!selected.isEmpty()) {
                int row = selected.first()->row();
                QTableWidgetItem* nameItem = processTable->item(row, 0);
                if (nameItem && !nameItem->text().isEmpty()) {
                    // Allow all processes, but we'll show different warnings for system processes
                    enable = true;
                }
            }
            endTaskButton->setEnabled(enable);
        });
        endTaskButton->setEnabled(false);

        // Connect header click to custom sort
        connect(processTable->horizontalHeader(), &QHeaderView::sectionClicked, this, &MainWindow::onTableHeaderClicked);
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Error", QString("Failed to setup UI: %1").arg(e.what()));
    }
}

QGroupBox* MainWindow::createResourceGroup()
{
    QGroupBox *groupBox = new QGroupBox("System Resources");
    QVBoxLayout *layout = new QVBoxLayout();
    
    // CPU usage
    QVBoxLayout *cpuLayout = new QVBoxLayout();
    cpuLabel = new QLabel("CPU Usage: 0%");
    cpuBar = new QProgressBar();
    cpuBar->setRange(0, 100);
    cpuBar->setStyleSheet("QProgressBar { border: 2px solid grey; border-radius: 5px; text-align: center; }"
                         "QProgressBar::chunk { background-color: #4CAF50; }");
    cpuLayout->addWidget(cpuLabel);
    cpuLayout->addWidget(cpuBar);
    
    // Memory usage
    QVBoxLayout *memoryLayout = new QVBoxLayout();
    memoryLabel = new QLabel("Memory Usage: 0%");
    memoryBar = new QProgressBar();
    memoryBar->setRange(0, 100);
    memoryBar->setStyleSheet("QProgressBar { border: 2px solid grey; border-radius: 5px; text-align: center; }"
                           "QProgressBar::chunk { background-color: #2196F3; }");
    memoryLayout->addWidget(memoryLabel);
    memoryLayout->addWidget(memoryBar);
    
    // Disk usage
    QVBoxLayout *diskLayout = new QVBoxLayout();
    diskLabel = new QLabel("Disk Usage: 0%");
    diskBar = new QProgressBar();
    diskBar->setRange(0, 100);
    diskBar->setStyleSheet("QProgressBar { border: 2px solid grey; border-radius: 5px; text-align: center; }"
                         "QProgressBar::chunk { background-color: #FF9800; }");
    diskLayout->addWidget(diskLabel);
    diskLayout->addWidget(diskBar);
    
    layout->addLayout(cpuLayout);
    layout->addLayout(memoryLayout);
    layout->addLayout(diskLayout);
    
    groupBox->setLayout(layout);
    return groupBox;
}

void MainWindow::onDataUpdated()
{
    // Throttle updates to prevent UI lag
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    if (currentTime - lastUpdateTime < UPDATE_INTERVAL_MS) {
        return;
    }
    lastUpdateTime = currentTime;
    
    // Update UI in the main thread
    QMetaObject::invokeMethod(this, &MainWindow::updateUI, Qt::QueuedConnection);
}

void MainWindow::updateUI()
{
    try {
        // Update resource usage with cached values
        updateResourceUsage();
        
        // Update process table with throttling
        if (shouldUpdateProcessTable()) {
            updateProcessTable();
        }
    } catch (const std::exception& e) {
        qWarning() << "Failed to update UI:" << e.what();
    }
}

void MainWindow::updateResourceUsage()
{
    // Prevent crash if UI not yet initialized
    if (!cpuBar || !memoryBar || !diskBar || !cpuLabel || !memoryLabel || !diskLabel ||
        !cpuSumLabel || !memSumLabel || !diskSumLabel || !netSumLabel)
        return;
    // Update resource bars with current values
    double cpuUsage = systemInfo->getCpuUsage();
    double memoryUsage = systemInfo->getMemoryUsage();
    double diskUsage = systemInfo->getDiskUsage();
    double networkUsage = systemInfo->getNetworkUsage();  // Get network usage in KB/s
    
    // Update progress bars
    cpuBar->setValue(static_cast<int>(cpuUsage));
    memoryBar->setValue(static_cast<int>(memoryUsage));
    diskBar->setValue(static_cast<int>(diskUsage));
    
    // Update detailed labels
    cpuLabel->setText(QString("CPU Usage: %1%").arg(cpuUsage, 0, 'f', 1));
    memoryLabel->setText(QString("Memory Usage: %1%").arg(memoryUsage, 0, 'f', 1));
    diskLabel->setText(QString("Disk Usage: %1%").arg(diskUsage, 0, 'f', 1));

    // Update summary labels
    cpuSumLabel->setText(QString("CPU: %1%").arg(cpuUsage, 0, 'f', 1));
    memSumLabel->setText(QString("Memory: %1%").arg(memoryUsage, 0, 'f', 1));
    diskSumLabel->setText(QString("Disk: %1%").arg(diskUsage, 0, 'f', 1));
    netSumLabel->setText(QString("Network: %1 KB/s").arg(networkUsage, 0, 'f', 1));  // Changed to KB/s

    qDebug() << "Resource values:" << cpuUsage << memoryUsage << diskUsage << networkUsage;
}

bool MainWindow::shouldUpdateProcessTable()
{
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    return (currentTime - processCache.timestamp) >= CACHE_DURATION_MS;
}

void MainWindow::updateProcessTable()
{
    try {
        // Use sortedProcesses (if available) or get a fresh list from systemInfo
        QVector<ProcessInfo> processes = sortedProcesses.isEmpty() ? systemInfo->getProcessList() : sortedProcesses;
        QString searchText = (searchBox) ? searchBox->text().toLower() : "";
        QMap<ProcessType, QVector<ProcessInfo>> grouped;
        for (const ProcessInfo &proc : processes) {
            if (shouldDisplayProcess(proc, searchText)) {
                grouped[proc.type].append(proc);
            }
        }
        int rowCount = 0;
        for (ProcessType type : groupOrder) {
            if (!grouped[type].isEmpty()) {
                rowCount += 1 + grouped[type].size();
            }
        }
        processTable->setRowCount(rowCount);
        int row = 0;
        for (ProcessType type : groupOrder) {
            if (grouped[type].isEmpty()) continue;
            // Group header (with process count)
            QString headerText = QString("%1 (%2)").arg(groupNames[type]).arg(grouped[type].size());
            QTableWidgetItem *headerItem = new QTableWidgetItem(headerText);
            headerItem->setFlags(Qt::NoItemFlags);
            headerItem->setBackground(QColor("#232323"));
            headerItem->setForeground(QColor("#80bfff"));
            QFont f = headerItem->font(); f.setBold(true); headerItem->setFont(f);
            processTable->setItem(row, 0, headerItem);
            processTable->setSpan(row, 0, 1, processTable->columnCount());
            processTable->setRowHeight(row, 28);
            row++;
            // Add processes for this group (using the sorted data)
            for (const ProcessInfo &proc : grouped[type]) {
                QTableWidgetItem *nameItem = new QTableWidgetItem(proc.name);
                QTableWidgetItem *statusItem = new QTableWidgetItem(proc.status);
                
                // CPU usage with color coding
                double cpuUsage = proc.cpuUsage;
                QString cpuText = QString::number(cpuUsage, 'f', 1) + "%";
                QTableWidgetItem *cpuItem = new QTableWidgetItem(cpuText);
                
                // Color coding based on CPU usage
                if (cpuUsage >= 80.0) {
                    cpuItem->setForeground(QColor("#FF4444")); // Red for high usage
                } else if (cpuUsage >= 50.0) {
                    cpuItem->setForeground(QColor("#FFA500")); // Orange for medium usage
                } else if (cpuUsage >= 20.0) {
                    cpuItem->setForeground(QColor("#FFD700")); // Yellow for moderate usage
                } else {
                    cpuItem->setForeground(QColor("#4CAF50")); // Green for low usage
                }
                
                QTableWidgetItem *memItem = new QTableWidgetItem(formatMemorySize(proc.memoryUsage));
                QTableWidgetItem *diskItem = new QTableWidgetItem(QString("%1 MB/s").arg(std::max(0.0, proc.diskUsage), 0, 'f', 2));
                QTableWidgetItem *netItem;
                if (proc.networkUsage < 0) {
                    netItem = new QTableWidgetItem("N/A");
                    netItem->setForeground(QColor("#888"));
                } else {
                    netItem = new QTableWidgetItem(QString("%1 MB/s").arg(proc.networkUsage, 0, 'f', 2));
                    netItem->setForeground(QColor("#00BFFF"));
                }
                
                nameItem->setForeground(QColor("#fff"));
                statusItem->setForeground(QColor("#b0b0b0"));
                memItem->setForeground(QColor("#2196F3"));
                diskItem->setForeground(QColor("#FF9800"));
                netItem->setForeground(QColor("#00BFFF"));
                
                // Color coding for disk usage
                if (proc.diskUsage >= 10.0) {
                    diskItem->setForeground(QColor("#FF4444")); // Red for high usage
                } else if (proc.diskUsage >= 5.0) {
                    diskItem->setForeground(QColor("#FFA500")); // Orange for medium usage
                } else if (proc.diskUsage >= 1.0) {
                    diskItem->setForeground(QColor("#FFD700")); // Yellow for moderate usage
                }
                
                // Color coding for network usage
                if (proc.networkUsage >= 5.0) {
                    netItem->setForeground(QColor("#FF4444")); // Red for high usage
                } else if (proc.networkUsage >= 2.0) {
                    netItem->setForeground(QColor("#FFA500")); // Orange for medium usage
                } else if (proc.networkUsage >= 0.5) {
                    netItem->setForeground(QColor("#FFD700")); // Yellow for moderate usage
                }
                
                processTable->setItem(row, 0, nameItem);
                processTable->setItem(row, 1, statusItem);
                processTable->setItem(row, 2, cpuItem);
                processTable->setItem(row, 3, memItem);
                processTable->setItem(row, 4, diskItem);
                processTable->setItem(row, 5, netItem);
                processTable->setRowHeight(row, 24);
                row++;
            }
        }
        processTable->resizeRowsToContents();
    } catch (const std::exception& e) {
        qWarning() << "Failed to update process table (grouped):" << e.what();
    }
}

bool MainWindow::shouldDisplayProcess(const ProcessInfo &process, const QString &searchText)
{
    // Check search text match
    bool matchesSearch = searchText.isEmpty() ||
        process.name.toLower().contains(searchText) ||
        QString::number(process.pid).contains(searchText) ||
        process.path.toLower().contains(searchText);
    
    // Check process type filter
    bool matchesType = currentProcessTypeFilter == ProcessType::Unknown ||
        process.type == currentProcessTypeFilter;
    
    return matchesSearch && matchesType;
}

void MainWindow::onProcessTypeFilterChanged(int index)
{
    currentProcessTypeFilter = static_cast<ProcessType>(
        processTypeFilter->itemData(index).toInt());
    updateProcessTable();
}

void MainWindow::onTableHeaderClicked(int column)
{
    static Qt::SortOrder lastOrder = Qt::AscendingOrder;
    lastOrder = (lastOrder == Qt::AscendingOrder) ? Qt::DescendingOrder : Qt::AscendingOrder;
    // Synchronously sort the underlying process list (using the helper)
    sortProcesses(column, lastOrder);
    // Then update the table (which now uses sortedProcesses if available)
    updateProcessTable();
}

void MainWindow::sortProcesses(int column, Qt::SortOrder order) {
    QVector<ProcessInfo> procList = systemInfo->getProcessList();
    std::sort(procList.begin(), procList.end(), [column, order](const ProcessInfo &a, const ProcessInfo &b) {
        if (column == 0) { // Name (string)
            return (order == Qt::AscendingOrder) ? (a.name < b.name) : (a.name > b.name);
        } else if (column == 1) { // Status (string)
            return (order == Qt::AscendingOrder) ? (a.status < b.status) : (a.status > b.status);
        } else if (column == 2) { // CPU (numeric)
            return (order == Qt::AscendingOrder) ? (a.cpuUsage < b.cpuUsage) : (a.cpuUsage > b.cpuUsage);
        } else if (column == 3) { // Memory (numeric)
            return (order == Qt::AscendingOrder) ? (a.memoryUsage < b.memoryUsage) : (a.memoryUsage > b.memoryUsage);
        } else if (column == 4) { // Disk (numeric)
            return (order == Qt::AscendingOrder) ? (a.diskUsage < b.diskUsage) : (a.diskUsage > b.diskUsage);
        } else if (column == 5) { // Network (numeric)
            return (order == Qt::AscendingOrder) ? (a.networkUsage < b.networkUsage) : (a.networkUsage > b.networkUsage);
        } else {
            return false;
        }
    });
    sortedProcesses = procList;
}

void MainWindow::onSearchTextChanged(const QString &text)
{
    updateProcessTable();  // This will apply the search filter
}

void MainWindow::sortByMemory()
{
    currentSortColumn = 4;  // Memory column
    currentSortOrder = Qt::DescendingOrder;
    processTable->sortItems(currentSortColumn, currentSortOrder);
}

void MainWindow::sortByCPU()
{
    currentSortColumn = 3;  // CPU column
    currentSortOrder = Qt::DescendingOrder;
    processTable->sortItems(currentSortColumn, currentSortOrder);
}

void MainWindow::sortByPID()
{
    currentSortColumn = 1;  // PID column
    currentSortOrder = Qt::AscendingOrder;
    processTable->sortItems(currentSortColumn, currentSortOrder);
}

QString MainWindow::formatTime(qint64 fileTime)
{
    FILETIME ft;
    ft.dwLowDateTime = (DWORD)(fileTime & 0xFFFFFFFF);
    ft.dwHighDateTime = (DWORD)(fileTime >> 32);
    
    SYSTEMTIME st;
    FileTimeToSystemTime(&ft, &st);
    
    return QString("%1-%2-%3 %4:%5:%6")
        .arg(st.wYear, 4, 10, QChar('0'))
        .arg(st.wMonth, 2, 10, QChar('0'))
        .arg(st.wDay, 2, 10, QChar('0'))
        .arg(st.wHour, 2, 10, QChar('0'))
        .arg(st.wMinute, 2, 10, QChar('0'))
        .arg(st.wSecond, 2, 10, QChar('0'));
}

QString MainWindow::formatMemorySize(qint64 kb)
{
    const char* units[] = {"KB", "MB", "GB", "TB"};
    int unit = 0;
    double size = static_cast<double>(kb);
    while (size >= 1024.0 && unit < 3) {
        size /= 1024.0;
        unit++;
    }
    return QString("%1 %2").arg(size, 0, 'f', 2).arg(units[unit]);
}

void MainWindow::toggleEfficiencyMode()
{
    bool newState = !efficiencyModeEnabled;
    if (newState) {
        // Create a custom dialog for efficiency mode
        QDialog dialog(this);
        dialog.setWindowTitle("Enable Efficiency Mode");
        dialog.setFixedWidth(400);  // Set fixed width for compactness
        
        QVBoxLayout *mainLayout = new QVBoxLayout(&dialog);
        mainLayout->setSpacing(10);
        
        // Header with description
        QLabel *headerLabel = new QLabel("Efficiency mode will optimize system performance by:");
        headerLabel->setStyleSheet("font-weight: bold; color: #fff;");
        mainLayout->addWidget(headerLabel);
        
        // Features list
        QLabel *featuresLabel = new QLabel(
            "• Reducing priority of background processes\n"
            "• Optimizing memory usage for non-essential processes\n"
            "• Throttling CPU usage for high-usage processes"
        );
        featuresLabel->setStyleSheet("color: #b0b0b0;");
        mainLayout->addWidget(featuresLabel);
        
        // Separator
        QFrame *line = new QFrame();
        line->setFrameShape(QFrame::HLine);
        line->setFrameShadow(QFrame::Sunken);
        line->setStyleSheet("background-color: #3a3a3a;");
        mainLayout->addWidget(line);
        
        // Scrollable area for process list
        QScrollArea *scrollArea = new QScrollArea();
        scrollArea->setWidgetResizable(true);
        scrollArea->setMaximumHeight(200);  // Limit height
        scrollArea->setStyleSheet(R"(
            QScrollArea {
                border: 1px solid #3a3a3a;
                border-radius: 4px;
                background-color: #232323;
            }
            QScrollBar:vertical {
                border: none;
                background: #232323;
                width: 10px;
                margin: 0px;
            }
            QScrollBar::handle:vertical {
                background: #4a4a4a;
                min-height: 20px;
                border-radius: 5px;
            }
            QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
                height: 0px;
            }
        )");
        
        QWidget *scrollContent = new QWidget();
        QVBoxLayout *scrollLayout = new QVBoxLayout(scrollContent);
        scrollLayout->setSpacing(5);
        
        // Add process list header
        QLabel *processHeader = new QLabel("High resource processes that will be affected:");
        processHeader->setStyleSheet("font-weight: bold; color: #fff;");
        scrollLayout->addWidget(processHeader);
        
        // Get and add high resource processes
        QVector<ProcessInfo> highResourceProcesses = systemInfo->getHighResourceProcesses();
        for (const ProcessInfo &proc : highResourceProcesses) {
            QString processText = QString("%1 (CPU: %2%, Memory: %3)")
                .arg(proc.name)
                .arg(proc.cpuUsage, 0, 'f', 1)
                .arg(formatMemorySize(proc.memoryUsage));
            
            QLabel *processLabel = new QLabel(processText);
            processLabel->setStyleSheet("color: #b0b0b0; padding: 2px;");
            scrollLayout->addWidget(processLabel);
        }
        
        scrollLayout->addStretch();
        scrollArea->setWidget(scrollContent);
        mainLayout->addWidget(scrollArea);
        
        // Button box
        QDialogButtonBox *buttonBox = new QDialogButtonBox(
            QDialogButtonBox::Yes | QDialogButtonBox::No
        );
        buttonBox->setStyleSheet(R"(
            QPushButton {
                min-width: 80px;
                padding: 6px 12px;
            }
        )");
        buttonBox->button(QDialogButtonBox::Yes)->setText("Enable");
        buttonBox->button(QDialogButtonBox::No)->setText("Cancel");
        mainLayout->addWidget(buttonBox);
        
        // Connect buttons
        connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        
        // Style the dialog
        dialog.setStyleSheet(R"(
            QDialog {
                background-color: #1e1e1e;
            }
            QLabel {
                color: #ffffff;
            }
        )");
        
        if (dialog.exec() == QDialog::Accepted) {
            systemInfo->setEfficiencyMode(true);
        } else {
            efficiencyBtn->setChecked(false);
        }
    } else {
        // Create a compact dialog for disabling efficiency mode
        QDialog dialog(this);
        dialog.setWindowTitle("Disable Efficiency Mode");
        dialog.setFixedWidth(300);
        
        QVBoxLayout *layout = new QVBoxLayout(&dialog);
        layout->setSpacing(10);
        
        QLabel *messageLabel = new QLabel(
            "This will restore normal process priorities and resource allocation.\n\n"
            "Do you want to disable efficiency mode?"
        );
        messageLabel->setStyleSheet("color: #ffffff;");
        messageLabel->setWordWrap(true);
        layout->addWidget(messageLabel);
        
        QDialogButtonBox *buttonBox = new QDialogButtonBox(
            QDialogButtonBox::Yes | QDialogButtonBox::No
        );
        buttonBox->setStyleSheet(R"(
            QPushButton {
                min-width: 80px;
                padding: 6px 12px;
            }
        )");
        buttonBox->button(QDialogButtonBox::Yes)->setText("Disable");
        buttonBox->button(QDialogButtonBox::No)->setText("Cancel");
        layout->addWidget(buttonBox);
        
        dialog.setStyleSheet(R"(
            QDialog {
                background-color: #1e1e1e;
            }
            QLabel {
                color: #ffffff;
            }
        )");
        
        connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        
        if (dialog.exec() == QDialog::Accepted) {
            systemInfo->setEfficiencyMode(false);
        } else {
            efficiencyBtn->setChecked(true);
        }
    }
}

void MainWindow::onEfficiencyModeChanged(bool enabled)
{
    efficiencyModeEnabled = enabled;
    updateEfficiencyButtonState();
    
    // Show status message
    if (enabled) {
        QMessageBox::information(this, "Efficiency Mode Enabled",
            "Efficiency mode is now active. System resources are being optimized.\n\n"
            "You can disable it at any time by clicking the Efficiency Mode button again.");
    } else {
        QMessageBox::information(this, "Efficiency Mode Disabled",
            "Efficiency mode has been disabled. All processes have been restored to their original priorities.");
    }
}

void MainWindow::updateEfficiencyButtonState()
{
    if (efficiencyBtn) {
        efficiencyBtn->setChecked(efficiencyModeEnabled);
        efficiencyBtn->setText(efficiencyModeEnabled ? "Efficiency mode: ON" : "Efficiency mode");
    }
}

void MainWindow::runNewTask()
{
    // Custom dialog for running a new task with browse
    QDialog dialog(this);
    dialog.setWindowTitle("Run New Task");
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    QHBoxLayout *inputLayout = new QHBoxLayout();
    QLineEdit *cmdEdit = new QLineEdit();
    cmdEdit->setPlaceholderText("Enter command or browse for an application...");
    QPushButton *browseBtn = new QPushButton("Browse...");
    inputLayout->addWidget(cmdEdit);
    inputLayout->addWidget(browseBtn);
    layout->addLayout(inputLayout);
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttonBox);
    // Browse button logic
    connect(browseBtn, &QPushButton::clicked, this, [&]() {
        QString file = QFileDialog::getOpenFileName(&dialog, "Select Application", QString(), "Executables (*.exe);;All Files (*)");
        if (!file.isEmpty()) {
            cmdEdit->setText(file);
        }
    });
    // OK/Cancel logic
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() == QDialog::Accepted) {
        QString cmd = cmdEdit->text().trimmed();
        if (!cmd.isEmpty()) {
            if (!QProcess::startDetached(cmd)) {
                QMessageBox::warning(this, "Error", "Failed to start the process.");
            }
        }
    }
}

void MainWindow::forceEndTask()
{
    try {
        QList<QTableWidgetItem*> selectedItems = processTable->selectedItems();
        if (selectedItems.isEmpty()) {
            QMessageBox::warning(this, "Warning", "Please select a process to end.");
            return;
        }

        int row = selectedItems.first()->row();
        QTableWidgetItem* nameItem = processTable->item(row, 0);
        if (!nameItem || nameItem->text().isEmpty()) {
            QMessageBox::warning(this, "Warning", "Please select a valid process.");
            return;
        }

        QString processName = nameItem->text();
        
        // Check if it's a system process
        bool isSystemProcess = false;
        QStringList systemProcesses = {
            "system", "registry", "csrss", "wininit", "services",
            "lsass", "svchost", "explorer", "taskmgr", "procmanager"
        };
        
        for (const QString& sysProc : systemProcesses) {
            if (processName.toLower().contains(sysProc)) {
                isSystemProcess = true;
                break;
            }
        }

        // Show appropriate warning dialog based on process type
        QString warningMessage;
        if (isSystemProcess) {
            warningMessage = QString(
                "WARNING: You are about to terminate a system process ('%1')!\n\n"
                "This is extremely dangerous and may cause:\n"
                "• System crash or blue screen\n"
                "• Data loss\n"
                "• System instability\n"
                "• Required system restart\n\n"
                "Are you absolutely sure you want to continue?\n"
                "This action cannot be undone!"
            ).arg(processName);
        } else {
            warningMessage = QString(
                "Are you sure you want to forcefully end '%1'?\n\n"
                "This action cannot be undone and may cause:\n"
                "• Data loss\n"
                "• Application instability\n"
                "• System instability\n\n"
                "Only use this if the process is not responding or causing problems."
            ).arg(processName);
        }

        // Show warning with appropriate icon and buttons
        QMessageBox::Icon icon = isSystemProcess ? QMessageBox::Critical : QMessageBox::Warning;
        QMessageBox::StandardButton reply = QMessageBox::warning(this, 
            isSystemProcess ? "CRITICAL WARNING: System Process Termination" : "Force End Task",
            warningMessage,
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No  // Default to No for safety
        );

        if (reply == QMessageBox::Yes) {
            // For system processes, show one final confirmation
            if (isSystemProcess) {
                QMessageBox::StandardButton finalReply = QMessageBox::critical(this,
                    "FINAL CONFIRMATION",
                    "You are about to terminate a critical system process.\n"
                    "This will likely crash your system.\n\n"
                    "Are you absolutely certain you want to proceed?",
                    QMessageBox::Yes | QMessageBox::No,
                    QMessageBox::No
                );
                
                if (finalReply != QMessageBox::Yes) {
                    return;
                }
            }

            // Convert QString to std::wstring for the process name
            std::wstring wProcessName = processName.toStdWString();
            
            // Create snapshot of all processes
            HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
            if (hSnap == INVALID_HANDLE_VALUE) {
                QMessageBox::warning(this, "Error", "Failed to create process snapshot.");
                return;
            }

            PROCESSENTRY32W pe;
            pe.dwSize = sizeof(PROCESSENTRY32W);
            bool processFound = false;

            if (Process32FirstW(hSnap, &pe)) {
                do {
                    if (wProcessName == pe.szExeFile) {
                        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                        if (hProcess != NULL) {
                            if (TerminateProcess(hProcess, 0)) {
                                processFound = true;
                                QString successMessage = isSystemProcess ? 
                                    QString("WARNING: System process '%1' has been terminated. Your system may become unstable.").arg(processName) :
                                    QString("Process '%1' has been terminated.").arg(processName);
                                
                                QMessageBox::information(this, "Success", successMessage);
                            }
                            CloseHandle(hProcess);
                        }
                    }
                } while (Process32NextW(hSnap, &pe));
            }

            CloseHandle(hSnap);

            if (!processFound) {
                QMessageBox::warning(this, "Error", 
                    QString("Failed to terminate process '%1'.\n"
                           "The process may be protected by the system or require administrator privileges.")
                        .arg(processName));
            }
        }
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Error", 
            QString("Failed to end task: %1").arg(e.what()));
    }
}

void MainWindow::updateProcessComboBox(QComboBox *comboBox)
{
    if (!comboBox) return;
    
    // Save current selection and view state
    QString currentSelection = comboBox->currentText();
    int currentIndex = comboBox->currentIndex();
    
    // Block signals to prevent unnecessary updates
    bool wasBlocked = comboBox->blockSignals(true);
    
    // Clear and update items
    comboBox->clear();
    QVector<ProcessInfo> processes = systemInfo->getProcessList();
    for (const ProcessInfo &proc : processes) {
        comboBox->addItem(proc.name);
    }
    
    // Restore selection and view position
    int newIndex = comboBox->findText(currentSelection);
    if (newIndex >= 0) {
        comboBox->setCurrentIndex(newIndex);
    } else if (currentIndex >= 0 && currentIndex < comboBox->count()) {
        comboBox->setCurrentIndex(currentIndex);
    }
    
    // Ensure the view is properly updated
    comboBox->view()->update();
    
    // Get model index and scroll to it
    QAbstractItemModel *model = comboBox->model();
    if (model) {
        QModelIndex index = model->index(comboBox->currentIndex(), 0);
        if (index.isValid()) {
            comboBox->view()->scrollTo(index, QAbstractItemView::PositionAtCenter);
        }
    }
    
    // Restore signal blocking state
    comboBox->blockSignals(wasBlocked);
}

void MainWindow::checkProcessHealth(const QString &processName, QTextEdit *statusDisplay, QTableWidget *diagnosticTable)
{
    statusDisplay->clear();
    diagnosticTable->setRowCount(0);
    
    // Get process information
    QVector<ProcessInfo> processes = systemInfo->getProcessList();
    ProcessInfo targetProcess;
    bool found = false;
    
    for (const ProcessInfo &proc : processes) {
        if (proc.name == processName) {
            targetProcess = proc;
            found = true;
            break;
        }
    }
    
    if (!found) {
        statusDisplay->setText("Process not found or no longer running.");
        return;
    }
    
    // Analyze process health
    QString status;
    QList<QStringList> issues;
    bool hasCriticalIssues = false;
    
    // Check CPU usage
    if (targetProcess.cpuUsage > 80.0) {
        issues.append({"High CPU Usage", "High", "Consider closing unnecessary applications or restarting the process."});
    } else if (targetProcess.cpuUsage > 50.0) {
        issues.append({"Moderate CPU Usage", "Medium", "Monitor the process for unusual behavior."});
    }
    
    // Check memory usage
    if (targetProcess.memoryUsage > 1024 * 1024 * 1024) { // 1GB
        issues.append({"High Memory Usage", "High", "Check for memory leaks or consider increasing system memory."});
    }
    
    // Check if process is responding
    if (targetProcess.status == "Not Responding") {
        issues.append({"Process Not Responding", "Critical", "Try ending the process and restarting it."});
        hasCriticalIssues = true;
    }
    
    // Check disk usage
    if (targetProcess.diskUsage > 10.0) {
        issues.append({"High Disk Usage", "Medium", "Check for disk-intensive operations."});
    }
    
    // Update status display
    status = QString("Process Health Report for: %1\n\n").arg(processName);
    status += QString("CPU Usage: %1%\n").arg(targetProcess.cpuUsage, 0, 'f', 1);
    status += QString("Memory Usage: %1\n").arg(formatMemorySize(targetProcess.memoryUsage));
    status += QString("Disk Usage: %1 MB/s\n").arg(targetProcess.diskUsage, 0, 'f', 2);
    status += QString("Status: %1\n\n").arg(targetProcess.status);
    
    if (issues.isEmpty()) {
        status += "No issues detected. Process appears to be running normally.";
    } else {
        status += "Issues detected. See diagnostic results below.";
        if (hasCriticalIssues) {
            status += "\n\nCritical issues detected! Consider ending the task.";
        }
    }
    
    statusDisplay->setText(status);
    
    // Update diagnostic table
    diagnosticTable->setRowCount(issues.size());
    for (int i = 0; i < issues.size(); ++i) {
        const QStringList &issue = issues[i];
        diagnosticTable->setItem(i, 0, new QTableWidgetItem(issue[0]));
        diagnosticTable->setItem(i, 1, new QTableWidgetItem(issue[1]));
        diagnosticTable->setItem(i, 2, new QTableWidgetItem(issue[2]));
        
        // Color code severity
        QColor severityColor;
        if (issue[1] == "Critical") {
            severityColor = QColor("#FF4444");
            hasCriticalIssues = true;
        }
        else if (issue[1] == "High") severityColor = QColor("#FFA500");
        else if (issue[1] == "Medium") severityColor = QColor("#FFD700");
        else severityColor = QColor("#4CAF50");
        
        diagnosticTable->item(i, 1)->setForeground(severityColor);
    }
    
    diagnosticTable->resizeColumnsToContents();

    // If critical issues are found, show end task dialog
    if (hasCriticalIssues) {
        QMessageBox::StandardButton reply = QMessageBox::question(this,
            "Critical Process Issue",
            QString("The process '%1' has critical issues.\n\nWould you like to end this task?").arg(processName),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::Yes
        );

        if (reply == QMessageBox::Yes) {
            // Find the process in the process table
            for (int row = 0; row < processTable->rowCount(); ++row) {
                QTableWidgetItem* nameItem = processTable->item(row, 0);
                if (nameItem && nameItem->text() == processName) {
                    processTable->selectRow(row);
                    forceEndTask();
                    break;
                }
            }
        }
    }
} 