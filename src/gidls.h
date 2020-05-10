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


// http://download.linuxsampler.org/doc/liblscp/
// https://www.linuxsampler.org/api/draft-linuxsampler-protocol.html


#ifndef GIDLS_H
#define GIDLS_H

#include <QObject>
#include <QMap>
#include <QProcess>
#include <QTimer>

#include "lscp/client.h"
#include "lscp/device.h"

// =============================================================================
struct GidLsPort
{
    GidLsPort(int index, lscp_device_port_info_t* port);

    QString name();

    QMap<QString, QString> params;
    int index;

    QString paramsToString();
};

// =============================================================================
struct GidLsDevice
{
    GidLsDevice();
    GidLsDevice(lscp_client_t *client, int index, bool audio, lscp_device_info_t* dev);

    QMap<QString, QString> params;
    QList<GidLsPort> ports;

    bool audio;
    int index;
    int numPorts();
    QString name() { return params.value("NAME", ""); }
    QString driver() { return params.value("DRIVER", ""); }

    QString toString();
    QString paramsToString();
};

// =============================================================================

struct GidLsChannel
{
    QString midiJackPort;
    QString audioLeftJackPort;
    QString audioRightJackPort;
    QString path;
};

// =============================================================================

class GidLs : public QObject
{
    Q_OBJECT
public:
    explicit GidLs(QObject *parent = 0);

    static lscp_status_t client_callback ( lscp_client_t *pClient,
                                           lscp_event_t event,
                                           const char *pchData, int cchData,
                                           void *pvData );

    void init(QString deviceName);
    void deinit();

    void printEngines();

    void refreshAudioDevices();
    bool audioDeviceExists(QString name);
    int getAudioDeviceIdByName(QString name);
    bool addAudioDevice(QString name);
    int addAudioChannel();

    void refreshMidiDevices();
    bool midiDeviceExists(QString name);
    int getMidiDeviceIdByName(QString name);
    bool addMidiDevice(QString name);
    int addMidiPort();

    QString jackClientName();
    QString printDevices();

    QString printChannels();
    int addSfzChannelAndPorts(QString file);
    GidLsChannel getSfzChannelInfo(int id);
    void removeSfzChannel(int id);

    static QString escapeString(QString s);
    static QString i2s(int value);
    static QString indentString(QString s, QString indent);

private:
    QString devName;
    lscp_client_t *client = NULL;
    QMap<int, GidLsDevice> adevs;
    QMap<int, GidLsDevice> mdevs;
    QMap<int, GidLsChannel> chans;

    const int SERVER_PORT = 8888;

    // Parameter constnats
    const char* KEY_NAME = "NAME";
    const char* KEY_CHANNELS = "CHANNELS";
    const char* KEY_PORTS = "PORTS";
    const char* VAL_0 = "0";

private slots:
    void clientInitialised();

signals:
    void print(QString msg);
    void initialised(bool error, QString errString);
};

#endif // GIDLS_H
