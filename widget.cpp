#include "widget.h"
#include "parameters.h"
#include <QFormLayout>
#include <QButtonGroup>
#include <QRadioButton>
#include <QIntValidator>
#include <QFileDialog>
#include <QLockFile>
#include <QSaveFile>
#include <QtConcurrent>


Widget::Widget(QWidget *parent)
    : QWidget(parent)
{
    QFormLayout *formLayout = new QFormLayout(this);
    formLayout->addRow(inputPath = new QLabel(this));
    inputPath->setMaximumWidth(400);
    formLayout->addRow(changeInputPath = new QPushButton(tr("&Select working directory"),this));
    connect(changeInputPath, &QPushButton::clicked, this, &Widget::inputPathButtonClicked);
    formLayout->addRow(tr("&Filename mask:"), fileMask = new QLineEdit(this));

    QButtonGroup* inputDeletion = new QButtonGroup(this);
    inputDeletion->addButton(inputDeletionNeeded = new QRadioButton("yes", this));
    formLayout->addRow(tr("Need to &delete input files:"), inputDeletionNeeded);
    QRadioButton* noInputDeletion = new QRadioButton("no", this);
    noInputDeletion->setChecked(true);
    inputDeletion->addButton(noInputDeletion);
    formLayout->addRow(tr(""), noInputDeletion);

    formLayout->addRow(outputPath = new QLabel(this));
    outputPath->setMaximumWidth(400);
    formLayout->addRow(changeOutputPath = new QPushButton(tr("Select &output directory")));
    connect(changeOutputPath, &QPushButton::clicked, this, &Widget::outputPathButtonClicked);


    QButtonGroup* nameConflictAction = new QButtonGroup(this);
    nameConflictAction->addButton(sameNameOverwrite = new QRadioButton("overwrite", this));
    formLayout->addRow(tr("On output file name &conflict:"), sameNameOverwrite);
    nameConflictAction->addButton(sameNameIncrement = new QRadioButton("increment counter", this));
    sameNameIncrement->setChecked(true);
    formLayout->addRow(tr(""), sameNameIncrement);
    nameConflictAction->addButton(sameNamePass = new QRadioButton("do nothing", this));
    formLayout->addRow(tr(""), sameNamePass);

    QButtonGroup* useTimer = new QButtonGroup(this);
    useTimer->addButton(timerNeeded = new QRadioButton("yes", this));
    formLayout->addRow(tr("&Repeat on timer:"), timerNeeded);
    QRadioButton* timerNotNeeded = new QRadioButton("no", this);
    timerNotNeeded->setChecked(true);
    useTimer->addButton(timerNotNeeded);
    formLayout->addRow(tr(""), timerNotNeeded);

    formLayout->addRow(tr("Restart &time (seconds):"), restartTime = new QLineEdit(this));
    restartTime->setValidator(new QIntValidator(restartTime));

    formLayout->addRow(tr("8 &bytes to work with (hex):"), modifierSequence = new QLineEdit(this));
    modifierSequence->setText     ("00 00 00 00 00 00 00 00");
    modifierSequence->setInputMask("HH HH HH HH HH HH HH HH");
    formLayout->addRow(start = new QPushButton("&Process files"));
    start->setCheckable(true);
    connect(start, &QPushButton::clicked, this, &Widget::startButtonClicked);

    timer = new QTimer(this);

    connect(&processFutureWatcher, &QFutureWatcher<void>::finished,
                this, [this] { finish(); });
}

Widget::~Widget()
{
}

void Widget::inputPathButtonClicked() {
    inputPath->setText(QFileDialog::getExistingDirectory(this));
}

void Widget::outputPathButtonClicked() {
    outputPath->setText(QFileDialog::getExistingDirectory(this));
}

void Widget::processFile(const Parameters& parameters,const QString& fname) {
    QLockFile lf(fname + ".lock");
    if (lf.tryLock()) {
        QFile inFile(parameters.inPath + "/" + fname);
        QString outFileName = parameters.outPath + "/" + fname;
        if (QFile::exists(outFileName)) {
            if (!parameters.nameConflictIncrement && !parameters.nameConflictOverwrite){
                lf.unlock();
                return;
            }
            if (parameters.nameConflictIncrement) {
                auto fdots = fname.split(".");
                QString ext = fdots.size() > 1 ? fdots.last() : "";
                if(fdots.size() > 1) {
                    fdots.removeLast();
                }
                int f = 1;
                while(QFile::exists(parameters.outPath + "/" + fdots.join(".") + " " + QString::number(f) + "." + ext))
                    f++;
                outFileName = parameters.outPath + "/" + fdots.join(".") + " " + QString::number(f) + "." + ext;
            }
        }
        QSaveFile outFile(outFileName);
        quint64 size = inFile.size();
        if (inFile.open(QIODevice::ReadOnly) && outFile.open(QIODevice::WriteOnly)) {
            QDataStream in(&inFile);
            QDataStream out(&outFile);
            quint64 data;
            size_t i;
            for(i = 8; i < size && in.atEnd() == false; i += 8) {
                in >> data;
                data ^= parameters.modifierU64;
                out << data;
            }
            quint8 byte;
            size_t j = 0;
            while(in.atEnd() == false) {
                in >> byte;
                byte ^= parameters.modifier.at(j++);
                out << byte;
            }
            inFile.close();
            if(parameters.deleteInputs) {
                inFile.remove();
            }
            outFile.commit();
        }

        lf.unlock();
    }
}

void Widget::startButtonClicked() {
    if (start->isChecked()) {
        start->setText(tr("Cancel"));
        {
            parameters.deleteInputs = inputDeletionNeeded->isChecked();
            parameters.nameConflictOverwrite = sameNameOverwrite->isChecked();
            parameters.nameConflictIncrement = sameNameIncrement->isChecked();
            parameters.inPath = inputPath->text();
            parameters.outPath = outputPath->text();
            parameters.timer = timerNeeded->isChecked();

            parameters.modifier = QByteArray::fromHex(modifierSequence->text().replace(" ", "").toStdString().c_str());
            parameters.modifierU64 = qFromBigEndian<quint64>((uchar*)parameters.modifier.data());
        }

        QDir inDirectory(inputPath->text());
        files = inDirectory.entryList(QStringList() << ("*" + fileMask->text() + "*"), QDir::Files);

        std::function<void(const QString&)> process = [&](const QString& fname){
            return Widget::processFile(parameters, fname);
        };
        if (parameters.timer) {
            timer->start(restartTime->text().toInt()*1000);
            connect(timer, &QTimer::timeout, this, [&, process](){
                processFutureWatcher.waitForFinished();
                files = QDir(inputPath->text()).entryList(QStringList() << ("*" + fileMask->text() + "*"), QDir::Files);
                processFutureWatcher.setFuture(QtConcurrent::map(files, process));
            });
        }

        processFutureWatcher.setFuture(QtConcurrent::map(files, process));
    } else {
        timer->disconnect();
        processFutureWatcher.cancel();
        start->setText(tr("Start"));
    }
}

void Widget::finish() {
    if (processFutureWatcher.isCanceled())
            return;


    if (!parameters.timer){
        start->setChecked(false);
        start->setText(tr("Start"));
    }
}

void Widget::startConcurrent() {

    start->setChecked(true);
    start->setText(tr("Cancel"));
}
