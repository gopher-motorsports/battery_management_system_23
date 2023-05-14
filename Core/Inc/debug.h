#ifndef INC_DEBUG_H_
#define INC_DEBUG_H_

/* ==================================================================== */
/* =================== GLOBAL FUNCTION DECLARATIONS =================== */
/* ==================================================================== */

void printBmsStruct();

/* ==================================================================== */
/* =========================== DEBUG MACROS =========================== */
/* ==================================================================== */

#define DEBUG_DATA 1
#if DEBUG_DATA
	#define DebugData() printBmsStruct()
#else
	#define DebugData()  
#endif

#define DEBUG_INIT 1
#if DEBUG_INIT
	#define DebugInit(__info,...) printf("DebugInit: " __info,##__VA_ARGS__)
#else
	#define DebugInit(__info,...)  
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

#endif // INC_DEBUG_H_
