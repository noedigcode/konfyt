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

#define SERVER_PORT 8888

#define n2s(x) QString::number(x)


QString indentString(QString s, QString indent);

// =============================================================================
struct GidLsPort
{
    GidLsPort(int index, lscp_device_port_info_t* port) :
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

    QString name()
    {
        return params.value("NAME", "");
    }

    QMap<QString, QString> params;
    int index;

    QString paramsToString()
    {
        QString ret;
        QList<QString> keys = params.keys();
        for (int i=0; i < keys.count(); i++) {
            if (!ret.isEmpty()) { ret += "\n"; }
            ret += keys[i] + ": " + params[keys[i]];
        }
        return ret;
    }
};

// =============================================================================
struct GidLsDevice
{
    GidLsDevice() : audio(true), index(-1) {}
    GidLsDevice(lscp_client_t *client, int index, bool audio, lscp_device_info_t* dev) :
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

    QMap<QString, QString> params;
    QList<GidLsPort> ports;

    bool audio;
    int index;
    int numPorts() {
        if (audio) {
            return params.value("CHANNELS", "0").toInt();
        } else {
            return params.value("PORTS", "0").toInt();
        }
    }
    QString name() { return params.value("NAME", ""); }
    QString driver() { return params.value("DRIVER", ""); }

    QString toString()
    {
        QString ret = "Device " + n2s(index) + ": '"
                + name() + "', "
                + (audio ? (n2s(numPorts()) + " channels, ") : (n2s(numPorts()) + " ports, "))
                + "(" + driver() + ")\n";
        for (int i=0; i < ports.count(); i++) {
            ret += "   Port " + n2s(i) + ":\n";
            ret += indentString( ports[i].paramsToString(), "      " );
            ret += "\n";
        }
        return ret;
    }

    QString paramsToString()
    {
        QString ret;
        QList<QString> keys = params.keys();
        for (int i=0; i < keys.count(); i++) {
            if (!ret.isEmpty()) { ret += "\n"; }
            ret += keys[i] + ": " + params[keys[i]];
        }
        return ret;
    }
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
    bool addSfzChannel(QString file);
    int addSfzChannelAndPorts(QString file);
    GidLsChannel getSfzChannelInfo(int id);
    void removeSfzChannel(int id);

private:
    QString devName;
    lscp_client_t *client;
    QMap<int, GidLsDevice> adevs;
    QMap<int, GidLsDevice> mdevs;
    QMap<int, GidLsChannel> chans;

private slots:
    void clientInitialised();


signals:
    void print(QString msg);
    void initialised(bool error, QString errString);

public slots:
};

#endif // GIDLS_H
