#include "konfytStructs.h"


KonfytReset konfytResetFromString(QString s, KonfytReset defaultValue)
{
    if (s.toLower() == "inherit") {
        return KonfytReset::Inherit;
    } else if (s.toLower() == "reset") {
        return KonfytReset::Reset;
    } else if (s.toLower() == "noreset") {
        return KonfytReset::NoReset;
    } else {
        return defaultValue;
    }
}

QString konfytResetToString(KonfytReset value)
{
    if (value == KonfytReset::Inherit) {
        return "Inherit";
    } else if (value == KonfytReset::Reset) {
        return "Reset";
    } else {
        return "NoReset";
    }
}

KonfytReset konfytResetFromInherits(QList<KonfytReset> inheritChain, KonfytReset defaultValue)
{
    KonfytReset ret = KonfytReset::Inherit;
    foreach (KonfytReset option, inheritChain) {
        if (ret == KonfytReset::Inherit) {
            ret = option;
        } else {
            break;
        }
    }
    if (ret == KonfytReset::Inherit) {
        ret = defaultValue;
    }
    return ret;
}
