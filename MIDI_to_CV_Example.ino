/*
 * Comments in this document always refer to the line underneath.
 * All the functions I used are decently explained by MocoLUFA's
 * documentation if you're not sure what they do.
 */
 
#include <MIDI.h>
#include <Adafruit_MCP4725.h>

uint8_t noteArray[16];
uint8_t noteArraySize = 0;
//By default, middle C is the MIDI note at startup
uint8_t note = 60;

//This initializes MocoLUFA's MIDI process
MIDI_CREATE_DEFAULT_INSTANCE();
/*
 * This creates an object that can be used to easily interface with 
 * Adafruit's breakout board DAC (although if you want, you
 * could just solder the chip somewhere yourself).
 */
Adafruit_MCP4725 dac0;
uint16_t dac0V = 0;

//According to the MIDI standard, the middle of the Pitchbend controller's 
//14 bit range translates to no pitch modulation.
int16_t bendGlobal = 8192;

void setup() {
  MIDI.begin(MIDI_CHANNEL_OMNI);
  MIDI.turnThruOff();
  dac0.begin(0x63); //CV dac
}

void loop() {
  midiInput();
}

/*
 * This function handles all the programmer-forseen midi input situations, 
 * and is called when midi data is in the serial buffer.
 * It is not a completely robust solution -- if you want to process a MIDI event, 
 * you'll have to account for it manually.
 * The advantage is that this offers great flexibility.
 */
void midiInput(){
  if (MIDI.read()){
      midi::MidiType type = MIDI.getType();
  
      switch (type){
        case midi::PitchBend:
          {
            //pitchbend is placed first as it sends the most data while in use
            bendGlobal = MIDI.getData2()*128 + MIDI.getData1();
            /*
             * The following calculation may need to be time optimized, as the Arduino isn't
             * particularly powerful and Pitchbend data can come in
             * quite fast. It seemed to work fine for me, though.
             */
            int16_t tempBend = floor((((float)bendGlobal - 8192)/4096)*68.267); 
            dac0.setVoltage(dac0V + tempBend, false);
            midiChange = true;
          }
          break;
          
        case midi::NoteOn:
          {
            note = MIDI.getData1();

            /*
             * This array allows up to sixteen notes to be held in memory, meaning that if you hold one note,
             * press another one and let it go, the CV will return back to the first note if it is still held.
             * I'm not sure what happens when seventeen notes are pressed, but it never seemed to crash for me.
             * You can obviously guard against this but I didn't care.
             */
            noteArray[noteArraySize] = note;
            noteArraySize++;

            //This is an arbitrary range that I used. It's five octaves because it 
            //assumes a 1v/octave CV, and the range of the DAC I used here is 0-5V.
            if (note >= 24 && note <= 84){
              //This converts the MIDI note value into a 12-bit value that can be used by the DAC
              dac0V = floor((note - 24)*68.267);
              dac0.setVoltage(dac0V, false);
            }
          }
          break;
          
        case midi::NoteOff:
          {
            uint8_t noteOff = MIDI.getData1();
            uint8_t offIndex;
            for (int i = 0; i < noteArraySize; i++){
              if (noteArray[i] == noteOff){
                offIndex = i;
                break;
              }
            }
            
            for (int i = 0; i < noteArraySize - 1 - offIndex; i++){
              noteArray[i + offIndex] = noteArray[i + offIndex + 1];
            }
            noteArraySize --;
      
            if (noteArraySize > 0){
              //stepping back one slot in the array so the previous note plays if it is still pressed
              note = noteArray[noteArraySize - 1];
              if (note >= 24 && note <= 84){
                dac0V = (note - 24)*68.267;
                dac0.setVoltage(dac0V, false);
              }
            }
          }
        break;
  
      case midi::ControlChange:
        {
          /*
           * CC messages have the CC number in the first data byte and the value in the second,
           * so first we have to see which CC it is. You can of course use a switch statement
           * in here as well, but I didn't have many CCs that I was looking for.
           */
          if (MIDI.getData1() == 1){
            modGlobal = MIDI.getData2();
            
          }else if (MIDI.getData1() == 5){
            //portamento time -- controls like these can be implemented purely through software if you want
          }
          break;
        }
      default:
        {
          //If you want to do something for an unhandled case, you can put it here.
        }
        break;
      }
    }
}
