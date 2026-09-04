#include <Arduino.h>
#include <ESP32Servo.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <time.h>

// 引脚定义
#define SERVO_PIN 7

// BLE UUID 定义
#define SERVICE_UUID        "4FAFC201-1FB5-459E-8FCC-C5C9C331914B"
#define CHARACTERISTIC_UUID "BEB5483E-36E1-4688-B7F5-EA07361B26A8"

Servo myServo;
BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;

// 定时任务结构体
struct TimerTask {
    bool active = false;
    int targetHour;
    int targetMinute;
    int angle;
    int durationMs;
};

#define MAX_TASKS 10
TimerTask tasks[MAX_TASKS];

// --- 舵机单阶段状态机变量 ---
int servoStage = 0;           // 0: 空闲/90度, 1: 动作中(目标角)
int currentTargetAngle = 90;   // 当前目标角度
int currentDurationMs = 0;     // 保持的时间
unsigned long stageExpireTime = 0; // 到期自动归中的时间戳

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      Serial.println("手机已连接");
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("手机已断开，重新开启广播...");
      pServer->getAdvertising()->start();
    }
};

// 触发舵机运动的入口 (转到目标角 -> 保持 -> 归中)
void startServoMotion(int angle, int durationMs) {
    currentTargetAngle = constrain(angle, 0, 180); // 限定角度边界
    currentDurationMs = durationMs;
    
    // 进入阶段 1：旋转到目标角度 x
    servoStage = 1; 
    myServo.write(currentTargetAngle);
    stageExpireTime = millis() + currentDurationMs;
    
    Serial.printf("[动作开始] 舵机转动至目标角: %d 度, 保持: %d 毫秒\n", currentTargetAngle, currentDurationMs);
}

// 核心解析逻辑
class MyCharacteristicCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      String value = pCharacteristic->getValue().c_str();
      value.trim(); 
      
      if (value.length() < 2) return;

      char cmdType = value.charAt(0); 
      String dataStr = value.substring(1); 

      Serial.printf("收到指令: %c -> %s\n", cmdType, dataStr.c_str());

      // 1. 时间同步 (T2607191931)
      if (cmdType == 'T' || cmdType == 't') {
        if (dataStr.length() == 10) {
          int year  = dataStr.substring(0, 2).toInt() + 100; 
          int month = dataStr.substring(2, 4).toInt() - 1;   
          int day   = dataStr.substring(4, 6).toInt();       
          int hour  = dataStr.substring(6, 8).toInt();       
          int min   = dataStr.substring(8, 10).toInt();       

          struct tm t;
          t.tm_year = year;
          t.tm_mon = month;
          t.tm_mday = day;
          t.tm_hour = hour;
          t.tm_min = min;
          t.tm_sec = 0;
          t.tm_isdst = -1;

          time_t epochTime = mktime(&t);
          if (epochTime != -1) {
            struct timeval tv;
            tv.tv_sec = epochTime;
            tv.tv_usec = 0;
            settimeofday(&tv, NULL);
            Serial.printf("时间校准成功：%04d-%02d-%02d %02d:%02d\n", 
                          t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min);
          }
        }
      }
      
      // 2. 即时控制 (R120 2500) -> 转到 120° 保持 2.5 秒后自动归中
      else if (cmdType == 'R' || cmdType == 'r') {
        int firstSpace = dataStr.indexOf(' ');
        if (firstSpace != -1) {
          int angle = dataStr.substring(0, firstSpace).toInt();
          int durationMs = dataStr.substring(firstSpace + 1).toInt();
          startServoMotion(angle, durationMs);
        }
      }
      
      // 3. 定时任务 (M0800 120 2500) -> 定时触发归中运动
      else if (cmdType == 'M' || cmdType == 'm') {
        int firstSpace = dataStr.indexOf(' ');
        int secondSpace = dataStr.indexOf(' ', firstSpace + 1);
        
        if (firstSpace == 4 && secondSpace != -1) { 
          String timeStr = dataStr.substring(0, 4);
          int hour = timeStr.substring(0, 2).toInt();
          int min = timeStr.substring(2, 4).toInt();
          int angle = dataStr.substring(firstSpace + 1, secondSpace).toInt();
          int durationMs = dataStr.substring(secondSpace + 1).toInt();

          for (int i = 0; i < MAX_TASKS; i++) {
            if (!tasks[i].active) {
              tasks[i].targetHour = hour;
              tasks[i].targetMinute = min;
              tasks[i].angle = angle;
              tasks[i].durationMs = durationMs;
              tasks[i].active = true;
              Serial.printf("定时任务存入槽位 [%d] -> %02d:%02d 触发\n", i, hour, min);
              break;
            }
          }
        }
      }

      // 4. 删除定时任务 (S0800) -> 删除指定时间的定时任务
      else if (cmdType == 'S' || cmdType == 's') {
        if (dataStr.length() == 4) {
          int hour = dataStr.substring(0, 2).toInt();
          int min  = dataStr.substring(2, 4).toInt();
          bool found = false;

          for (int i = 0; i < MAX_TASKS; i++) {
            if (tasks[i].active && tasks[i].targetHour == hour && tasks[i].targetMinute == min) {
              tasks[i].active = false; // 取消激活，相当于删除
              found = true;
              Serial.printf("❌ 已删除槽位 [%d] 处 %02d:%02d 的定时任务\n", i, hour, min);
            }
          }

          if (!found) {
            Serial.printf("⚠️ 未找到 %02d:%02d 的定时任务\n", hour, min);
          }
        } else {
          Serial.println("⚠️ 删除指令格式错误！应为 S+4位时间，例如: S0800");
        }
      }

      // 5. 直接控制，不归中 (D120) -> 保持在 120°
      else if (cmdType == 'D' || cmdType == 'd') {
        int angle = dataStr.toInt();
        angle = constrain(angle, 0, 180); 
        
        servoStage = 0;       // 关闭定时归中状态机
        myServo.write(angle); // 转到目标角度并保持
        
        Serial.printf("[直接控制] 舵机转动至: %d 度，保持不动\n", angle);
      }
    }
};

void setup() {
  Serial.begin(115200);

  // 初始化舵机
  ESP32PWM::allocateTimer(0);
  myServo.setPeriodHertz(50); 
  myServo.attach(SERVO_PIN, 500, 2500);
  myServo.write(90); 

  // 初始化 BLE 蓝牙
  BLEDevice::init("RandomPlay-Mroning");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_WRITE |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );

  pCharacteristic->setCallbacks(new MyCharacteristicCallbacks());
  pCharacteristic->addDescriptor(new BLE2902());

  pService->start();
  pServer->getAdvertising()->start();
  Serial.println("BLE 蓝牙已就绪...");
}

void loop() {
  // --- 1. 状态机：负责 R/M 指令的自动归中判定 ---
  if (servoStage == 1 && millis() >= stageExpireTime) {
    servoStage = 0;
    myServo.write(90);
    Serial.println("[动作结束] 保持时间已到，舵机自动回归 90 度");
  }

  // --- 2. 检查单次定时任务 ---
  static int lastCheckedMinute = -1;
  struct tm timeinfo;
  
  if (getLocalTime(&timeinfo)) {
    if (timeinfo.tm_min != lastCheckedMinute) {
      lastCheckedMinute = timeinfo.tm_min; 
      
      for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].active) {
          if (timeinfo.tm_hour == tasks[i].targetHour &&
              timeinfo.tm_min == tasks[i].targetMinute) {
            
            Serial.printf("⏰ 定时任务触发！槽位 [%d]\n", i);
            startServoMotion(tasks[i].angle, tasks[i].durationMs);
            
            tasks[i].active = false; // 执行后销毁单次任务
          }
        }
      }
    }
  }
  
  delay(20);
}