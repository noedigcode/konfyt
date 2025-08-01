// random pan-per-note script
// written in flexible way
// so it can be extended to randomize CC's too


function gen_superpad(){
  let patch = [
    {PG:50, MSB: 36, LSB:0, CC: [ [73,115], [72,117] ] },
    {PG:88, MSB:  4, LSB:0, CC: [ [73,115], [72,117] ] },
    {PG:50, MSB: 37, LSB:0, CC: [ [74,115], [72,117] ] },
    {PG:88, MSB:  5, LSB:0, CC: [ [73,115], [72,117] ] },
    {PG:50, MSB: 38, LSB:0, CC: [ [74,115], [72,117] ] },  
  ]
  return patch
}


function init()
{
    preset = {
        PATCH: gen_superpad()
    }
    
    // apply rotate functions
    for( let i in preset ){
      preset[i].rotate = function(){
          let v = this.pop()
          this.unshift(v)
          print(v)
          return v
      }.bind(preset[i])
   }      
}

function midiEvent(ev)
{
    if( ev.type == NOTEON ){
      let patch = preset.PATCH.rotate()
      if( patch.CC ){
        for( let i in patch.CC ){
          Midi.send( Midi.cc(ev.channel, patch.CC[i][0], patch.CC[i][1] ) )
        }
      }
      if( patch.PG ) Midi.send( Midi.program(ev.channel, patch.PG, patch.MSB, patch.LSB ) )
      print(ev.channel +" pg:"+patch.PG+" msb: "+patch.MSB)
    }
    Midi.send(ev);
}
