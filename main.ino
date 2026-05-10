#include "BluetoothA2DPSource.h"
#include <LiquidCrystal_I2C.h>
#include <SD.h>
#include <SPI.h>

// --- USER SETTINGS ---
const char* SPEAKER_NAME = "ZEB-NOBLE+";
const char* MUSIC_FOLDER = "/Music_ESP";

// --- PIN DEFINITIONS ---
#define BTN_PLAY   12
#define BTN_PREV   13
#define BTN_NEXT   14
#define BTN_VOL_UP 32
#define BTN_VOL_DN 33
#define SD_CS_PIN   5

// --- AUDIO BUFFER (8KB) ---
#define BUFFER_SIZE 8192
static uint8_t audioBuffer[BUFFER_SIZE];
static volatile int bufferReadPos  = 0;
static volatile int bufferWritePos = 0;
static volatile int bufferFilled   = 0;
static volatile bool bufferReady   = false;

// --- OBJECTS ---
BluetoothA2DPSource a2dp_source;
LiquidCrystal_I2C lcd(0x27, 16, 2);

// --- STATE VARIABLES ---
File root;
File currentFile;
bool isPlaying        = false;
bool songFinished     = false;
int  volume           = 80;
String currentSongName = "";

// --- SCROLL VARIABLES ---
unsigned long lastScrollTime = 0;
int scrollPosition = 0;

// --- DEBOUNCE ---
unsigned long lastDebounceTime = 0;

// ---------------------------------------------------------------
// FILL BUFFER FROM SD CARD
// ---------------------------------------------------------------
void fillBuffer() {
  if (!isPlaying || !currentFile) return;

  while (bufferFilled < BUFFER_SIZE - 128) {
    if (!currentFile.available()) {
      songFinished = true;
      break;
    }
    int toRead = min(128, BUFFER_SIZE - bufferFilled);
    uint8_t temp[128];
    int bytesRead = currentFile.read(temp, toRead);
    if (bytesRead <= 0) break;

    for (int i = 0; i < bytesRead; i++) {
      audioBuffer[bufferWritePos] = temp[i];
      bufferWritePos = (bufferWritePos + 1) % BUFFER_SIZE;
    }
    bufferFilled += bytesRead;
  }

  if (bufferFilled > BUFFER_SIZE / 2) bufferReady = true;
}

// ---------------------------------------------------------------
// AUDIO CALLBACK
// ---------------------------------------------------------------
int32_t get_sound_data(uint8_t* data, int32_t length) {
  Frame* frame = (Frame*)data;
  int32_t frame_count = length / sizeof(Frame);

  if (!isPlaying || !bufferReady) {
    memset(data, 0, length);
    return length;
  }

  for (int i = 0; i < frame_count; i++) {
    if (bufferFilled >= 2) {
      int16_t sample = (int16_t)(audioBuffer[bufferReadPos] |
                       (audioBuffer[(bufferReadPos + 1) % BUFFER_SIZE] << 8));
      bufferReadPos = (bufferReadPos + 2) % BUFFER_SIZE;
      bufferFilled -= 2;
      frame[i].channel1 = sample;
      frame[i].channel2 = sample;
    } else {
      frame[i].channel1 = 0;
      frame[i].channel2 = 0;
    }
  }

  return length;
}

// ---------------------------------------------------------------
// FORWARD DECLARATIONS
// ---------------------------------------------------------------
void updateStaticScreen();
void playNextSong();
void playPrevSong();
void handleButtons();
void handleScrolling();
void showMessage(String line1, String line2 = "");

// ---------------------------------------------------------------
// SETUP
// ---------------------------------------------------------------
void setup() {
  Serial.begin(115200);

  pinMode(BTN_PLAY,   INPUT_PULLUP);
  pinMode(BTN_PREV,   INPUT_PULLUP);
  pinMode(BTN_NEXT,   INPUT_PULLUP);
  pinMode(BTN_VOL_UP, INPUT_PULLUP);
  pinMode(BTN_VOL_DN, INPUT_PULLUP);

  lcd.init();
  lcd.backlight();
  showMessage("Init SD Card...");

  delay(500);
  SPI.begin(18, 19, 23, 5);

  if (!SD.begin(SD_CS_PIN)) {
    showMessage("SD Mount FAIL!", "Check wiring");
    Serial.println("SD Card mount failed!");
    while (1);
  }

  root = SD.open(MUSIC_FOLDER);
  if (!root || !root.isDirectory()) {
    showMessage("Folder Missing!", MUSIC_FOLDER);
    Serial.println("Music folder not found!");
    while (1);
  }

  Serial.println("SD OK. Connecting Bluetooth...");
  showMessage("Pairing...", SPEAKER_NAME);

  a2dp_source.set_auto_reconnect(true);
  a2dp_source.start_raw(SPEAKER_NAME, get_sound_data);
  a2dp_source.set_volume(volume);

  showMessage("Connected!", SPEAKER_NAME);
  delay(1000);

  playNextSong();
}

// ---------------------------------------------------------------
// MAIN LOOP
// ---------------------------------------------------------------
void loop() {
  fillBuffer();

  if (songFinished) {
    songFinished = false;
    playNextSong();
  }

  handleButtons();
  handleScrolling();
  delay(5);
}

// ---------------------------------------------------------------
// BUTTON HANDLING
// ---------------------------------------------------------------
void handleButtons() {
  if (millis() - lastDebounceTime < 250) return;

  if (digitalRead(BTN_PLAY) == LOW) {
    isPlaying = !isPlaying;
    updateStaticScreen();
    lastDebounceTime = millis();
  }
  else if (digitalRead(BTN_NEXT) == LOW) {
    playNextSong();
    lastDebounceTime = millis();
  }
  else if (digitalRead(BTN_PREV) == LOW) {
    playPrevSong();
    lastDebounceTime = millis();
  }
  else if (digitalRead(BTN_VOL_UP) == LOW) {
    volume = min(127, volume + 10);
    a2dp_source.set_volume(volume);
    updateStaticScreen();
    lastDebounceTime = millis();
  }
  else if (digitalRead(BTN_VOL_DN) == LOW) {
    volume = max(0, volume - 10);
    a2dp_source.set_volume(volume);
    updateStaticScreen();
    lastDebounceTime = millis();
  }
}

// ---------------------------------------------------------------
// PLAY NEXT SONG
// ---------------------------------------------------------------
void playNextSong() {
  isPlaying      = false;
  bufferReady    = false;
  bufferFilled   = 0;
  bufferReadPos  = 0;
  bufferWritePos = 0;

  if (currentFile) currentFile.close();

  while (true) {
    File entry = root.openNextFile();

    if (!entry) {
      root.rewindDirectory();
      entry = root.openNextFile();
      if (!entry) {
        showMessage("No songs found!", "");
        return;
      }
    }

    if (entry.isDirectory()) {
      entry.close();
      continue;
    }

    String name = String(entry.name());
    name.toUpperCase();

    if (!name.endsWith(".WAV")) {
      entry.close();
      continue;
    }

    currentFile = entry;
    currentFile.seek(44);

    currentSongName = String(entry.name());
    currentSongName.replace(".wav", "");
    currentSongName.replace(".WAV", "");

    songFinished   = false;
    scrollPosition = 0;

    Serial.print("Now playing: ");
    Serial.println(currentSongName);

    showMessage("Buffering...", currentSongName.substring(0, 16));
    isPlaying = true;
    fillBuffer();

    updateStaticScreen();
    return;
  }
}

// ---------------------------------------------------------------
// PLAY PREV SONG
// ---------------------------------------------------------------
void playPrevSong() {
  root.rewindDirectory();
  int total = 0;
  while (true) {
    File f = root.openNextFile();
    if (!f) break;
    if (!f.isDirectory()) {
      String n = String(f.name());
      n.toUpperCase();
      if (n.endsWith(".WAV")) total++;
    }
    f.close();
  }
  if (total == 0) return;

  root.rewindDirectory();
  int currentIndex = 0;
  while (true) {
    File f = root.openNextFile();
    if (!f) break;
    if (!f.isDirectory()) {
      String n = String(f.name());
      n.toUpperCase();
      if (n.endsWith(".WAV")) {
        String baseName = String(f.name());
        baseName.replace(".wav", "");
        baseName.replace(".WAV", "");
        if (baseName == currentSongName) break;
        currentIndex++;
      }
    }
    f.close();
  }

  int prevIndex = (currentIndex - 1 + total) % total;

  isPlaying      = false;
  bufferReady    = false;
  bufferFilled   = 0;
  bufferReadPos  = 0;
  bufferWritePos = 0;

  root.rewindDirectory();
  if (currentFile) currentFile.close();

  int count = 0;
  while (true) {
    File entry = root.openNextFile();
    if (!entry) break;
    if (!entry.isDirectory()) {
      String n = String(entry.name());
      n.toUpperCase();
      if (n.endsWith(".WAV")) {
        if (count == prevIndex) {
          currentFile = entry;
          currentFile.seek(44);
          currentSongName = String(entry.name());
          currentSongName.replace(".wav", "");
          currentSongName.replace(".WAV", "");
          songFinished   = false;
          scrollPosition = 0;
          Serial.print("Now playing (prev): ");
          Serial.println(currentSongName);
          showMessage("Buffering...", currentSongName.substring(0, 16));
          isPlaying = true;
          fillBuffer();
          updateStaticScreen();
          return;
        }
        count++;
        entry.close();
      } else {
        entry.close();
      }
    } else {
      entry.close();
    }
  }
}

// ---------------------------------------------------------------
// LCD: STATIC STATUS LINE (row 1)
// ---------------------------------------------------------------
void updateStaticScreen() {
  lcd.setCursor(0, 1);
  if (isPlaying) lcd.print("PLAY  Vol:");
  else           lcd.print("PAUSE Vol:");

  int volPercent = map(volume, 0, 127, 0, 100);
  String volStr  = String(volPercent) + "%   ";
  lcd.print(volStr);
}

// ---------------------------------------------------------------
// LCD: SCROLLING SONG NAME (row 0)
// ---------------------------------------------------------------
void handleScrolling() {
  if (millis() - lastScrollTime < 350) return;
  lastScrollTime = millis();

  lcd.setCursor(0, 0);

  if (currentSongName.length() <= 16) {
    String padded = currentSongName;
    while (padded.length() < 16) padded += ' ';
    lcd.print(padded);
    return;
  }

  String paddedName = currentSongName + "   ";
  int len = paddedName.length();

  String display = "";
  for (int i = 0; i < 16; i++) {
    display += paddedName[(scrollPosition + i) % len];
  }
  lcd.print(display);

  scrollPosition = (scrollPosition + 1) % len;
}

// ---------------------------------------------------------------
// LCD: SHOW A TEMPORARY MESSAGE
// ---------------------------------------------------------------
void showMessage(String line1, String line2) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1.substring(0, 16));
  if (line2.length() > 0) {
    lcd.setCursor(0, 1);
    lcd.print(line2.substring(0, 16));
  }
}
