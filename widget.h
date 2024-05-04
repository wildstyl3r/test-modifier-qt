#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QRadioButton>
#include <QtConcurrent>
#include "parameters.h"


class Widget : public QWidget
{
    Q_OBJECT
    QLabel* inputPath;
    QPushButton* changeInputPath;
    QLineEdit* fileMask;
    QRadioButton* inputDeletionNeeded;
    QLabel* outputPath;
    QPushButton* changeOutputPath;
    QRadioButton* sameNameOverwrite;
    QRadioButton* sameNameIncrement;
    QRadioButton* sameNamePass;
    QRadioButton* timerNeeded;
    QLineEdit* restartTime;
    QLineEdit* modifierSequence;

    QTimer* timer;

    QPushButton* start;

    QStringList files;


    Parameters parameters;
    QFutureWatcher<void> processFutureWatcher;

    static void processFile(const Parameters& p, const QString& filename);

public:
    Widget(QWidget *parent = nullptr);
    ~Widget();

private slots:
  void inputPathButtonClicked();
  void outputPathButtonClicked();
  void startButtonClicked();
  void finish();
  void startConcurrent();
};
#endif // WIDGET_H
