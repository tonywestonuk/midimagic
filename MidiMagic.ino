#include "MidiReader.h"
#include "UI.h"

#include <HardwareSerial.h>
#include <atomic>

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>

#include "FS.h"
#include "SD.h"
#include "SPI.h"

#include <PsychicHttp.h>  // Make sure this library is installed

//#define mididebug

HardwareSerial midi(2);  // pins

//#define mididebug TRUE;

void IRAM_ATTR noteOff(int chan, int pitch);
void IRAM_ATTR bankChg(int chan, int bank);
void IRAM_ATTR pgmChg(int chan, int pgm);
void IRAM_ATTR song_loop();

int rotaryNob;

MidiReader<32>* midiReader = nullptr;

//char songName[25];
//std::atomic<bool> songChanged(false);

UI* ui = nullptr;

struct ChannelState {
  byte currentInstr;
  byte currentNotesOn[15];
};
ChannelState channelState[16];

struct Instrument {
  byte midiChannel;
  byte midiBank;
  byte midiPgm;
  byte vol;
  byte flags;
};


int lastMidiMessage = 0;
int lastKeyVelocity = 0;

byte lowKey = 0;
byte highKey = 0;

hw_timer_t* playLoopTimer = NULL;

byte currentKeysPressed[15];

boolean displayUpdate = false;
byte currentButton = 0;
byte nextButton = 0;

uint8_t midiOutputBuffer[512];
size_t midiOutputBufferPtr = 0;

boolean buttonPressedFlag;

int mode = -1;


void setup() {
  Serial.begin(115200);

  // SD Card reader.. sck,miso,mosi,cs
  int sd_cs = 4;
  if (SD.begin(sd_cs)) {
    Serial.println("SD Card mounted");
  }

  midi.begin(31250, SERIAL_8N1, 16, 17);

  ws_setup();
  ui = new UI();


  snprintf(ui->songName, sizeof(ui->songName), "");
  ui->songChanged.store(true);

  midiReader = new MidiReader<32>(midi, 10, 59);
}

int count = 0;
int lastReadNote = 0;


void IRAM_ATTR noteOn(int chan, int pitch, int velocity) {
  if (lastMidiMessage != 0x90 + chan) {
    lastMidiMessage = 0x90 + chan;
    midiOutputBuffer[midiOutputBufferPtr++] = lastMidiMessage;
  }

  midiOutputBuffer[midiOutputBufferPtr++] = pitch;
  midiOutputBuffer[midiOutputBufferPtr++] = velocity;
  byte bytenum = pitch / 8;
  byte bitnum = pitch % 8;
  channelState[chan].currentNotesOn[bytenum] |= 1 << bitnum;

#ifdef mididebug
  Serial.print("note on ");
  Serial.print(chan);
  Serial.print(" ");
  Serial.print(pitch);
  Serial.print(" ");
  Serial.println(velocity);
#endif
}

void IRAM_ATTR midiFlush() {

  if (midiOutputBufferPtr == 0) return;
  midi.write(midiOutputBuffer, midiOutputBufferPtr);
  midi.flush();

#ifdef mididebug
  Serial.print("Flush Bytes: ");
  Serial.println(midiOutputBufferPtr);
#endif

  midiOutputBufferPtr = 0;
}

void IRAM_ATTR midiClear() {
  midiOutputBufferPtr = 0;
}

void IRAM_ATTR allOff(int chan) {
  for (int i = 0; i < 15; i++) {
    for (int j = 0; j < 8; j++) {
      if (channelState[chan].currentNotesOn[i] & (1 << j)) {
        noteOff(chan, i * 8 + j);
      }
    }
  }
}

void IRAM_ATTR noteOff(int chan, int pitch) {

  if (lastMidiMessage != 0x80 + chan) {
    lastMidiMessage = 0x80 + chan;
    midiOutputBuffer[midiOutputBufferPtr++] = lastMidiMessage;
  }

  midiOutputBuffer[midiOutputBufferPtr++] = 0x80 + chan;
  midiOutputBuffer[midiOutputBufferPtr++] = pitch;
  midiOutputBuffer[midiOutputBufferPtr++] = 0;

  byte bytenum = pitch / 8;
  byte bitnum = pitch % 8;
  channelState[chan].currentNotesOn[bytenum] &= ~(1 << bitnum);


#ifdef mididebug
  Serial.print("Note Off ");
  Serial.print(chan);
  Serial.print(" ");
  Serial.println(pitch);
#endif
}

void IRAM_ATTR slotFocus(int slot) {
  midiOutputBuffer[midiOutputBufferPtr++] = 0xB0;
  midiOutputBuffer[midiOutputBufferPtr++] = 115;
  midiOutputBuffer[midiOutputBufferPtr++] = 8 << slot;
}


void IRAM_ATTR voiceChg(int chan, int bank, int pgm) {
  bankChg(chan, bank);
  pgmChg(chan, pgm);
}

void IRAM_ATTR bankChg(int chan, int bank) {
  midiOutputBuffer[midiOutputBufferPtr++] = 0xB0 + chan;
  midiOutputBuffer[midiOutputBufferPtr++] = 0x20;
  midiOutputBuffer[midiOutputBufferPtr++] = bank;

#ifdef mididebug
  Serial.print("Bank Change ");
  Serial.print(chan);
  Serial.print(" ");
  Serial.println(bank);
#endif
}

void IRAM_ATTR pgmChg(int chan, int pgm) {
  lastMidiMessage = 0;
  midiOutputBuffer[midiOutputBufferPtr++] = 0xC0 + chan;
  midiOutputBuffer[midiOutputBufferPtr++] = pgm;

#ifdef mididebug
  Serial.print("pgm Change ");
  Serial.print(chan);
  Serial.print(" ");
  Serial.println(pgm);
#endif
}

void loop() {
  if (ui->songChanged.load()) {
    vTaskPrioritySet(NULL, tskIDLE_PRIORITY + 2);
    song_setup();
    ui->songChanged.store(false);
  }
  song_loop();

  // Delay to allow other processes to continue ()
  delay(1);
}

