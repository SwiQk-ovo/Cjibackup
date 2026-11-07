# 代码签名（SignPath 开源免费计划）

## 现状（2026-11-07）

- **v1.1.1 已经发布，但没有数字签名**：exe 属性里能看到发布者 `SwiQk-ovo`（那是版本资源里的 CompanyName），
  但 SmartScreen / UAC 仍然显示"未知发布者"——那两处只认 Authenticode 证书。
- 仓库里已经准备好的东西：
  - `signpath/artifact-configuration.xml` —— 告诉 SignPath 签哪几个 exe
  - `signpath/github-workflow.yml` —— 完整的 GitHub Actions 流水线（编译 → 测试 → 打包 → 提交签名）
  - `scripts/package.ps1` —— 本地出包支持 `-SignPfx` / `-SignPassword` 直接签
- 之所以 workflow 还没放到 `.github/workflows/`，是因为当前用于上传的 token 没有 `workflow` 权限
  （GitHub 对 `.github/workflows/**` 的写入要求 token 带 `workflow` scope，否则返回 404）。

## 第 0 步：启用 workflow（2 次点击，或用带 workflow 权限的 token）

**方式 A（网页）**

1. 打开 <https://github.com/SwiQk-ovo/Cjibackup/new/main/.github/workflows>
2. 文件名填 `build-and-sign.yml`，内容复制 `signpath/github-workflow.yml` 全文，提交

**方式 B（换 token）**

把 token 加上 `workflow` 权限后告诉我，我直接推上去。

## 第 1 步：注册 SignPath 并申请开源免费签名

1. 打开 <https://signpath.io> → **Sign up**（用 GitHub 账号登录最快）
2. 打开 <https://signpath.org/> → **Apply for free open-source signing**
   - Repository：`https://github.com/SwiQk-ovo/Cjibackup`
   - License：**MIT**（仓库根目录已有 LICENSE）
   - 用途：Windows 桌面备份工具的安装包分发
3. 等邮件（一般几个工作日）

## 第 2 步：审批通过后，在 SignPath 后台建项目

| 项目 | 填什么 |
|------|--------|
| Project slug | `cjibackup` |
| Signing policy slug | `release-signing` |
| Artifact configuration | 新建，slug `cjibackup-binaries`，内容粘贴 `signpath/artifact-configuration.xml` |

然后安装 SignPath 的 **GitHub App**，授权 `SwiQk-ovo/Cjibackup`。

## 第 3 步：把两个值填进 GitHub Secrets

仓库 → Settings → Secrets and variables → Actions → New repository secret：

| Secret | 从哪拿 |
|--------|--------|
| `SIGNPATH_API_TOKEN` | SignPath → User settings → API tokens |
| `SIGNPATH_ORGANIZATION_ID` | SignPath → Organization settings |

## 第 4 步：打开签名开关

把 `.github/workflows/build-and-sign.yml` 里两处 `if: ${{ false }}` 改成 `if: ${{ always() }}`。
之后每次推 tag 或手动运行 workflow，都会：编译 → 跑测试 → 打包 → 提交 SignPath 签名 → 上传签名产物。

## 常见问题

- **签名会改变软件行为吗？** 不会，只影响 SmartScreen / UAC 的"发布者"和信誉。
- **签了就不弹 SmartScreen 了吗？** OV 证书需要一段时间的下载量积累；EV 证书才是立刻有信誉（贵很多）。
- **想自己买证书本地签**：
  ```powershell
  ./scripts/package.ps1 -Version 1.1.1 -SignPfx C:/path/cert.pfx -SignPassword "密码"
  ```
- **SignPath 免费计划对仓库的要求**：公开仓库 + OSI 认可的开源许可证 + 用 CI 构建（都已满足）。
