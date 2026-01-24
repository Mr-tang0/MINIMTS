QT       += core gui
QT       += charts
QT      += serialport
QT += xml
QT += network
QT += concurrent

QT += webenginewidgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

DEFINES += VersionID=8  # 改这里会自动更新版本号 8 = 1.0.9

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    aboutwidget.cpp \
    loginwin.cpp \
    main.cpp \
    mainwidget.cpp \
    mainwindow.cpp \
    mylcd.cpp \
    systemwidget.cpp \
    userwidget.cpp

HEADERS += \
    aboutwidget.h \
    loginwin.h \
    mainwidget.h \
    mainwindow.h \
    mylcd.h \
    systemwidget.h \
    userwidget.h

FORMS += \
    aboutwidget.ui \
    loginwin.ui \
    mainwidget.ui \
    mainwindow.ui \
    mylcd.ui \
    systemwidget.ui \
    userwidget.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

CONFIG(debug, debug|release) {
    DEFINES += DEBUG=1
}

RESOURCES += \
    res.qrc


RC_ICONS = res\icon\icon.ico

msvc:QMAKE_CXXFLAGS += -execution-charset:utf-8
msvc:QMAKE_CXXFLAGS += -source-charset:utf-8

win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../../Share/components/release/ -lcomponents
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../../Share/components/debug/ -lcomponents
else:unix: LIBS += -L$$OUT_PWD/../../Share/components/ -lcomponents

INCLUDEPATH += $$PWD/../../Share/components
DEPENDPATH += $$PWD/../../Share/components

win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../../Share/components/release/libcomponents.a
else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../../Share/components/debug/libcomponents.a
else:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../../Share/components/release/components.lib
else:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../../Share/components/debug/components.lib
else:unix: PRE_TARGETDEPS += $$OUT_PWD/../../Share/components/libcomponents.a

win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../../Share/MiniMTSLogic/release/ -lMiniMTSLogic
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../../Share/MiniMTSLogic/debug/ -lMiniMTSLogic
else:unix: LIBS += -L$$OUT_PWD/../../Share/MiniMTSLogic/ -lMiniMTSLogic

INCLUDEPATH += $$PWD/../../Share/MiniMTSLogic
DEPENDPATH += $$PWD/../../Share/MiniMTSLogic

win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../../Share/MiniMTSLogic/release/libMiniMTSLogic.a
else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../../Share/MiniMTSLogic/debug/libMiniMTSLogic.a
else:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../../Share/MiniMTSLogic/release/MiniMTSLogic.lib
else:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../../Share/MiniMTSLogic/debug/MiniMTSLogic.lib
else:unix: PRE_TARGETDEPS += $$OUT_PWD/../../Share/MiniMTSLogic/libMiniMTSLogic.a


INCLUDEPATH += $$PWD/../../Qt-Advanced-Docking-System/src
DEPENDPATH += $$PWD/../../Qt-Advanced-Docking-System/src


win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../build/Qt-Advanced-Docking-System/lib/ -lqtadvanceddocking
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../build/Qt-Advanced-Docking-System/lib/ -lqtadvanceddocking
else:unix: LIBS += -L$$PWD/../build/Qt-Advanced-Docking-System/ -lqtadvanceddocking



INCLUDEPATH += $$PWD/../fftw3/include
DEPENDPATH += $$PWD/../fftw3/include

LIB_DIR = $$PWD/../fftw3/lib/
LIB_FILES = $$files($$LIB_DIR/*.lib, true)
LIBS += $$LIB_FILES


INCLUDEPATH += $$PWD/../Eigen3
INCLUDEPATH += $$PWD/../Eigen3/Eigen

INCLUDEPATH += $$PWD/../nanoflann





win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../../Share/qtTools/release/ -lqtTools
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../../Share/qtTools/debug/ -lqtTools

INCLUDEPATH += $$PWD/../../Share/qtTools
DEPENDPATH += $$PWD/../../Share/qtTools

INCLUDEPATH += $$PWD/../../Share/qtTools/tool/
DEPENDPATH += $$PWD/../../Share/qtTools/tool/
include($$PWD/../../Share/qtTools/tool/tool.pri)

INCLUDEPATH += $$PWD/../../Share/qtTools/mycore/
DEPENDPATH += $$PWD/../../Share/qtTools/mycore/
include ($$PWD/../../Share/qtTools/mycore/mycore.pri)

INCLUDEPATH += $$PWD/../../Share/qtTools/PDF/
DEPENDPATH += $$PWD/../../Share/qtTools/PDF/
include ($$PWD/../../Share/qtTools/PDF/PDF.pri)

win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../../Share/qtTools/release/libqtTools.a
else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../../Share/qtTools/debug/libqtTools.a
else:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../../Share/qtTools/release/qtTools.lib
else:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../../Share/qtTools/debug/qtTools.lib



win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../../Share/HIKCamera/release/ -lHIKCamera
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../../Share/HIKCamera/debug/ -lHIKCamera
else:unix: LIBS += -L$$OUT_PWD/../../Share/HIKCamera/ -lHIKCamera

INCLUDEPATH += $$PWD/../../Share/HIKCamera
DEPENDPATH += $$PWD/../../Share/HIKCamera

INCLUDEPATH += $$PWD/../../Share/HIKCamera/HIKlib/includes
DEPENDPATH += $$PWD/../../Share/HIKCamera/HIKlib/includes
INCLUDEPATH += $$PWD/../../Share/HIKCamera/external/release64/include
DEPENDPATH += $$PWD/../../Share/HIKCamera/external/release64/include

LIB_DIR = $$PWD/../../Share/HIKCamera/external/release/lib/
LIB_FILES = $$files($$LIB_DIR/*.lib, true)
LIBS += -L$$LIB_FILES

INCLUDEPATH += $$PWD/../../Share/HIKCamera/Extensometer

win32: LIBS += -L$$OUT_PWD/../../Share/HIKCamera/release/ -lHIKCamera

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../../Share/HIKCamera/HIKlib/x64/ -lMvCameraControl
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../../Share/HIKCamera/HIKlib/x64/ -lMvCameraControl
else:unix: LIBS += -L$$PWD/../../Share/HIKCamera/HIKlib/x64/ -lMvCameraControl

INCLUDEPATH += $$PWD/../../Share/HIKCamera/HIKlib/includes
DEPENDPATH += $$PWD/../../Share/HIKCamera/HIKlib/includes
