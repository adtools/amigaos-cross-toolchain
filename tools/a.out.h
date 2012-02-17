#ifndef _AOUT_H_
#define _AOUT_H_

#include <stdint.h>

/* Header prepended to each a.out file. */
struct exec {
  uint16_t a_mid;    /* machine ID */
  uint16_t a_magic;  /* magic number */
  uint32_t a_text;   /* text segment size */
  uint32_t a_data;   /* initialized data size */
  uint32_t a_bss;    /* uninitialized data size */
  uint32_t a_syms;   /* symbol table size */
  uint32_t a_entry;  /* entry point */
  uint32_t a_trsize; /* text relocation size */
  uint32_t a_drsize; /* data relocation size */
};

/* a_magic */
#define OMAGIC    0407  /* old impure format */
#define NMAGIC    0410  /* read-only text */
#define ZMAGIC    0413  /* demand load format */

/* a_mid */
#define MID_ZERO        0 /* unknown - implementation dependent */
#define MID_SUN010      1 /* sun 68010/68020 binary */
#define MID_SUN020      2 /* sun 68020-only binary */
#define MID_HP200     200 /* hp200 (68010) BSD binary */
#define MID_HP300     300 /* hp300 (68020+68881) BSD binary */
#define MID_HPUX    0x20C /* hp200/300 HP-UX binary */
#define MID_HPUX800 0x20B /* hp800 HP-UX binary */

#define __LDPGSZ  8192

/* Valid magic number check. */
#define N_BADMAG(ex) \
  ((ex).a_magic != NMAGIC && (ex).a_magic != OMAGIC && \
      (ex).a_magic != ZMAGIC)

/* Address of the bottom of the text segment. */
#define N_TXTADDR(ex) ((ex).a_magic == ZMAGIC ? __LDPGSZ : 0)

/* Address of the bottom of the data segment. */
#define N_DATADDR(ex) \
  (N_TXTADDR(ex) + ((ex).a_magic == OMAGIC ? (ex).a_text \
  : __LDPGSZ + ((ex).a_text - 1 & ~(__LDPGSZ - 1))))

#define N_BSSADDR(ex) (N_DATADDR(ex)+(ex).a_data)

/* Text segment offset. */
#define N_TXTOFF(ex) \
  ((ex).a_magic == ZMAGIC ? 0 : sizeof(struct exec))

/* Data segment offset. */
#define N_DATOFF(ex) \
  (N_TXTOFF(ex) + ((ex).a_magic != ZMAGIC ? (ex).a_text \
  : __LDPGSZ + ((ex).a_text - 1 & ~(__LDPGSZ - 1))))

/* Symbol table offset. */
#define N_SYMOFF(ex) \
  (N_TXTOFF(ex) + (ex).a_text + (ex).a_data + (ex).a_trsize + \
      (ex).a_drsize)

/* String table offset. */
#define N_STROFF(ex)  (N_SYMOFF(ex) + (ex).a_syms)

/* Relocation format. */
struct relocation_info {
  int r_address;                  /* offset in text or data segment */
  unsigned int r_symbolnum : 24,  /* ordinal number of add symbol */
               r_pcrel     :  1,  /* 1 if value should be pc-relative */
               r_length    :  2,  /* log base 2 of value's width */
               r_extern    :  1,  /* 1 if need to add symbol to value */
               r_baserel   :  1,  /* 1 if linkage table relative */
               r_jmptable  :  1,  /* 1 if pc-relative to jump table */
                           :  2;  /* reserved */
};

/*
 * Symbol table entry format.
 */

#define N_UNDF  0x00    /* undefined */
#define N_EXT   0x01    /* external (global) bit, OR'ed in */
#define N_ABS   0x02    /* absolute address */
#define N_TEXT  0x04    /* text segment */
#define N_DATA  0x06    /* data segment */
#define N_BSS   0x08    /* bss segment */
#define N_INDR  0x0a    /* alias definition */
#define N_SIZE  0x0c    /* pseudo type, defines a symbol's size */
#define N_COMM  0x12    /* common reference */
#define N_FN    0x1e    /* file name (N_EXT on) */
#define N_WARN  0x1e    /* warning message (N_EXT off) */
#define N_TYPE  0x1e    /* mask for all the type bits */
#define N_SLINE 0x44    /* line number in text segment */
#define N_SO    0x64    /* name of main source file */
#define N_SOL   0x84    /* name of sub-source file (#include file) */
#define N_STAB  0xe0    /* mask for debugger symbols -- stab(5) */

struct nlist {
  union {
    char   *n_name;  /* symbol name (in memory) */
    int32_t n_strx;  /* file string table offset (on disk) */
  } n_un;

  uint8_t  n_type;   /* type defines */
  int8_t   n_other;  /* spare */
  int16_t  n_desc;   /* used by stab entries */
  uint32_t n_value;  /* address/value of the symbol */
};

#endif /* !_AOUT_H_ */
