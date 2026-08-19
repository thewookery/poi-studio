#ifndef _OPEN_PIXEL_POI_IMU_CPP
#define _OPEN_PIXEL_POI_IMU_CPP

#include <Arduino.h>
#include <Wire.h>
#include <math.h>

#ifndef SDA_PIN
#define SDA_PIN 4
#endif
#ifndef SCL_PIN
#define SCL_PIN 5
#endif

enum IMUType {
  IMU_NONE = 0,
  IMU_MPU6050 = 1,
  IMU_LIS3DH = 2,
  IMU_LSM6DS3 = 3
};

class OpenPixelPoiIMU {
private:
  uint8_t i2cAddr = 0;
  IMUType sensorType = IMU_NONE;

  float rawGx = 0.0f;
  float rawGy = 0.0f;
  float rawGz = 1.0f;
  float totalG = 1.0f;
  float spinG = 0.0f;
  float filteredRpm = 0.0f;

  unsigned long lastSampleTime = 0;
  unsigned long lastMotionTime = 0;

  bool writeRegister(uint8_t addr, uint8_t reg, uint8_t val) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.write(val);
    return (Wire.endTransmission() == 0);
  }

  bool readRegisters(uint8_t addr, uint8_t reg, uint8_t* buffer, uint8_t length) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    uint8_t count = Wire.requestFrom((int)addr, (int)length);
    if (count != length) return false;
    for (uint8_t i = 0; i < length; i++) {
      buffer[i] = Wire.read();
    }
    return true;
  }

public:
  bool imuDetected = false;
  bool isIdle = false;
  float idleMorphBlend = 0.0f; // 0.0 = POV Mode, 1.0 = Ambient Mood Lamp Mode
  float spinSpeedFactor = 1.0f; // 1.0 = Base Speed, >1.0 = Faster spin speed

  OpenPixelPoiIMU() {}

  void setup() {
    Wire.begin(SDA_PIN, SCL_PIN, 400000);
    delay(20);

    // 1. Probe for MPU-6050 / MPU-6500 / MPU-9250 / ICM-20600 (0x68 or 0x69)
    uint8_t testByte = 0;
    if (readRegisters(0x68, 0x75, &testByte, 1) || readRegisters(0x69, 0x75, &testByte, 1)) {
      i2cAddr = (testByte != 0) ? 0x68 : 0x69;
      sensorType = IMU_MPU6050;
      // Wake up from sleep (PWR_MGMT_1 = 0x00)
      writeRegister(i2cAddr, 0x6B, 0x00);
      delay(10);
      // Set accelerometer full scale to +/-16G (ACCEL_CONFIG = 0x18)
      writeRegister(i2cAddr, 0x1C, 0x18);
      imuDetected = true;
      lastMotionTime = millis();
      return;
    }

    // 2. Probe for LIS3DH / LIS2DH (0x18 or 0x19)
    if (readRegisters(0x18, 0x0F, &testByte, 1) || readRegisters(0x19, 0x0F, &testByte, 1)) {
      i2cAddr = (testByte == 0x33) ? 0x18 : 0x19;
      sensorType = IMU_LIS3DH;
      // CTRL_REG1 (0x20) = 0x77 (400Hz, Normal / All axes enabled)
      writeRegister(i2cAddr, 0x20, 0x77);
      // CTRL_REG4 (0x23) = 0x38 (+/-16G, High-Resolution)
      writeRegister(i2cAddr, 0x23, 0x38);
      imuDetected = true;
      lastMotionTime = millis();
      return;
    }

    // 3. Probe for LSM6DS3 / LSM6DSO (0x6A or 0x6B)
    if (readRegisters(0x6A, 0x0F, &testByte, 1) || readRegisters(0x6B, 0x0F, &testByte, 1)) {
      i2cAddr = (testByte == 0x69 || testByte == 0x6A || testByte == 0x6C) ? 0x6A : 0x6B;
      sensorType = IMU_LSM6DS3;
      // CTRL1_XL (0x10) = 0x5C (208Hz, +/-16G)
      writeRegister(i2cAddr, 0x10, 0x5C);
      imuDetected = true;
      lastMotionTime = millis();
      return;
    }

    // No sensor present on I2C bus
    imuDetected = false;
    sensorType = IMU_NONE;
  }

  void loop() {
    if (!imuDetected) {
      idleMorphBlend = 0.0f;
      spinSpeedFactor = 1.0f;
      isIdle = false;
      return;
    }

    unsigned long now = millis();
    if (now - lastSampleTime < 10) return; // Sample at ~100Hz
    float dt = (now - lastSampleTime) / 1000.0f;
    lastSampleTime = now;

    int16_t ax = 0, ay = 0, az = 0;
    uint8_t buf[6];

    if (sensorType == IMU_MPU6050) {
      if (readRegisters(i2cAddr, 0x3B, buf, 6)) {
        ax = (int16_t)((buf[0] << 8) | buf[1]);
        ay = (int16_t)((buf[2] << 8) | buf[3]);
        az = (int16_t)((buf[4] << 8) | buf[5]);
        rawGx = (float)ax / 2048.0f;
        rawGy = (float)ay / 2048.0f;
        rawGz = (float)az / 2048.0f;
      }
    } else if (sensorType == IMU_LIS3DH) {
      if (readRegisters(i2cAddr, 0x28 | 0x80, buf, 6)) {
        ax = (int16_t)((buf[1] << 8) | buf[0]);
        ay = (int16_t)((buf[3] << 8) | buf[2]);
        az = (int16_t)((buf[5] << 8) | buf[4]);
        rawGx = (float)ax / 1280.0f;
        rawGy = (float)ay / 1280.0f;
        rawGz = (float)az / 1280.0f;
      }
    } else if (sensorType == IMU_LSM6DS3) {
      if (readRegisters(i2cAddr, 0x28, buf, 6)) {
        ax = (int16_t)((buf[1] << 8) | buf[0]);
        ay = (int16_t)((buf[3] << 8) | buf[2]);
        az = (int16_t)((buf[5] << 8) | buf[4]);
        rawGx = (float)ax / 2048.0f;
        rawGy = (float)ay / 2048.0f;
        rawGz = (float)az / 2048.0f;
      }
    }

    // 1. Calculate total G-force magnitude
    totalG = sqrtf(rawGx * rawGx + rawGy * rawGy + rawGz * rawGz);

    // 2. Centrifugal Spin Acceleration (Extract radial force beyond 1G earth gravity)
    spinG = max(0.0f, totalG - 1.0f);

    // 3. Motion vs. Idle State Machine
    if (spinG > 0.35f || fabsf(totalG - 1.0f) > 0.30f) {
      lastMotionTime = now;
      isIdle = false;
    } else if (now - lastMotionTime > 1200) {
      // Resting still for > 1.2 seconds -> Enter Smart Idle Mood Lamp Mode!
      isIdle = true;
    }

    // 4. Smooth Idle Crossfade (Smooth ramp between POV mode and Ambient Mood Lamp)
    if (isIdle) {
      idleMorphBlend += dt * 2.5f; // Crossfade to Idle in ~400ms
      if (idleMorphBlend > 1.0f) idleMorphBlend = 1.0f;
    } else {
      idleMorphBlend -= dt * 10.0f; // Instant snap back to POV in ~100ms when flicked/spun
      if (idleMorphBlend < 0.0f) idleMorphBlend = 0.0f;
    }

    // 5. Centrifugal RPM Adaptive Speed Calculation
    // For tether length ~0.5m: RPM = 42.4 * sqrt(spinG). Normal spin is 1.5 - 3.5 rev/sec (90-210 RPM)
    float instantRpm = 42.4f * sqrtf(spinG);
    filteredRpm = (filteredRpm * 0.85f) + (instantRpm * 0.15f);

    if (spinG > 0.3f && filteredRpm > 40.0f) {
      spinSpeedFactor = max(0.5f, min(3.5f, filteredRpm / 120.0f));
    } else {
      spinSpeedFactor = 1.0f;
    }
  }

  float getSpinSpeedMultiplier() {
    return (imuDetected && !isIdle) ? spinSpeedFactor : 1.0f;
  }

  float getIdleBlend() {
    return (imuDetected) ? idleMorphBlend : 0.0f;
  }
};

#endif // _OPEN_PIXEL_POI_IMU_CPP
