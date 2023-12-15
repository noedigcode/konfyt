## Scripting Overview

Scripting for Konfyt is done in JavaScript.

A basic script skeleton looks like this:
```
function init()
{
    // This gets called once
    // when the script is loaded
}

function midiEvent(ev)
{
    // This gets called for
    // every incoming MIDI event.
}
```

- `init()` is called once when the script is loaded.

- `midiEvent(ev)` is called for every incoming MIDI event.

- Argument `ev` is the MIDI event. See the sections for the different types of
  events below.

- Use `print("Hello World")` to print a string to the console for debugging.

## Handling incoming MIDI events

The event properties can be used to decide what to do based on the incoming
events.

For example, the following snippet converts a CC 10 event to a noteon or noteoff,
and passes all other events.

```
function midiEvent(ev)
{
    if ((ev.type == CC) && (ev.cc == 10)) {
        // Convert to noteon or off
        if (ev.value == 127) {
            let n = Midi.noteon(1, 60, 100);
            Midi.send(n);
        } else {
            let n = Midi.noteoff(1, 60);
            Midi.send(n);
        }
    } else {
        // Pass through original event
        Midi.send(ev);
    }
}
```

See the sections below for details on the different types of events.

## Sending MIDI

Use `Midi.send(ev)` to send a MIDI event, where `ev` is the event to send.

`ev` could either be the original or modified version of an incoming event,
or a newly created event. See the sections for the different types of events
below for how to create new MIDI events.

## Note events

Assuming `ev` is a noteon or noteoff MIDI event:

- `ev.type`: will be equal to either the `NOTEON` or `NOTEOFF` constants.
- `ev.channel`: MIDI channel, 1-16
- `ev.note`: 0-127
- `ev.velocity`: 0-127

`Midi.noteon(channel, note, velocity)` creates a new noteon event.

`Midi.noteoff(channel, note, velocity)` creates a new noteoff event.
The velocity argument is optional and set to 0 if not specified.

## CC events

- `ev.type`: will be equal to the `CC` constant.
- `ev.channel`: MIDI channel, 1-16
- `ev.cc`: 0-127
- `ev.value`: 0-127

`Midi.cc(channel, cc, value)` creates a new CC event.

## Program change events

- `ev.type`: will be equal to the `PROGRAM` constant.
- `ev.channel`: MIDI channel, 1-16
- `ev.program`: 0-127
- `ev.bankmsb`: 0-127, or -1 if not applicable
- `ev.banklsb`: 0-127, or -1 if not applicable

`Midi.program(channel, program, bankmsb, banklsb)` creates a new program change event.
The bankmsb and banklsb arguments are optional and are disabled (set to -1)
if not specified.

When sending a program change message, if `bankmsb` and `banklsb` are set,
these two messages will be sent right before the program change message is sent.
If they are set to `-1`, only the program change message will be sent.

## Pitchbend events

- `ev.type` will be equal to the `PITCHBEND` constant.
- `ev.channel`: MIDI channel, 1-16
- `ev.value`: signed value, -8192 to 8191

`Midi.pitchbend(channel, value)` creates a new pitchbend event.

A value of `0` corresponds to no pitchbend. The constants `PITCHBEND_MIN` and
`PITCHBEND_MAX` can be used which correspond to signed pitchbend minimum and
maximum values, -8192 and 8191, respectively.

## Sysex events

- `ev.type` will be equal to the `SYSEX` constant.
- `ev.channel`: MIDI channel, 1-16
- `ev.data`: string of two-character hexadecimal values (e.g. "A1 B2"...).
  Values may be space separated. Excludes the opening F0.

## Aftertouch MIDI events

- `ev.type` will be equal to either the `POLYAFTERTOUCH` or `AFTERTOUCH` constants.
- `ev.channel`: MIDI channel, 1-16
- `ev.note`: 0-127 (only for poly aftertouch)
- `ev.pressure`: 0-127

