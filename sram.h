
/*---------------------------------------------------------------------*
        Copyright (C) 1997 Nintendo.
        
        $RCSfile: sram.h,v $
        $Revision: 1.2 $
        $Date: 1998/09/25 21:51:24 $
 *---------------------------------------------------------------------*/
/* 
 * defines for SRAM
 */
#ifndef _CI_H
#define _CI_H
 
#define SRAM_START_ADDR		0x08000000
#define SRAM_SIZE		0x8000
#define SRAM_LATENCY		0x5
#define SRAM_PULSE		0x0c
#define SRAM_PAGE_SIZE		0xd
#define SRAM_REL_DURATION	0x2

#define SRAM_OFFSET_DDRAM	0x0000
#define SRAM_SIZE_DDRAM		0x0100

//prototypes
OSPiHandle *SramInit(void);

#endif