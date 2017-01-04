#include "scanfoldersthread.h"

scanFoldersThread::scanFoldersThread(QObject *parent) :
    QThread(parent)
{

}

void scanFoldersThread::run()
{
    /*
    double b;
    for (double i=0; i<1000000000; i++) {
        b = i/2;
    }
    emit finished();
    return;
    */

    int sfid = fluid_synth_sfload(synth, filename.toLocal8Bit().data(), 1);
    if (sfid == -1) {
        userMessage("THREAD: error loading soundfont");
    }
    fluid_sfont_t* sf = fluid_synth_get_sfont_by_id(synth,sfid);

    // Iterate through all the presets within the soundfont
    int more = 1;
    fluid_preset_t* preset = new fluid_preset_t();
    // Reset the iteration
    sf->iteration_start(sf);
    while (more) {
        more = sf->iteration_next(sf, preset);
        if (more) {
            // Get preset name
            char* presetname = preset->get_name(preset);
            QString qpresetname = QString::fromAscii(presetname);
            int banknum = preset->get_banknum(preset);
            int num = preset->get_num(preset);
            userMessage("Preset " + qpresetname + " " + QString::number(banknum)
                        + ", " + QString::number(num));
        }
    }

    // Unload soundfont to save memory
    fluid_synth_sfunload(synth,sfid,1);
    delete preset;

    emit finished();    
}
