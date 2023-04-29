#ifndef INC_DEBUG_H_
#define INC_DEBUG_H_

#define DEBUG 1
#if DEBUG
	#define Debug(__info,...) printf("Debug: " __info,##__VA_ARGS__)
#else
	#define Debug(__info,...)  
#endif

#define DEBUG_COMM 0
#if DEBUG_COMM
	#define DebugComm(__info,...) printf("DebugComm: " __info,##__VA_ARGS__)
#else
	#define DebugComm(__info,...)  
#endif

#define DEBUG_EPAPER 0
#if DEBUG_EPAPER
	#define DebugEpaper(__info,...) printf("DebugEpaper: " __info,##__VA_ARGS__)
#else
	#define DebugEpaper(__info,...)  
#endif

#endif