#ifndef TOOL_GLOBAL_H
#define TOOL_GLOBAL_H

#include <QtCore/qglobal.h>

#if defined(TOOL_LIBRARY)
#  define TOOL_EXPORT Q_DECL_EXPORT
#else
#  define TOOL_EXPORT Q_DECL_IMPORT
#endif

#endif // TOOL_GLOBAL_H
