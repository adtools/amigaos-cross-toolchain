/*
 * Convert amiga object files in the hunk format into a.out-object files.
 * ALINK style (ie. concatenated) libraries are supported as well, in that
 * case a collection of object files is generated, you'll have to run
 * `ar rs libfoo.a obj.*' on them to make an a.out-style library out of 
 * them.
 *
 * Currently untested are:
 *  o  base-relative relocs and references
 *     I'll first have to teach gnu-ld how to deal with them ;-)
 *  o  conversion of load files...
 *  o  common symbols, amiga objects normally don't use them, sigh
 *
 * Currently not implemented are:
 *  o  BLINK style indexed libraries (HUNK_LIB and HUNK_INDEX). If you're
 *     generating such libraries yourself, you can as well generate oldstyle
 *     libraries, and the libraries from Commodore-Amiga are in ALINK format
 *     (amiga.lib and friends).
 *     This index-format is so weird and complicated that I didn't bother
 *     long to decide not to support it. If someone volunteers (you got
 *     the source;-)) please send me enhancements!
 *
 * V1.0  91-10-08  M.Wild  first hack
 * V1.1  91-10-19  M.Wild  add support for CHIP hunk attribute in
 *			   DATA/BSS hunks (not CODE, ever used??)
 *			   Now, when are multiple hunks supported ? ;-)
 * V2.0  97-03-01  fnf     Change name to hunk2aout, apply patch from
 *			   Hans Verkuil, use standard AmigaOS versioning.
 *
 * This is free software. This means that I don't care what you do with it
 * as long as you don't claim you wrote it. If you enhance it, please
 * send me your diffs! 
 * Oh, of course, you use this stuff entirely at your own risk, I'm not 
 * responsible for any damage this program might cause to you, your computer,
 * your data, your cat, your whateveryoucanthinkof, no warranty whatsoever is
 * granted.
 *
 * I can be reached on internet as: wild@nessie.cs.id.ethz.ch (Markus Wild)
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#include "defs.h"

/* strangely enough, this is missing from the doshunks.h file */
#define HUNK_ATTRIBUTE(h) ((h) >> 30)
#define HUNK_VALUE(h) ((h) & 0x3fffffff)
#define HUNK_ATTR_CHIP 0x01

/* These are the symbol names we are generating to denote the limit between
 * the `normal' and `chip' data/bss.
 */
#define CHIP_DATA_START "__chip_data"
#define CHIP_BSS_START  "__chip_bss"

#ifndef NDEBUG
#define DP(a) printf a
#else
#define DP(a)
#endif

char *version_tag = "\0$VER: hunk2aout 2.0 (1.3.97)\r\n";

static uint32_t *file_buf = 0;

/* if set no progress reports are output */
static int silent_mode = 0;

struct reloc {
  /* at which offset to relocate */
  int offset;
  /* based on which hunk base */
  int from_hunk, to_hunk;
  int size;
  int pcrel;
  int baserel;
  /* if != -1, the "to_hunk" field is not used */
  int sym_num;
};

/* NOTE: this symbol definition is compatible with struct nlist, and it will
 * be converted into a struct nlist in 'emit_aout_obj' */
struct symbol {
  int name;		/* string as offset into string table */
  uint8_t type;
  uint8_t pad1;		/* really n_other */
  short hunk_num;	/* really n_desc  */
  uint32_t value;
};

struct table {
  void *base;
  int el_size;
  int i;
  int max_el;
};

static void emit_aout_file(int fd, void *text, void *data, void *chip_data,
                           struct exec *hdr, int chip_data_size,
                           int chip_bss_size, struct table *ch_tab,
                           struct table *dh_tab, struct table *bh_tab,
                           struct table *cdh_tab, struct table *cbh_tab,
                           struct table *reloc_tab,  struct table *symbol_tab,
                           int max_hunk);

#define TAB_START_SIZE 1024

/* advance the hunk-pointer to the next hunk. Assumes the hunk-pointer is
 * pointing at the length-field currently
 */
static void next_hunk(uint32_t **hp)
{
  /* skip over the length field and the there given length */
  *hp += 1 + GETLONG(**hp);
}


/* save a lot of space for duplicate string, that all say "only.. with size.. */
static void limit_hunk (char *hunk_name)
{
  fprintf (stderr, "only one %s hunk with size!=0 supported.\n", hunk_name);
}

/****************************************************************************/

/* these two function maintain the string table. You may only free the last
 * string allocated. (This could be done with an obstack as well, but I really
 * don't need the whole functionality of an obstack, so I kept it simple ;-))
 * Only use the offset, since the table itself may be reallocated.
 */
static char *str_table = 0;
static int strtab_size, strtab_index;

static int stralloc (int len)
{
  int res;

  /* always include the zero terminator */
  len++;

  if (! str_table)
  {
    strtab_size = TAB_START_SIZE;
    /* start the table at index 4, leaving space to later fill in the 
     * size of the table */
    strtab_index = 4;
    str_table = malloc (strtab_size);
  }

  while (strtab_index + len > strtab_size)
  {
    strtab_size <<= 1;
    str_table = realloc (str_table, strtab_size);
  }

  res = strtab_index;
  strtab_index += len;
  return res;
}

static void strfree (int str)
{
  strtab_index = str;
}

/****************************************************************************/

static void add_table (struct table *tab, void *el)
{
  if (tab->i == tab->max_el)
  {
    if (! tab->base)
    {
      tab->max_el = TAB_START_SIZE;
      tab->base = malloc (tab->max_el * tab->el_size);
    }
    else
    {
      tab->max_el <<= 1;
      tab->base = realloc (tab->base, tab->max_el * tab->el_size);
    }
    if (! tab->base)
    {
      fprintf (stderr, "Out of memory adding to table.\n");
      /* exit does close all outstanding files ;-) */
      exit (20);
    }
  }
  bcopy (el, (uint8_t *)tab->base + (tab->i++ * tab->el_size), tab->el_size);
}

static void add_reloc (struct table *tab, int from, int to, int offset,
                       int size, int pcrel, int baserel, int sym_num)
{
  struct reloc r;

  DP(("reloc: from=%d, to=%d, off=%d, size=%d, pc=%d, ba=%d, syn=%d\n",
      from, to, offset, size, pcrel, baserel, sym_num));

  r.from_hunk = from;
  r.to_hunk   = to;
  r.offset    = offset;
  r.size      = size;
  r.pcrel     = pcrel;
  r.baserel   = baserel;
  r.sym_num   = sym_num;
  add_table (tab, &r);
}

static void add_symbol (struct table *tab, int num, uint32_t type,
                        int value, char *name)
{
  struct symbol s;

  s.hunk_num = num;
  s.type     = type >> 24;
  s.value    = value;
  s.name     = stralloc ((type & 0xffffff)<<2);
  bcopy (name, str_table+s.name, (type & 0xffffff)<<2);
  (str_table+s.name)[(type & 0xffffff)<<2] = 0;

  /* some (really stupid..) compilers mention symbols twice, once as 
   * a definition, and once as an EXT_SYMB. So we really have to search 
   * the symbol_table before adding an EXT_SYMB and check if a symbol of this name
   * isn't already defined for this value. If this hack should slow down
   * the conversion dramatically, I'll have to resort to hashing, I don't
   * like that idea... */
  if (s.type == EXT_SYMB)
  {
    int i;

    for (i = 0; i < tab->i; i++)
      /* we have CSE in the compiler, right? ;-)) */
      if (((struct symbol *)tab->base)[i].value    == s.value    &&
          ((struct symbol *)tab->base)[i].hunk_num == s.hunk_num &&
          ! strcmp (str_table+((struct symbol *)tab->base)[i].name,
            str_table+s.name))
      {
        strfree (s.name);
        return;
      }
  }

  add_table (tab, &s);
}

/****************************************************************************/

static void digest_objfile (uint32_t **file_pptr, uint32_t *max_fp)
{
  /* this makes it less clumsy.. */
  uint32_t *file_ptr = *file_pptr;
  static int obj_file_num = 0;
  int units = 0;
  int fd = -1;
  /* if processed hunk has a CHIP attribute */
  int is_chip;

  /* buffer-pointers, where text & data sections are stored.
   * Currently only one hunk with size!=0 of type text/data/bss each is
   * supported.
   * There is now an additional buffer to support one chip hunk as well.
   * (chip-bss is supported too, but doesn't need a buffer ;-))
   */
  uint8_t *text, *data, *chip_data;
  uint32_t chip_data_size, chip_bss_size;
  struct exec hdr;

  /* hunk numbers, needed to associate reloc blocks with segments */ 
  int hunk_num;
  static struct table code_hunks      = { 0, 4, 0, 0 };
  static struct table data_hunks      = { 0, 4, 0, 0 };
  static struct table bss_hunks       = { 0, 4, 0, 0 };
  static struct table chip_data_hunks = { 0, 4, 0, 0 };
  static struct table chip_bss_hunks  = { 0, 4, 0, 0 };

  /* static so that no initialization code is required */
  static struct table reloc_tab  = { 0, sizeof (struct reloc), 0, 0 };
  static struct table symbol_tab = { 0, sizeof (struct symbol), 0, 0 };

  /* (re) init tables */
  strtab_index = 4;
  code_hunks.i = 0;
  data_hunks.i = 0;
  bss_hunks.i = 0;
  chip_data_hunks.i = 0;
  reloc_tab.i = 0;
  symbol_tab.i = 0;

  while (units < 2)
  {
    switch (HUNK_VALUE(GETLONG(*file_ptr++)))
    {
      case HUNK_UNIT:
        DP(("HUNK_UNIT: units = %d\n", units));
        if (units++) break;
#if 0
        if (! file_ptr[0])
#endif
        {
          /* need [], not *, so that gcc allocates a fresh copy for
           * mkstemp() to modify! */
          char tmp_nam[] = "obj.YYYY.XXXXXX";
          /* this trick makes mkstemp() lots faster ;-) */
          sprintf (tmp_nam, "obj.%04d.XXXXXX", obj_file_num++);
          if ((fd = mkstemp (tmp_nam)) < 0)
            fprintf (stderr, "Can't create %s (%s). Ignoring it.\n",
                tmp_nam, strerror (errno));
        }
#if 0
        /* this idea was too good.. there are so many stupid (and duplicate!) names
         * of program units, that this generated ridiculous results... */

        else
        {
          char *file_name;
          file_name = alloca (file_ptr[0] + 1);
          strncpy (file_name, (char *)&file_ptr[1], file_ptr[0]);
          if ((fd = open (file_name, O_RDWR|O_CREAT|O_EXCL, 0666)) < 0)
            fprintf (stderr, "Can't create %s: %s. Ignoring it.\n",
                file_name, strerror (errno));
        }
#endif

        /* init data for new object file */
        text = data = chip_data = 0;
        bzero (&hdr, sizeof (hdr));
        chip_data_size = chip_bss_size = 0;
        /* if someone want's to use'em on a sun, why shouldn't we make
         * the files sun-conformant? */
        hdr.a_mid = MID_SUN010;
        hdr.a_magic = OMAGIC;
        hunk_num = 0;
        next_hunk (& file_ptr);
        if (silent_mode)
        {
          putchar ('.');
          fflush (stdout);
        }
        break;

      case HUNK_NAME:
      case HUNK_DEBUG:
        DP(("HUNK_NAME/DEBUG\n"));
        /* this hunk is silently ignored ;-) */
        next_hunk (& file_ptr);
        break;

      case HUNK_OVERLAY:
        fprintf (stderr, "warning: overlay hunk ignored!\n");
        next_hunk (& file_ptr);
        break;

      case HUNK_BREAK:
        fprintf (stderr, "warning: break hunk ignored!\n");
        break;

      case HUNK_HEADER:
        fprintf (stderr, "warning: load file header encountered.\n");
        fprintf (stderr, "         are you sure this is an object file?\n");
        /* nevertheless, we go on. perhaps some day I need this feature to
         * be able to convert a loadfile into an object file?! */

        /* skip (never used) resident library names */
        while (GETLONG(file_ptr[0])) next_hunk (& file_ptr);
        /* skip null-word, table_size, F & L, and size-table */
        file_ptr += 4 + (GETLONG(file_ptr[1]) - GETLONG(file_ptr[2]) + 1);
        break;

      case HUNK_CODE:
        DP(("HUNK_CODE, size = $%x\n", GETLONG(file_ptr[0]) << 2));
        is_chip = HUNK_ATTRIBUTE(GETLONG(file_ptr[-1])) == HUNK_ATTR_CHIP;
        if (is_chip)
          fprintf (stderr, "CHIP code hunks are not supported, "
              "ignoring CHIP attribute\n");
        if (GETLONG(file_ptr[0]))
        {
          /* only accept one code hunk with size != 0 */
          if (hdr.a_text)
            limit_hunk ("code");
          else
          {
            text = (uint8_t *)&file_ptr[1];
            hdr.a_text = GETLONG(file_ptr[0]) << 2;
          }
        }
        next_hunk (& file_ptr);
        add_table (& code_hunks, &hunk_num);
        hunk_num++;
        break;

      case HUNK_DATA:
        DP(("HUNK_DATA, size = $%x\n", GETLONG(file_ptr[0]) << 2));
        is_chip = HUNK_ATTRIBUTE(GETLONG(file_ptr[-1])) == HUNK_ATTR_CHIP;
        if (GETLONG(file_ptr[0]))
        {
          /* only accept one data hunk with size != 0 */
          if (is_chip)
          {
            if (chip_data_size)
              limit_hunk ("chip data");
            else
            {
              chip_data = (uint8_t *) &file_ptr[1];
              chip_data_size = GETLONG(file_ptr[0]) << 2;
            }
          }
          else
          {
            if (hdr.a_data)
              limit_hunk ("data");
            else
            {
              data = (uint8_t *)&file_ptr[1];
              hdr.a_data = GETLONG(file_ptr[0]) << 2;
            }
          }
        }
        next_hunk (& file_ptr);
        add_table (is_chip ? & chip_data_hunks : & data_hunks, & hunk_num);
        hunk_num++;
        break;

      case HUNK_BSS:
        DP(("HUNK_BSS, size = $%x\n", GETLONG(file_ptr[0]) << 2));
        is_chip = HUNK_ATTRIBUTE(file_ptr[-1]) == HUNK_ATTR_CHIP;
        if (GETLONG(file_ptr[0]))
        {
          /* only accept one bss hunk with size != 0 */
          if (is_chip)
          {
            if (chip_bss_size)
              limit_hunk ("chip bss");
            else
              chip_bss_size = GETLONG(file_ptr[0]) << 2;
          }
          else
          {
            if (hdr.a_bss)
              limit_hunk ("bss");
            else
              hdr.a_bss = GETLONG(file_ptr[0]) << 2;
          }
        }
        file_ptr++;
        add_table (is_chip ? & chip_bss_hunks : & bss_hunks, & hunk_num);
        hunk_num++;
        break;

      case HUNK_RELOC8:
      case HUNK_RELOC16:
      case HUNK_RELOC32:
        /* do they work like this? don't know... */
      case HUNK_DREL8:
      case HUNK_DREL16:
      case HUNK_DREL32:
        {
          int size, brel;

          brel = GETLONG(file_ptr[-1]) >= HUNK_DREL32;
          size = (brel ? HUNK_DREL8 : HUNK_RELOC8) - GETLONG(file_ptr[-1]);
          DP(("HUNK_RELOC/DREL ($%x), brel = %d, size = %d\n", GETLONG(file_ptr[-1]), brel, size));

          while (GETLONG(file_ptr[0]))
          {
            long to_reloc = GETLONG(file_ptr[1]);
            long n        = GETLONG(file_ptr[0]);

            while (n--)
              /* amiga relocs are automatically pcrel, when size < 2
               * according to the AmigaDOS-Manual 2nd ed. */
              add_reloc (&reloc_tab, hunk_num-1, to_reloc, GETLONG(file_ptr[n+2]),
                  size, size != 2, brel, -1);

            file_ptr += GETLONG(file_ptr[0]) + 2;
          }
        }
        file_ptr++;
        break;

      case HUNK_SYMBOL:
      case HUNK_EXT:
        DP(("HUNK_SYMBOL/EXT\n"));
        while (GETLONG(file_ptr[0]))
        {
          int n, size, brel;

#if 0
          DP(("  EXT_: %d, %-*.*s\n", file_ptr[0] >> 24, 
                4*(file_ptr[0] & 0xffffff), 4*(file_ptr[0] & 0xffffff), &file_ptr[1]));
#endif

          switch (GETLONG(file_ptr[0]) >> 24)
          {
            case EXT_SYMB:
            case EXT_DEF:
            case EXT_ABS:
            case EXT_RES:
              add_symbol (&symbol_tab, hunk_num-1,
                  GETLONG(file_ptr[0]),
                  file_ptr[1+(GETLONG(file_ptr[0]) & 0xffffff)], 
                  (char *)&file_ptr[1]);
              file_ptr += 2+(GETLONG(file_ptr[0]) & 0xffffff);
              break;

            case EXT_COMMON:
              /* first define the common symbol, then add the relocs */
              add_symbol (&symbol_tab, hunk_num-1,
                  GETLONG(file_ptr[0]), 
                  file_ptr[1+(GETLONG(file_ptr[0]) & 0xffffff)],
                  (char *)&file_ptr[1]);
              file_ptr += 2+(GETLONG(file_ptr[0]) & 0xffffff);

              /* now the references, translated into relocs */
              for (n = file_ptr[0]; n--; )
                add_reloc (&reloc_tab, hunk_num - 1, -1, GETLONG(file_ptr[n]),
                    2, 0, 0, symbol_tab.i - 1);
              next_hunk (&file_ptr);
              break;

            case EXT_REF8:
            case EXT_REF16:
            case EXT_REF32:
            case EXT_DEXT8:
            case EXT_DEXT16:
            case EXT_DEXT32:
              size = GETLONG(file_ptr[0]) >> 24;
              brel = size >= EXT_DEXT32;
              size = (size == EXT_REF32 || size == EXT_DEXT32) ? 2 :
                ((size == EXT_REF16 || size == EXT_DEXT16) ? 1 : 0);
              /* first define the symbol (as undefined ;-)), 
               * then add the relocs */
              add_symbol (&symbol_tab, hunk_num-1, GETLONG(file_ptr[0]), 
                  0, (char *)&file_ptr[1]);
              file_ptr += 1+(GETLONG(file_ptr[0]) & 0xffffff);

              /* now the references, translated into relocs */
              for (n = GETLONG(file_ptr[0]); n; n--)
                add_reloc (&reloc_tab, hunk_num - 1, -1, GETLONG(file_ptr[n]),
                    size, size < 2, brel, symbol_tab.i - 1);
              next_hunk (&file_ptr);
              break;

            default:
              fprintf (stderr, 
                  "Unknown symbol type %d, don't know how to handle!\n",
                  GETLONG(file_ptr[0]) >> 24);
              /* can't continue, don't know how much to advance the file_ptr
               * to reach the next valid hunk/block */
              exit(20);
          }
        }
        file_ptr++;
        break;

      case HUNK_END:
        DP(("HUNK_END\n"));
        break;

      case HUNK_LIB:
      case HUNK_INDEX:
        fprintf (stderr, "Convert this library into ALINK (join type) format.\n");
        exit (20);

      default:
        fprintf (stderr, "Unknown hunk type $%x, unit offset = $%x.\n",
            GETLONG(file_ptr[-1]), ((file_ptr-1)-*file_pptr) * 2);
        /* can't continue, don't know how much to advance the file_ptr
         * to reach the next valid hunk/block */
        exit(20);
    }

    if (file_ptr >= max_fp) break;
  }

  *file_pptr = file_ptr >= max_fp ? max_fp : file_ptr-1;

  if (fd != -1)
    emit_aout_file (fd, text, data, chip_data,
        & hdr, chip_data_size, chip_bss_size,
        & code_hunks, & data_hunks, & bss_hunks,
        & chip_data_hunks, & chip_bss_hunks,
        & reloc_tab, & symbol_tab, hunk_num);
}


static void emit_aout_file (int fd, void *text, void *data, void *chip_data,
                            struct exec *hdr, int chip_data_size,
                            int chip_bss_size, struct table *ch_tab,
                            struct table *dh_tab, struct table *bh_tab,
                            struct table *cdh_tab, struct table *cbh_tab,
                            struct table *reloc_tab, struct table *symbol_tab,
                            int max_hunk)
{
  int *code_hunks = ch_tab->base;
  int *data_hunks = dh_tab->base;
  int *bss_hunks  = bh_tab->base;
  int *chip_data_hunks = cdh_tab->base;
  int *chip_bss_hunks  = cbh_tab->base;

  char htype[max_hunk];  
  int i;
  struct reloc *r;
  struct symbol *s;
  static struct table text_relocs = { 0, sizeof (struct relocation_info), 0, 0 };
  static struct table data_relocs = { 0, sizeof (struct relocation_info), 0, 0 };

  text_relocs.i = data_relocs.i = 0;

  /* convert hunk-numbers into N_TEXT,N_DATA,N_BSS types
   * I temporarily use N_EXT to really mean `N_CHIP'
   */
  for (i = 0; i < ch_tab->i; i++) htype[code_hunks[i]] = N_TEXT;
  for (i = 0; i < dh_tab->i; i++) htype[data_hunks[i]] = N_DATA;
  for (i = 0; i < bh_tab->i; i++) htype[bss_hunks[i]]  = N_BSS;
  for (i = 0; i < cdh_tab->i; i++) htype[chip_data_hunks[i]] = N_DATA|N_EXT;
  for (i = 0; i < cbh_tab->i; i++) htype[chip_bss_hunks[i]]  = N_BSS|N_EXT;

#ifndef NDEBUG
  for (i = 0; i < max_hunk; i++) DP(("htype[%d] = %d\n", i, htype[i]));
#endif

  /* first conversion run. Change all hunk-numbers into N_TEXT, N_DATA and
   * N_BSS rsp.
   * (If you wanted to support multi-hunk files (ie. files with more than
   * one code/data/bss hunk with size > 0) you'd want to do the necessary
   * conversions here.)
   * A less general solution is currently implemented: one chip data and one
   * chip bss hunk are supported as well. Multiple hunks should work analogous,
   * but with tables instead of variables like `chip_data_size' and `data'.
   */
  for (i = 0, r = (struct reloc *)reloc_tab->base; i < reloc_tab->i; i++, r++)
  {
    /* have to convert the destination hunk before the source hunk, since I
     * need the information, whether I have to change a data or a chip data
     * source. */

    /* Convert the destination hunk, if this is a local reloc. If it has
     * an associated symbol, that symbol will be converted from CHIP to whatever
     * is needed 
     * If the target lies in a chip hunk, we have to change the offset in the
     * source hunk to include the `hunk-gap' between source and target
     */
    if (r->to_hunk > -1)
    {
      /* the base address of the used source hunk */
      void *base_hunk = NULL;
      /* this is the mentioned hunk-gap */
      uint32_t offset = 0;

      switch (htype[r->from_hunk])
      {
        case N_TEXT:
          base_hunk = text;
          break;

        case N_DATA:
          base_hunk = data;
          break;

        case N_DATA|N_EXT:
          base_hunk = chip_data;
          break;

        default:
          fprintf (stderr, "Local reloc from illegal hunk ($%x)!\n",
              htype[r->from_hunk]);
          break;
      }

      /* account for an eventual shift of the former N_BSS space by
       * chip_data_size bytes */
      switch (htype[r->to_hunk])
      {
        /* those don't need a shift */
        case N_TEXT:
        case N_DATA:
          offset = 0;
          break;

        case N_BSS:
          offset = chip_data_size;
          break;

        case N_DATA|N_EXT:
          offset = hdr->a_data;
          break;

        case N_BSS|N_EXT:
          offset = chip_data_size + hdr->a_bss;
          break;
      }

      DP(("r->from = %d, r->to = %d, base = %d, offset = %d\n", r->from_hunk, r->to_hunk, htype[r->from_hunk], offset));

      /* I really don't know how much sense non-long relocs make here,
       * but it's easy to support, so .. ;-) */
      switch (r->size)
      {
        case 2:
          *(long *)(base_hunk + r->offset)  += (long)offset;
          break;

        case 1:
          *(short *)(base_hunk + r->offset) += (short)offset;
          break;

        case 0:
          *(char *)(base_hunk + r->offset)  += (char)offset;
          break;
      }

      r->to_hunk = htype[r->to_hunk] & ~N_EXT;
    }

    /* if it's a CHIP hunk, I have to increment the relocation address by 
     * the size of the base hunk */
    if (htype[r->from_hunk] & N_EXT)
    {
      /* only N_DATA should come here, since there is no such thing as a
       * reloc originating from BSS space, but we nevertheless check for it.. */
      if (htype[r->from_hunk] == (N_DATA|N_EXT))
        r->offset += hdr->a_data;
      else
        fprintf (stderr, "Reloc from CHIP-BSS space, undefined!!\n");
    }
    r->from_hunk = htype[r->from_hunk] & ~N_EXT;

  }


  /* now convert the symbols into nlist's */
  for (i = 0, s = (struct symbol *)symbol_tab->base; i < symbol_tab->i; i++, s++)
  {
    int nl_type = 0;

    /* change hunk numbers into types */
    s->hunk_num = htype[s->hunk_num];

    switch (s->type)
    {
      case EXT_DEF:
        /* externally visible symbol */
        nl_type = N_EXT;
        /* fall into */

      case EXT_SYMB:
        nl_type |= s->hunk_num & ~N_EXT;
        /* adjust multi-hunk values to the one-seg model */
        if (s->hunk_num == N_DATA)
          s->value += hdr->a_text;
        else if (s->hunk_num == N_BSS)
          s->value += hdr->a_text + hdr->a_data + chip_data_size;
        else if (s->hunk_num == (N_DATA|N_EXT))
          s->value += hdr->a_text + hdr->a_data;
        else if (s->hunk_num == (N_BSS|N_EXT))
          s->value += hdr->a_text + hdr->a_data + chip_data_size + hdr->a_bss;
        break;

      case EXT_ABS:
        nl_type = N_ABS | N_EXT;
        break;

      case EXT_COMMON:
        /* ok for common as well, because the value field is only setup
         * for common-symbols */

      case EXT_REF32:
      case EXT_REF16:
      case EXT_REF8:
      case EXT_DEXT32:
      case EXT_DEXT16:
      case EXT_DEXT8:
        nl_type = N_UNDF | N_EXT;
        break;

      default:
        fprintf (stderr, "What kind of symbol is THAT? (%d)\n", s->type);
        break;
    }

    s->type = nl_type;
    s->pad1 = s->hunk_num = 0;  /* clear nl_other & nl_desc fields */
  }

  /* now convert the reloc table. Adjust (like above) data/bss values to the
   * one-segment model for local relocations */
  for (i =  0, r = (struct reloc *)reloc_tab->base; i < reloc_tab->i; i++, r++)
  {
    struct relocation_info rel;
    uint8_t *core_addr;
    uint32_t delta;

    memset(&rel, 0, sizeof(rel));
    rel.r_address = r->offset;
    core_addr = (r->from_hunk == N_TEXT) ? text : 
      ((r->offset < hdr->a_data) ? data : chip_data);
    /* r->offset has already been corrected to point at the chip data part
     * appended to the data part. Since we don't physically join these
     * segments (we just write them out after each other) we have to
     * ignore these changes for patches, this is what DELTA is used for. */
    delta = (core_addr == chip_data) ? hdr->a_data : 0;

    DP(("r_add = $%x, core = %s, delta = %d, ", rel.r_address,
          (core_addr == text) ? "text" : ((core_addr == data) ? "data" : "chip_data"),delta));

    if (r->to_hunk == N_DATA) {
      if (r->size == 2)
        *(uint32_t *)(core_addr + rel.r_address - delta) += hdr->a_text;
      else
        fprintf (stderr, "%s reloc into N_DATA, what should I do?\n",
            r->size == 1 ? "Short" : "Byte");
    } else if (r->to_hunk == N_BSS) {
      if (r->size == 2)
        *(uint32_t *)(core_addr + rel.r_address - delta) += hdr->a_text + hdr->a_data;
      else
        fprintf (stderr, "%s reloc into N_BSS, what should I do?\n",
            r->size == 1 ? "Short" : "Byte");
    }

#if 0
    /* don't know, what went wrong, but this conversion surely converts
     * _LVO calls wrong. I'm sure leaving this out will generate other bugs..
     * sigh... */


    /* hmm... amigadog-hunks seem to do this in a strange way...
     * Those relocs *are* treated as pcrel relocs by the linker (blink), 
     * but they're not setup as such... geez, this hunk format.. */
    if (r->pcrel)
      switch (r->size)
      {
        case 2:
          *(long *)(core_addr + rel.r_address  - delta) -= rel.r_address;
          break;

        case 1:
          *(short *)(core_addr + rel.r_address - delta) -= (short)rel.r_address;
          break;

        case 0:
          *(char *)(core_addr + rel.r_address  - delta) -= (char)rel.r_address;
          break;
      }

#endif

    rel.r_symbolnum = r->sym_num > -1 ? r->sym_num : r->to_hunk;
    rel.r_pcrel     = r->pcrel;
    rel.r_length    = r->size;
    rel.r_extern    = r->sym_num > -1;
    rel.r_baserel   = r->baserel;
    rel.r_jmptable = /* rel.r_relative = */ 0;

    DP(("r68: %s reloc\n", (r->from_hunk == N_TEXT) ? "text" : "data"));
    add_table ((r->from_hunk == N_TEXT) ? & text_relocs : & data_relocs,
        &rel);
  }

  DP(("r68: #tr = %d, #dr = %d\n", text_relocs.i, data_relocs.i));

  /* depending on whether we had any actual CHIP data, we have to adjust
   * some of the header data, and we have to generate symbols containing the
   * real size of the non-chip section(s) */
  if (chip_data_size)
  {
    /* slightly different syntax, now that we directly add an nlist symbol */
    add_symbol (symbol_tab, 0, (N_ABS << 24) | ((sizeof (CHIP_DATA_START)+3)>>2),
        hdr->a_data, CHIP_DATA_START);
    hdr->a_data += chip_data_size;
  }
  if (chip_bss_size)
  {
    add_symbol (symbol_tab, 0, (N_ABS << 24) | ((sizeof (CHIP_BSS_START)+3)>>2),
        hdr->a_bss, CHIP_BSS_START);
    hdr->a_bss  += chip_bss_size;
  }

  /* oky, fill out the rest of the header and dump everything */
  hdr->a_syms = symbol_tab->i * sizeof (struct nlist);
  hdr->a_trsize = text_relocs.i * sizeof (struct relocation_info);
  hdr->a_drsize = data_relocs.i * sizeof (struct relocation_info);
  *(uint32_t *)str_table = PUTLONG(strtab_index);
  {
    struct exec hdr_be;;
    
    hdr_be.a_mid    = PUTWORD(hdr->a_mid);
    hdr_be.a_magic  = PUTWORD(hdr->a_magic);
    hdr_be.a_text   = PUTLONG(hdr->a_text);
    hdr_be.a_data   = PUTLONG(hdr->a_data);
    hdr_be.a_bss    = PUTLONG(hdr->a_bss);
    hdr_be.a_syms   = PUTLONG(hdr->a_syms);
    hdr_be.a_entry  = PUTLONG(hdr->a_entry);
    hdr_be.a_trsize = PUTLONG(hdr->a_trsize);
    hdr_be.a_drsize = PUTLONG(hdr->a_drsize);

    write (fd, (char *)&hdr_be, sizeof (hdr_be));
  }
  if (hdr->a_text) write (fd, text, hdr->a_text);
  if (hdr->a_data - chip_data_size > 0)
    write (fd, data, hdr->a_data - chip_data_size);
  if (chip_data_size) write (fd, chip_data, chip_data_size);
  if (hdr->a_trsize) write (fd, text_relocs.base, hdr->a_trsize);
  if (hdr->a_drsize) write (fd, data_relocs.base, hdr->a_drsize);
  if (hdr->a_syms) write (fd, symbol_tab->base, hdr->a_syms);
  write (fd, str_table, strtab_index);
  close (fd);
}



int main (int argc, char *argv[])
{
  struct stat stb;
  int ret_val = 0;
  uint32_t *obj_pointer;

  FILE *fp;

  /* loop over all arguments. Can be either
   *  o  object files
   *  o  libraries (ALINK style for now)
   */
  while (*++argv)
  {
    if (! strcmp (*argv, "-s"))
    {
      silent_mode = 1;
      continue;
    }

    if (stat (*argv, &stb) == -1)
    {
      fprintf (stderr, "%s: %s\n", *argv, strerror (errno));
      continue;
    }

    file_buf = file_buf ? realloc (file_buf, stb.st_size) 
      : malloc (stb.st_size);
    if (! file_buf)
    {
      fprintf (stderr, "%s: can't allocate %d byte of memory.\n", 
          *argv, (int)stb.st_size);
      ret_val = 20;
      break;
    }

    if (!(fp = fopen (*argv, "r")))
    {
      fprintf (stderr, "Can't open %s: %s.\n", *argv, strerror (errno));
      continue;
    }

    /* read the file in at once */
    if (fread (file_buf, stb.st_size, 1, fp) != 1)
    {
      fprintf (stderr, "Can't read %s: %s.\n", *argv, strerror (errno));
      fclose (fp);
      continue;
    }

    if (! silent_mode)
      printf ("Converting %s:\n", *argv);

    /* oky, now digest the file (possibly more than one object file) */
    for (obj_pointer = file_buf;
        obj_pointer < (uint32_t *)((uint8_t *)file_buf + stb.st_size); )
      digest_objfile (&obj_pointer, (uint32_t *)((uint8_t *)file_buf + stb.st_size));

    if (! silent_mode)
      putchar ('\n');

    fclose (fp);
  }

  return ret_val;
}
