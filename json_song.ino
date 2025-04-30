struct note {
  byte row;
  byte track;
  byte pitch;
  byte vol;
  byte instrument;
};

struct event {
  byte row;
  byte previous_rows;
  uint16_t wait;
  char block[12];
  bool mayskip = false;
};

struct instrument {
  byte id;
  byte midi_channel;
  byte midi_bank;
  byte midi_pgm;
  short transpose;
};

struct block {
  short start_note;
  short end_note;
  short start_event;
  short end_event;
  char transpose[12];
  bool restart_on_keychange = true;
};

struct song_state {
  bool paused = true;
  byte currentRow = 0;
  byte currentBlock = 0;
  short lastNotesPlayedRow = -1;
  short lastEventHandled = -1;
  short rowDuration = 100;
  unsigned long rowStartTime;
  unsigned long lastEventHandledTime = 0;
  short rowsSinceLastEvent = 0;
  unsigned long waitingForEventStart = 0;
  short start_rowDuration = 100;
  short start_maxDuration = 400;
  short start_minDuration = 50;
  
};
struct song_state initialSongState;
struct song_state ss;


struct note notes[1000];
struct event events[1000];
struct instrument instruments[20];
struct block blocks[100];

uint8_t transposeMap[256];

unsigned short blockCount = 0;
unsigned short noteCount = 0;
unsigned short eventCount = 0;
unsigned short instCount = 0;
unsigned short rowCounter = 0;

byte mm_transpose = 0;


int lastTrackNote[10];
int lastTrackChan[10];

void song_setup() {

  transposeMap['c'] = 0;
  transposeMap['C'] = 1;
  transposeMap['d'] = 2;
  transposeMap['D'] = 3;
  transposeMap['e'] = 4;
  transposeMap['f'] = 5;
  transposeMap['F'] = 6;
  transposeMap['g'] = 7;
  transposeMap['G'] = 8;
  transposeMap['a'] = 9;
  transposeMap['A'] = 10;
  transposeMap['b'] = 11;

  allOff(1);
  allOff(2);
  allOff(3);
  allOff(4);

  blockCount = 0;
  noteCount = 0;
  eventCount = 0;
  instCount = 0;

  // No song

  ss = initialSongState;

  char tbuf[100];
  snprintf(tbuf, sizeof(tbuf), "{\"song\":\"%s\"}", ui->songName);
  sendWS(tbuf);

  if (strlen(ui->songName) == 0) {
    return;
  }
  
  // Open the file for reading
  snprintf(tbuf, sizeof(tbuf), "/tracks/%s", ui->songName);
  File file = SD.open(tbuf, "r");
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }

  // Allocate a buffer to store the contents of the file
  size_t size = file.size();
  std::shared_ptr<char[]> buf(new char[size]);

  file.readBytes(buf.get(), size);

  // Use ArduinoJson to parse the JSON data
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, buf.get());

  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.f_str());
    return;
  }

  lowKey = doc["low_key"];
  highKey = doc["high_key"];
  slotFocus(doc["slot_focus"]);

  if (doc.containsKey("start_tempo")){
    int tempo = doc["start_tempo"];
    ss.start_rowDuration = 6000 / tempo;
  }

  if (doc.containsKey("low_tempo")){
    int tempo = doc["low_tempo"];
    ss.start_maxDuration = 6000 / tempo;
  }

  if (doc.containsKey("high_tempo")){
    int tempo = doc["high_tempo"];
    ss.start_minDuration = 6000 / tempo;
  }

  ss.rowDuration = ss.start_rowDuration;

  // Load Instruments
  JsonArray j_instruments = doc["instruments"];
  for (JsonArray j_inst : j_instruments) {
    instrument inst;
    inst.id = j_inst[0];
    inst.midi_channel = j_inst[1];
    inst.midi_bank = j_inst[2];
    inst.midi_pgm = j_inst[3];
    inst.transpose = j_inst[4];
    instruments[instCount] = inst;
    instCount += 1;
  }

  // Load Blocks
  JsonArray j_blocks = doc["blocks"];
  for (JsonVariant j_block : j_blocks) {
    JsonArray j_notes = j_block["notes"];
    block block;

    const char* _transpose = j_block["transpose"];
    strncpy(block.transpose, _transpose, 12);

    if (j_block.containsKey("restart_on_keychange")){
            block.restart_on_keychange = j_block["restart_on_keychange"];
    }

    block.start_note = noteCount;

    blockCount += 1;
    for (JsonArray j_note : j_notes) {
      note n;
      n.row = j_note[0];
      n.track = j_note[1];
      n.pitch = j_note[2];
      n.instrument = j_note[3];
      notes[noteCount] = n;
      noteCount += 1;
    }
    block.end_note = noteCount;

    JsonArray j_events = j_block["events"];
    block.start_event = eventCount;
    for (JsonArray j_event : j_events) {
      event n;
      n.row = j_event[0];
      n.previous_rows = j_event[1];
      JsonVariant event_attr = j_event[2];
      const char* e_wait = event_attr["wait"];
      n.wait = 0;
      for (int i = 0; i < 12; i++) {
        if (e_wait[i] != '-') n.wait |= (1 << i);
      }

      const char* e_block = event_attr["block"];
      strncpy(n.block, e_block, 12);

      if (event_attr.containsKey("mayskip")){
        n.mayskip = true;
      }

      events[eventCount] = n;
      eventCount += 1;
    }
    block.end_event = eventCount;

    int blockId = j_block["id"];
    blocks[blockId] = block;
  }

  for (int i = 0; i < 10; i++) {
    lastTrackNote[i] = -1;
  }

  // send pgm changes...
  for (int i = 0; i < 4; i++) {
    voiceChg(instruments[i].midi_channel, instruments[i].midi_bank, instruments[i].midi_pgm);
  }

  midiFlush();

  Serial.println("New song loaded");

  midiReader->clear();


  // Start the song playing.
  ss.rowStartTime = millis();
  ss.paused = false;
}

void IRAM_ATTR playNote(short n);

short IRAM_ATTR getNextEvent() {
  short nextEvent = blocks[ss.currentBlock].start_event;
  for (short eventInBlock = blocks[ss.currentBlock].start_event; eventInBlock < blocks[ss.currentBlock].end_event; eventInBlock++) {
    if (events[eventInBlock].row >= ss.currentRow) {
      nextEvent = eventInBlock;
      break;
    }
  }
  return nextEvent;
}

#define NO_NOTE 255

uint8_t IRAM_ATTR getNextIncomingNote() {
  uint8_t note;

  if (!midiReader->popNote(note)) {
    return NO_NOTE;
  } 

  return note;
}

void IRAM_ATTR song_loop() {
  if (ss.paused) {
    return;
  }

  uint8_t nextIncomingNote = getNextIncomingNote();
  short nextEvent = getNextEvent();

  while (nextIncomingNote != NO_NOTE) {
    bool transposed = setTranspose(nextIncomingNote, ss.currentBlock);
    // If we've transposed, maybe shift block.
    if (transposed) {
        short lastHandled = ss.lastEventHandled;
        if (lastHandled == -1){
          lastHandled = blocks[ss.currentBlock].start_event;
        }    

        if (events[lastHandled].block[mm_transpose] != '-'){
          ss.currentBlock = events[lastHandled].block[mm_transpose] - '0';
        }
        if (blocks[ss.currentBlock].restart_on_keychange){
          ss.currentRow = 0;
          ss.lastEventHandled = blocks[ss.currentBlock].start_event;
          ss.lastEventHandledTime = millis();
          ss.rowsSinceLastEvent = 0;
          ss.rowStartTime = millis();
        }

        nextEvent = getNextEvent();
      }


    // First time handling this event.
    if (ss.lastEventHandled != nextEvent) {

      // move song forward to this event, if not already there...
      short skipRowCounter = 0;
      short newRow = ss.currentRow;
      while (events[nextEvent].row != newRow) {
        newRow = (newRow + 1) % 32;
        skipRowCounter+=1;
      }

      if (skipRowCounter>events[nextEvent].previous_rows){
        nextIncomingNote = getNextIncomingNote();
        continue;
      }

      ss.rowsSinceLastEvent += skipRowCounter;
      ss.currentRow = newRow;

      // row time, is number of rows passed since last event
      if (ss.lastEventHandledTime != 0 && ss.rowsSinceLastEvent != 0) {
        int tmpDur = (millis() - ss.lastEventHandledTime) / ss.rowsSinceLastEvent;
        if (tmpDur<=ss.start_maxDuration && tmpDur>= ss.start_minDuration) ss.rowDuration = tmpDur;
      }

      // Mark we've handled the event
      ss.lastEventHandled = nextEvent;
      ss.lastEventHandledTime = millis();
      ss.rowsSinceLastEvent = 0;
      ss.rowStartTime = millis();
    }

    nextIncomingNote = getNextIncomingNote();
  }

  // send the current row to the webclient.
  char tbuf[100];
  snprintf(tbuf, sizeof(tbuf), "{\"row\":\"%u\",\"block\":\"%u\"}", ss.currentRow, ss.currentBlock);

  //Serial.println(tbuf);
  sendWS(tbuf);

  // if the event is on current row, and unhandled, and not skippable ....procede no futher.
  if (events[nextEvent].row == ss.currentRow && nextEvent != ss.lastEventHandled && !events[nextEvent].mayskip) {
    if (ss.waitingForEventStart == 0) ss.waitingForEventStart = millis();
    
    if ((millis() - ss.waitingForEventStart)> 1000){
      ss.currentRow = 0;
      ss.lastEventHandled = -1;
      ss.lastNotesPlayedRow = -1;
      ss.waitingForEventStart = 0;
      ss.rowsSinceLastEvent = 0;
      ss.rowStartTime = millis();
    }

    return;
  }

  ss.waitingForEventStart = 0;\
  
  // Play notes on this row.
  if (ss.lastNotesPlayedRow != ss.currentRow) {
    for (short noteInBlock = blocks[ss.currentBlock].start_note; noteInBlock < blocks[ss.currentBlock].end_note; noteInBlock++) {
      if (notes[noteInBlock].row == ss.currentRow) {
        playNote(noteInBlock);
      }
    }
    midiFlush();
    ss.lastNotesPlayedRow = ss.currentRow;
  }

  // If row no longer current, move to next row.
  if ((ss.rowStartTime + ss.rowDuration) < millis()) {
    ss.currentRow = (ss.currentRow + 1) % 32;
    ss.rowsSinceLastEvent += 1;
    ss.rowStartTime = millis();
  }
}

void IRAM_ATTR playNote(short n) {

  // Turn off note that is playing in this track
  if (lastTrackNote[notes[n].track] >= 0) {
    noteOff(lastTrackChan[notes[n].track], lastTrackNote[notes[n].track]);
    lastTrackNote[notes[n].track] = -1;
  }

  // turn on new note to play in this track.
  if (notes[n].instrument == 0) {
    lastTrackNote[notes[n].track] = -1;
  } else {
    for (byte i = 0; i < instCount; i++) {
      if (instruments[i].id == notes[n].instrument) {

        int pitch = notes[n].pitch + mm_transpose + instruments[i].transpose;
        if (instruments[i].transpose>=1000){
          pitch = notes[n].pitch + instruments[i].transpose - 1000;
        }

        noteOn(instruments[i].midi_channel, pitch, 80);
        lastTrackNote[notes[n].track] = pitch;
        lastTrackChan[notes[n].track] = instruments[i].midi_channel;
        break;
      }
    }
  }
}

bool setTranspose(uint8_t note, uint8_t currentBlock) {
  byte newTrans = transposeMap[blocks[currentBlock].transpose[note % 12]];
  if (newTrans != mm_transpose) {
    mm_transpose = newTrans;
    return true;
  }

  return false;
}
