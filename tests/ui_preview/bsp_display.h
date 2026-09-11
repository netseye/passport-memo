#pragma once
#include <stdbool.h>
static inline bool bsp_lvgl_lock(unsigned ms){(void)ms;return true;}
static inline void bsp_lvgl_unlock(void){}
