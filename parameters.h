#ifndef PARAMETERS_H
#define PARAMETERS_H

#include <QString>

struct Parameters{
    bool deleteInputs;
    bool nameConflictOverwrite;
    bool nameConflictIncrement;
    bool timer;
    QString inPath;
    QString outPath;
    quint64 modifierU64;
    QByteArray modifier;
};

#endif // PARAMETERS_H
