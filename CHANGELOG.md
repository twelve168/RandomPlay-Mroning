# Changelog

本项目的所有显著变更都记录在此文件中。

格式基于 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)，
版本号遵循 [Semantic Versioning](https://semver.org/lang/zh-CN/)。

## [Unreleased]

### Changed（工程规范化重构，无功能变更）

- 仓库结构重排：`firmware/arduino/`、`firmware/esphome/`、`cad/`、
  `docs/`、`images/`，CAD 文件改为不含空格的规范命名
- ESPHome 配置规范化：新增 `substitutions`（行程时长/速度角集中调参）、
  修正拼写错误（`swith` → `switch`）、统一一键开/关的状态文案与实际参数
  （此前显示 8500 ms 实际执行 10000 ms）、修正 `frequency: 50Hz` 为合法
  写法 `50 Hz`，并提供 `secrets.example.yaml` 模板
- Arduino 固件重构：补充文件头与协议文档引用、魔法数抽取为具名常量、
  指令解析增加格式错误日志、注释统一
- README 重写为专业开源项目布局（badges、快速开始、固件选型表、
  技术要点、仓库结构说明）
- 新增 docs 体系：`bom.md`、`assembly.md`、`ble-protocol.md`、
  `design-history.md`
- 新增 CI（GitHub Actions）：yamllint + ESPHome config 校验 +
  arduino-cli 编译冒烟测试
- 新增 `CHANGELOG.md`、`CONTRIBUTING.md`，扩展 `.gitignore`
  （secrets、固件产物）

## [v5.2] - 2026-09-04

### Added

- v5.2 机械结构：为开源重构模型，Type-C 直供电，全面适配小直径主动轮
- ESPHome 固件（`curtain-machine.yaml`）：HA 集成、三态控制、一键开关、
  即时控制组件、`rotate_temporary` API action
- Arduino BLE 固件：R/D/M/S/T 文本指令协议、单次定时任务、RTC 校准
- 初版 README 与项目图片
- MIT License

### 硬件迭代摘要

v1 鱼线收拉（鱼线打结）→ v2 挂轨橡皮筋（拓扑缺陷）→ v3 双组轮 →
v4 直挂 + 配重、去副轮 → v5 集成配重钩 → v5.1 小轮径 + 接线端子 →
v5.2 Type-C 直供电（本仓库首发版本）。详见
[docs/design-history.md](docs/design-history.md)。
