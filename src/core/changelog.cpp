#include "changelog.h"
#include "core/version.h"

namespace Changelog {

QString kindName(Kind kind)
{
    switch (kind) {
    case Kind::Added:    return QStringLiteral("新增");
    case Kind::Fixed:    return QStringLiteral("修复");
    case Kind::Improved: return QStringLiteral("优化");
    case Kind::Changed:  return QStringLiteral("调整");
    }
    return QStringLiteral("更新");
}

namespace {

QVector<Release> buildReleases()
{
    QVector<Release> list;

    // ---------------- 1.1.4 ----------------
    {
        Release release;
        release.version = QStringLiteral("1.1.4");
        release.date = QStringLiteral("2026-11-07");
        release.items = {
            {Kind::Changed, QStringLiteral("软件名统一成英文 KOPAL：窗口标题、侧栏、托盘、安装程序、快捷方式、exe 文件名全部换成 KOPAL，中文名不再出现")},
            {Kind::Improved, QStringLiteral("安装/卸载时会一并清掉旧的中文名快捷方式，不会在桌面留下打不开的图标")},
        };
        list.append(release);
    }

    // ---------------- 1.1.3 ----------------
    {
        Release release;
        release.version = QStringLiteral("1.1.3");
        release.date = QStringLiteral("2026-11-07");
        release.items = {
            {Kind::Improved, QStringLiteral("图标换成标准 SD 卡正面：白色卡片剪影 + 右上切角 + 写保护凹槽 + 底部触点，背景回到最初的蓝色渐变")},
        };
        list.append(release);
    }

    // ---------------- 1.1.2 ----------------
    {
        Release release;
        release.version = QStringLiteral("1.1.2");
        release.date = QStringLiteral("2026-11-07");
        release.items = {
            {Kind::Improved, QStringLiteral("全新图标：琥珀色渐变 + 白色相机剪影，16 / 32 / 48 / 64 / 256 各档单独渲染")},
            {Kind::Added, QStringLiteral("英文名 Kopal（柯巴脂，琥珀的原料）：把照片封存起来的意思")},
        };
        list.append(release);
    }

    // ---------------- 1.1.1 ----------------
    {
        Release release;
        release.version = QStringLiteral("1.1.1");
        release.date = QStringLiteral("2026-11-07");
        release.items = {
            {Kind::Changed, QStringLiteral("软件从 C极备份 改名：窗口标题、托盘、安装程序、快捷方式、exe 文件名全部换掉")},
            {Kind::Improved, QStringLiteral("换了新图标：蓝色渐变圆角方块 + 白色「拷」字，大中小尺寸都重新排版")},
            {Kind::Improved, QStringLiteral("exe 补齐版本信息：属性里能看到发布者 SwiQk-ovo、产品名 KOPAL、版本号")},
            {Kind::Fixed, QStringLiteral("从旧版本升级时，桌面上旧的「C极备份」快捷方式会自动清掉，不会留一个打不开的图标")},
        };
        list.append(release);
    }

    // ---------------- 1.1.0 ----------------
    {
        Release release;
        release.version = QStringLiteral("1.1.0");
        release.date = QStringLiteral("2026-11-01");
        release.items = {
            {Kind::Improved, QStringLiteral("所有大圆角表面（页面卡片 / 更新窗口 / 浮层 / 启动卡片 / 引导高亮）改成连续曲率圆角（squircle），不再是普通圆弧角")},
            {Kind::Added, QStringLiteral("软件开着的时候每 20 分钟静默查一次更新：GitHub 上发了新版本，右下角浮层 + 托盘气泡立刻提示，同一个版本只提醒一次")},
            {Kind::Added, QStringLiteral("自定义启动动画支持 MP4 / MOV / WebM 视频：自动循环、静音、铺满卡片，GIF 照旧可用")},
            {Kind::Improved, QStringLiteral("启动动画重做渲染：2 倍超采样、泛光、暗角、胶片颗粒，logo 加了投影和扫光，画面不再是纯矢量味")},
            {Kind::Improved, QStringLiteral("历史 / 事件页加了空状态提示，没内容时不再是一片空白")},
            {Kind::Improved, QStringLiteral("检查更新重做成独立的「软件更新」窗口：正式版 / 预发布版分栏，更新说明可滚动，一键升级（有更新时状态条的小铃铛会左右摇）")},
            {Kind::Added, QStringLiteral("首次启动的使用引导：10 步把源/目标/流程图/重命名/开始/历史/事件/设置/关于都点一遍，可跳过，也能在关于页重看")},
            {Kind::Added, QStringLiteral("新增软件内「更新日志」窗口：新增 / 修复 / 优化 分条列出，可折叠查看历史版本和 TODO")},
            {Kind::Added, QStringLiteral("流程图新增「相机型号」节点：照片按相机型号自动分文件夹，例如 Nikon Z 6II、Sony A7M4")},
            {Kind::Added, QStringLiteral("备份页新增「文件重命名」胶囊区：前缀、序号位数、自定义内容，双击就能改，右侧实时预览")},
            {Kind::Added, QStringLiteral("自动识别相机 EXIF 型号并统一写法：NIKON Z 6_2 会显示成 Nikon Z 6II")},
            {Kind::Added, QStringLiteral("读不到型号的视频、截图自动归到「未知相机1 / 2 / 3」，不会混进相机文件夹")},
            {Kind::Improved, QStringLiteral("备份先算好每个文件的目标路径再拷贝，多线程下序号稳定、校验结果更准")},
            {Kind::Improved, QStringLiteral("只在使用「相机型号」节点时才读 EXIF，普通备份不会因此变慢")},
            {Kind::Changed, QStringLiteral("设置页原来的「重命名格式」合并到备份页，避免两处规则打架")},
            {Kind::Fixed, QStringLiteral("多线程备份时同一个目标目录被多个线程同时创建，偶发「无法创建文件夹」导致少拷文件")},
            {Kind::Fixed, QStringLiteral("单文件安装包里可能装不到主程序的问题")},
        };
        list.append(release);
    }

    // ---------------- 1.0.7 ----------------
    {
        Release release;
        release.version = QStringLiteral("1.0.7");
        release.date = QStringLiteral("2026-10-31");
        release.items = {
            {Kind::Added, QStringLiteral("启动动画扩充到 13 套，其中 3 套是真 3D：3D 翻转、3D 悬浮、3D 切片")},
            {Kind::Added, QStringLiteral("历史记录支持导出 CSV，带 UTF-8 BOM，Excel 打开中文不乱码")},
            {Kind::Improved, QStringLiteral("所有消息框改成自绘暗色对话框，不再出现系统白框和英文 Yes / No")},
            {Kind::Improved, QStringLiteral("文件 / 文件夹选择器重写：深色主题、常用位置、隐藏英文类型列")},
            {Kind::Improved, QStringLiteral("系统托盘气泡换成应用内右下角浮层提示，最小化时也能看到")},
            {Kind::Improved, QStringLiteral("流程图节点加入弹入动画，连线改为圆角折线，错位和拖拽时都不打结")},
            {Kind::Fixed, QStringLiteral("拖拽流程图节点后程序卡死（动画对象野指针）")},
            {Kind::Fixed, QStringLiteral("快速切换页面时内容消失")},
        };
        list.append(release);
    }

    // ---------------- 1.0.0 ----------------
    {
        Release release;
        release.version = QStringLiteral("1.0.0");
        release.date = QStringLiteral("2026-10-01");
        release.items = {
            {Kind::Added, QStringLiteral("首个正式版：把归档规则画成流程图，拖节点、连箭头就生成目录层级")},
            {Kind::Added, QStringLiteral("多线程备份，可暂停 / 继续 / 终止")},
            {Kind::Added, QStringLiteral("备份后自动逐文件校验，缺失文件一键补拷")},
            {Kind::Added, QStringLiteral("校验通过后引导打开 Windows 格式化窗口，是否格式化永远由你确认")},
            {Kind::Added, QStringLiteral("启动动画、系统托盘、定时备份提醒、GitHub 自动更新")},
            {Kind::Added, QStringLiteral("单文件安装包与卸载程序，卸载不留注册表垃圾")},
        };
        list.append(release);
    }

    return list;
}

QStringList buildTodo()
{
    return {
        QStringLiteral("自定义启动动画除了 GIF，也支持 MP4 / MOV / WebM 视频（走 Qt 多媒体模块，需要系统带 H.264 解码器）"),
        QStringLiteral("安装包约 41 MB：引导程序为了不依赖 Qt 运行时，自带了一份运行库"),
        QStringLiteral("多线程对老机械硬盘不一定更快，可以在设置页把线程数设成 1"),
        QStringLiteral("流程图拖拽排序的动效细节、托盘右键「暂停提醒」还在待办里"),
    };
}

} // namespace

const QVector<Release> &releases()
{
    static const QVector<Release> list = buildReleases();
    return list;
}

const Release &current()
{
    static const QVector<Release> list = releases();
    static const Release fallback;
    for (const Release &release : list) {
        if (release.version == Cji::versionNumber())
            return release;
    }
    if (!list.isEmpty())
        return list.first();
    return fallback;
}

const QStringList &todo()
{
    static const QStringList list = buildTodo();
    return list;
}

} // namespace Changelog

