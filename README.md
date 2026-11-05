<div align="center">

# RandomPlay-Mroning

**￥17成本的DIY自动窗帘机** · ESP32-C3 + 360° SG90 · 3D打印全结构

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![CI](https://img.shields.io/badge/CI-GitHub_Actions-blue?logo=github-actions)](.github/workflows/ci.yml)
[![Platform](https://img.shields.io/badge/Platform-ESP32--C3-000?logo=espressif)](https://www.espressif.com/en/products/socs/esp32-c3)
[![Firmware](https://img.shields.io/badge/Firmware-ESPHome%20%7C%20Arduino-informational)](firmware/)

</div>

---

## 项目简介

RandomPlay-Mroning是一台为**调节作息**而生的自动窗帘机：整机直接挂在窗帘
轨道上，以摩擦驱动方式推拉窗帘，**BOM成本仅￥17**（同期市售成品约￥70）。
硬件为ESP32-C3 Super Mini + 360°连续旋转SG90，结构件全部3D打印，
提供 **ESPHome / Arduino BLE双固件方案**，可接入Home Assistant或纯本地
BLE直连控制。

| | |
|---|---|
| 整机成本 | **￥17**（研发损耗全程共￥40，含报废一块开发板 + 两个舵机） |
| 结构版本 | v5.2（历经6个硬件版本的机械迭代，见[研发迭代史](docs/design-history.md)） |
| 适配轨道 | ≤ 23 mm宽方形轨道（圆轨需自行改模） |
| 固件方案 | ESPHome（HA集成）/ Arduino（BLE直连，二选一） |

![成品图1](images/b82c1e4c60fa027ac0ada0ae37fbac46_720.jpg)
![成品图2](images/b4b694caf5948c8ceacdd4bd7a9d51a1_720.jpg)

### 3D模型

![Fusion 360模型](images/fusion2609041821.png)

模型源文件（Fusion 360）与通用STEP格式见 [cad/](cad/) 目录。

## 快速开始

1. 按[物料清单](docs/bom.md)备料 —— ⚠️ 舵机务必选 **360°连续旋转版**，180°版无法工作
2. 打印 [cad/](cad/) 中的结构件并按[装配指南](docs/assembly.md)组装
3. 按下表选择固件方案并烧录：

| 需求 | 推荐固件 | 入口 |
|------|----------|------|
| 已有Home Assistant，要定时/自动化 | **ESPHome版** | [`firmware/esphome/`](firmware/esphome/) |
| 无网关，手机BLE直连控制 | **Arduino BLE版** | [`firmware/arduino/`](firmware/arduino/) |

### ESPHome版

1. 安装Home Assistant及其ESPHome插件
2. 复制 `firmware/esphome/secrets.example.yaml` 为 `secrets.yaml` 并填入真实凭据
3. 编译烧录 `firmware/esphome/curtain-machine.yaml`
4. 按自家窗帘行程调整substitutions中的 `curtain_travel_ms`（默认10000 ms）
5. 定时任务推荐在HA上安装 [scheduler-card](https://github.com/nielsfaber/scheduler-card)
   与 [scheduler-component](https://github.com/nielsfaber/scheduler-component) 配套使用

### Arduino BLE版

固件提供纯本地BLE控制协议（无需Wi-Fi），支持即时旋转、持续保持、
单次定时任务与RTC校准。完整指令集见 [docs/ble-protocol.md](docs/ble-protocol.md)：

```text
R180 5000      # 逆时针全速旋转5000 ms，到期自动归中
D180           # 持续逆时针全速旋转
M1145 180 5000 # 下一个11:45执行一次性动作
S1145          # 删除11:45的定时任务
T2603261145    # RTC校准为2026-03-26 11:45
```

调试阶段可用nRF Connect手写指令验证。

## 360°舵机角度语义（两套固件通用）

| 角度 | 行为 |
|------|------|
| `< 90°` | 顺时针旋转，越趋近0°转速越快 |
| `= 90°` | 完全静止 |
| `> 90°` | 逆时针旋转，越趋近180°转速越快 |

## 仓库结构

```text
.
├── cad/                  # 3D模型 (Fusion 360 .f3d + 通用 .step)
├── docs/                 # BOM / 装配 / BLE协议 / 研发迭代史
├── firmware/
│   ├── arduino/          # Arduino BLE直连固件
│   └── esphome/          # ESPHome (Home Assistant) 固件 + secrets模板
├── images/               # 实物与模型截图
├── .github/workflows/    # CI: YAML lint + 固件配置/编译校验
├── CHANGELOG.md
├── CONTRIBUTING.md
└── LICENSE               # MIT
```

## 装配提示（完整步骤见 [docs/assembly.md](docs/assembly.md)）

- 主动轮用M2 × 20 mm螺丝固定，轮面粘一圈胶带增加摩擦
- 挂载配重（如矿泉水）压紧主动轮 —— 摩擦驱动的核心
- 建议深色耗材打印或用遮光胶带遮挡ESP32电源LED
- 实物图版本与本仓库上传版本不一致，以本仓库文档为准

## 技术要点

- **摩擦驱动取力**：无皮带/鱼线等传动件，靠配重将主动轮压紧轨道直接
  推拉，成本与故障点最小化（v1鱼线方案的教训，见[研发迭代史](docs/design-history.md)）
- **小轮径主动轮**：扭矩恒定下切向力与轮径成反比，避免推帘吃力与
  堵转烧电机（v5.1关键改进）
- **非阻塞BLE状态机**：写回调仅更新状态变量，归中判定在主循环
  `millis()` 中完成，不阻塞BLE栈
- **双固件架构**：同一硬件，ESPHome走HA生态（NTP对时、scheduler
  定时），Arduino版走无网关BLE直连（RTC + 单次定时任务）
- **CI固件校验**：每次提交自动对ESPHome YAML做config校验、
  Arduino固件做arduino-cli编译冒烟测试

## Roadmap

RandomPlay是一个DIY低成本智能家居系列，本仓库为系列第一个项目。
后续计划开源：IR & RF收发器、插销门锁控制器、RF遥控器桥接、本地
语音控制agent（见[研发迭代史](docs/design-history.md)末表）。

## 贡献

欢迎issue / PR，参与方式见 [CONTRIBUTING.md](CONTRIBUTING.md)。

## 许可证

[MIT License](LICENSE) © 2026 twelve168
