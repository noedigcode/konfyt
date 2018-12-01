/******************************************************************************
 *
 * Copyright 2018 Gideon van der Kolf
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

GidLs::GidLs(QObject *parent) : QObject(parent), client(NULL)
{

}

lscp_status_t GidLs::client_callback(lscp_client_t *pClient, lscp_event_t event, const char *pchData, int cchData, void *pvData)
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
    params[0].key = "NAME"; params[0].value = (char*)name.toLocal8Bit().constData();
    params[1].key = "CHANNELS"; params[1].value = "0";
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
        ret += "Channel " + n2s(chans[i]) + ":\n";
        lscp_channel_info_t* info = lscp_get_channel_info(client, chans[i]);
        ret += "   Engine: " + QString(info->engine_name) + "\n";
        ret += "   File: " + QString(info->instrument_file) + "\n";
        ret += "   Inst nr: " + n2s(info->instrument_nr) + "\n";
        ret += "   Audio Channels: " + n2s(info->audio_channels) + "\n";
        if (info->audio_routing == NULL) {
            ret += "      No routing\n";
        } else {
            for (int j=0; j < info->audio_channels; j++) {
                ret += "      Chan " + n2s(j) + " routing: " + n2s(info->audio_routing[j]) + "\n";
            }
        }
        ret += "   Audio device: " + n2s(info->audio_device) + "\n";
        ret += "   MIDI channel: " + n2s(info->midi_channel) + "\n";
        ret += "   MIDI device: " + n2s(info->midi_device) + "\n";
        ret += "   MIDI port: " + n2s(info->midi_port) + "\n";
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

bool GidLs::addSfzChannel(QString file)
{
    file = file.replace(" ", "\\x20"); // debug remove?

    lscp_status_t status;

    int chan = lscp_add_channel(client);
    if (chan < 0) {
        print("Failed adding channel.");
        print("Result: " + QString( lscp_client_get_result(client) ));
        return false;
    }

    status = lscp_load_engine(client, "SFZ", chan);
    if (status != LSCP_OK) {
        print("Failed loading SFZ engine.");
        print("Result: " + QString( lscp_client_get_result(client) ));
        return false;
    }

    status = lscp_set_channel_audio_device(client, chan, 0);
    if (status != LSCP_OK) {
        print("Failed connecting audio device to channel.");
        print("Result: " + QString( lscp_client_get_result(client) ));
        return false;
    }
    lscp_set_channel_audio_channel(client, chan, 0, 0);
    lscp_set_channel_audio_channel(client, chan, 1, 1);

    lscp_set_channel_midi_device(client, chan, 0);
    lscp_set_channel_midi_port(client, chan, 0);

    status = lscp_load_instrument_non_modal(client, file.toLocal8Bit().constData(),
                                            0, chan);
    if (status != LSCP_OK) {
        print("Failed loading instrument: " + file);
        print("Result: " + QString( lscp_client_get_result(client) ));
        return false;
    }



    return true;
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

    status = lscp_load_instrument_non_modal(client, file.toLocal8Bit().constData(),
                                            0, chan);
    if (status != LSCP_OK) {
        print("Failed loading instrument: " + file);
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
    param.key = "CHANNELS";
    param.value = (char*)n2s(chan+1).toLocal8Bit().constData();
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
    params[0].key = "NAME"; params[0].value = (char*)name.toLocal8Bit().constData();
    params[1].key = "PORTS"; params[1].value = "0";
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
    param.key = "PORTS";
    param.value = (char*)n2s(port+1).toLocal8Bit().constData();
    lscp_set_midi_device_param(client, dev.index, &param);

    return port;
}

QString GidLs::jackClientName()
{
    return devName;
}

QString indentString(QString s, QString indent)
{
    QStringList l = s.split("\n");
    QString ret;
    for (int i=0; i < l.count(); i++) {
        ret += indent + l[i] + "\n";
    }
    return ret;
}
