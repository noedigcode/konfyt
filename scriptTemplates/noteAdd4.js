function init()
{
    Sys.print('Hi!')
}

function process()
{
    for (let ev of Sys.events) {
        if ((ev.type == 'noteon') || (ev.type == 'noteoff')) {
            // Add 4 notes to event and send
            ev.note += 4
            Sys.send(ev)
        }
    }
}

