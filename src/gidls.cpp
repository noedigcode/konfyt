/******************************************************************************
 *
 * Copyright 2020 Gideon van der Kolf
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

#include "gidls.h"

GidLs::GidLs(QObject *parent) : QObject(parent)
{

}

lscp_status_t GidLs::client_callback(lscp_client_t* /*pClient*/, lscp_event_t event, const char *pchData, int cchData, void* /*pvData*/)
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

void GidLs::init(QString deviceName)
{
    devName = deviceName;

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
                    clientInitialised();
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
        clientInitialised();
    }
}

void GidLs::deinit()
{
    lscp_reset_sampler(client);
    lscp_client_destroy(client);
}

void GidLs::printEngines()
{
    const char** engines = lscp_list_available_engines(client);
    int i=0;
    print("Engines:");
    while (engines[i] != NULL) {
        print(" - " + QString(engines[i]));
        i++;
    }
}

void GidLs::refreshAudioDevices()
{
    adevs.clear();

    int* devs = lscp_list_audio_devices(client);
    if (devs != NULL) {
        int i=0;
        while (devs[i] >= 0) {
            lscp_device_info_t* dev = lscp_get_audio_device_info(client, devs[i]);
            if (dev != NULL) {
                GidLsDevice device(client, devs[i], true, dev);
                adevs.insert(devs[i], device);
            }
            i++;
        }
    }
}

bool GidLs::audioDeviceExists(QString name)
{
    return getAudioDeviceIdByName(name) >= 0;
}

int GidLs::getAudioDeviceIdByName(QString name)
{
    refreshAudioDevices();

    int ret = -1;

    QList<int> keys = adevs.keys();
    for (int i=0; i < keys.count(); i++) {
        int key = keys[i];
        GidLsDevice &dev = adevs[key];
        if (dev.name() == name) {
            ret = key;
            break;
        }
    }
    return ret;
}

bool GidLs::addAudioDevice(QString name)
{
    lscp_param_t params[3];
    params[0].key = (char*)KEY_NAME; params[0].value = (char*)name.toLocal8Bit().constData();
    params[1].key = (char*)KEY_CHANNELS; params[1].value = (char*)VAL_0;
    params[2].key = NULL; params[2].value = NULL;
    int ret = lscp_create_audio_device(client, "JACK", params);

    return ret >= 0;
}

QString GidLs::printDevices()
{
    QString ret;

    refreshAudioDevices();
    refreshMidiDevices();

    ret += "Audio devices:\n";
    for (int i=0; i < adevs.count(); i++) {
        GidLsDevice &d = adevs[i];
        ret += " - " + d.toString();
        ret += indentString(d.paramsToString(), "   ");
    }
    ret += "\nMIDI devices:\n";
    for (int i=0; i < mdevs.count(); i++) {
        GidLsDevice &d = mdevs[i];
        ret += " - " + d.toString();
        ret += indentString(d.paramsToString(), "   ");
    }

    return ret;
}

QString GidLs::printChannels()
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
            GidLsChannel info2 = getSfzChannelInfo(chans[i]);
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
int GidLs::addSfzChannelAndPorts(QString file)
{
    lscp_status_t status;

    int chan = lscp_add_channel(client);
    if (chan < 0) {
        print("Failed adding channel.");
        print("Result: " + QString( lscp_client_get_result(client) ));
        return -1;
    }

    status = lscp_load_engine(client, "SFZ", chan);
    if (status != LSCP_OK) {
        print("Failed loading SFZ engine.");
        print("Result: " + QString( lscp_client_get_result(client) ));
        return -1;
    }

    status = lscp_set_channel_audio_device(client, chan, 0);
    if (status != LSCP_OK) {
        print("Failed connecting audio device to channel.");
        print("Result: " + QString( lscp_client_get_result(client) ));
        return -1;
    }

    int aleft = addAudioChannel();
    int aright = addAudioChannel();
    int midi = addMidiPort();

    lscp_set_channel_audio_channel(client, chan, 0, aleft);
    lscp_set_channel_audio_channel(client, chan, 1, aright);

    lscp_set_channel_midi_device(client, chan, 0);
    lscp_set_channel_midi_port(client, chan, midi);

    QString fileEscaped = escapeString(file);
    status = lscp_load_instrument_non_modal(client,
                                            fileEscaped.toLocal8Bit().constData(),
                                            0, chan);
    if (status != LSCP_OK) {
        print("Failed loading instrument: " + fileEscaped);
        print("Result: " + QString( lscp_client_get_result(client) ));
        return -1;
    }

    GidLsChannel info;
    info.path = file;

    int idev = getAudioDeviceIdByName(devName);
    if (idev < 0) {
        print("Error getting audio device named " + devName);
        return -1;
    }
    GidLsDevice &dev = adevs[idev];
    info.audioLeftJackPort = devName + ":" + dev.ports[aleft].name();
    info.audioRightJackPort = devName + ":" + dev.ports[aright].name();

    idev = getMidiDeviceIdByName(devName);
    if (idev < 0) {
        print("Error getting MIDI device named " + devName);
        return -1;
    }
    GidLsDevice &dev2 = mdevs[idev];
    info.midiJackPort = devName + ":" + dev2.ports[midi].name();

    chans.insert(chan, info);

    return chan;
}

GidLsChannel GidLs::getSfzChannelInfo(int id)
{
    return chans.value(id);
}

void GidLs::removeSfzChannel(int id)
{
    if (chans.contains(id)) {
        chans.remove(id);
        lscp_remove_channel(client, id);
    }
}

QString GidLs::escapeString(QString s)
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

QString GidLs::i2s(int value)
{
    return QString::number(value);
}

QString GidLs::indentString(QString s, QString indent)
{
    QStringList l = s.split("\n");
    QString ret;
    for (int i=0; i < l.count(); i++) {
        ret += indent + l[i] + "\n";
    }
    return ret;
}

void GidLs::clientInitialised()
{
    QString errString;
    bool error = false;

    print("Connected to Linuxsampler.");

    print("Resetting sampler");
    lscp_status_t rst = lscp_reset_sampler(client);
    if (rst != LSCP_OK) {
        print("Error resetting sampler.");
    }

    lscp_set_volume(client, 1);

    if (audioDeviceExists(devName)) {
        print("Audio device '" + devName + "' already exists.");
    } else {
        print("Creating audio device '" + devName + "'.");
        if (addAudioDevice(devName)) {
            print("Audio device created.");
        } else {
            print("Error creating audio device.");
            error = true;
            errString += "Error creating audio device.\n";
        }
    }

    if (midiDeviceExists(devName)) {
        print("MIDI device '" + devName + "' already exists.");
    } else {
        print("Creating MIDI device '" + devName + "'.");
        if (addMidiDevice(devName)) {
            print("MIDI device created.");
        } else {
            print("Error creating MIDI device.");
            error = true;
            errString += "Error creating MIDI device.\n";
        }
    }

    print("Initialisation complete.");

    emit initialised(error, errString);
}

int GidLs::addAudioChannel()
{
    int index = getAudioDeviceIdByName(devName);
    if (index < 0) {
        print("Error getting audio device named " + devName);
        return -1;
    }

    GidLsDevice &dev = adevs[index];
    int chan = dev.numPorts();

    lscp_param_t param;
    param.key = (char*)KEY_CHANNELS;
    param.value = (char*)i2s(chan+1).toLocal8Bit().constData();
    lscp_set_audio_device_param(client, dev.index, &param);

    return chan;
}

void GidLs::refreshMidiDevices()
{
    mdevs.clear();

    int* devs = lscp_list_midi_devices(client);
    if (devs != NULL) {
        int i=0;
        while (devs[i] >= 0) {
            lscp_device_info_t* dev = lscp_get_midi_device_info(client, devs[i]);
            if (dev != NULL) {
                GidLsDevice device(client, devs[i], false, dev);
                mdevs.insert(devs[i], device);
            }
            i++;
        }
    }
}

bool GidLs::midiDeviceExists(QString name)
{
    return getMidiDeviceIdByName(name) >= 0;
}

int GidLs::getMidiDeviceIdByName(QString name)
{
    refreshMidiDevices();

    int ret = -1;

    QList<int> keys = mdevs.keys();
    for (int i=0; i < keys.count(); i++) {
        int key = keys[i];
        GidLsDevice &dev = mdevs[key];
        if (dev.name() == name) {
            ret = key;
            break;
        }
    }

    return ret;
}

bool GidLs::addMidiDevice(QString name)
{
    lscp_param_t params[3];
    params[0].key = (char*)KEY_NAME; params[0].value = (char*)name.toLocal8Bit().constData();
    params[1].key = (char*)KEY_PORTS; params[1].value = (char*)VAL_0;
    params[2].key = NULL; params[2].value = NULL;
    int ret = lscp_create_midi_device(client, "JACK", params);

    return ret >= 0;
}

int GidLs::addMidiPort()
{
    int index = getMidiDeviceIdByName(devName);
    if (index < 0) {
        print("Error getting MIDI device named " + devName);
        return -1;
    }

    GidLsDevice &dev = mdevs[index];
    int port = dev.numPorts();

    lscp_param_t param;
    param.key = (char*)KEY_PORTS;
    param.value = (char*)i2s(port+1).toLocal8Bit().constData();
    lscp_set_midi_device_param(client, dev.index, &param);

    return port;
}

QString GidLs::jackClientName()
{
    return devName;
}



GidLsDevice::GidLsDevice() : audio(true), index(-1) {}

GidLsDevice::GidLsDevice(lscp_client_t *client, int index, bool audio, lscp_device_info_t *dev) :
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
            GidLsPort prt(i, port);
            ports.append(prt);
        }
    }

}

int GidLsDevice::numPorts() {
    if (audio) {
        return params.value("CHANNELS", "0").toInt();
    } else {
        return params.value("PORTS", "0").toInt();
    }
}

QString GidLsDevice::toString()
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
                .arg( GidLs::indentString( ports[i].paramsToString(), "      ") );
    }
    return ret;
}

QString GidLsDevice::paramsToString()
{
    QString ret;
    QList<QString> keys = params.keys();
    for (int i=0; i < keys.count(); i++) {
        if (!ret.isEmpty()) { ret += "\n"; }
        ret += keys[i] + ": " + params[keys[i]];
    }
    return ret;
}

GidLsPort::GidLsPort(int index, lscp_device_port_info_t *port) :
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

QString GidLsPort::name()
{
    return params.value("NAME", "");
}

QString GidLsPort::paramsToString()
{
    QString ret;
    QList<QString> keys = params.keys();
    for (int i=0; i < keys.count(); i++) {
        if (!ret.isEmpty()) { ret += "\n"; }
        ret += keys[i] + ": " + params[keys[i]];
    }
    return ret;
}
