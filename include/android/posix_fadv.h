#ifndef __POSIX_FADV_CONST
#define __POSIX_FADV_CONST

// Belows are not used ANYWAY in zynq board
// Added for code compatibility

#define POSIX_FADV_NORMAL       0 /* No further special treatment.  */
#define POSIX_FADV_RANDOM       1 /* Expect random page references.  */
#define POSIX_FADV_SEQUENTIAL   2 /* Expect sequential page references.  */
#define POSIX_FADV_WILLNEED     3 /* Will need these pages.  */

#endif
