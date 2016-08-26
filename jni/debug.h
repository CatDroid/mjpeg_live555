/*
 * debug.h
 *
 *  Created on: 2016年8月19日
 *      Author: rd0394
 */

#ifndef DEBUG_H_
#define DEBUG_H_

#include <android/log.h>

#define DBG(...)  __android_log_print( ANDROID_LOG_WARN, "TOM",   __VA_ARGS__ )

#endif /* DEBUG_H_ */
