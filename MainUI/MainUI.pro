QT       += core gui
QT       += charts
QT      += serialport
QT += xml
QT += network
QT += concurrent

QT += webenginewidgets

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

win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../MiniMTSLogic/release/ -lMiniMTSLogic
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../MiniMTSLogic/debug/ -lMiniMTSLogic
else:unix: LIBS += -L$$OUT_PWD/../MiniMTSLogic/ -lMiniMTSLogic

INCLUDEPATH += $$PWD/../MiniMTSLogic
DEPENDPATH += $$PWD/../MiniMTSLogic

win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../MiniMTSLogic/release/libMiniMTSLogic.a
else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../MiniMTSLogic/debug/libMiniMTSLogic.a
else:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../MiniMTSLogic/release/MiniMTSLogic.lib
else:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../MiniMTSLogic/debug/MiniMTSLogic.lib
else:unix: PRE_TARGETDEPS += $$OUT_PWD/../MiniMTSLogic/libMiniMTSLogic.a


INCLUDEPATH += $$PWD/../../Qt-Advanced-Docking-System/src
DEPENDPATH += $$PWD/../../Qt-Advanced-Docking-System/src


win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../build/Qt-Advanced-Docking-System/lib/ -lqtadvanceddocking
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../build/Qt-Advanced-Docking-System/lib/ -lqtadvanceddocking
else:unix: LIBS += -L$$PWD/../build/Qt-Advanced-Docking-System/ -lqtadvanceddocking




win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../build/qtTools/release/ -lqtTools
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../build/qtTools/debug/ -lqtTools
else:unix: LIBS += -L$$PWD/../build/qtTools/ -lqtTools

INCLUDEPATH += $$PWD/../../qtTools
DEPENDPATH += $$PWD/../../qtTools

INCLUDEPATH += $$PWD/../../qtTools/tool/
DEPENDPATH += $$PWD/../../qtTools/tool/
include($$PWD/../../qtTools/tool/tool.pri)

INCLUDEPATH += $$PWD/../../qtTools/mycore/
DEPENDPATH += $$PWD/../../qtTools/mycore/
include ($$PWD/../../qtTools/mycore/mycore.pri)

INCLUDEPATH += $$PWD/../../qtTools/PDF/
DEPENDPATH += $$PWD/../../qtTools/PDF/
include ($$PWD/../../qtTools/PDF/PDF.pri)


win32: LIBS += -L$$OUT_PWD/../HIKCamera/release/ -lHIKCamera

INCLUDEPATH += $$PWD/../HIKCamera
DEPENDPATH += $$PWD/../HIKCamera

INCLUDEPATH += $$PWD/../HIKCamera/HIKlib/includes
DEPENDPATH += $$PWD/../HIKCamera/HIKlib/includes

INCLUDEPATH += $$PWD/../HIKCamera/external/release64/include




