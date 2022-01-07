/******************************************************************************
 *
 * Copyright 2021 Gideon van der Kolf
 *
 * This file is part of Konfyt.
 *
 *     Konfyt is free software: you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation, either version 3 of the License, or
 *     (at your option) any later version.
 *
 *     Konfyt is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public License
 *     along with Konfyt.  If not, see <http://www.gnu.org/licenses/>.
 *
 *****************************************************************************/

#include "konfytLscp.h"

KonfytLscp::KonfytLscp(QObject *parent) : QObject(parent)
{

}

lscp_status_t KonfytLscp::client_callback(lscp_client_t* /*pClient*/, lscp_event_t event, const char *pchData, int cchData, void* /*pvData*/)
{
    lscp_status_t ret = LSCP_FAILED;

    char *pszData = (char *) malloc(cchData + 1);
    if (pszData) {
        memcpy(pszData, pchData, cchData);
        pszData[cchData] = (char) 0;
        printf("client_callback: event=%s (0x%04x) [%s]\n", lscp_event_to_text(event), (int) event, pszData);
        free(pszData);
        ret = LSCP_OK;
    }

    return ret;
}

void KonfytLscp::init()
{
    print("Initialising client.");
    client = lscp_client_create("localhost", SERVER_PORT, client_callback, NULL);
    if (client == NULL) {

        print("Could not initialise client. Attempting to run Linuxsampler...");
        QProcess* p = new QProcess();
        connect(p, &QProcess::started, [this, p](){

            print("Linuxsampler started. Attempting to initialise client after delay.");
            QTimer* t = new QTimer(this);
            t->setSingleShot(true);
            connect(t, &QTimer::timeout, [this, t](){

                print("Initialising client.");
                client = lscp_client_create("localhost", SERVER_PORT, client_callback, NULL);
                if (client == NULL) {
                    print("Could not initialise client. Retrying after delay...");
                    t->start(1000);
                } else {
                    t->deleteLater();
                    print("Client initialised.");
                    emit initialised(false, "");
                }
            });

            t->start(1000);
        });
        connect(p, static_cast<void(QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
              [=](int exitCode, QProcess::ExitStatus /*exitStatus*/){
            print(QString("Linuxsampler process finished with exit code %1.").arg(exitCode));
        });
#if (QT_VERSION >= QT_VERSION_CHECK(5, 6, 0))
        connect(p, &QProcess::errorOccurred,
                [=](QProcess::ProcessError /*error*/){
            print("Error occurred running Linuxsampler process: " + p->errorString());
        });
#endif

        p->start("linuxsampler");

    } else {
        print("Client initialised.");
        emit initialised(false, "");
    }
}

void KonfytLscp::setupDevices(QString clientName)
{
    mClientName = clientName;

    print("Connected to Linuxsampler.");

    lscp_set_volume(client, 1);

    if (audioDeviceExists(mClientName)) {
        print("Audio device '" + mClientName + "' already exists.");
    } else {
        print("Creating audio device '" + mClientName + "'.");
        if (addAudioDevice(mClientName)) {
            print("Audio device created.");
        } else {
            print("Error creating audio device.");
        }
    }

    if (midiDeviceExists(mClientName)) {
        print("MIDI device '" + mClientName + "' already exists.");
    } else {
        print("Creating MIDI device '" + mClientName + "'.");
        if (addMidiDevice(mClientName)) {
            print("MIDI device created.");
        } else {
            print("Error creating MIDI device.");
        }
    }
}

void KonfytLscp::deinit()
{
    removeAllRelatedToClient(mClientName);
    lscp_client_destroy(client);
}

void KonfytLscp::printEngines()
{
    const char** engines = lscp_list_available_engines(client);
    int i=0;
    print("Engines:");
    while (engines[i] != NULL) {
        print(" - " + QString(engines[i]));
        i++;
    }
}

void KonfytLscp::resetSampler()
{
    print("Resetting sampler");
    lscp_status_t rst = lscp_reset_sampler(client);
    if (rst != LSCP_OK) {
        print("Error resetting sampler.");
    }
}

void KonfytLscp::removeAllRelatedToClient(QString clientName)
{
    print("Removing everything related to " + clientName + "...");

    // Get audio device with name
    int adev = getAudioDeviceIdByName(clientName);
    if (adev >= 0) {
        print("    Found audio device: " + i2s(adev));
    } else {
        print("    No audio device found.");
    }

    // Get MIDI device with name
    int mdev = getMidiDeviceIdByName(clientName);
    if (mdev >= 0) {
        print("    Found MIDI device: " + i2s(mdev));
    } else {
        print("    No MIDI device found.");
    }

    // Remove all channels (sfzs) connected to above devices
    int* chans = lscp_list_channels(client);
    if (chans != NULL) {
        int i = 0;
        while (chans[i] >= 0) {
            lscp_channel_info_t* info = lscp_get_channel_info(client, chans[i]);
            bool remove = false;
            if ( (adev >= 0) && (info->audio_device == adev) ) { remove = true; }
            if ( (mdev >= 0) && (info->midi_device == mdev) ) { remove = true; }
            if (remove) {
                print(QString("    Found related channel: %1").arg(info->instrument_file));
                lscp_status_t ok = lscp_remove_channel(client, chans[i]);
                if (ok == LSCP_OK) {
                    print("        Channel removed.");
                } else {
                    print("        Could not remove channel.");
                }
            }
            i++;
        }
    }

    if (adev >= 0) {
        lscp_status_t ok = lscp_destroy_audio_device(client, adev);
        if (ok == LSCP_OK) {
            print("    Destroyed audio device.");
        } else {
            print("    Could not destroy audio device.");
        }
    }
    if (mdev >= 0) {
        lscp_status_t ok = lscp_destroy_midi_device(client, mdev);
        if (ok == LSCP_OK) {
            print("    Destoryed MIDI device.");
        } else {
            print("    Could not destroy MIDI device.");
        }
    }
}

void KonfytLscp::refreshAudioDevices()
{
    adevs.clear();

    int* devs = lscp_list_audio_devices(client);
    if (devs != NULL) {
        int i=0;
        while (devs[i] >= 0) {
            lscp_device_info_t* dev = lscp_get_audio_device_info(client, devs[i]);
            if (dev != NULL) {
                LsDevice device(client, devs[i], true, dev);
                adevs.insert(devs[i], device);
            }
            i++;
        }
    }
}

bool KonfytLscp::audioDeviceExists(QString name)
{
    return getAudioDeviceIdByName(name) >= 0;
}

int KonfytLscp::getAudioDeviceIdByName(QString name)
{
    refreshAudioDevices();

    int ret = -1;

    QList<int> keys = adevs.keys();
    for (int i=0; i < keys.count(); i++) {
        int key = keys[i];
        LsDevice &dev = adevs[key];
        if (dev.name() == name) {
            ret = key;
            break;
        }
    }
    return ret;
}

bool KonfytLscp::addAudioDevice(QString name)
{
    lscp_param_t params[3];
    params[0].key = (char*)KEY_NAME; params[0].value = (char*)name.toLocal8Bit().constData();
    params[1].key = (char*)KEY_CHANNELS; params[1].value = (char*)VAL_0;
    params[2].key = NULL; params[2].value = NULL;
    int ret = lscp_create_audio_device(client, "JACK", params);

    return ret >= 0;
}

QString KonfytLscp::printDevices()
{
    QString ret;

    refreshAudioDevices();
    refreshMidiDevices();

    ret += "Audio devices:\n";
    for (int i=0; i < adevs.count(); i++) {
        LsDevice &d = adevs[i];
        ret += " - " + d.toString();
        ret += indentString(d.paramsToString(), "   ");
    }
    ret += "\nMIDI devices:\n";
    for (int i=0; i < mdevs.count(); i++) {
        LsDevice &d = mdevs[i];
        ret += " - " + d.toString();
        ret += indentString(d.paramsToString(), "   ");
    }

    return ret;
}

QString KonfytLscp::printChannels()
{
    QString ret;

    int* chans = lscp_list_channels(client);
    if (chans == NULL) {
        ret += "No Channels.";
        return ret;
    }
    int i=0;
    while (chans[i] >= 0) {
        ret += "Channel " + i2s(chans[i]) + ":\n";
        lscp_channel_info_t* info = lscp_get_channel_info(client, chans[i]);
        ret += "   Engine: " + QString(info->engine_name) + "\n";
        ret += "   File: " + QString(info->instrument_file) + "\n";
        ret += "   Inst nr: " + i2s(info->instrument_nr) + "\n";
        ret += "   Audio Channels: " + i2s(info->audio_channels) + "\n";
        if (info->audio_routing == NULL) {
            ret += "      No routing\n";
        } else {
            for (int j=0; j < info->audio_channels; j++) {
                ret += "      Chan " + i2s(j) + " routing: " + i2s(info->audio_routing[j]) + "\n";
            }
        }
        ret += "   Audio device: " + i2s(info->audio_device) + "\n";
        ret += "   MIDI channel: " + i2s(info->midi_channel) + "\n";
        ret += "   MIDI device: " + i2s(info->midi_device) + "\n";
        ret += "   MIDI port: " + i2s(info->midi_port) + "\n";
        if (this->chans.contains(chans[i])) {
            LsChannel info2 = getSfzChannelInfo(chans[i]);
            ret += "   Channel belongs to us:\n";
            ret += "   Left JACK port: " + info2.audioLeftJackPort + "\n";
            ret += "   Right JACK port: " + info2.audioRightJackPort + "\n";
            ret += "   MIDI JACK port: " + info2.midiJackPort + "\n";
            ret += "   Path: " + info2.path + "\n";
        }

        i++;
    }

    return ret;
}

/* Adds channel (sfz instrument) with accompanying MIDI and audio ports and
 * returns ID. (The ID is also the same ID that Linuxsampler uses for the
 * channel.)
 * On error, -1 is returned. */
int KonfytLscp::addSfzChannelAndPorts(QString file)
{
    lscp_status_t status;

    int iAudioDev = getAudioDeviceIdByName(mClientName);
    if (iAudioDev < 0) {
        print("Error getting audio device named " + mClientName);
        return -1;
    }

    int iMidiDev = getMidiDeviceIdByName(mClientName);
    if (iMidiDev < 0) {
        print("Error getting MIDI device named " + mClientName);
        return -1;
    }

    int chan = lscp_add_channel(client);
    if (chan < 0) {
        print("Failed adding channel.");
        print("Result: " + QString( lscp_client_get_result(client) ));
        return -1;
    }

    QString engineName = "SFZ";
    if (file.toLower().endsWith(".gig")) {
        engineName = "GIG";
    }
    status = lscp_load_engine(client, engineName.toLocal8Bit().constData(), chan);
    if (status != LSCP_OK) {
        print("Failed loading SFZ engine.");
        print("Result: " + QString( lscp_client_get_result(client) ));
        return -1;
    }

    int audioPortLeft = getFreeAudioChannel();
    int audioPortRight = getFreeAudioChannel();
    int midiPort = getFreeMidiPort();

    status = lscp_set_channel_audio_device(client, chan, iAudioDev);
    if (status != LSCP_OK) {
        print(QString("Failed connecting audio device %1 to channel %2.")
              .arg(iAudioDev).arg(chan));
        print("Result: " + QString( lscp_client_get_result(client) ));
        return -1;
    }
    lscp_set_channel_audio_channel(client, chan, 0, audioPortLeft);
    lscp_set_channel_audio_channel(client, chan, 1, audioPortRight);

    status = lscp_set_channel_midi_device(client, chan, iMidiDev);
    if (status != LSCP_OK) {
        print(QString("Failed connecting MIDI device %1 to port %2.")
              .arg(iMidiDev).arg(chan));
        print("Result: " + QString( lscp_client_get_result(client) ));
        return -1;
    }
    lscp_set_channel_midi_port(client, chan, midiPort);

    QString fileEscaped = escapeString(file);
    status = lscp_load_instrument_non_modal(client,
                                            fileEscaped.toLocal8Bit().constData(),
                                            0, chan);
    if (status != LSCP_OK) {
        print("Failed loading instrument: " + fileEscaped);
        print("Result: " + QString( lscp_client_get_result(client) ));
        return -1;
    }

    LsChannel info;
    info.path = file;
    info.midiPortIndex = midiPort;
    info.audioLeftChanIndex = audioPortLeft;
    info.audioRightChanIndex = audioPortRight;

    // Refresh device info for added audio/midi ports to be listed
    refreshAudioDevices();
    refreshMidiDevices();

    LsDevice &adev = adevs[iAudioDev];
    if (indexValid(audioPortLeft, adev.ports.count())) {
        info.audioLeftJackPort = mClientName + ":" + adev.ports[audioPortLeft].name();
    } else {
        print("Audio left port out of bounds: " + i2s(audioPortLeft));
    }
    if (indexValid(audioPortRight, adev.ports.count())) {
        info.audioRightJackPort = mClientName + ":" + adev.ports[audioPortRight].name();
    } else {
        print("Audio right port out of bounds: " + i2s(audioPortRight));
    }

    LsDevice &mdev = mdevs[iMidiDev];
    //if (qBound(0, midiPort, mdev.ports.count()-1)) {

    if (indexValid(midiPort, mdev.ports.count())) {
        info.midiJackPort = mClientName + ":" + mdev.ports[midiPort].name();
    } else {
        print("MIDI port out of bounds: " + i2s(midiPort));
    }

    chans.insert(chan, info);

    return chan;
}

KonfytLscp::LsChannel KonfytLscp::getSfzChannelInfo(int id)
{
    return chans.value(id);
}

void KonfytLscp::removeSfzChannel(int id)
{
    if (chans.contains(id)) {
        LsChannel chan = chans.take(id);
        // Free right before left as they are assigned FIFO left then right again
        freeAudioChannel(chan.audioRightChanIndex);
        freeAudioChannel(chan.audioLeftChanIndex);
        freeMidiPort(chan.midiPortIndex);
        lscp_remove_channel(client, id);
    }
}

QString KonfytLscp::escapeString(QString s)
{
    s.replace("\\", "\\\\"); // Must be first to not interfere with rest.
    s.replace("\n", "\\n");
    s.replace("\r", "\\r");
    s.replace("\f", "\\f");
    s.replace("\t", "\\t");
    s.replace("\v", "\\v");
    s.replace("'", "\\'");
    s.replace("\"", "\\\"");

    return s;
}

QString KonfytLscp::i2s(int value)
{
    return QString::number(value);
}

QString KonfytLscp::indentString(QString s, QString indent)
{
    QStringList l = s.split("\n");
    QString ret;
    for (int i=0; i < l.count(); i++) {
        ret += indent + l[i] + "\n";
    }
    return ret;
}

bool KonfytLscp::indexValid(int index, int listCount)
{
    return ( (index >= 0) && (index < listCount) );
}

int KonfytLscp::addAudioChannel()
{
    int index = getAudioDeviceIdByName(mClientName);
    if (index < 0) {
        print("Error getting audio device named " + mClientName);
        return -1;
    }

    LsDevice &dev = adevs[index];
    int chan = dev.numPorts();

    lscp_param_t param;
    param.key = (char*)KEY_CHANNELS;
    param.value = (char*)i2s(chan+1).toLocal8Bit().constData();
    lscp_set_audio_device_param(client, dev.index, &param);

    return chan;
}

void KonfytLscp::freeAudioChannel(int index)
{
    freeAudioChannels.append(index);
}

int KonfytLscp::getFreeAudioChannel()
{
    int chan = -1;
    if (freeAudioChannels.count()) {
        chan = freeAudioChannels.takeLast();
    } else {
        chan = addAudioChannel();
    }
    return chan;
}

void KonfytLscp::refreshMidiDevices()
{
    mdevs.clear();

    int* devs = lscp_list_midi_devices(client);
    if (devs != NULL) {
        int i=0;
        while (devs[i] >= 0) {
            lscp_device_info_t* dev = lscp_get_midi_device_info(client, devs[i]);
            if (dev != NULL) {
                LsDevice device(client, devs[i], false, dev);
                mdevs.insert(devs[i], device);
            }
            i++;
        }
    }
}

bool KonfytLscp::midiDeviceExists(QString name)
{
    return getMidiDeviceIdByName(name) >= 0;
}

int KonfytLscp::getMidiDeviceIdByName(QString name)
{
    refreshMidiDevices();

    int ret = -1;

    QList<int> keys = mdevs.keys();
    for (int i=0; i < keys.count(); i++) {
        int key = keys[i];
        LsDevice &dev = mdevs[key];
        if (dev.name() == name) {
            ret = key;
            break;
        }
    }

    return ret;
}

bool KonfytLscp::addMidiDevice(QString name)
{
    lscp_param_t params[3];
    params[0].key = (char*)KEY_NAME; params[0].value = (char*)name.toLocal8Bit().constData();
    params[1].key = (char*)KEY_PORTS; params[1].value = (char*)VAL_0;
    params[2].key = NULL; params[2].value = NULL;
    int ret = lscp_create_midi_device(client, "JACK", params);

    return ret >= 0;
}

int KonfytLscp::addMidiPort()
{
    int index = getMidiDeviceIdByName(mClientName);
    if (index < 0) {
        print("Error getting MIDI device named " + mClientName);
        return -1;
    }

    LsDevice &dev = mdevs[index];
    int port = dev.numPorts();

    lscp_param_t param;
    param.key = (char*)KEY_PORTS;
    param.value = (char*)i2s(port+1).toLocal8Bit().constData();
    lscp_set_midi_device_param(client, dev.index, &param);

    return port;
}

void KonfytLscp::freeMidiPort(int index)
{
    freeMidiPorts.append(index);
}

int KonfytLscp::getFreeMidiPort()
{
    int port = -1;
    if (freeMidiPorts.count()) {
        port = freeMidiPorts.takeLast();
    } else {
        port = addMidiPort();
    }
    return port;
}

QString KonfytLscp::jackClientName()
{
    return mClientName;
}



KonfytLscp::LsDevice::LsDevice() : audio(true), index(-1) {}

KonfytLscp::LsDevice::LsDevice(lscp_client_t *client, int index, bool audio, lscp_device_info_t *dev) :
    audio(audio), index(index)
{
    if (dev->params != NULL) {
        int prm = 0;
        while (dev->params[prm].key != NULL) {
            params.insert( QString(dev->params[prm].key),
                           QString(dev->params[prm].value) );
            prm++;
        }
        params.insert("DRIVER", QString(dev->driver));
    }

    // Get ports
    int nports = numPorts();
    for (int i=0; i < nports; i++) {
        lscp_device_port_info_t* port;
        if (audio) {
            port = lscp_get_audio_channel_info(client, index, i);
        }
        else { port = lscp_get_midi_port_info(client, index, i); }
        if (port != NULL) {
            LsPort prt(i, port);
            ports.append(prt);
        }
    }

}

int KonfytLscp::LsDevice::numPorts() {
    if (audio) {
        return params.value("CHANNELS", "0").toInt();
    } else {
        return params.value("PORTS", "0").toInt();
    }
}

QString KonfytLscp::LsDevice::toString()
{
    QString ret = QString("Device %1: '%2', %3 %4, (%5)\n")
            .arg(index)
            .arg(name())
            .arg(numPorts())
            .arg(audio ? "channels" : "ports")
            .arg(driver());
    for (int i=0; i < ports.count(); i++) {
        ret += QString("   Port %1:\n%2\n")
                .arg(i)
                .arg( KonfytLscp::indentString( ports[i].paramsToString(), "      ") );
    }
    return ret;
}

QString KonfytLscp::LsDevice::paramsToString()
{
    QString ret;
    QList<QString> keys = params.keys();
    for (int i=0; i < keys.count(); i++) {
        if (!ret.isEmpty()) { ret += "\n"; }
        ret += keys[i] + ": " + params[keys[i]];
    }
    return ret;
}

KonfytLscp::LsPort::LsPort(int index, lscp_device_port_info_t *port) :
    index(index)
{
    if (port->params != NULL) {
        int p = 0;
        while (port->params[p].key != NULL) {
            params.insert( QString(port->params[p].key),
                           QString(port->params[p].value) );
            p++;
        }
        params.insert("NAME", QString(port->name));
    }
}

QString KonfytLscp::LsPort::name()
{
    return params.value("NAME", "");
}

QString KonfytLscp::LsPort::paramsToString()
{
    QString ret;
    QList<QString> keys = params.keys();
    for (int i=0; i < keys.count(); i++) {
        if (!ret.isEmpty()) { ret += "\n"; }
        ret += keys[i] + ": " + params[keys[i]];
    }
    return ret;
}
