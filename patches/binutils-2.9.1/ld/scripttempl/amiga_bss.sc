cat <<EOF
OUTPUT_FORMAT("${OUTPUT_FORMAT}")
OUTPUT_ARCH(${ARCH})

${RELOCATING+${LIB_SEARCH_DIRS}}
${STACKZERO+${RELOCATING+${STACKZERO}}}
${SHLIB_PATH+${RELOCATING+${SHLIB_PATH}}}

SECTIONS
{
  ${RELOCATING+. = ${TEXT_START_ADDR};}
  .text :
  {
    ${RELOCATING+___machtype = ABSOLUTE(0x0);}
    ${RELOCATING+__stext = .;}
    *(.text)
    ${RELOCATING+___datadata_relocs = .;}
    ${RELOCATING+__etext = .;}
    ${RELOCATING+___text_size = ABSOLUTE(__etext - __stext);}
    ${PAD_TEXT+${RELOCATING+. = ${DATA_ALIGNMENT};}}
  }
  ${RELOCATING+. = ${DATA_ALIGNMENT};}
  .data :
  {
    ${RELOCATING+__sdata = .;}
    ${CONSTRUCTING+CONSTRUCTORS}
    *(.data)
    ${RELOCATING+___a4_init = 0x7ffe;}
    ${RELOCATING+___data_size = ABSOLUTE(__edata - __sdata);}
    ${RELOCATING+___bss_size = ABSOLUTE(0);}
  }
  .bss :
  {
    *(.bss)
    *(COMMON)
    ${RELOCATING+__edata  =  .;}
    ${RELOCATING+__bss_start  =  .;}
    ${RELOCATING+__end = ALIGN(4) };
  }
}
EOF
