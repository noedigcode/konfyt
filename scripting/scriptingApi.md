## Scripting

`init()` is called once when the script is loaded.

`midiEvent(ev)` is called for every incoming MIDI event.

Argument `ev` is the MIDI event.


## Global functions and objects:

- `print(message)`: print a string to the console.

Global object `Midi`:
- `Midi.send(ev)`: Send MIDI event.

The following functions create and return a new MIDI event:
- `Midi.noteon(channel, note, velocity)`
- `Midi.noteoff(channel, note, velocity)` - velocity argument is optional and 0 if not specified.
- `Midi.cc(channel, cc, value)`
- `Midi.program(channel, program, bankmsb, banklsb)` - bankmsb and banklsb are optional and -1 if not specified.
- `Midi.pitchbend(channel, value)` - value is a signed value, -8192 to 8191.


## MIDI event objects:

For all event types:

- `ev.type`: string corresponding to one of the type constants:

  NOTEON, NOTEOFF, CC, PROGRAM, PITCHBEND, POLYAFTERTOUCH, AFTERTOUCH, SYSEX

- `ev.channel`: MIDI channel, 1-16

For noteon and noteoff events:
- `ev.note`: 0-127
- `ev.velocity`: 0-127

For CC events:
- `ev.cc`: 0-127
- `ev.value`: 0-127

For program events:
- `ev.program`: 0-127
- bankmsb: 0-127, or -1 if not applicable
- banklsb: 0-127, or -1 if not applicable

For pitchbend events:
- `ev.value`: signed value, -8192 to 8191

For sysex events:
- `ev.data`: string of two-character hexadecimal values (e.g. A1, B2, etc.), may be space separated. Excludes opening F0.

For aftertouch and poly aftertouch events:
- `ev.note`: 0-127 (only for poly aftertouch)
- `ev.pressure`: 0-127


## Constants

- See type constants above
- `PITCHBEND_MIN` and `PITCHBEND_MAX` correspond to signed pitchbend minimum and maximum values, -8192 and 8191, respectively.

