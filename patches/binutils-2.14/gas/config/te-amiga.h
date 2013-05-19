/*
 * te-amiga.h -- Amiga target environment declarations.
 */

#define TE_AMIGA 1

#define LOCAL_LABELS_DOLLAR 1
#define LOCAL_LABELS_FB 1

#ifdef OBJ_HEADER
#include OBJ_HEADER
#else
#include "obj-format.h"
#endif

#define TC_IMPLICIT_LCOMM_ALIGNMENT(SIZE, P2VAR)	\
  do {							\
    if ((SIZE) >= 4)                                 	\
      (P2VAR) = 2;					\
    else if ((SIZE) >= 2)				\
      (P2VAR) = 1;					\
    else						\
      (P2VAR) = 0;					\
  } while (0)
