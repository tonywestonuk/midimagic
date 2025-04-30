#include "driver/i2s.h"

static const i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN),
    .sample_rate = 22050,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = 0,       
    .dma_buf_count = 2,
    .dma_buf_len = 1024, 
    .use_apll=0
};

void play_wav(const char* filename){
  File file = SD.open(filename);

  int16_t fbuf[256];
  uint32_t obuf[256];

  size_t len = file.size();
  size_t bytes_written;
  size_t bytes_read;
  
  // Read 44 byte header.
    len -=file.read(reinterpret_cast<uint8_t*>(fbuf), 44);

  // Read first part of sample.
  for (int i=0; i<16; i++){
    len -=file.read(reinterpret_cast<uint8_t*>(fbuf), sizeof(fbuf));
  }

  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);        // ESP32 will allocated resources to run I2S
  i2s_set_pin(I2S_NUM_0, NULL);                        // Tell it the pins you will be using                    
  i2s_set_dac_mode(I2S_DAC_CHANNEL_RIGHT_EN); // 25 + 26
  i2s_write( I2S_NUM_0, obuf, sizeof(obuf), &bytes_written, portMAX_DELAY);

  // Read rest of file
  int bytesIn = file.readBytes(reinterpret_cast<char*>(fbuf), sizeof(fbuf));
  while (bytesIn > 0){
    for (int i=0; i<(bytesIn/2); i++){
      obuf[i] = (fbuf[i]+32768) << 16;
    }

    i2s_write( I2S_NUM_0, obuf, bytesIn*2, &bytes_written, portMAX_DELAY);
    bytesIn = file.readBytes(reinterpret_cast<char*>(fbuf), sizeof(fbuf));
  }

  //ramp down to zero.
  int j=0;
  for (int i=127; i>63; i-- ){
      obuf[j] = i<<24;
      obuf[j+1] = i<<24;
      obuf[j+2] = i<<24;
      obuf[j+3] = i<<24;
      j+=4;
  }
  i2s_write( I2S_NUM_0, obuf, 1024, &bytes_written, portMAX_DELAY);
  j=0;
  for (int i=63; i>-1; i-- ){
      obuf[j] = i<<24;
      obuf[j+1] = i<<24;
      obuf[j+2] = i<<24;
      obuf[j+3] = i<<24;
      j+=4;
  }

  delay(50);
  i2s_driver_uninstall(I2S_NUM_0);        // ESP32 will allocated resources to run I2S
}
