#ifndef QTTOOLS_GLOBAL_H
#define QTTOOLS_GLOBAL_H

#include <QtCore/qglobal.h>

#if defined(QTTOOLS_LIBRARY)
#define QTTOOLS_EXPORT Q_DECL_EXPORT
#else
#define QTTOOLS_EXPORT Q_DECL_IMPORT
#endif

#endif // QTTOOLS_GLOBAL_H
