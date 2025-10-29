QT -= gui

TEMPLATE = lib
DEFINES += QTOPENCORR_LIBRARY

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    oc_calibration.cpp \
    oc_cubic_bspline.cpp \
    oc_deformation.cpp \
    oc_dic.cpp \
    oc_epipolar_search.cpp \
    oc_feature_affine.cpp \
    oc_fftcc.cpp \
    oc_gradient.cpp \
    oc_icgn.cpp \
    oc_iclm.cpp \
    oc_image.cpp \
    oc_io.cpp \
    oc_nearest_neighbor.cpp \
    oc_nr.cpp \
    oc_sift.cpp \
    oc_stereovision.cpp \
    oc_strain.cpp \
    oc_subset.cpp \
    qtopencorr.cpp

HEADERS += \
    QTOpenCorr_global.h \
    oc_array.h \
    oc_calibration.h \
    oc_cubic_bspline.h \
    oc_deformation.h \
    oc_dic.h \
    oc_epipolar_search.h \
    oc_feature.h \
    oc_feature_affine.h \
    oc_fftcc.h \
    oc_gradient.h \
    oc_icgn.h \
    oc_iclm.h \
    oc_image.h \
    oc_interpolation.h \
    oc_io.h \
    oc_nearest_neighbor.h \
    oc_nr.h \
    oc_poi.h \
    oc_point.h \
    oc_sift.h \
    oc_stereovision.h \
    oc_strain.h \
    oc_subset.h \
    opencorr.h \
    qtopencorr.h

# Default rules for deployment.
unix {
    target.path = /usr/lib
}
!isEmpty(target.path): INSTALLS += target

win32-msvc {
    QMAKE_CXXFLAGS += /openmp
    QMAKE_LFLAGS += /openmp
} else {
    QMAKE_CXXFLAGS += -fopenmp
    QMAKE_LFLAGS += -fopenmp
}

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../HIKCamera/external/release/lib -lopencv_world4120
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../HIKCamera/external/release/lib -lopencv_world4120
else:unix: LIBS += -L$$PWD/../HIKCamera/external/release/ -lopencv_world4120

INCLUDEPATH += $$PWD/../HIKCamera/external/release/include
DEPENDPATH += $$PWD/../HIKCamera/external/release/include

win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$PWD/../HIKCamera/external/release/lib/libopencv_world4120.a
else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$PWD/../HIKCamera/external/release/lib/libopencv_world4120.a
else:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$PWD/../HIKCamera/external/release/lib/opencv_world4120.lib
else:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$PWD/../HIKCamera/external/release/lib/opencv_world4120.lib
else:unix: PRE_TARGETDEPS += $$PWD/../HIKCamera/external/release/libopencv_world4120.a

INCLUDEPATH += $$PWD/../fftw3/include
DEPENDPATH += $$PWD/../fftw3/include

LIB_DIR = $$PWD/../fftw3/lib/
LIB_FILES = $$files($$LIB_DIR/*.lib, true)
LIBS += $$LIB_FILES


INCLUDEPATH += $$PWD/../Eigen3
INCLUDEPATH += $$PWD/../Eigen3/Eigen

INCLUDEPATH += $$PWD/../nanoflann

