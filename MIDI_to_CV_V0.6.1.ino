#include <MIDI.h>
#include <Adafruit_MCP4725.h>
#include <wiring_private.h>

/*
* Arduino Uno pin codes:
* 
* PORTD maps to Arduino digital pins 0 to 7 
* 
* DDRD - The Port D Data Direction Register - read/write 
* PORTD - The Port D Data Register - read/write 
* PIND - The Port D Input Pins Register - read only 
* 
* PORTB maps to Arduino digital pins 8 to 13 The two high bits (6 & 7) map to the crystal pins and are not usable 
* (DDRB, PORTB, PINB)
* 
* PORTC maps to Arduino analog pins 0 to 5. Pins 6 & 7 are only accessible on the Arduino Mini 
* (DDRC, PORTC, PINC)
* 
*/

const uint8_t rotaryPin0 = 2;
const uint8_t rotaryBPin = 3;
const uint8_t gatePin = 4;
const uint8_t bendPin = 5;
const uint8_t modPin = 6;
const uint8_t pwLEDPin = 9;
const uint8_t cutoffLEDPin = 10;
const uint8_t rateLEDPin = 11;
const uint8_t portamentoPin = 13;
const uint8_t rotaryPin1 = A0;

volatile uint8_t rotaryVal = 0;
volatile uint8_t buttonVal = 0;

uint8_t noteArray[16];
uint8_t noteArraySize = 0;

MIDI_CREATE_DEFAULT_INSTANCE();
Adafruit_MCP4725 dac0;
Adafruit_MCP4725 dac1;
uint16_t dac0V = 0;
uint16_t dac1V = 0;

uint8_t note = 60;
uint8_t prevNote = 60;

volatile bool port = false; //determines whether portamento will be applied
bool portGo = false;
bool portRate  = false; //true changes pitch at a constant rate, false changes pitch over a constant time
bool untouched = true;
float portFloat = 0;
float prevPortFloat = 0;
float portDiff = 0;
volatile uint8_t portVal = 127; //rate in rate mode, time in time mode
uint8_t portValPrev = 127;
bool midiChange = false; //variable to optimize LED handling

//timer variables
unsigned long t = 0;
unsigned long tprev10 = 0;
unsigned long tprev300 = 0;
unsigned long tprev500 = 0;

uint8_t modGlobal = 0;
int16_t bendGlobal = 8192;

//midi change variables
uint8_t gateLED = 0;

uint8_t encState = 0;

void setup() {
  MIDI.begin(MIDI_CHANNEL_OMNI);
  //MIDI.turnThruOff(); //uncomment for real-world application: only for testing!
  dac0.begin(0x63); //pitch dac
  //dac1.begin(0x62); //control dac
  pinMode(modPin, OUTPUT);
  pinMode(bendPin, OUTPUT);
  pinMode(gatePin, OUTPUT);
  
  pinMode(rateLEDPin, OUTPUT);
  pinMode(cutoffLEDPin, OUTPUT);
  pinMode(pwLEDPin, OUTPUT);
  pinMode(portamentoPin, OUTPUT);
  
  pinMode(rotaryPin0, INPUT);
  pinMode(rotaryPin1, INPUT);
  pinMode(rotaryBPin, INPUT);
  
  attachInterrupt(digitalPinToInterrupt(2), rotaryTurn, FALLING); //rotary encoder turning
  //the button doesnt work lol
  //attachInterrupt(digitalPinToInterrupt(3), rotaryButton, RISING); //rotary encoder push button (assuming normally open spst)
}

void loop() {
  
  midiInput();

  //time optimized operations:
  t = millis();
  if (t >= tprev10 + 10){
    tprev10 = t;
    portamento();
    portLEDS();
    midiLEDS();

    if (t >= tprev300 + 300){ //no need to check this every cycle too
      tprev300 = t;
      button2LED();
    }
    
    if (t >= tprev500 + 500){ //no need to check this every cycle too
      tprev500 = t;
      button1LED();
    }
  }
}

//pin 2 ISR
void rotaryTurn(){
  byte bitTest = PINC & (1);
  if (bitTest != 0){ //if pin A0 (rotaryPin1) is high
    rotaryVal -= 1;
  }else{
    rotaryVal += 1;
  }
}

//pin 3 ISR
/*
void rotaryButton(){
  if (buttonVal == 2){
    buttonVal = 0;
    port = false;
    PORTB &= ~(1 << 5); //turning off the portamento pin

    //turning off PWM
    cbi(TCCR1A, COM1A1); //pin 9
    cbi(TCCR1A, COM1B1); //pin 10
    cbi(TCCR2A, COM2A1); //pin 11
    PORTB &= ~(7 << 1); //turning off all 3 pins
  }else{
    buttonVal += 1;
    port = true;
    if (buttonVal == 1){
      portRate = false;
    }else{
      portRate = true;
    }
    PORTB |= 1 << 5; //turning on the portamento pin
  }
}
*/

void portamento(){
  if (portGo){
    if (untouched){
      portFloat = dac0V;
      untouched = false;
    }
    if (abs((float)dac0V - portFloat) > abs(portDiff)){
      portFloat += portDiff;
      dac0.setVoltage(constrain((uint16_t)portFloat, 0, 4095), false);
    }else{
      dac0.setVoltage(constrain(dac0V, 0, 4095), false);
      portFloat = dac0V;
      portGo = false;
    }
  }
}

void portLEDS(){
  if (buttonVal != 0){
    if (portValPrev != portVal){
      portValPrev = portVal;
      if (portVal < 85){
        analogWrite(rateLEDPin, portVal*3);

        //turning off PWM
        cbi(TCCR1A, COM1A1); //pin 9
        cbi(TCCR1A, COM1B1); //pin 10
        
        PORTB &= ~(3 << 1); // sets both 9 and 10 -- mask is 11111100 => 11111001
        //analogWrite(cutoffLEDPin, 0);
        //analogWrite(pwLEDPin, 0);
      } else if (portVal >= 85 && portVal < 170){
        cbi(TCCR2A, COM2A1); //turning off PWM on pin 11
        cbi(TCCR1A, COM1A1); //turning off PWM on pin 9
        
        PORTB |= 1 << 3;
        //analogWrite(rateLEDPin, 255);
        analogWrite(cutoffLEDPin, (portVal - 85)*3);
        PORTB &= ~(1 << 1);
        //analogWrite(pwLEDPin, 0);
      }else if (portVal >= 170){
        cbi(TCCR1A, COM1B1); //turning off PWM on pin 10
        cbi(TCCR2A, COM2A1); //turning off PWM on pin 11
        
        PORTB |= 3 << 2;
        //analogWrite(rateLEDPin, 255);
        //analogWrite(cutoffLEDPin, 255);
        analogWrite(pwLEDPin, (portVal - 170)*3);
      }
    }
  }
}

void button1LED(){
  if (buttonVal == 1){
    /*
    if (digitalRead(portamentoPin) == HIGH){
      digitalWrite(portamentoPin, LOW);
      }else{
        digitalWrite(portamentoPin, HIGH);
      }
    */
    byte bitTest = PINB & (1 << 5);
    if (bitTest != 0){ //bitwise operations, equivalent to the above commented code but much faster
        PORTB &= ~(1 << 5); //setting pin 13 low
      }else{
        PORTB |= 1 << 5; //setting pin 13 high
      }
      
  }
}

void button2LED(){
  if (buttonVal == 2){
    /*
    if (digitalRead(portamentoPin) == HIGH){
      digitalWrite(portamentoPin, LOW);
      }else{
        digitalWrite(portamentoPin, HIGH);
      }
    */
    byte bitTest = PINB & (1 << 5);
    if (bitTest != 0){ //bitwise operations, equivalent to the above commented code but much faster
        PORTB &= ~(1 << 5); //setting pin 13 low
      }else{
        PORTB |= 1 << 5; //setting pin 13 high
      }
      
  }
}

void midiLEDS(){
  if (midiChange){
    
    //digitalWrite(gatePin, gateLED);
    if (gateLED == 1){
      PORTD |= 1 << 4;
    }else{
      PORTD &= ~(1 << 4);
    }
    
    analogWrite(modPin, modGlobal*2);
    float tempBend = abs(bendGlobal - 8192);
    analogWrite(bendPin, floor(tempBend/32.125));
    midiChange = false;
  }
}

//this function handles all the forseen midi input situations, and is called when midi data is in the serial buffer
void midiInput(){
  if (MIDI.read()){
      midi::MidiType type = MIDI.getType();
  
      switch (type){
        case midi::PitchBend:
          {
            //pitchbend is placed first as it sends the most data while in use
            bendGlobal = MIDI.getData2()*128 + MIDI.getData1();
            int16_t tempBend = floor((((float)bendGlobal - 8192)/4096)*68.267); //this may need to be time optimized
            dac0.setVoltage(dac0V + tempBend, false);
            midiChange = true;
          }
          break;
          
        case midi::NoteOn:
          {
            prevNote = note;
            note = MIDI.getData1();
    
            noteArray[noteArraySize] = note;
            noteArraySize++;
            
            midiChange = true;
            gateLED = 1;
  
            if (note >= 24 && note <= 84){
              dac0V = floor((note - 24)*68.267);
              if (port == false){
                dac0.setVoltage(dac0V, false);
              }else{
                portGo = true;
                prevPortFloat = portFloat;
                if (portRate){
                  portDiff = ((float)portVal/-12.75) + 20; //constant rate regardless of intervallic distance
                }else{
                  portDiff = ((float)dac0V - prevPortFloat)/((float)portVal + 1); //constant time regardless of intervallic distance
                }
              }
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
      
            if (noteArraySize == 0){
              midiChange = true;
              gateLED = 0;
            }else{
              note = noteArray[noteArraySize - 1];
              if (note >= 24 && note <= 84){
                dac0V = (note - 24)*68.267;
                if (port == false){
                  dac0.setVoltage(dac0V, false);
                }else{
                  portGo = true;
                  prevPortFloat = portFloat;
                  if (portRate){
                    portDiff = ((float)portVal/-12.75) + 20;
                  }else{
                    portDiff = ((float)dac0V - prevPortFloat)/((float)portVal + 1);
                  }
                }
              }
            }
          }
        break;
  
      case midi::ControlChange:
        {
          if (MIDI.getData1() == 1){
            modGlobal = MIDI.getData2();
            midiChange = true;
            
          }else if (MIDI.getData1() == 5){
            //portamento time -- must be implemented via software, though it could be selected and manipulated with a pushbutton rotary encoder
          }
          break;
        }
      default:
        {
          //nothing lul
        }
        break;
      }
    }
}
