#*************************************************************************
# FunctionBlockName:  qtTools
# Version:            1.0.2
# Author:             tangLifa
# UpdateTime:         2025-05-20 16:116
# Comment:
#**************************************************************************

QT += widgets
QT += charts
QT += xml
QT += network

TEMPLATE = lib
CONFIG += staticlib

CONFIG += c++17


# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    calculator.cpp \
    chartform.cpp \
    ipcontroller.cpp \
    qttools.cpp \
    xmlreader.cpp

HEADERS += \
    calculator.h \
    chartform.h \
    ipcontroller.h \
    qttools.h \
    xmlreader.h

# Default rules for deployment.
unix {
    target.path = /usr/lib
}
!isEmpty(target.path): INSTALLS += target

FORMS += \
    calculator.ui \
    chartform.ui

msvc:QMAKE_CXXFLAGS += -execution-charset:utf-8
msvc:QMAKE_CXXFLAGS += -source-charset:utf-8
