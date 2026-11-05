/**
 * @file    arduino-randmron.ino
 * @brief   RandomPlay-Mroning 自动窗帘机 — BLE 直连固件（ESP32-C3 + 360° SG90）
 *
 * 项目:     RandomPlay-Mroning
 * 许可证:   MIT License
 * 目标硬件: ESP32-C3 Super Mini (GPIO7 → 舵机 PWM)
 *
 * 功能:
 *   - BLE GATT 服务对外暴露一个可读/可写/可通知的特征值
 *   - 文本指令协议: 即时旋转(R)、持续保持(D)、单次定时任务(M)、
 *     删除定时任务(S)、RTC 时间校准(T)
 *   - R/M 指令到达后由主循环状态机在保持时长结束时自动归中（90°）
 *
 * 依赖:
 *   - ESP32Servo          (https://github.com/madhephaestus/ESP32Servo)
 *   - ESP32 BLE (Arduino core 内置)
 *
 * 上游文档: docs/ble-protocol.md
 */

#include <Arduino.h>
#include <ESP32Servo.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <time.h>

/* ============================== 用户配置 ============================== */

static const int   SERVO_PIN           = 7;      // 舵机 PWM 信号引脚
static const int   SERVO_NEUTRAL_DEG   = 90;     // 360° 舵机的"停止"角度
static const int   SERVO_MIN_PULSE_US  = 500;    // 对应 0°
static const int   SERVO_MAX_PULSE_US  = 2500;   // 对应 180°
static const int   SERVO_MIN_DEG       = 0;
static const int   SERVO_MAX_DEG       = 180;

static const char* BLE_DEVICE_NAME     = "RandomPlay-Mroning";
static const char* SERVICE_UUID        = "4FAFC201-1FB5-459E-8FCC-C5C9C331914B";
static const char* CHARACTERISTIC_UUID = "BEB5483E-36E1-4688-B7F5-EA07361B26A8";

static const int   MAX_TASKS           = 10;     // 定时任务槽位上限

/* ============================== 内部状态 ============================== */

Servo myServo;
BLEServer*         pServer         = nullptr;
BLECharacteristic* pCharacteristic = nullptr;
bool deviceConnected = false;

/// 单次定时任务（执行后自动销毁，见 docs/ble-protocol.md）
struct TimerTask {
  bool active = false;
  int  targetHour;
  int  targetMinute;
  int  angle;
  int  durationMs;
};
TimerTask tasks[MAX_TASKS];

/// 舵机动作状态机: 0 = 空闲(90°), 1 = 动作中(目标角)
int  servoStage         = 0;
int  currentTargetAngle = 90;
int  currentDurationMs  = 0;
unsigned long stageExpireTime = 0;               // 到期自动归中的时间戳

/* ============================== BLE 回调 ============================== */

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) override {
    deviceConnected = true;
    Serial.println("[BLE] 客户端已连接");
  }

  void onDisconnect(BLEServer* pServer) override {
    deviceConnected = false;
    Serial.println("[BLE] 客户端已断开，重新开启广播...");
    pServer->getAdvertising()->start();
  }
};

/* ============================== 舵机控制 ============================== */

/**
 * 触发一次"旋转到目标角 → 保持 durationMs → 归中"的动作。
 * 归中判定由 loop() 中的状态机完成，避免在 BLE 回调里 delay()。
 */
void startServoMotion(int angle, int durationMs) {
  currentTargetAngle = constrain(angle, SERVO_MIN_DEG, SERVO_MAX_DEG);
  currentDurationMs  = durationMs;

  servoStage      = 1;
  stageExpireTime = millis() + (unsigned long)currentDurationMs;
  myServo.write(currentTargetAngle);

  Serial.printf("[动作开始] 舵机转动至目标角: %d 度, 保持: %d 毫秒\n",
                currentTargetAngle, currentDurationMs);
}

/* ============================ 指令解析层 ============================= */

/**
 * 指令协议入口，完整语法见 docs/ble-protocol.md：
 *   R<angle> <durationMs>            即时旋转后自动归中
 *   D<angle>                         持续保持目标角（不归中）
 *   M<HHMM> <angle> <durationMs>     注册单次定时任务
 *   S<HHMM>                          删除指定时间的定时任务
 *   T<YYMMDDHHMM>                    校准 RTC
 */
class MyCharacteristicCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) override {
    String value = pCharacteristic->getValue().c_str();
    value.trim();
    if (value.length() < 2) return;

    char   cmdType = value.charAt(0);
    String dataStr = value.substring(1);

    Serial.printf("[RX] 指令: %c -> %s\n", cmdType, dataStr.c_str());

    /* ---- 1. 时间校准: T2603261145 => 2026-03-26 11:45 ---- */
    if (cmdType == 'T' || cmdType == 't') {
      if (dataStr.length() == 10) {
        struct tm t = {};
        t.tm_year  = dataStr.substring(0, 2).toInt() + 100;   // 两位年份 + 1900
        t.tm_mon   = dataStr.substring(2, 4).toInt() - 1;     // 月份 0-11
        t.tm_mday  = dataStr.substring(4, 6).toInt();
        t.tm_hour  = dataStr.substring(6, 8).toInt();
        t.tm_min   = dataStr.substring(8, 10).toInt();
        t.tm_sec   = 0;
        t.tm_isdst = -1;

        time_t epochTime = mktime(&t);
        if (epochTime != -1) {
          struct timeval tv = { .tv_sec = epochTime, .tv_usec = 0 };
          settimeofday(&tv, NULL);
          Serial.printf("[RTC] 时间校准成功：%04d-%02d-%02d %02d:%02d\n",
                        t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min);
        }
      } else {
        Serial.println("[RTC] 指令格式错误，应为 T + 10 位时间，例如: T2603261145");
      }
    }

    /* ---- 2. 即时旋转: R180 5000 ---- */
    else if (cmdType == 'R' || cmdType == 'r') {
      int firstSpace = dataStr.indexOf(' ');
      if (firstSpace != -1) {
        int angle      = dataStr.substring(0, firstSpace).toInt();
        int durationMs = dataStr.substring(firstSpace + 1).toInt();
        startServoMotion(angle, durationMs);
      } else {
        Serial.println("[RX] 指令格式错误，应为 R + 角度 + 空格 + 毫秒，例如: R180 5000");
      }
    }

    /* ---- 3. 注册定时任务: M1145 180 5000 ---- */
    else if (cmdType == 'M' || cmdType == 'm') {
      int firstSpace  = dataStr.indexOf(' ');
      int secondSpace = dataStr.indexOf(' ', firstSpace + 1);

      if (firstSpace == 4 && secondSpace != -1) {
        int hour       = dataStr.substring(0, 2).toInt();
        int min        = dataStr.substring(2, 4).toInt();
        int angle      = dataStr.substring(firstSpace + 1, secondSpace).toInt();
        int durationMs = dataStr.substring(secondSpace + 1).toInt();

        for (int i = 0; i < MAX_TASKS; i++) {
          if (!tasks[i].active) {
            tasks[i].targetHour   = hour;
            tasks[i].targetMinute = min;
            tasks[i].angle        = angle;
            tasks[i].durationMs   = durationMs;
            tasks[i].active       = true;
            Serial.printf("[定时] 任务存入槽位 [%d] -> %02d:%02d 触发\n", i, hour, min);
            break;
          }
        }
      } else {
        Serial.println("[RX] 指令格式错误，应为 M + 时间 + 角度 + 毫秒，例如: M1145 180 5000");
      }
    }

    /* ---- 4. 删除定时任务: S1145 ---- */
    else if (cmdType == 'S' || cmdType == 's') {
      if (dataStr.length() == 4) {
        int hour = dataStr.substring(0, 2).toInt();
        int min  = dataStr.substring(2, 4).toInt();
        bool found = false;

        for (int i = 0; i < MAX_TASKS; i++) {
          if (tasks[i].active && tasks[i].targetHour == hour && tasks[i].targetMinute == min) {
            tasks[i].active = false;
            found = true;
            Serial.printf("[定时] 已删除槽位 [%d] 处 %02d:%02d 的任务\n", i, hour, min);
          }
        }
        if (!found) {
          Serial.printf("[定时] 未找到 %02d:%02d 的任务\n", hour, min);
        }
      } else {
        Serial.println("[RX] 指令格式错误，应为 S + 4 位时间，例如: S1145");
      }
    }

    /* ---- 5. 持续保持: D180（关闭自动归中） ---- */
    else if (cmdType == 'D' || cmdType == 'd') {
      int angle = constrain(dataStr.toInt(), SERVO_MIN_DEG, SERVO_MAX_DEG);

      servoStage     = 0;      // 关闭定时归中状态机
      myServo.write(angle);

      Serial.printf("[直接控制] 舵机转动至: %d 度，保持不动\n", angle);
    }
  }
};

/* ============================== 初始化 ============================== */

void setup() {
  Serial.begin(115200);

  // 舵机: ESP32Servo 需要显式分配 LEDC 定时器
  ESP32PWM::allocateTimer(0);
  myServo.setPeriodHertz(50);                       // SG90 要求 50 Hz PWM
  myServo.attach(SERVO_PIN, SERVO_MIN_PULSE_US, SERVO_MAX_PULSE_US);
  myServo.write(SERVO_NEUTRAL_DEG);

  // BLE GATT 服务
  BLEDevice::init(BLE_DEVICE_NAME);
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService* pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_READ |
      BLECharacteristic::PROPERTY_WRITE |
      BLECharacteristic::PROPERTY_NOTIFY);

  pCharacteristic->setCallbacks(new MyCharacteristicCallbacks());
  pCharacteristic->addDescriptor(new BLE2902());

  pService->start();
  pServer->getAdvertising()->start();
  Serial.println("[BLE] 已就绪，开始广播...");
}

/* ============================== 主循环 ============================== */

void loop() {
  /* 1. 状态机: R/M 指令到期后自动归中 */
  if (servoStage == 1 && millis() >= stageExpireTime) {
    servoStage = 0;
    myServo.write(SERVO_NEUTRAL_DEG);
    Serial.println("[动作结束] 保持时间已到，舵机自动回归 90 度");
  }

  /* 2. 每分钟扫描一次单次定时任务 */
  static int lastCheckedMinute = -1;
  struct tm timeinfo;

  if (getLocalTime(&timeinfo)) {
    if (timeinfo.tm_min != lastCheckedMinute) {
      lastCheckedMinute = timeinfo.tm_min;

      for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].active &&
            timeinfo.tm_hour == tasks[i].targetHour &&
            timeinfo.tm_min  == tasks[i].targetMinute) {
          Serial.printf("[定时] 任务触发！槽位 [%d]\n", i);
          startServoMotion(tasks[i].angle, tasks[i].durationMs);
          tasks[i].active = false;   // 单次任务，触发后销毁
        }
      }
    }
  }

  delay(20);
}
