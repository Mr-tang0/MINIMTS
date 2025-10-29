QT       += core gui
QT       += concurrent

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

DEFINES += BUILD_LIB

contains(DEFINES, BUILD_APP) {
    SOURCES += main.cpp
    TARGET = HIKCAMERA
}else{
    TEMPLATE = lib
    DEFINES += HIKCAMERA_LIBRARY
    HEADERS += \
        hikcamera_global.h
}


# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    camera.cpp \
    hikcamera.cpp

HEADERS += \
    camera.h \
    hikcamera.h \

FORMS += \
    hikcamera.ui

# # Default rules for deployment.
# qnx: target.path = /tmp/$${TARGET}/bin
# else: unix:!android: target.path = /opt/$${TARGET}/bin
# !isEmpty(target.path): INSTALLS += target


unix {
    target.path = /usr/lib
}
!isEmpty(target.path): INSTALLS += target

msvc:QMAKE_CXXFLAGS += -execution-charset:utf-8
msvc:QMAKE_CXXFLAGS += -source-charset:utf-8

INCLUDEPATH += $$PWD/Extensometer
include ($$PWD/Extensometer/Extensometer.pri)

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/HIKlib/x64/ -lMvCameraControl
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/HIKlib/x64/ -lMvCameraControl
else:unix: LIBS += -L$$PWD/./ -lMvCameraControl

INCLUDEPATH += $$PWD/HIKlib/includes
DEPENDPATH += $$PWD/HIKlib/includes

win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$PWD/HIKlib/x64/libMvCameraControl.a
else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$PWD/HIKlib/x64/libMvCameraControl.a
else:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$PWD/HIKlib/x64/MvCameraControl.lib
else:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$PWD/HIKlib/x64/MvCameraControl.lib
else:unix: PRE_TARGETDEPS += $$PWD/HIKlib/x64/libMvCameraControl.a


INCLUDEPATH += $$PWD/external/release/include
DEPENDPATH += $$PWD/external/release/include

LIB_DIR = $$PWD/external/release/lib/

LIB_FILES = $$files($$LIB_DIR/*.lib, true)

LIBS += -L$$LIB_FILES




win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../QTOpenCorr/release/ -lQTOpenCorr
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../QTOpenCorr/debug/ -lQTOpenCorr
else:unix: LIBS += -L$$OUT_PWD/../QTOpenCorr/ -lQTOpenCorr

INCLUDEPATH += $$PWD/../QTOpenCorr
DEPENDPATH += $$PWD/../QTOpenCorr

INCLUDEPATH += $PWD/../fftw3/include
DEPENDPATH += $$PWD/../fftw3/include

LIB_DIR = $$PWD/../fftw3/lib/
LIB_FILES = $$files($$LIB_DIR/*.lib, true)
LIBS += $$LIB_FILES


INCLUDEPATH += $$PWD/../Eigen3
INCLUDEPATH += $$PWD/../Eigen3/Eigen

INCLUDEPATH += $$PWD/../nanoflann
