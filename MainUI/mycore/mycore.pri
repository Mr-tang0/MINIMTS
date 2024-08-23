HEADERS += \
    $$PWD/appinit.h \
    $$PWD/header.h \
    $$PWD/iconhelper.h \
    $$PWD/quihelper.h

SOURCES += \
    $$PWD/appinit.cpp \
    $$PWD/iconhelper.cpp \
    $$PWD/quihelper.cpp


!contains(DEFINES, no_qrc_image) {
RESOURCES += $$PWD/qrc/image.qrc
}

!contains(DEFINES, no_qrc_qm) {
RESOURCES += $$PWD/qrc/qm.qrc
}

!contains(DEFINES, no_qrc_font) {
RESOURCES += $$PWD/qrc/font.qrc
}
