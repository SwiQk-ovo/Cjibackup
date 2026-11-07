# 代码签名（SignPath 开源免费计划）

## 现状

- v1.1.0 / v1.1.1 发布的 exe **没有数字签名**：Windows 属性里能看到发布者 SwiQk-ovo（那是版本资源），
  但 SmartScreen / UAC 仍会显示"未知发布者"，因为那两处只认 Authenticode 证书。
- 已经准备好的东西（在仓库里）：
  - `.github/workflows/build-and-sign.yml`：GitHub Actions 里编译 + 跑测试 + 打包 + 提交签名
  - `signpath/artifact-configuration.xml`：告诉 SignPath 签哪几个 exe
  - `scripts/package.ps1`：本地出包时也支持 `-SignPfx` 直接签

## 你需要做的（只有你能做，涉及账号与身份验证）

1. **注册 SignPath**：<https://signpath.io> → Sign up（用 GitHub 账号登录最快）
2. **申请开源免费计划**：<https://signpath.org/> → Apply for free open-source signing
   - 填仓库地址 `https://github.com/SwiQk-ovo/Cjibackup`
   - 许可证选 **MIT**（仓库里已经有 LICENSE）
   - 说明用途：桌面备份工具的分发安装包
   - 通过后会收到邮件，通常几个工作日
3. **授权仓库**：在 SignPath 后台安装 GitHub App，把 `SwiQk-ovo/Cjibackup` 勾上
4. **建项目**（审批通过后）：
   - Project slug：`cjibackup`
   - Signing policy slug：`release-signing`
   - Artifact configuration：新建一个，slug 用 `cjibackup-binaries`，内容粘贴 `signpath/artifact-configuration.xml`
5. **拿两个值填到 GitHub Secrets**（仓库 Settings → Secrets and variables → Actions）：
   - `SIGNPATH_API_TOKEN`：SignPath 后台 User settings → API tokens 生成
   - `SIGNPATH_ORGANIZATION_ID`：SignPath 后台 Organization settings 里能看到

## 之后怎么生效

把 `.github/workflows/build-and-sign.yml` 里两处 `if: ${{ false }}` 改成 `if: ${{ always() }}`，
下次推 tag / 手动跑 workflow 就会自动：编译 → 测试 → 打包 → 提交 SignPath 签名 → 上传签名后的产物。

## 注意

- 签名不会改变程序行为，只影响 SmartScreen / UAC 的"发布者"显示和信誉
- 首次签名后，SmartScreen 仍可能需要一段时间的下载量积累才完全不提示（EV 证书才是立刻有信誉，但很贵）
- 本地出包想自己签（买了证书的情况）：
  ```powershell
  ./scripts/package.ps1 -Version 1.1.1 -SignPfx C:/path/cert.pfx -SignPassword "密码"
  ```
