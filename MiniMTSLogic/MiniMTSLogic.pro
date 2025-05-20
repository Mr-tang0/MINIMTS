QT += widgets
QT      += serialport


TEMPLATE = lib
CONFIG += staticlib

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    minimtsdecoder.cpp \
    minimtslogic.cpp \
    minimtsport.cpp

HEADERS += \
    minimtsdecoder.h \
    minimtslogic.h \
    minimtsport.h

# Default rules for deployment.
unix {
    target.path = $$[QT_INSTALL_PLUGINS]/generic
}
!isEmpty(target.path): INSTALLS += target

FORMS += \
    minimtsport.ui

msvc:QMAKE_CXXFLAGS += -execution-charset:utf-8
msvc:QMAKE_CXXFLAGS += -source-charset:utf-8

win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../tool/release/ -ltool
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../tool/debug/ -ltool
else:unix: LIBS += -L$$OUT_PWD/../tool/ -ltool

INCLUDEPATH += $$PWD/../tool
DEPENDPATH += $$PWD/../tool
