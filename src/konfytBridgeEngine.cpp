#include "konfytBridgeEngine.h"

KonfytBridgeEngine::KonfytBridgeEngine(QObject *parent) :
    KonfytBaseSoundEngine(parent),
    jack(NULL),
    idCounter(300)
{

}

void KonfytBridgeEngine::setKonfytExePath(QString path)
{
    exePath = path;
}

void KonfytBridgeEngine::initEngine(KonfytJackEngine *jackEngine)
{
    jack = jackEngine;
    userMessage("KonfytBridgeEngine initialised.");
}
