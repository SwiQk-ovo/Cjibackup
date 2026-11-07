#include "filedialogs.h"

#include "filepickerdialog.h"

namespace UiDialogs {

QString chooseDirectory(QWidget *parent, const QString &title, const QString &initial)
{
    return FilePickerDialog::pickDirectory(parent, title, initial);
}

QString chooseOpenFile(QWidget *parent, const QString &title, const QString &filter,
                       const QString &initial)
{
    return FilePickerDialog::pickFile(parent, title, filter, initial);
}

} // namespace UiDialogs