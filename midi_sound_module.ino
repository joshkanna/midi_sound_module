// midi_sound_module.ino

#include <usbh_midi.h>
#include <usbhub.h>
#include <SPI.h>
#include <math.h>
#include "driver/i2s.h"
#include <SD.h>
#include <FS.h>

#include "WaveGenerator.h"
#include "SineWave.h"
#include "SquareWave.h"
#include "TriangleWave.h"
#include "SawtoothWave.h"

// --- USB Host Shield Setup --- 
USB Usb; 
USBH_MIDI Midi(&Usb);

// I2S settings
#define I2S_SAMPLE_RATE     44100
#define I2S_BCK_IO          26
#define I2S_WS_IO           25
#define I2S_DATA_OUT_IO     27

// Hardware pins
#define LED_PIN 2
#define BUTTON_PIN 22


// SPI Config
SPIClass SD_SPI(HSPI);

const int SD_CLK = 2;
const int SD_MISO = 4;
const int SD_MOSI = 15;
const int SD_SS = 21;


int lastButtonState = 1;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

// Wave generator registry
WaveGenerator* waveGenerators[] = {
  new SineWave(),
  new SquareWave(),
  new TriangleWave(),
  new SawtoothWave()
};

const int numWaveTypes = sizeof(waveGenerators) / sizeof(waveGenerators[0]);
int currentWaveIndex = 0;
float currentWavetable[WAVETABLE_SIZE];


// --- Voice Management ---
# define NUM_VOICES 30


struct Voice {
  bool active = false;
  byte midiNote = 0;
  bool isPressed = false;
  float frequency = 0.0f;
  float phase = 0.0f;
  float phaseIncrement = 0.0f;
};

Voice voices[NUM_VOICES];
bool sustainPedalOn = false;
TaskHandle_t audioTaskHandle = NULL;

// MIDI note to frequency conversion
float midiNoteToFrequency(byte note) {
  return 440.0f * powf(2.0f, (note - 69) / 12.0f);
}

// Load current wave generator
void loadCurrentWave() {
  WaveGenerator* currentGen = waveGenerators[currentWaveIndex];
  currentGen->generateWavetable(currentWavetable);
  Serial.println("Loaded wave: " + String(currentGen->getName()));
}

void saveCurrentWave() {
  File file = SD.open("/current_wave.txt", FILE_WRITE);
  if (file) {
    file.println(currentWaveIndex);
    file.close();
    Serial.println("Wave saved: " + String(waveGenerators[currentWaveIndex]->getName()));
  }
}

void loadWaveFromSD() {
  if (SD.exists("/current_wave.txt")) {  // Added missing slash
    File file = SD.open("/current_wave.txt", FILE_READ);
    if (file) {
      int savedWave = file.parseInt();
      if (savedWave >= 0 && savedWave < numWaveTypes) {
        currentWaveIndex = savedWave;
        Serial.println("Wave loaded from SD: " + String(waveGenerators[currentWaveIndex]->getName()));
      } else {
        Serial.println("Invalid wave index in SD file, using default");
        currentWaveIndex = 0;
      }
      file.close();
    }
  } else {
    Serial.println("No saved wave found, using default (Sine)");
    currentWaveIndex = 0;
  }

  loadCurrentWave();
}

void switchWave() {
  currentWaveIndex = (currentWaveIndex + 1) % numWaveTypes;
  loadCurrentWave();

  // LED feedback
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
    delay(100);
  }

  saveCurrentWave();
}

// Button handling
void handleButton() {
  int reading = digitalRead(BUTTON_PIN);
  
  // Simple edge detection: look for transition from 1 to 0
  if (reading == 0 && lastButtonState == 1) {
    // Button just pressed - check if enough time has passed
    if (millis() - lastDebounceTime > debounceDelay) {
      Serial.println("Button pressed!");
      switchWave();
      lastDebounceTime = millis();
    }
  }
  
  lastButtonState = reading;
}

// Audio generation task
void audioTask(void *parameter) {
  const int samples = 256;
  int16_t buffer[samples * 2];

  Serial.println("Audio task started on core " + String(xPortGetCoreID()));

  while (true) {
    WaveGenerator* currentGen = waveGenerators[currentWaveIndex];

    for (int i = 0; i < samples; ++i) {
      float total_value = 0.0f;
      int active_voices_count = 0;

      for (int v = 0; v < NUM_VOICES; ++v) {
        if (voices[v].active) {
          float value;

          if (currentGen->useCustomPhase()) {
            value = currentGen->generateSample(voices[v].phase);
          } else {

            // Convert phase from 0-2pi to 0-1024
            float phase_normalised = voices[v].phase / (twoPi);
            float table_pos = phase_normalised * WAVETABLE_SIZE;

            // Indices for linear interpolation
            int index1 = (int)table_pos % WAVETABLE_SIZE;
            int index2 = (index1 + 1) % WAVETABLE_SIZE;
            float fraction = table_pos - (int)table_pos;

            // linear interpolation to smooth out staircase effect
            value = currentWavetable[index1] * (1.0f - fraction) + currentWavetable[index2] * fraction;
          }
          total_value += value;
          active_voices_count++;

          // Update phase
          voices[v].phase += voices[v].phaseIncrement;
          if (voices[v].phase >= twoPi) {
            voices[v].phase -= twoPi;
          }
        }
      }
      float scale_factor = (active_voices_count > 0) ?
        currentGen->getVolumeScale() / active_voices_count : 0.0f;
      int16_t audio_sample = (int16_t)(total_value * scale_factor);

      buffer[2 * i] = audio_sample;
      buffer[2 * i + 1] = audio_sample;
    }

    size_t bytes_written;
    i2s_write(I2S_NUM_0, buffer, sizeof(buffer), &bytes_written, portMAX_DELAY);
    taskYIELD();
  }
}

// MIDI handlers

void handleNoteOn(byte channel, byte note, byte velocity) {
    if (velocity > 0) {
        for (int v = 0; v < NUM_VOICES; ++v) {
            if (!voices[v].active) {
                voices[v].active = true;
                voices[v].midiNote = note;
                voices[v].frequency = midiNoteToFrequency(note);
                voices[v].phaseIncrement = 2 * 3.14159265359f * voices[v].frequency / I2S_SAMPLE_RATE;
                voices[v].isPressed = true;
                voices[v].phase = 0.0f;
                return;
            }
        }
    }
}

void handleNoteOff(byte channel, byte note, byte velocity) {
    for (int v = 0; v < NUM_VOICES; ++v) {
        if (voices[v].active && voices[v].midiNote == note && voices[v].isPressed) {
            if (!sustainPedalOn) {
                voices[v].active = false;
            }
            voices[v].isPressed = false;
            return;
        }
    }
}

void handleControlChange(byte channel, byte number, byte value) {
    if (number == 64) {
        if (value >= 64) {
            sustainPedalOn = true;
        } else {
            sustainPedalOn = false;
            for (int v = 0; v < NUM_VOICES; ++v) {
                if (voices[v].active && !voices[v].isPressed) {
                    voices[v].active = false;
                }
            }
        }
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT);

    SD_SPI.begin(SD_CLK, SD_MISO, SD_MOSI, SD_SS);
        
    // Initialize SD card
    if (SD.begin(SD_SS, SD_SPI, 25000000)) {
        Serial.println("SD Card initialized");
        loadWaveFromSD();
    } else {
        Serial.println("SD Card failed, using defaults");
        loadCurrentWave();
    }

    // USB Host Shield init
    if (Usb.Init() == -1) {
        Serial.println("USB Host Shield init failed!");
        while (1);
    }
    
    // I2S setup (same as before)
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = I2S_SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S_MSB,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 256,
        .use_apll = true,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BCK_IO,
        .ws_io_num = I2S_WS_IO,
        .data_out_num = I2S_DATA_OUT_IO,
        .data_in_num = I2S_PIN_NO_CHANGE
    };

    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pin_config);
    i2s_set_clk(I2S_NUM_0, I2S_SAMPLE_RATE, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_STEREO);

    xTaskCreatePinnedToCore(audioTask, "AudioTask", 16384, NULL, 3, &audioTaskHandle, 1);
    
    Serial.println("System ready - Current wave: " + String(waveGenerators[currentWaveIndex]->getName()));
}

void loop() {
    Usb.Task();

    handleButton();

    uint8_t buf[3];
    uint8_t size = Midi.RecvData(buf);
    if (size > 0) {
        uint8_t status = buf[0] & 0xF0;
        uint8_t data1 = buf[1];
        uint8_t data2 = buf[2];

        if (status == 0x90 && data2 > 0) {
            handleNoteOn(0, data1, data2);
        } else if ((status == 0x90 && data2 == 0) || status == 0x80) {
            handleNoteOff(0, data1, data2);
        } else if (status == 0xB0) {
            handleControlChange(0, data1, data2);
        }
    }

    delayMicroseconds(50);
}

