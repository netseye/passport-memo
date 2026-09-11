**简体中文** · [English](README.md)

# EE04 墨水屏接收端

面向 **EE04 / XIAO ESP32-S3** 的可选独立固件，采用 **GPL-3.0-only**。Passport 可以单独使用；EE04 只接收确认后的文字，不需要火山凭据。目前已完成主机与编译验证，双设备实机同步尚未测试。

## 构建

安装 Arduino CLI，在本目录运行固定版本依赖安装器：

```sh
./scripts/setup.command
./tests/run.sh
./scripts/build.command
./scripts/flash.command
```

脚本固定 ESP32 Arduino 3.3.11、GxEPD2 1.6.9、Adafruit GFX 1.12.6、Adafruit BusIO 1.17.4、ArduinoJson 7.4.3 和 U8g2_for_Adafruit_GFX 1.8.0。刷机脚本提供板卡 / 串口选择流程，写入前核实是 XIAO ESP32-S3，不要使用 Passport 的 C3 镜像。

## 配对

1. 长按 EE04 **K2 约 1.2 秒**开启管理，用屏幕密码连接 **EE04-Setup**，打开 **http://192.168.4.1** 设置路由器 Wi-Fi。通过屏幕显示的局域网地址访问时，用户名为 `ee04`，密码仍是屏幕上的密码。
2. 启用“Passport 联动”，复制新生成的 **32 位配对码**。只在生成时显示；重新生成会撤销旧码。
3. 在 Passport 设置中填写 EE04 局域网 IP 及配对码。两台设备连接同一可信局域网，必要时在路由器中固定 EE04 IP。
4. Passport 保存记录时尝试同步；历史页选中记录后长按下键可重试。

EE04 保存一条当前记录及完成状态，显示在原有便签页；时钟、天气、图片等页面仍可切换。签名回执代表持久化成功，墨水屏刷新随后进行。网页修改或完成状态变化需要手动重试同步。

## 协议

启用且联网时接收端监听 **8080**，临时管理窗口关闭后仍可接收；关闭联动会停止接收端。

通过 `GET /v1/challenge` 获取有效 30 秒的一次性 nonce。向 `POST /v1/memo` 提交 `nonce`、`id`、`done`、`text` 及小写十六进制 `mac`。使用 HMAC-SHA256 对以下精确 UTF-8 字节签名，末尾不加换行：

```text
memo-v1
<nonce>
<decimal id>
<0 or 1>
<text>
```

密钥使用配对码的 **ASCII 字符串**，不要按十六进制解码。回执签名内容为 `saved
<nonce>
<decimal id>`。保留四个有效挑战槽，鉴权并持久化后消耗 nonce；相同重复记录直接回执，避免重复写 Flash。

协议提供鉴权，不提供局域网加密。见[数据与安全](../../docs/security.zh_CN.md)和[来源与许可](../../docs/attributions.zh_CN.md)。
