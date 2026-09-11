**简体中文** · [English](architecture.md)

# 代码架构

## 文件分工

| 路径 | 职责 |
| --- | --- |
| `main/main.c` | 初始化 I2C、屏幕、音频、电量，再启动应用 |
| `main/memo_app.c` | 工作队列、状态、配置、NVS 记录与草稿 |
| `main/memo_ui.c` | LVGL 页面、换行滚动、状态与麦克风动画 |
| `main/ui_pixel*` | 上游像素主题和布局辅助函数 |
| `main/memo_asr.c` | 采集任务、Opus 编码、TLS/WebSocket 与识别结果 |
| `main/memo_core.c` | 可在主机测试的 UTF-8、协议、流重组、错误分类 |
| `main/memo_ogg.c` | Ogg 头、CRC、页序号及 EOS |
| `main/memo_network.c` | Wi-Fi、校时、临时网页服务与 EE04 签名传输 |
| `main/memo_portal.html` | 无前端依赖的嵌入式管理网页 |
| `components/bsp/` | 上游硬件支持和引脚定义 |
| `tests/`、`tools/` | 主机测试、界面预览、字体生成及构建校验 |
| `companion/ee04/` | 独立构建的 GPL 接收端，仅通过局域网接收文字 |

按键回调只投递事件，耗时操作交给应用工作任务。状态快照由互斥锁保护；音频和网络回调只发布状态，不操作 LVGL。UI 定时器负责渲染，非 LVGL 上下文使用 BSP 的 LVGL 锁。

## 音频与 ASR

采集格式为 16 kHz、单声道、16 位有符号 PCM。每 20 ms（640 字节）编码成 40 字节 Opus：16 kbps CBR、VOIP、复杂度 0、关闭 VBR/DTX。每个 Opus 包放入一个 Ogg 页，约每 100 ms 发送五页，音频负载约 3.4 KB/s，原始 PCM 为 32 KB/s。Ogg 包含 CRC、序号、48 kHz 粒度位置、312 样本 pre-skip，以及结尾补帧/EOS。

接口为 `wss://openspeech.bytedance.com/api/v3/sauc/bigmodel_async`，使用[官方 v3 协议](https://docs.volcengine.com/docs/6561/1354869?lang=zh)，传输未压缩 JSON 与 Ogg 音频。请求声明 `format=ogg`、`codec=opus`、`rate=16000`、`bits=16`、`channel=1`、`result_type=full`；开启 ITN、标点及二遍处理，关闭 DDC 和分句元数据。完整文字替换上一版，只有会话最终帧标记才结束会话。

不自动重连重放；中断后已有文字保留供检查。队列拥堵时停止采集，不静默丢弃音频。应用日志只记录固定错误类别、耗时和内存，不输出火山原始错误正文及识别文字。

## C3 内存预算

| 分配 / 设置 | 当前值 |
| --- | --- |
| LVGL 堆 | 40 KB |
| 采集 / 编码任务栈 | 40 KB，TLS 前预留 |
| WebSocket 任务栈 | 6 KB |
| 应用工作任务栈 | 8 KB |
| 音频上传队列 | 4 槽，每槽 512 字节负载 |
| 响应累计上限 | 8 KB |
| HTTP 升级缓冲 | 动态 8 KB，与 1 KB WebSocket 帧缓冲独立 |
| Wi-Fi RX | 静态 3 / 动态 6 个缓冲，BA 窗口 3 |

TLS 握手前初始化 Opus，识别首包发送成功后才开始读麦克风。Wi-Fi 快速路径使用 Flash，减少 IRAM；关闭 IPv6、蓝牙与 TLS 重新协商。动态 TLS 缓冲释放配置及对端证书存储，同时保留证书校验和网络校时。不要直接替换成通用 C3 默认配置。

已测短会话中 40 KB 采集栈最少仍余 19,604 字节，但这不能证明最坏情况，详见[验证记录](validation.zh_CN.md)。新增字体、任务或网络缓冲时，需要评估片内 RAM，不能只看 Flash 空间。

## 最近一段原声回放

`memo_replay.c` 管理独立 256 KB Flash 分区，`memo_replay_format.c` 提供主机可测试的 CRC、元数据和精确采样裁切。录音前擦除缓存，录音时复用 40 字节 Opus 帧，以 4 KB 缓冲写入；120 秒及尾部补帧共 240,040 字节，另留 4 KB 元数据区。会话结束写入元数据，校验失败或写入中断时禁用回放，识别文字仍可保存。

草稿通过文字 SHA-256 关联，保存后绑定记录 ID；编辑文字保留原声，删除该记录清除缓存。重启读取元数据，播放前校验整段 CRC。解码去掉 16 kHz 下的 104 个预跳过采样及尾部补齐，按实际采样数结束。

`MEMO_PLAYBACK` 将配置与记录写入标记为忙；双击只触发回放，单击保留原操作。按键回调仅用原子变量控制停止和音量。24 KB 解码任务栈仅在 Opus 编码器及 TLS 释放后创建；音频写入由该任务串行执行，随后填充静音并等待 I2S DMA 排空。4 KB 静态缓冲也用于回放前校验，不保留整段 PCM。音量默认 85%，范围 10–100%，每次按键 5%，重启恢复默认，不改变 NVS 配置结构。

`memo_playback_pcm.c` 只处理解码后的播放 PCM：低电平放大 2 倍（约 +6 dB），绝对值超过 24,000 后进入连续软限幅，输出趋近 32,000，不溢出为反向波形。完整 16 位输入范围已通过单调、对称与边界测试。原始 Opus 和上传音频保持不变。ES8311 默认曲线中 65% 对应 −17.5 dB、85% 对应 −7.5 dB；新默认值减少了 10 dB 输出衰减。
