#ifndef EXIFSCANNER_H
#define EXIFSCANNER_H

#include <QString>
#include <QVector>

// 一台相机的汇总信息
struct CameraInfo
{
    QString model;   // 规范化后的型号，如 "Nikon Z 6II"
    int files = 0;   // 属于这台相机的文件数
};

/**
 * 从照片 / 相机原文件里读 EXIF，识别相机型号。
 * 支持 JPEG 和 TIFF 结构的 RAW（CR2/CR3/NEF/ARW/DNG/ORF/RW2/PEF/RAF）。
 * MOV/MP4 视频没有通用型号字段，跳过。
 */
class ExifScanner
{
public:
    // 读单个文件的相机型号；读不到返回空
    static QString readCameraModel(const QString &filePath);

    // 递归扫描目录，汇总各相机型号的文件数（按数量降序）
    static QVector<CameraInfo> scanDirectory(const QString &root, int maxFiles = 600);

    // 把型号变成合法的文件夹名（去掉 \ / : * ? " < > |）
    static QString folderSafeModel(const QString &model);
};

#endif // EXIFSCANNER_H
