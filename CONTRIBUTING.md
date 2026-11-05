# 贡献指南

欢迎以任何形式参与本项目：复刻反馈、文档勘误、结构改进、固件增强。

## 如何贡献

### 报告问题（Issue）

提交 issue 前请先检索已有 issue。新 issue 请包含：

- 硬件版本（结构件版本 + 舵机型号，尤其注明 360° / 180°）
- 固件方案（ESPHome / Arduino）及相关日志
- 复现步骤与照片/视频

### 提交代码（Pull Request）

1. Fork 本仓库并从 `main` 拉出特性分支
   `git checkout -b feat/your-feature`
2. 保持改动聚焦：一个 PR 只做一件事
3. 固件改动请确保：
   - ESPHome YAML 通过 `esphome config` 校验
   - Arduino 代码可被 arduino-cli 编译（CI 会执行同样检查）
   - `docs/` 中的相关文档同步更新
4. 提交信息遵循 Conventional Commits
   （`feat:` / `fix:` / `docs:` / `chore:` / `refactor:`）
5. 变更写入 [CHANGELOG.md](CHANGELOG.md) 的 Unreleased 区段

## 开发环境

### ESPHome

```bash
pip install esphome
cp firmware/esphome/secrets.example.yaml firmware/esphome/secrets.yaml
# 填入真实凭据后:
esphome config firmware/esphome/curtain-machine.yaml
```

### Arduino

- 板卡：`esp32:esp32:esp32c3`（Arduino-ESP32 core ≥ 2.0.x）
- 库依赖：`ESP32Servo`（BLE 栈为 core 内置）

## 安全红线

- **绝不提交** `secrets.yaml` 或任何真实 Wi-Fi/API 密钥
- 硬件接线改动请在 PR 描述中给出完整接线表与安全说明（舵机堵转、
  电源电流预算等）

## 行为准则

保持友善与对事不对人。DIY 社区的价值在于分享失败经验——
[研发迭代史](docs/design-history.md) 里六个报废版本就是最好的示范。
