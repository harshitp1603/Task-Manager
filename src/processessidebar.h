#ifndef PROCESSESSIDEBAR_H
#define PROCESSESSIDEBAR_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QTableWidget>
#include <QHeaderView>
#include "systeminfo.h"

class ProcessesSidebar : public QWidget {
    Q_OBJECT

public:
    explicit ProcessesSidebar(QWidget *parent = nullptr);
    ~ProcessesSidebar();

signals:
    void processSelected(const QString &processName);
    void processTypeFilterChanged(int index);
    void searchTextChanged(const QString &text);

private slots:
    void onSearchTextChanged(const QString &text);
    void onProcessTypeFilterChanged(int index);
    void onTableHeaderClicked(int column);
    void updateProcessList(const QVector<ProcessInfo> &processes);

private:
    QVBoxLayout *mainLayout;
    QLineEdit *searchBox;
    QComboBox *processTypeFilter;
    QTableWidget *processTable;
    QPushButton *refreshButton;
    QPushButton *sortMemoryButton;
    QPushButton *sortCPUButton;
    QPushButton *sortPIDButton;

    void setupUI();
    void setupProcessTable();
    void setupSearchBox();
    void setupProcessTypeFilter();
    void setupSortingButtons();
    void applySorting(int column, Qt::SortOrder order);
    bool shouldDisplayProcess(const ProcessInfo &process, const QString &searchText);
    QTableWidgetItem* createTableItem(const QString &text, const QString &style, double sortValue = 0.0);
};

#endif // PROCESSESSIDEBAR_H 