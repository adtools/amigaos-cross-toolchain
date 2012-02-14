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
    ${RELOCATING+__edata = .;}
    ${RELOCATING+___data_size = ABSOLUTE(__edata - __sdata);}
  }
  ${RELOCATING+. = ALIGN(0x0);}
  .bss :
  {
    ${RELOCATING+ __bss_start = .};
    *(.bss)
    *(COMMON)
    ${RELOCATING+__end = . };
    ${RELOCATING+___bss_size = ABSOLUTE(__end - __bss_start);}
  }
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
