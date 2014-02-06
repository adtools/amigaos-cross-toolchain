#include <exec/tasks.h>
#include <mmu/descriptor.h>
#include <mmu/context.h>
#include <mmu/mmubase.h>
#include <mmu/mmutags.h>
#include <utility/tagitem.h>
#include <proto/mmu.h>
#include <proto/dos.h>
#include <proto/exec.h>

long __nocommandline = 1;

static BOOL PrintMMUInfo() {
  PutStr("MMU: ");

  switch (GetMMUType()) {
    case MUTYPE_68851:
      PutStr("68851\n");
      break;

    case MUTYPE_68030:
      PutStr("68030\n");
      break;

    case MUTYPE_68040:
      PutStr("68040\n");
      break;

    case MUTYPE_68060:
      PutStr("68060\n");
      break;

    default:
      PutStr("None\n");
      return FALSE;
  }

  return TRUE;
}

static void PrintMapping(struct MappingNode *mn) {
  while (mn->map_succ) {
    Printf(" 0x%08lx - 0x%08lx : ", mn->map_Lower, mn->map_Higher);

    if (mn->map_Properties & MAPP_WRITEPROTECTED)
      PutStr("WriteProtected ");
    if (mn->map_Properties & MAPP_USED)
      PutStr("U ");
    if (mn->map_Properties & MAPP_MODIFIED)
      PutStr("M ");
    if (mn->map_Properties & MAPP_GLOBAL)
      PutStr("Global ");
    if (mn->map_Properties & MAPP_TRANSLATED)
      PutStr("TT ");
    if (mn->map_Properties & MAPP_ROM)
      PutStr("ROM ");
    if (mn->map_Properties & MAPP_USERPAGE0)
      PutStr("UP0 ");
    if (mn->map_Properties & MAPP_USERPAGE1)
      PutStr("UP1 ");
    if (mn->map_Properties & MAPP_CACHEINHIBIT)
      PutStr("CacheInhibit ");
    if (mn->map_Properties & MAPP_IMPRECISE)
      PutStr("Imprecise ");
    if (mn->map_Properties & MAPP_NONSERIALIZED)
      PutStr("NonSerial ");
    if (mn->map_Properties & MAPP_COPYBACK)
      PutStr("CopyBack ");
    if (mn->map_Properties & MAPP_SUPERVISORONLY)
      PutStr("SuperOnly ");
    if (mn->map_Properties & MAPP_BLANK)
      PutStr("Blank ");
    if (mn->map_Properties & MAPP_SHARED)
      PutStr("Shared ");
    if (mn->map_Properties & MAPP_SINGLEPAGE)
      PutStr("Single ");
    if (mn->map_Properties & MAPP_REPAIRABLE)
      PutStr("Repairable ");
    if (mn->map_Properties & MAPP_IO)
      PutStr("I/O space ");
    if (mn->map_Properties & MAPP_USER0)
      PutStr("U0 ");
    if (mn->map_Properties & MAPP_USER1)
      PutStr("U1 ");
    if (mn->map_Properties & MAPP_USER2)
      PutStr("U2 ");
    if (mn->map_Properties & MAPP_USER3)
      PutStr("U3 ");
    if (mn->map_Properties & MAPP_INVALID)
      Printf("Invalid (0x%08lx) ", (LONG)mn->map_un.map_UserData);
    if (mn->map_Properties & MAPP_SWAPPED)
      Printf("Swapped (0x%08lx) ", (LONG)mn->map_un.map_UserData);
    if (mn->map_Properties & MAPP_REMAPPED)
      Printf("Remapped to 0x%08lx ", (LONG)mn->map_un.map_Delta+mn->map_Lower);
    if (mn->map_Properties & MAPP_BUNDLED)
      Printf("Bundled to 0x%08lx ", (LONG)mn->map_un.map_Page);
    if (mn->map_Properties & MAPP_INDIRECT)
      Printf("Indirect at 0x%08lx ", (LONG)mn->map_un.map_Descriptor);
    PutStr("\n");

    mn = mn->map_succ;
  } 
}

static void PrintMemoryLayout(struct MMUContext *ctx) {
  struct MinList *list = GetMapping(ctx);

  PutStr("\n");
  PutStr("Memory layout:\n");
  PrintMapping((struct MappingNode *)(list->mlh_Head));
  ReleaseMapping(ctx, list);
}

int main() {
  if (PrintMMUInfo()) {
    struct MMUContext *ctx = DefaultContext();
    struct MMUContext *privctx;
    ULONG error;

    if ((privctx = CreateMMUContext(MCXTAG_COPY, (LONG)ctx,
                                    MCXTAG_ERRORCODE, (LONG)&error,
                                    TAG_DONE)))
    {
      LONG psize;

      Printf("CreateMMUContext succeeded with %ld.\n", error);

      psize = GetPageSize(privctx);
      Printf("Page size: %ld\n", psize);

      EnterMMUContext(privctx, FindTask(NULL));
      PrintMemoryLayout(privctx);

      {
        void *ptr = AllocAligned(psize * 16, MEMF_FAST, psize);

        PutStr("\n");
        Printf("16 pages allocated at 0x%08lx - 0x%08lx.\n", (LONG)ptr, (LONG)ptr + psize * 16 - 1);
        PutStr("Let's make them CacheInhibit!\n");

        SetProperties(privctx, MAPP_CACHEINHIBIT, MAPP_COPYBACK|MAPP_CACHEINHIBIT, (LONG)ptr, psize * 16, TAG_DONE);
        RebuildTree(privctx);
        PrintMemoryLayout(privctx);

        FreeMem(ptr, psize * 16);
      }

      LeaveMMUContext(FindTask(NULL));
      DeleteMMUContext(privctx);
    } else {
      Printf("CreateMMUContext failed with %ld.\n", error);
    }
  }

  return 0;
}
