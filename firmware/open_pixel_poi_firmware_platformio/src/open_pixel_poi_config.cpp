#ifndef _OPEN_PIXEL_POI_CONFIG
#define _OPEN_PIXEL_POI_CONFIG

#include <FS.h>
#include <LittleFS.h>
#include <Preferences.h>
#include "config.h"


//#define DEBUG  // Comment this line out to remove printf statements in released version
#ifdef DEBUG
#define debugf(...) Serial.print("  <<config>> ");Serial.printf(__VA_ARGS__);
#define debugf_noprefix(...) Serial.printf(__VA_ARGS__);
#else
#define debugf(...)
#define debugf_noprefix(...)
#endif

enum DisplayState {
  DS_PATTERN,
  DS_PATTERN_ALL,
  DS_PATTERN_ALL_ALL,
  DS_WAITING,
  DS_WAITING2,
  DS_WAITING3,
  DS_WAITING4,
  DS_WAITING5,
  DS_VOLTAGE,
  DS_VOLTAGE2,
  DS_BANK,
  DS_BRIGHTNESS,
  DS_SPEED,
  DS_SHUTDOWN,
  DS_PALETTE_MENU,
  DS_PALETTE_SELECT
};


enum BatteryState {
  BAT_OK,
  BAT_LOW,
  BAT_CRITICAL,
  BAT_SHUTDOWN,
};
  
class OpenPixelPoiConfig {
  private:
    Preferences preferences;
    File patternFile;
    
  public:
    // Runtime State
    float batteryVoltage = BATTERY_VOLTAGE_LOW;
    BatteryState batteryState = BAT_OK;
    DisplayState displayState = DS_PATTERN;
    long displayStateLastUpdated = 0;
    // Hardware Settings (Defaults from config.h but can be overriden using the app) 
    uint8_t hardwareVersion;
    uint8_t ledType;
    uint8_t ledCount;
    String deviceName;
    // Display Settings (come in from the app or changed via button)
    uint8_t ledBrightness;
    uint8_t ledBrightnessOptions[6]; 
    uint16_t animationSpeed;
    uint16_t animationSpeedOptions[6];
    uint8_t patternSlot;
    uint8_t patternBank;
    uint8_t patternShuffleDuration;
    // Pattern
    uint8_t frameHeight; 
    uint16_t frameCount;
    uint8_t *pattern = (uint8_t *) malloc(PATTERN_PIXEL_LIMIT * 3 * sizeof(uint8_t));
    uint32_t patternLength;

    // Real-Time FX & Transition Engine (Zero-RAM Overhead)
    uint8_t paletteFxMode = 0; // 0=Normal True-Color, 1..20=Palettes
    uint8_t motionFxMode = 0;  // 0=Solid, 1=Flow Up, 2=Flow Down, 3=Matrix Rain, 4=Tidal Wave, 5=Plasma, 6=Stardust, 7=Pulse, 8=POV Spin, 9=Spiral, 10=Strobe
    uint8_t blendMode = 0;     // 0=Cut (OFF by default). Transitions ONLY run when chosen!
    uint8_t paletteSpeed = 3;  // 1-10 speed multiplier
    unsigned long blendStartTime = 0;
    bool autoPaletteMorph = false;
    unsigned long lastPaletteMorphTime = 0;
    uint16_t paletteMorphIntervalSeconds = 10;

    // Sequencer
    uint8_t *sequencer = (uint8_t *) malloc(1785*sizeof(uint8_t)); // 255 Instruction max (7 bits per instruction)
    uint16_t sequencerLength;
    int sequencerStep;
    ulong sequencerDelayed;

    // Variables
    long configLastUpdated;

    uint8_t getActivePatternIndex() {
      return ((this->patternSlot % PATTERN_BANK_SIZE) + ((this->patternBank % PATTERN_BANK_COUNT) * PATTERN_BANK_SIZE));
    }

    void setPaletteFxMode(uint8_t mode) {
      this->paletteFxMode = mode % 25;
      this->configLastUpdated = millis();
    }

    void setMotionFxMode(uint8_t mode) {
      this->motionFxMode = mode % 15;
      this->configLastUpdated = millis();
    }

    void setBlendMode(uint8_t mode) {
      this->blendMode = mode % 10;
      if (this->blendMode > 0) {
        this->blendStartTime = millis(); // Instant visual preview
      }
      this->configLastUpdated = millis();
    }

    void setPaletteSpeed(uint8_t speed) {
      this->paletteSpeed = max((uint8_t)1, min((uint8_t)10, speed));
      this->configLastUpdated = millis();
    }

    void setHardwareVersion(uint8_t hardwareVersion) {
      this->hardwareVersion = 2;
      preferences.putUChar("hardwareVersion", 2);
    }

    void setLedType(uint8_t ledType) {
      this->ledType = 2; // Always DotStar
      preferences.putUChar("ledType", 2);
    }

    void setLedCount(uint8_t ledCount) {
      this->ledCount = 55; // Always 55 LEDs
      preferences.putUChar("ledCount", 55);
    }

    void setDeviceName(String deviceName) {
      debugf("Save Device Name = %s\n", deviceName);
      preferences.putString("deviceName", deviceName);
    }

    void setLedBrightness(uint8_t ledBrightness) {
      debugf("Save Brightness = %d\n", ledBrightness);
      this->ledBrightness = ledBrightness;
      preferences.putChar("brightness", this->ledBrightness);
      this->configLastUpdated = millis();
    }

    void setLedBrightnessOptions(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, uint8_t b5) {
      debugf("Save Brightness Options = %d %d %d %d %d %d\n", b0, b1, b2, b3, b4, b5);
      this->ledBrightnessOptions[0] = max((uint8_t)1, min((uint8_t)100, b0));
      this->ledBrightnessOptions[1] = max((uint8_t)1, min((uint8_t)100, b1));
      this->ledBrightnessOptions[2] = max((uint8_t)1, min((uint8_t)100, b2));
      this->ledBrightnessOptions[3] = max((uint8_t)1, min((uint8_t)100, b3));
      this->ledBrightnessOptions[4] = max((uint8_t)1, min((uint8_t)100, b4));
      this->ledBrightnessOptions[5] = max((uint8_t)1, min((uint8_t)100, b5));
      preferences.putChar("brightnessOp0", this->ledBrightnessOptions[0]);
      preferences.putChar("brightnessOp1", this->ledBrightnessOptions[1]);
      preferences.putChar("brightnessOp2", this->ledBrightnessOptions[2]);
      preferences.putChar("brightnessOp3", this->ledBrightnessOptions[3]);
      preferences.putChar("brightnessOp4", this->ledBrightnessOptions[4]);
      preferences.putChar("brightnessOp5", this->ledBrightnessOptions[5]);
      this->configLastUpdated = millis();
    }

    void setAnimationSpeed(uint16_t animationSpeed) {
      debugf("Save Speed = %d\n", animationSpeed);
      this->animationSpeed = animationSpeed;
      preferences.putUShort("animationSpeed", this->animationSpeed);
      this->configLastUpdated = millis();
    }

    void setAnimationSpeedOptions(uint16_t s0, uint16_t s1, uint16_t s2, uint16_t s3, uint16_t s4, uint16_t s5) {
      debugf("Save Brightness Options = %d %d %d %d %d %d\n", s0, s1, s2, s3, s4, s5);
      this->animationSpeedOptions[0] = max((uint16_t)1, min((uint16_t)2000, s0));
      this->animationSpeedOptions[1] = max((uint16_t)1, min((uint16_t)2000, s1));
      this->animationSpeedOptions[2] = max((uint16_t)1, min((uint16_t)2000, s2));
      this->animationSpeedOptions[3] = max((uint16_t)1, min((uint16_t)2000, s3));
      this->animationSpeedOptions[4] = max((uint16_t)1, min((uint16_t)2000, s4));
      this->animationSpeedOptions[5] = max((uint16_t)1, min((uint16_t)2000, s5));
      preferences.putUShort("animSpeedOp0", this->animationSpeedOptions[0]);
      preferences.putUShort("animSpeedOp1", this->animationSpeedOptions[1]);
      preferences.putUShort("animSpeedOp2", this->animationSpeedOptions[2]);
      preferences.putUShort("animSpeedOp3", this->animationSpeedOptions[3]);
      preferences.putUShort("animSpeedOp4", this->animationSpeedOptions[4]);
      preferences.putUShort("animSpeedOp5", this->animationSpeedOptions[5]);
      this->configLastUpdated = millis();
    }

    void setPatternSlot(uint8_t patternSlot, bool save) {
      debugf("Save Pattern Slot = %d\n", patternSlot);

      if (this->blendMode > 0) {
        this->blendStartTime = millis();
      }

      this->patternSlot = patternSlot % PATTERN_BANK_SIZE;

      if(save){
        preferences.putChar("patternSlot", this->patternSlot);
      }

      loadFrameHeight();
      loadFrameCount();
      startLoadingPattern();

      debugf("- frame\n");
      debugf("  - height = %d\n", this->frameHeight);
      debugf("  - count = %d\n", this->frameCount);
      
      this->configLastUpdated = millis();
    }

    void setPatternBank(uint8_t patternBank, bool save) {
      if (this->blendMode > 0) {
        this->blendStartTime = millis();
      }
      this->patternBank = patternBank % PATTERN_BANK_COUNT;
      this->patternSlot = this->patternSlot % PATTERN_BANK_SIZE;
      this->paletteFxMode = 0; // 100% True-Color RGB by default (no unwanted rainbow bleed)
      this->motionFxMode = 0;

      if(save){
        preferences.putChar("patternBank", this->patternBank);
        preferences.putChar("patternSlot", this->patternSlot);
      }
      loadFrameHeight();
      loadFrameCount();
      startLoadingPattern();
      
      this->configLastUpdated = millis();
    }






    void setPatternShuffleDuration(uint8_t patternShuffleDuration) {
      debugf("Save Pattern Shuffle Duration = %d\n", patternShuffleDuration);
      this->patternShuffleDuration = patternShuffleDuration;
      preferences.putChar("patShuffleDur", this->patternShuffleDuration);
      this->configLastUpdated = millis();
    }
    
    void setFrameHeight(uint8_t frameHeight) {
      uint8_t h = (frameHeight >= 20) ? frameHeight : 55;
      this->frameHeight = h;
      String key = "p";
      key += this->getActivePatternIndex();
      key += "Height";
      preferences.putUChar(key.c_str(), h);
      this->configLastUpdated = millis();
    }
    
    void setFrameCount(uint16_t frameCount) {
      uint16_t fc = (frameCount >= 2) ? frameCount : 30;
      this->frameCount = fc;
      String key = "p";
      key += this->getActivePatternIndex();
      key += "FCount";
      preferences.putUShort(key.c_str(), fc);
      this->configLastUpdated = millis();
    }
    
    void savePattern() {
      debugf("Save Pattern\n");
      debugf("- length = %d", this->patternLength);
      for (int i=0; i<this->patternLength; i+=3) {
        if (i%this->frameHeight*3 == 0) {
          debugf_noprefix("\n");
          debugf("  ");
        }
        debugf_noprefix("0x%02X%02X%02X ", this->pattern[i], this->pattern[i+1], this->pattern[i+2]);
      }
      debugf_noprefix("\n");

      File file = LittleFS.open(String("/pattern") + this->getActivePatternIndex() + ".oppp", FILE_WRITE);
      if(!file || file.isDirectory()){
        debugf("− failed to open file for writing\n");
      }else{
        file.write(this->pattern, this->patternLength);
        file.close();
      }
      this->configLastUpdated = millis();
    }

    void fillDefaultPattern(){
      this->frameHeight = 55;
      this->frameCount = 30;
      this->patternLength = this->frameHeight * this->frameCount * 3;
      for (int i = 0; i < this->frameCount; i++) {
        for (int j = 0; j < this->frameHeight; j++) {
          uint8_t hue = (j * 256 / 55 + i * 8) & 0xFF;
          uint8_t r = (hue < 85) ? (hue * 3) : ((hue < 170) ? (255 - (hue - 85) * 3) : 0);
          uint8_t g = (hue < 85) ? (255 - hue * 3) : ((hue < 170) ? 0 : ((hue - 170) * 3));
          uint8_t b = (hue < 85) ? 0 : ((hue < 170) ? ((hue - 85) * 3) : (255 - (hue - 170) * 3));
          pattern[(i * this->frameHeight * 3) + (j*3) + 0] = r;
          pattern[(i * this->frameHeight * 3) + (j*3) + 1] = g;
          pattern[(i * this->frameHeight * 3) + (j*3) + 2] = b;
        }
      }
    }

    void startLoadingPattern(){
      if(patternFile){
        patternFile.close();
      }
      String filename = String("/pattern") + this->getActivePatternIndex() + ".oppp";
      patternFile = LittleFS.open(filename, "r");
      
      bool isCorruptOrMissing = false;
      if(!patternFile || patternFile.isDirectory()){
        isCorruptOrMissing = true;
      } else {
        size_t availableBytes = patternFile.available();
        // A valid 55px pattern must have at least 1 frame (55 * 3 = 165 bytes)
        if (availableBytes < 165) {
          isCorruptOrMissing = true;
        } else {
          size_t bytesToRead = min(availableBytes, (size_t)(PATTERN_PIXEL_LIMIT * 3));
          patternFile.read(pattern, bytesToRead);
          this->patternLength = bytesToRead;
        }
        patternFile.close();
      }

      if (isCorruptOrMissing || this->patternLength < 165) {
        fillDefaultPattern();
        File file = LittleFS.open(filename, FILE_WRITE);
        if(file && !file.isDirectory()){
          file.write(this->pattern, this->patternLength);
          file.close();
        }
        String keyH = String("p") + this->getActivePatternIndex() + "Height";
        String keyC = String("p") + this->getActivePatternIndex() + "FCount";
        preferences.putUChar(keyH.c_str(), 55);
        preferences.putUShort(keyC.c_str(), 30);
        this->frameHeight = 55;
        this->frameCount = 30;
      }
    }

    void continueLoadingPattern(){
      // No-op: Full pattern is already loaded instantaneously into RAM!
    }

    void loadFrameHeight(){
      String key = "p";
      key += this->getActivePatternIndex();
      key += "Height";
      uint8_t h = preferences.getUChar(key.c_str(), 55);
      if (h < 20 || h > 255) h = 55; // Strict 55px safety clamp (prevents 2-LED glitch)
      this->frameHeight = h;
    }

    void loadFrameCount(){
      String key = "p";
      key += this->getActivePatternIndex();
      key += "FCount";
      uint16_t fc = preferences.getUShort(key.c_str(), 30);
      if (fc < 2 || fc > 2000) fc = 30;
      this->frameCount = fc;
    }

    void saveSequencer() {
      debugf("Save Sequencer\n");
      debugf("- length = %d\n", this->sequencerLength);

      preferences.putUShort("sequencerLength", this->sequencerLength);

      File file = LittleFS.open("/sequencer.opps", FILE_WRITE);
      if(!file || file.isDirectory()){
        debugf("− failed to open file for writing\n");
      }else{
        debugf(" - opened file for writing: %d\n");
        
        int written = file.write(this->sequencer, this->sequencerLength);
        file.close();
        debugf(" - this much written: %d\n", written);
      }
    }

    void loadSequencer(){
      this->sequencerLength = preferences.getUShort("sequencerLength", 0x00);
      File file = LittleFS.open("/sequencer.opps");
      if(!file || file.isDirectory()){
        debugf("− failed to open file for reading\n");
      }else{
        file.read(this->sequencer, file.available());
        file.close();
      }
      this->sequencerStep = -1;
    }
      
    
    void setup() {
      debugf("Setup begin\n");
      debugf("Load Config (setup)\n");

      // Initialize storage access
      if(!LittleFS.begin(true)){
        debugf("LittleFS Mount Failed\n");
      }

      preferences.begin("led_pattern", false);
      debugf("Preffs free entries: %d\n", preferences.freeEntries());

      // Permanently lock hardware to 55px DotStar and overwrite any stale NVS corruptions
      this->hardwareVersion = 2;
      this->ledType = 2; // 2 = DotStar SPI
      this->ledCount = 55;
      preferences.putUChar("hardwareVersion", 2);
      preferences.putUChar("ledType", 2);
      preferences.putUChar("ledCount", 55);

      this->deviceName = preferences.getString("deviceName", DEFAULT_DEVICE_NAME);

      // Load Display settings (guarantee visible default brightness)
      this->ledBrightness = preferences.getChar("brightness", 25);
      if (this->ledBrightness < 10) this->ledBrightness = 25;
      debugf("- brightness = %d\n", this->ledBrightness);


      this->ledBrightnessOptions[0] = preferences.getChar("brightnessOp0", BRIGHTNESS_OPTIONS[0]);
      this->ledBrightnessOptions[1] = preferences.getChar("brightnessOp1", BRIGHTNESS_OPTIONS[1]);
      this->ledBrightnessOptions[2] = preferences.getChar("brightnessOp2", BRIGHTNESS_OPTIONS[2]);
      this->ledBrightnessOptions[3] = preferences.getChar("brightnessOp3", BRIGHTNESS_OPTIONS[3]);
      this->ledBrightnessOptions[4] = preferences.getChar("brightnessOp4", BRIGHTNESS_OPTIONS[4]);
      this->ledBrightnessOptions[5] = preferences.getChar("brightnessOp5", BRIGHTNESS_OPTIONS[5]);

      this->animationSpeed = preferences.getUShort("animationSpeed", SPEED_OPTIONS[5]);
      debugf("- animation speed = %d frames per sec\n", this->animationSpeed);

      this->animationSpeedOptions[0] = preferences.getUShort("animSpeedOp0", SPEED_OPTIONS[0]);
      this->animationSpeedOptions[1] = preferences.getUShort("animSpeedOp1", SPEED_OPTIONS[1]);
      this->animationSpeedOptions[2] = preferences.getUShort("animSpeedOp2", SPEED_OPTIONS[2]);
      this->animationSpeedOptions[3] = preferences.getUShort("animSpeedOp3", SPEED_OPTIONS[3]);
      this->animationSpeedOptions[4] = preferences.getUShort("animSpeedOp4", SPEED_OPTIONS[4]);
      this->animationSpeedOptions[5] = preferences.getUShort("animSpeedOp5", SPEED_OPTIONS[5]);

      this->patternSlot = preferences.getChar("patternSlot", 0x00);
      debugf("- pattern slot = %d\n", this->patternSlot);

      this->patternBank = preferences.getChar("patternBank", 0x00);
      debugf("- pattern bank = %d\n", this->patternBank);

      this->patternShuffleDuration = preferences.getChar("patShuffleDur", PATTERN_SHUFFLE_DURATION);
      debugf("- pattern shuffle duration = %d\n", this->patternShuffleDuration);

      loadFrameHeight();
      loadFrameCount();
      debugf("- frame\n");
      debugf("  - height = %d\n", this->frameHeight);
      debugf("  - count = %d\n", this->frameCount);

      startLoadingPattern();
      loadSequencer();

      debugf("- pattern\n");
      for (int i = 0; i < this->frameHeight * this->frameCount; i+=3 ) {
        if (i%this->frameHeight*3 == 0) {
          debugf_noprefix("\n");
          debugf("    ");
        }
        debugf_noprefix("0x%02X%02X%02X ", this->pattern[i], this->pattern[i+1], this->pattern[i+2]);
      }
      debugf_noprefix("\n");
      
      debugf("Setup complete\n");
    }

    void loop(){
      // Pattern Shuffle across active bank (or all 5 banks)
      bool shouldShuffle = (this->displayState == DS_PATTERN_ALL || this->displayState == DS_PATTERN_ALL_ALL);
      if(shouldShuffle && millis() - this->displayStateLastUpdated > this->patternShuffleDuration * 1000){
        this->setPatternSlot((this->patternSlot + 1) % PATTERN_BANK_SIZE, false);
        if(this->patternSlot == 0 && this->displayState == DS_PATTERN_ALL_ALL){
          this->setPatternBank((this->patternBank + 1) % PATTERN_BANK_COUNT, false);
        }
        this->displayStateLastUpdated += this->patternShuffleDuration * 1000;
      }




      // Sequencer (pattern slot, battern bank, brightness, speedx2, durationx2)
      if(this->displayState == DS_PATTERN && this->sequencerStep < (this ->sequencerLength / 7) - 1){
        uint16_t offset; 
        uint16_t previousTargetDuration;
        if(sequencerStep == -1){
          this->displayStateLastUpdated = millis();
          previousTargetDuration = 0;
          this->sequencerDelayed = 0;
        }else{
          offset = this->sequencerStep * 7; 
          previousTargetDuration = this->sequencer[offset+ 5] << 8 | this->sequencer[offset + 6];
        }
        ulong lastStepStartTime = (this->displayStateLastUpdated - this->sequencerDelayed);
        if(millis() - lastStepStartTime >= previousTargetDuration){
          this->sequencerStep++;
          offset = this->sequencerStep * 7; 
          this->patternSlot = this->sequencer[offset + 0];
          this->patternBank = this->sequencer[offset + 1];
          this->ledBrightness = this->sequencer[offset + 2];
          this->animationSpeed = this->sequencer[offset+ 3] << 8 | this->sequencer[offset + 4];
          loadFrameHeight();
          loadFrameCount();
          startLoadingPattern();
          // Roll over time from previous update to keep real time regardless of pattern load times
          // But offset displayStateLastUpdated by the load time, so we start rendering from the start of the pattern
          // This is important because the file is not fully loaded yet.
          this->sequencerDelayed = millis() - (lastStepStartTime + previousTargetDuration);
          this->displayStateLastUpdated = lastStepStartTime + previousTargetDuration + this->sequencerDelayed;
        }
      }else{
        // Abort sequencer on button press
        this->sequencerStep = this->sequencerLength / 7;
      }

      // Chunked pattern loading to avoid lag spike on pattern chanage
      continueLoadingPattern();

      // Battery latching state
      if(batteryVoltage <= BATTERY_VOLTAGE_SHUTDOWN || batteryState == BAT_SHUTDOWN){
        batteryState = BAT_SHUTDOWN;
      }else if(batteryVoltage <= BATTERY_VOLTAGE_CRITICAL || (batteryState == BAT_CRITICAL && batteryVoltage <= BATTERY_VOLTAGE_CRITICAL + BATTERY_VOLTAGE_LATCH)){
        batteryState = BAT_CRITICAL;
      }else if(batteryVoltage <= BATTERY_VOLTAGE_LOW || (batteryState == BAT_LOW && batteryVoltage <= BATTERY_VOLTAGE_LOW + BATTERY_VOLTAGE_LATCH)){
        batteryState = BAT_LOW;
      }else {
        batteryState = BAT_OK;
      }
      
    }
};

#endif
