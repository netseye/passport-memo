**简体中文** · [English](README.md)

# 界面预览

此目标将真实 `memo_ui.c`、像素主题、字体与主机 LVGL 一起编译，使用合成备忘录、示例 IP 和演示密码 `12345678`，不会连接设备。

先通过固件构建获取托管依赖，再运行：

```sh
cmake -S tests/ui_preview -B build/ui-preview
cmake --build build/ui-preview
build/ui-preview/memo_preview 0 build/home.ppm
build/ui-preview/memo_preview 2 build/recording.ppm
build/ui-preview/memo_preview 4 build/review.ppm
build/ui-preview/memo_preview 6 build/settings.ppm
build/ui-preview/memo_preview 8 build/playback.ppm
build/ui-preview/memo_preview 9 build/actions.ppm
MEMO_PREVIEW_CONFIRM=1 build/ui-preview/memo_preview 9 build/delete-confirm.ppm
```

状态编号对应 `main/memo_core.h` 中的 `memo_phase_t`。设置 `MEMO_PREVIEW_MAX_TEXT=1` 可使用 240 字内存压力数据，渲染器会打印 LVGL 堆占用。

演示时钟固定为北京时间 09:41；`MEMO_PREVIEW_UNSYNCED=1` 显示 `--:--`，`MEMO_PREVIEW_CLOCK_ROLLOVER=1` 验证视图 revision 不变时仍能跨分钟更新。状态 9 渲染操作菜单，`MEMO_PREVIEW_CONFIRM=1` 打开确认页，`MEMO_PREVIEW_DELETE_SELECTED=1` 选中删除。渲染器同时断言时钟与菜单不越界。

重新生成文档图片：安装 Pillow，在预览构建后运行 `python3 tools/render_docs.py`。管理网页演示由 `python3 tools/preview_portal.py` 启动，打开 localhost 地址并用 `12345678` 登录。它只提供合成数据，拒绝配置 / 记录写入，不连接开发板或云服务。截图时进入设备设置并展开旧版凭据。
