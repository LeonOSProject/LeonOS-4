# SDK 实际使用验证（2026-09-19）

从新链 `out/full-migration/sdk/devtools` 复制到带空格的
`out/sdk-validation/SDK relocated`，以 `/usr/bin/clang` 运行随包 Makefile：

- 默认 helloworld 动态链接：通过。
- `STATIC=1 BUILD_DIR=build-static` 静态链接：通过。
- `APP=examples/stardusthello USE_STARDUSTUI=1` C++ GUI 示例：通过。
- 独立 stdio 最小程序动态/静态编译后，在宿主 Linux 上实际运行，均输出 `sdk runtime smoke`。
  动态程序使用包内 musl loader 和库搜索路径；ELF interpreter 为 `/lib/ld-musl-x86_64.so.1`。
- 这证明 SDK 的可重定位编译/链接及标准 libc 执行能力；不证明 LeonOS 来宾 GUI 运行成功。

修复：移除随包 Makefile 的 Python 前缀、修正 Lua/SQLite 条件嵌套，导出 StardustUI
兼容头与许可证，按 SDK 而非 BUILD 标记选择可选库，补齐 Lua 源码/移植组件。
SDK 中 zlib/png 静态库为实际对象档案；组件元数据为 JSON；非主文件缺失可触发重建。
