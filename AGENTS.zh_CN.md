**简体中文** · [English](AGENTS.md)

# 智能体贡献说明

修改代码前读 `docs/architecture.zh_CN.md`，修改构建、CI、分区前读 `docs/build.zh_CN.md`，修改硬件前读对应 BSP 头文件。先看 Git 状态，保留无关改动。

- 目标 ESP32-C3、8 MB、无 PSRAM、ESP-IDF 5.5.3。保留 3 MB 应用上限及 `cardid` 的 `0x356000` / `0x4000` 布局。
- 应用逻辑放 `main/`，通用硬件支持放 `components/bsp/`。
- 按键回调不阻塞，非 LVGL 调用方必须持有 BSP LVGL 锁。
- 协议与文本逻辑应能在主机测试，保持采集 / TLS 分配顺序及已测内存预算。
- `companion/ee04/` 是独立 GPL-3.0-only Arduino 程序，不混入 MIT Passport 二进制。
- Markdown 默认英文，并配对互链 `.zh_CN.md`，同步维护；用户可见变化记入 `docs/CHANGELOG.md`。
- 不提交真实密钥、设置密码、识别正文、私有地址、日志或 Flash 备份；文档使用合成示例数据。
- 迭代中跑相关检查，交付前跑 `./tools/validate.sh`，分别报告 **Build / Host tests / Device tests / Unverified**。预览或编译不等于实机测试。
- 用户要求时提交推送，采用 Conventional Commits；标签和 Release 属于单独操作，CI 不自动创建。
