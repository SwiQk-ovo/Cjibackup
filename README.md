# 拷备（Cjibackup）

给摄影师的相机存储卡备份工具：把卡里的照片/视频按 `年/月/日/相机型号/事件/序号` 这类规则自动归档，带校验、历史记录和断点续传式的文件跳过。

- Windows 10 / 11（64 位）
- C++17 + Qt 6.5.3（mingw_64）
- 安装包 / 免安装版见 [Releases](https://github.com/SwiQk-ovo/Cjibackup/releases)

## 功能

- **分类流程图**：把「年 / 月 / 日 / 相机型号 / 事件 / 序号 / 后缀 / 自定义」拖进画布，从左到右决定目录层级
- **相机型号识别**：读 EXIF 里的机身型号并统一写法（`NIKON Z 6_2` → `Nikon Z 6II`），读不到的归到「未知相机 N」
- **文件重命名**：前缀 + 补零序号 + 自定义内容，实时预览
- **先规划再拷贝**：多线程下序号稳定，校验结果准确
- **备份校验**：逐文件比对，通过后可打开 Windows 格式化界面
- **历史 / 事件 / 设置 / 关于** 四个辅助页；首次启动有 10 步使用引导
- **13 套启动动画**（含 3 套真 3D），也支持自定义 GIF / MP4 / MOV / WebM
- **软件内更新日志 + 自动检查更新**：发现新版本在右下角提示，一键升级

## 从源码构建

需要 Qt 6.5.3 mingw_64（含 `qtmultimedia` 模块）、MinGW 13.1、CMake 3.20+。

```powershell
cmake -S . -B build -G "MinGW Makefiles" `
      -DCMAKE_PREFIX_PATH="C:/Qt/6.5.3/mingw_64" `
      -DCMAKE_BUILD_TYPE=Release -DCJI_BUILD_TESTS=ON
cmake --build build -j 8
```

测试：

```powershell
cd build
./flowpath_test.exe; ./core_tests.exe; ./backup_smoke.exe; ./payload_test.exe ./packer.exe
```

出安装包（可选带代码签名）：

```powershell
./scripts/package.ps1 -Version 1.1.1
# 有证书时：
./scripts/package.ps1 -Version 1.1.1 -SignPfx C:/path/cert.pfx -SignPassword ******
```

## 代码签名

本项目参与 SignPath 的开源免费签名计划（审批中）。CI 流水线见 `.github/workflows/build-and-sign.yml`，
签名配置见 `signpath/artifact-configuration.xml`，申请与接入步骤见 `docs/SIGNING.md`。

## 许可证

MIT，见 [LICENSE](LICENSE)。

---

## 反馈与交流

- **Bug 反馈 / 使用问题：QQ 群 `1135151552`**（也可以直接扫群号搜索进群）
- 提 Issue：<https://github.com/SwiQk-ovo/Cjibackup/issues>
- 软件里「关于」页也有这个群号，左键单击就能复制

