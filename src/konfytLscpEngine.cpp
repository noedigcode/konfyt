#include "konfytLscpEngine.h"

KonfytLscpEngine::KonfytLscpEngine(QObject *parent) : KonfytBaseSoundEngine(parent)
{
    connect(&ls, &GidLs::print, this, &KonfytLscpEngine::userMessage);
    connect(&ls, &GidLs::initialised, [this](bool error, QString errMsg){
        if (error) {
            userMessage("LSCP engine initialisation error:");
            userMessage(errMsg);
        } else {
            userMessage("LSCP engine initialised!");
        }
    });
}

KonfytLscpEngine::~KonfytLscpEngine()
{
    ls.deinit();
}

void KonfytLscpEngine::initEngine(KonfytJackEngine *jackEngine)
{
    ls.init( jackEngine->clientName() + "_LS" );
}

QString KonfytLscpEngine::jackClientName()
{
    return ls.jackClientName();
}

int KonfytLscpEngine::addSfz(QString path)
{
    return ls.addSfzChannelAndPorts(path);
}

QString KonfytLscpEngine::pluginName(int id)
{
    return "LS_sfz_" + n2s(id);
}

QString KonfytLscpEngine::midiInJackPortName(int id)
{
    GidLsChannel info = ls.getSfzChannelInfo(id);
    return info.midiJackPort;
}

QStringList KonfytLscpEngine::audioOutJackPortNames(int id)
{
    GidLsChannel info = ls.getSfzChannelInfo(id);
    QStringList ret;
    ret.append(info.audioLeftJackPort);
    ret.append(info.audioRightJackPort);
    return ret;
}

void KonfytLscpEngine::removeSfz(int id)
{
    ls.removeSfzChannel(id);
}

void KonfytLscpEngine::setGain(int id, float newGain)
{
    userMessage("TODO: setGain " + n2s(id) + " to " + n2s(newGain));
}
