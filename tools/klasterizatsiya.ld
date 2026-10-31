ENTRY(klapany)

SECTIONS
{
	. = 0x8000;

	.text :
	{
		*(.text.entry)  /* Acá metemos el entry */
		*(.text)
		*(.init)
	}

	.rodata :
	{
		*(.rodata)
	}

	.data :
	{
		*(.data)
	}

	.bss :
	{
		__bss_start = .;
		*(COMMON)
		*(.bss)
		*(.bss.*)
		__bss_end = .;
	}
}
