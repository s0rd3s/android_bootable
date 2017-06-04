OUTPUT_ARCH(arm)

ENTRY(_start)

romBase = 0x00201000;
ramBase = 0x00236000;
stackBase = 0x0011B400;

MEMORY {
    ram : ORIGIN = 0x00236000, LENGTH = 0xA000
    rom : ORIGIN = 0x00201000, LENGTH = 0x35000
    stack : ORIGIN = 0x0011B400, LENGTH = 0xC00
}

SECTIONS {

    . = romBase;
    .start ALIGN(4) : {	
        *(.text.start)
    } >rom
    
    . = romBase + 0x01FC;
    .rom_info ALIGN(4) : {	    	    	    	     
    	*(.data.rom_info)
    } >rom 

    .text ALIGN(4) : {
        *(.text)
        *(.text.*)        
    } >rom

    .rodata ALIGN(4) : {
        *(.rodata)
        *(.rodata.*)        
    } >rom

    .data ALIGN(4) : {
        *(.data)
        *(.data.*)        
    } >rom

    .got ALIGN(4) : {
        *(.got)
        *(.got.*)        
    } >rom

    __boot_end = .;

    . = ramBase;
    .bss ALIGN(16) : {
        _bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        _bss_end = .;
    } >ram

    . = stackBase;
	.stack ALIGN(4) : {
		_stack_start = .;
		*(.stack)
		_stack_end = .;
	} >stack

}
