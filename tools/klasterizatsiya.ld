PHDRS
{
    text PT_LOAD FLAGS(5); /* R_E (Lectura + Ejecución) */
    data PT_LOAD FLAGS(6); /* RW_ (Lectura + Escritura) */
}

ENTRY(klapany)

SECTIONS
{
    . = 0x8000;

    .text :
    {
        *(.text.entry)  /* Acá metemos el entry */
        *(.text)
        *(.text.*)
        *(.init)
    } :text

    .rodata :
    {
        *(.rodata)
        *(.rodata.*)
    } :text

    . = ALIGN(4K);

    .data :
    {
        *(.data)
        *(.data.*)
    } :data

    .bss :
    {
        __bss_start = .;
        *(COMMON)
        *(.bss)
        *(.bss.*)
        __bss_end = .;
    } :data
}
