cat <<EOF
OUTPUT_FORMAT("${OUTPUT_FORMAT}")
OUTPUT_ARCH(${ARCH})

${RELOCATING+${LIB_SEARCH_DIRS}}
${STACKZERO+${RELOCATING+${STACKZERO}}}
${SHLIB_PATH+${RELOCATING+${SHLIB_PATH}}}

SECTIONS
{
  ${RELOCATING+PROVIDE(___machtype = 0x0);}
  ${RELOCATING+. = ${TEXT_START_ADDR};}
  .text :
  {
    ${RELOCATING+__stext = .;}
    *(.text)
    ${RELOCATING+___datadata_relocs = .;}
    ${RELOCATING+__etext = .;}
    ${PAD_TEXT+${RELOCATING+. = ${DATA_ALIGNMENT};}}
  }
  ${RELOCATING+___text_size = SIZEOF(.text);}
  ${RELOCATING+. = ${DATA_ALIGNMENT};}
  .data :
  {
    ${RELOCATING+__sdata = .;}
    ${CONSTRUCTING+CONSTRUCTORS}
    *(.data)
    ${RELOCATING+___a4_init = 0x7ffe;}
    ${RELOCATING+__edata = .;}
  }
  ${RELOCATING+___data_size = SIZEOF(.data);}
  .bss :
  {
    ${RELOCATING+__bss_start = .;}
    *(.bss)
    *(COMMON)
    ${RELOCATING+__end = .;}
  }
  ${RELOCATING+___bss_size = SIZEOF(.bss);}
  .data_chip :
  {
    *(.data_chip)
  }
  .bss_chip :
  {
    *(.bss_chip)
  }
}
EOF
