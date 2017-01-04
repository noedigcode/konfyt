#ifndef KONFYT_STRUCTS_H
#define KONFYT_STRUCTS_H

#include <jack/jack.h>
#include <QString>



/* This represents a program/preset within a soundfont (i.e. a single voice/instrument). */
typedef struct konfytSoundfontProgram_t {

    QString name;               // Program name
    int bank;                   // Bank number
    int program;                // Program/preset number
    QString parent_soundfont;   // Filename of parent soundfont

    // Constructor
    konfytSoundfontProgram_t() : bank(0), program(0) {}

} konfytSoundfontProgram;

typedef struct konfytSoundfont_t {

    QString filename;
    QString name;
    QList<konfytSoundfontProgram> programlist;
    QList<konfytSoundfontProgram> searchResults;

} konfytSoundfont;





#endif // KONFYT_STRUCTS_H
