QT       += core gui
QT       += charts
 # QT += webenginewidgets
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

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
    # mywebengineview.cpp \
    systemwidget.cpp \
    userwidget.cpp

HEADERS += \
    aboutwidget.h \
    loginwin.h \
    mainwidget.h \
    mainwindow.h \
    mylcd.h \
    # mywebengineview.h \
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

INCLUDEPATH += $$PWD/mycore
include ($$PWD/mycore/mycore.pri)

RESOURCES += \
    res.qrc



win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../tool/release/ -ltool
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../tool/debug/ -ltool
else:unix: LIBS += -L$$OUT_PWD/../tool/ -ltool

INCLUDEPATH += $$PWD/../tool
DEPENDPATH += $$PWD/../tool

win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../MGMotorLogic/release/ -lMGMotorLogic
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../MGMotorLogic/debug/ -lMGMotorLogic
else:unix: LIBS += -L$$OUT_PWD/../MGMotorLogic/ -lMGMotorLogic

INCLUDEPATH += $$PWD/../MGMotorLogic
DEPENDPATH += $$PWD/../MGMotorLogic


RC_ICONS = res\icon\icon.ico

msvc:QMAKE_CXXFLAGS += -execution-charset:utf-8
msvc:QMAKE_CXXFLAGS += -source-charset:utf-8

# win32:LIBS+=$$PWD/libs/libcomponents32.a
# else:LIBS+=$$PWD/libs/libcomponents64.a
# INCLUDEPATH+=$$PWD/libs/Include



# win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../../qt-material-widgets/components/release/ -lcomponents
# else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../../qt-material-widgets/components/debug/ -lcomponents
# else:unix: LIBS += -L$$OUT_PWD/../../qt-material-widgets/components/ -lcomponents

# INCLUDEPATH += $$PWD/../../qt-material-widgets/components
# DEPENDPATH += $$PWD/../../qt-material-widgets/components

# win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../../qt-material-widgets/components/release/libcomponents.a
# else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../../qt-material-widgets/components/debug/libcomponents.a
# else:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../../qt-material-widgets/components/release/components.lib
# else:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../../qt-material-widgets/components/debug/components.lib
# else:unix: PRE_TARGETDEPS += $$OUT_PWD/../../qt-material-widgets/components/libcomponents.a

win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../components/release/ -lcomponents
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../components/debug/ -lcomponents
else:unix: LIBS += -L$$OUT_PWD/../components/ -lcomponents

INCLUDEPATH += $$PWD/../components
DEPENDPATH += $$PWD/../components

win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../components/release/libcomponents.a
else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../components/debug/libcomponents.a
else:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../components/release/components.lib
else:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../components/debug/components.lib
else:unix: PRE_TARGETDEPS += $$OUT_PWD/../components/libcomponents.a
