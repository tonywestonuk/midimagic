#ifndef MIDI_READER_H
#define MIDI_READER_H

#include <Arduino.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mutex>


template <size_t QUEUE_SIZE>
class MidiReader {
public:
    MidiReader(Stream& midiStream, 
              uint8_t lowKey = 0, 
              uint8_t highKey = 127)
        : midiStream(&midiStream),
          lowKey(lowKey),
          highKey(highKey)
    {
        incoming_note_writePtr.store(0);
        incoming_note_readPtr.store(0);
        startTask();
    }

    ~MidiReader() {
        if (taskHandle) {
            vTaskDelete(taskHandle);
        }
    }

    bool popNote(uint8_t& note) {
        const uint8_t readPtr = incoming_note_readPtr.load();
        const uint8_t writePtr = incoming_note_writePtr.load();

        if (readPtr == writePtr) {
            return false; // Queue is empty
        }

        note = incoming_note_queue[readPtr];
        incoming_note_readPtr.store((readPtr + 1) % QUEUE_SIZE);
        return true;
    }

    void clear() {
      std::lock_guard<std::mutex> lock(queueMutex);
      incoming_note_writePtr.store(0);
      incoming_note_readPtr.store(0);
    }

private:
    Stream* midiStream;
    uint8_t lowKey;
    uint8_t highKey;
    TaskHandle_t taskHandle = nullptr;
    
    std::atomic<uint8_t> incoming_note_writePtr;
    std::atomic<uint8_t> incoming_note_readPtr;
    uint8_t incoming_note_queue[QUEUE_SIZE];

    std::mutex queueMutex;

    void startTask() {
        xTaskCreate(
            readMidiTaskWrapper,
            "MidiReaderTask", 
            5000, 
            this,
            1, 
            &taskHandle
        );
    }

    static void readMidiTaskWrapper(void* params) {
        MidiReader* instance = static_cast<MidiReader*>(params);
        instance->runTask();
    }

    uint8_t readMidi() {
        uint8_t midiByteIn;
        do {
            while (!midiStream->available()) {
                vTaskDelay(pdMS_TO_TICKS(1)); 
            }
            midiByteIn = midiStream->read();
        } while ((midiByteIn & 0xF0) == 0xF0);
        return midiByteIn;
    }

    void runTask() {
        uint8_t headerByte = 0, midiReadData1, midiReadData2;
        
        for (;;) {
            midiReadData1 = readMidi();
            
            if ((midiReadData1 & 0x80) == 0x80) {
                headerByte = midiReadData1;
                continue;
            }

            if ((headerByte & 0xE0) != 0xC0) {
                midiReadData2 = readMidi();
                if ((midiReadData2 & 0x80) == 0x80) {
                    headerByte = midiReadData2;
                    continue;
                }
            }

            if ((headerByte & 0xF0) == 0x90) {
                if (midiReadData1 >= lowKey && 
                    midiReadData1 <= highKey && 
                    midiReadData2 >= 20) {
                    
                    uint8_t writePtr = incoming_note_writePtr.load();
                    incoming_note_queue[writePtr] = midiReadData1;
                    incoming_note_writePtr.store(
                        (writePtr + 1) % QUEUE_SIZE
                    );
                }
            }
        }
    }
};

#endif // MIDI_READER_H