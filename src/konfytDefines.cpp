#include "konfytDefines.h"



QString sanitiseFilename(QString filename)
{
    static QString invalid = "<>:\"/\\|?*";

    QString ret = filename;
    ret.remove(QChar('\0'));
    for (int i=0; i < invalid.length(); i++) {
        ret.remove(invalid.at(i));
    }
    ret = ret.simplified();

    return ret;
}


