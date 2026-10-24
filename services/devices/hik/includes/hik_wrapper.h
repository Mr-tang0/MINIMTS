#ifndef HIK_WRAPPER_H
#define HIK_WRAPPER_H

// 在包含海康头文件之前，把 bool 屏蔽掉
#define bool hik_bool 
#include "MvCameraControl.h"
#undef bool

#endif