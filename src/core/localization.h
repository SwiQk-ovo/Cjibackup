#ifndef LOCALIZATION_H
#define LOCALIZATION_H

class QApplication;

namespace Localization {

// 加载 Qt 自带的中文翻译，让文件选择框等系统对话框显示中文
void installChineseTranslations(QApplication &app);

} // namespace Localization

#endif // LOCALIZATION_H
