// random pan-per-note script
// written in flexible way
// so it can be extended to randomize CC's too

function init()
{
    PAN = 80
    preset = {
        PAN: [80,0,127]     // predefined roundrobin values
    }
    
    // apply rotate functions
    for( let i in preset ){
      preset[i].rotate = function(){
          let v = this.pop()
          this.unshift(v)
          return v
      }.bind(preset[i])
   }      
}

function rotate(){
    PAN = preset.PAN.rotate()
    print("rotating: PAN("+PAN+")")
}

function midiEvent(ev)
{
    let pan = Midi.cc(ev.channel, 10, PAN)
    Midi.send(pan)
    Midi.send(ev);
    rotate()
}
