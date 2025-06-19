#ifndef HIKCAMERA_GLOBAL_H
#define HIKCAMERA_GLOBAL_H

#include <QtCore/qglobal.h>

#if defined(HIKCAMERA_LIBRARY)
#define HIKCAMERA_EXPORT Q_DECL_EXPORT
#else
#define HIKCAMERA_EXPORT Q_DECL_IMPORT
#endif

#endif // HIKCAMERA_GLOBAL_H
