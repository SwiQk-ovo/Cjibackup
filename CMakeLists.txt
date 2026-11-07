cmake_minimum_required(VERSION 3.16)
project(Cjibackup VERSION 1.1.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 版本号：CMakeLists 的 project(VERSION) 和 src/core/version.h 要一起改
set(APP_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.ico")
configure_file(resources/app.rc.in ${CMAKE_CURRENT_BINARY_DIR}/app.rc @ONLY)
configure_file(installer/setup.rc.in ${CMAKE_CURRENT_BINARY_DIR}/setup.rc @ONLY)
configure_file(installer/uninstall.rc.in ${CMAKE_CURRENT_BINARY_DIR}/uninstall.rc @ONLY)
configure_file(installer/bootstrap.rc.in ${CMAKE_CURRENT_BINARY_DIR}/bootstrap.rc @ONLY)

option(CJI_BUILD_TESTS "构建命令行测试与截图工具" OFF)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

if(WIN32)
    enable_language(RC)
endif()

set(APP_SOURCES
    src/mainwindow.cpp
    src/core/appicon.cpp
    src/core/settingsmanager.cpp
    src/core/backupengine.cpp
    src/core/historymanager.cpp
    src/core/flowpath.cpp
    src/core/remindermanager.cpp
    src/core/updatemanager.cpp
    src/core/soundplayer.cpp
    src/core/winutils.cpp
    src/core/localization.cpp
    src/core/exifscanner.cpp
    src/core/changelog.cpp
    src/pages/backup_page.cpp
    src/pages/history_page.cpp
    src/pages/events_page.cpp
    src/pages/settings_page.cpp
    src/pages/about_page.cpp
    src/widgets/flowcanvas.cpp
    src/widgets/typewritertooltip.cpp
    src/widgets/darkdialog.cpp
    src/widgets/changelogdialog.cpp
    src/widgets/guideoverlay.cpp
    src/widgets/updatedialog.cpp
    src/widgets/squircle.cpp
    src/widgets/toast.cpp
    src/widgets/uianim.cpp
    src/widgets/filedialogs.cpp
    src/widgets/filepickerdialog.cpp
    src/widgets/splashwindow.cpp
    src/widgets/splash3d.cpp
)

set(APP_HEADERS
    src/mainwindow.h
    src/core/appicon.h
    src/core/settingsmanager.h
    src/core/backupengine.h
    src/core/historymanager.h
    src/core/flowpath.h
    src/core/remindermanager.h
    src/core/updatemanager.h
    src/core/soundplayer.h
    src/core/winutils.h
    src/core/localization.h
    src/core/exifscanner.h
    src/core/cameranaming.h
    src/core/changelog.h
    src/core/version.h
    src/pages/backup_page.h
    src/pages/history_page.h
    src/pages/events_page.h
    src/pages/settings_page.h
    src/pages/about_page.h
    src/widgets/flowcanvas.h
    src/widgets/typewritertooltip.h
    src/widgets/darkdialog.h
    src/widgets/changelogdialog.h
    src/widgets/guideoverlay.h
    src/widgets/updatedialog.h
    src/widgets/squircle.h
    src/widgets/toast.h
    src/widgets/uianim.h
    src/widgets/filedialogs.h
    src/widgets/filepickerdialog.h
    src/widgets/splashwindow.h
    src/widgets/splash3d.h
)

function(cji_configure_gui_target target)
    target_link_libraries(${target} PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Network
    )
    if(WIN32)
        target_link_libraries(${target} PRIVATE winmm shell32 advapi32 ole32 uuid dwmapi)
    endif()
    target_include_directories(${target} PRIVATE src)
endfunction()

# ---------------- 主程序 ----------------
qt_add_executable(Cjibackup WIN32
    src/main.cpp
    ${APP_SOURCES}
    ${APP_HEADERS}
    resources.qrc
    ${CMAKE_CURRENT_BINARY_DIR}/app.rc
)

cji_configure_gui_target(Cjibackup)

set_target_properties(Cjibackup PROPERTIES
    OUTPUT_NAME "拷备"
)

# ---------------- 安装器 / 卸载器 / 打包工具 ----------------
qt_add_executable(CjiInstaller WIN32
    installer/main.cpp
    installer/installerwindow.cpp
    installer/installerwindow.h
    src/widgets/darkdialog.cpp
    src/widgets/squircle.cpp
    src/widgets/toast.cpp
    src/widgets/uianim.cpp
    src/widgets/filedialogs.cpp
    src/widgets/filepickerdialog.cpp
    src/widgets/darkdialog.h
    src/widgets/squircle.h
    src/widgets/toast.h
    src/widgets/uianim.h
    src/widgets/filedialogs.h
    src/widgets/filepickerdialog.h
    installer/installdata.cpp
    installer/installdata.h
    src/core/localization.cpp
    src/core/exifscanner.cpp
    src/core/localization.h
    src/core/exifscanner.h
    src/core/cameranaming.h
    installer/payload.cpp
    installer/payload.h
    installer/installer.qrc
    ${CMAKE_CURRENT_BINARY_DIR}/setup.rc
)
cji_configure_gui_target(CjiInstaller)
set_target_properties(CjiInstaller PROPERTIES OUTPUT_NAME "CjibackupSetup")

qt_add_executable(CjiUninstaller WIN32
    installer/uninstall_main.cpp
    src/widgets/darkdialog.cpp
    src/widgets/squircle.cpp
    src/widgets/toast.cpp
    src/widgets/uianim.cpp
    src/widgets/filedialogs.cpp
    src/widgets/filepickerdialog.cpp
    src/widgets/darkdialog.h
    src/widgets/squircle.h
    src/widgets/toast.h
    src/widgets/uianim.h
    src/widgets/filedialogs.h
    src/widgets/filepickerdialog.h
    installer/installdata.cpp
    installer/installdata.h
    src/core/localization.cpp
    src/core/exifscanner.cpp
    src/core/localization.h
    src/core/exifscanner.h
    src/core/cameranaming.h
    installer/installer.qrc
    ${CMAKE_CURRENT_BINARY_DIR}/uninstall.rc
)
cji_configure_gui_target(CjiUninstaller)
set_target_properties(CjiUninstaller PROPERTIES OUTPUT_NAME "CjibackupUninstall")

    qt_add_executable(packer tools/packer.cpp)
target_link_libraries(packer PRIVATE Qt6::Core)

# 单文件安装包的引导程序：纯 Win32，不依赖 Qt（静态链接，保证单文件可运行）
add_executable(CjibackupBootstrap WIN32 installer/bootstrap.cpp ${CMAKE_CURRENT_BINARY_DIR}/bootstrap.rc)
target_link_libraries(CjibackupBootstrap PRIVATE user32 gdi32)
if(MINGW)
    target_link_options(CjibackupBootstrap PRIVATE -static -static-libgcc -static-libstdc++)
endif()
set_target_properties(CjibackupBootstrap PROPERTIES OUTPUT_NAME "CjibackupBootstrap")

if(CJI_BUILD_TESTS)
    qt_add_executable(payload_test
        tests/payload_test.cpp
        installer/payload.cpp
        installer/payload.h
    )
    target_link_libraries(payload_test PRIVATE Qt6::Core)
    target_include_directories(payload_test PRIVATE installer)
endif()

# ---------------- 测试 ----------------
if(CJI_BUILD_TESTS)
    qt_add_executable(flowpath_test
        tests/flowpath_test.cpp
        src/core/flowpath.cpp
        src/core/flowpath.h
    )
    target_link_libraries(flowpath_test PRIVATE Qt6::Core)
    target_include_directories(flowpath_test PRIVATE src)

    qt_add_executable(backup_smoke
        tests/backup_smoke.cpp
        src/core/backupengine.cpp
        src/core/backupengine.h
        src/core/exifscanner.cpp
        src/core/exifscanner.h
        src/core/flowpath.cpp
        src/core/flowpath.h
        src/core/settingsmanager.cpp
        src/core/settingsmanager.h
    )
    target_link_libraries(backup_smoke PRIVATE Qt6::Core)
    target_include_directories(backup_smoke PRIVATE src)

    qt_add_executable(core_tests
        tests/core_tests.cpp
        src/core/updatemanager.cpp
        src/core/updatemanager.h
        src/core/settingsmanager.cpp
        src/core/settingsmanager.h
        src/core/remindermanager.cpp
        src/core/remindermanager.h
    )
    target_link_libraries(core_tests PRIVATE Qt6::Core Qt6::Network)
    target_include_directories(core_tests PRIVATE src)

    # 离屏截图工具：给文档 / 预览用，不参与发布
    qt_add_executable(shot_tool
        tests/shot_tool.cpp
        ${APP_SOURCES}
        ${APP_HEADERS}
        resources.qrc
    )
    cji_configure_gui_target(shot_tool)
endif()

# ---------------- 发布工具（独立小工具，不依赖主程序） ----------------
add_executable(CjiPublisher WIN32 src/publish/publisher_main.cpp)
target_link_libraries(CjiPublisher PRIVATE Qt6::Widgets Qt6::Network)
set_target_properties(CjiPublisher PROPERTIES OUTPUT_NAME "CjiPublisher")

# ---------------- Qt Multimedia（可选）：自定义启动动画播放 MP4 ----------------
# 装了 Qt Multimedia 就编进去；没装也不影响构建，只是自定义动画只支持 GIF
find_package(Qt6 QUIET COMPONENTS Multimedia)
if(TARGET Qt6::Multimedia)
    message(STATUS "Qt Multimedia found: MP4 splash animation enabled")
    target_link_libraries(Cjibackup PRIVATE Qt6::Multimedia)
    target_compile_definitions(Cjibackup PRIVATE CJI_HAS_MULTIMEDIA=1)
    if(TARGET shot_tool)
        target_link_libraries(shot_tool PRIVATE Qt6::Multimedia)
        target_compile_definitions(shot_tool PRIVATE CJI_HAS_MULTIMEDIA=1)
    endif()
endif()