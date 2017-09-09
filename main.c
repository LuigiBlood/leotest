//64DD Test
//Based on pfs SDK demo

#include	<ultra64.h>
#include	<PR/leo.h>
#include	"thread.h"
#include	"textlib.h"
#include	"ncode.h"
#include	<math.h>
#include	<string.h>

#include "asicdrive.h"
#include "sram.h"

#define NUM_MESSAGE 	1

#define	CONT_VALID	1
#define	CONT_INVALID	0

#define WAIT_CNT	5

extern OSMesgQueue retraceMessageQ;

extern u16      *bitmap_buf;
extern u16      bitmap_buf1[];
extern u16      bitmap_buf2[];
extern u8	backup_buffer[];

OSMesgQueue  		pifMesgQueue;
OSMesg       		dummyMessage;
OSMesg       		pifMesgBuf[NUM_MESSAGE];

static OSContStatus     contstat[MAXCONTROLLERS];
static OSContPad        contdata[MAXCONTROLLERS];
static int		controller[MAXCONTROLLERS];
OSPfs			pfs0;

//DMA PI
#define DMA_QUEUE_SIZE 2
OSMesgQueue dmaMessageQ;
OSMesg    dmaMessageBuf[DMA_QUEUE_SIZE];
OSIoMesg  dmaIOMessageBuf;
OSIoMesg  dmaIoMesgBuf;

OSPiHandle *pi_handle;
OSPiHandle *pi_ddrom_handle;
OSPiHandle *pi_sram_handle;
OSPiHandle *LeoDiskHandle;

//64DD Leo
#define	DISK_MSG_NUM 8
#define NUM_LEO_MESGS 8
static OSMesg           LeoMessages[NUM_LEO_MESGS];
static OSMesgQueue	LeoMsgQueue;

OSMesgQueue	diskMsgQ;
OSMesg	diskMsgBuf[DISK_MSG_NUM];

static u32  IPLoffset;

LEOStatus		leostat;
LEODiskID		_diskID, _savedID;
static LEOCmd		_cmdBlk;
static LEOVersion	_leover;

static u32	selectLBA;
static s32	LBAsize;

static u8 blockData[19720];

static s8 menumode;
static s8 menuselect;

#include "leotest.h"

//READ/WRITE STUFF
s16 LBAselect;
u32 SECselect;
s16 LBAselected;

//REGISTER STUFF
u32 REGselect;
u32 REGselected;
u32 REGread;
u32 REGwrite;
u32 REGwrite2;
s16 REGsetbit;

void SramDma(u32 rw_flag, u32 *data_addr, u32 offset, u32 size)
{

  osWritebackDCache(data_addr, size);

  dmaIOMessageBuf.hdr.pri  =  OS_MESG_PRI_NORMAL;
  dmaIOMessageBuf.hdr.retQueue  =  &dmaMessageQ;
  dmaIOMessageBuf.dramAddr  =  data_addr;
  dmaIOMessageBuf.devAddr  =  SRAM_START_ADDR + offset;
  dmaIOMessageBuf.size  = size;

  osEPiStartDma(pi_sram_handle, &dmaIOMessageBuf, rw_flag);
  (void)osRecvMesg(&dmaMessageQ, NULL, OS_MESG_BLOCK);
}

void mainproc(void *arg)
{	
  int	i, j;
  int	cont_no = 0;
  int	error, readerror;
  u8	bitpattern;
  u16	button = 0, oldbutton = 0, newbutton = 0; 
  u32	stat;
  char 	console_text[50];

  osCreateMesgQueue(&pifMesgQueue, pifMesgBuf, NUM_MESSAGE);
  osSetEventMesg(OS_EVENT_SI, &pifMesgQueue, dummyMessage);

  osContInit(&pifMesgQueue, &bitpattern, &contstat[0]);
  for (i = 0; i < MAXCONTROLLERS; i++) {
    if ((bitpattern & (1<<i)) &&
       ((contstat[i].type & CONT_TYPE_MASK) == CONT_TYPE_NORMAL)) {
      controller[i] = CONT_VALID;
    } else {
      controller[i] = CONT_INVALID;
    }
  }
  
  osCreateMesgQueue(&diskMsgQ, diskMsgBuf, 1);
  
  osCreateMesgQueue(&dmaMessageQ, dmaMessageBuf, 1);

  pi_handle = osCartRomInit();
  pi_ddrom_handle = osDriveRomInit();
  
  bzero(blockData, 19720);
  readerror = -1;

  init_draw();

  setcolor(0,255,0);
  draw_puts("If you see this for a long period of time,\nsomething fucked up. Sorry.");
  
  osCreatePiManager((OSPri)OS_MESG_PRI_NORMAL, &dmaMessageQ, dmaMessageBuf, DMA_QUEUE_SIZE);
  
  LeoCJCreateLeoManager((OSPri)OS_PRIORITY_LEOMGR-1, (OSPri)OS_PRIORITY_LEOMGR, LeoMessages, NUM_LEO_MESGS);
  LeoResetClear();
  
  osCreateMesgQueue(&LeoMsgQueue, LeoMessages, NUM_LEO_MESGS);
  
  LeoDiskHandle = osLeoDiskInit();
  pi_sram_handle = SramInit();

  setbgcolor(15,15,15);
  clear_draw();
  
  setcolor(0,255,0);
  draw_puts("\f\n\n    leotest\n    ----------------------------------------\n");
  
  menumode = 0;
  menuselect = 0;
  LBAselect = 0;
  SECselect = 0;
  LBAselected = 0;
  error = -1;
  
  REGread = 0;
  REGwrite = 0;
  REGselect = REGselected = ASIC_DATA;
  REGsetbit = 0;

  while(1)
  {
    //Read Controller Data all the time
    osContStartReadData(&pifMesgQueue);
    osRecvMesg(&pifMesgQueue, NULL, OS_MESG_BLOCK);
    osContGetReadData(&contdata[0]);
    if (contdata[cont_no].errno & CONT_NO_RESPONSE_ERROR) {
      button = oldbutton;
    } else {
      oldbutton = button;
      button = contdata[cont_no].button;
    }
    newbutton = ~oldbutton & button;
    
    //Manage Menu
    if (menumode == 0)
    {
    	//Main Menu Mode
    	
    	//Button Management
        if (newbutton & D_JPAD)
        {
            menuselect++;
            if (menuselect > 4)
            	menuselect = 0;
        }
        else if (newbutton & U_JPAD)
        {
            menuselect--;
            if (menuselect < 0)
            	menuselect = 4;
        }
        else if (newbutton & A_BUTTON || newbutton & Z_TRIG || newbutton & START_BUTTON)
        {
            switch (menuselect)
            {
            	case 0:
            	    menumode = 1;
            	    break;
            	case 1:
            	    menumode = 2;
            	    break;
            	case 2:
            	    menumode = 3;
            	    bzero(blockData, 19720);
            	    break;
            	case 3:
            	    menumode = 4;
            	    break;
            }
        }
        
        //Render Text
        setcolor(255,255,255);
        draw_puts("\f\n\n\n\n  -WARNING: If you don't know what you're doing, stop using this immediately.-\n\n");
        
        if (menuselect == 0) setcolor(0,255,0);
        else setcolor(255,255,255);
        draw_puts("     READ/WRITE TEST\n");
        if (menuselect == 1) setcolor(0,255,0);
        else setcolor(255,255,255);
        draw_puts("     REGISTER TEST\n");
        if (menuselect == 2) setcolor(0,255,0);
        else setcolor(255,255,255);
        draw_puts("     64DD INTERNAL RAM DUMP\n");
        if (menuselect == 3) setcolor(0,255,0);
        else setcolor(255,255,255);
        draw_puts("     COMMAND TEST\n");
        if (menuselect == 4) setcolor(0,255,0);
        else setcolor(255,255,255);
        draw_puts("     OPTION 4\n");
        
        if (menumode != 0)
        {
            clear_draw();
  
	    setcolor(0,255,0);
  	    draw_puts("\f\n\n    leotest\n    ----------------------------------------\n");
        }
    }
    else if (menumode == 1)
    {
    	//READ/WRITE TEST MENU (only does Read for now)
    	//Button Management
        if (newbutton & D_JPAD)
        {
            LBAselect++;
        }
        else if (newbutton & U_JPAD)
        {
            LBAselect--;
        }
        else if (newbutton & L_JPAD)
        {
            LBAselect -= 0x80;
        }
        else if (newbutton & R_JPAD)
        {
            LBAselect += 0x80;
        }
        else if (newbutton & D_CBUTTONS)
        {
            SECselect++;
        }
        else if (newbutton & U_CBUTTONS)
        {
            SECselect--;
        }
        else if (newbutton & A_BUTTON)
        {
            SECselect = 0;
            LBAselected = LBAselect;
            bzero(blockData, 19720);
            LeoReadWrite(&_cmdBlk, OS_READ, LBAselected, (void*)&blockData, 1, &diskMsgQ);
            osRecvMesg(&diskMsgQ, (OSMesg *)&error, OS_MESG_BLOCK);
            
            clear_draw();
  
	    setcolor(0,255,0);
  	    draw_puts("\f\n\n    leotest\n    ----------------------------------------\n");
        }
        else if (newbutton & L_TRIG)
        {
            LeoReadWrite(&_cmdBlk, OS_WRITE, LBAselected, (void*)&blockData, 1, &diskMsgQ);
            osRecvMesg(&diskMsgQ, (OSMesg *)&error, OS_MESG_BLOCK);
    	    
    	    clear_draw();
  
	    setcolor(0,255,0);
  	    draw_puts("\f\n\n    leotest\n    ----------------------------------------\n");
        }
        else if (newbutton & Z_TRIG)
        {
            LeoReadDiskID(&_cmdBlk, &_diskID, &diskMsgQ);
    	    osRecvMesg(&diskMsgQ, (OSMesg *)&error, OS_MESG_BLOCK);
    	    
    	    clear_draw();
  
	    setcolor(0,255,0);
  	    draw_puts("\f\n\n    leotest\n    ----------------------------------------\n");
        }
        else if (newbutton & B_BUTTON)
        {
            menumode = 0;
        }
        
        //Render Text
    	setcolor(255,255,255);
        draw_puts("\f\n\n\n\n                         \n                      \n");
        
        sprintf(console_text, "     LBA: %04X / Sector: %04X        \n                  \n    ", LBAselect, SECselect);
        draw_puts(console_text);
        
        sprintf(console_text, "Disk ID: %c%c%c%c", _diskID.gameName[0], _diskID.gameName[1], _diskID.gameName[2], _diskID.gameName[3]);
    	draw_puts(console_text);
    	draw_puts("\n\n    ");
        errorcheck(error);
        draw_puts("\n\n\n    ");
        
        //Render Byte Data
        setcolor(255,255,255);
        for(i = 1; i < GetSectorSize(LBAselected); i++)
        {
            sprintf(console_text, " %02X", blockData[GetSectorSize(LBAselected) * SECselect + (i - 1)]);
            draw_puts(console_text);
            if (i % 16 == 0)
            	draw_puts("\n    ");
        }
        
        if (menumode != 1)
        {
            clear_draw();
  
	    setcolor(0,255,0);
  	    draw_puts("\f\n\n    leotest\n    ----------------------------------------\n");
        }
    }
    else if (menumode == 2)
    {
    	//REGISTER TEST
    	//Button Management
    	if (newbutton & D_JPAD)
        {
            //Next Reg
            REGselect += 4;
            if (REGselect > ASIC_TEST_PIN_SEL)
            	REGselect = ASIC_DATA;
        }
        else if (newbutton & U_JPAD)
        {
            //Prev Reg
            REGselect -= 4;
            if (REGselect < ASIC_DATA)
            	REGselect = ASIC_TEST_PIN_SEL;
        }
        else if (newbutton & R_CBUTTONS)
        {
            //++
            REGsetbit++;
            //0-15 = bit
            //16-19 = nibble
            if (REGsetbit > 19)
            	REGsetbit = 0;
        }
        else if (newbutton & L_CBUTTONS)
        {
            //--
            REGsetbit--;
            if (REGsetbit < 0)
            	REGsetbit = 19;
        }
        else if (newbutton & D_CBUTTONS)
        {
            //set bit++
            if (REGsetbit < 16)
            	REGwrite ^= (0x80000000 >> REGsetbit);
            else
            	REGwrite += (0x10000000 >> ((REGsetbit - 16) * 4));
        }
        else if (newbutton & U_CBUTTONS)
        {
            //set bit--
            if (REGsetbit < 16)
            	REGwrite ^= (0x80000000 >> REGsetbit);
            else
            	REGwrite -= (0x10000000 >> ((REGsetbit - 16) * 4));
        }
    	else if (newbutton & A_BUTTON)
        {
            //READ
            REGselected = REGselect;
            REGread = ReadLeoReg(REGselected);
        }
    	else if (newbutton & Z_TRIG)
        {
            //WRITE
            REGselected = REGselect;
            WriteLeoReg(REGselected, REGwrite);
        }
        else if (newbutton & R_TRIG)
        {
            //Swap
            u32 REGswap = REGwrite;
            REGwrite = REGwrite2;
            REGwrite2 = REGswap;
        }
        else if (newbutton & B_BUTTON)
        {
            menumode = 0;
        }
        
        //Render
        setcolor(255,255,255);
        draw_puts("\f\n\n\n\n    D-Pad Up/Down to select Register\n    C-Buttons to set bits / R to swap\n    A to read register / Z to write to register\n\n");
        
        sprintf(console_text, "     Register Select: %08X\n\n", REGselect);
        draw_puts(console_text);
        
        draw_puts("     Data Setup: ");
        //-Render Bits
        for (i = 0; i < 16; i++)
        {
            setcolor(255,255,255);
            if (i == REGsetbit)
            	setcolor(0,255,0);
            if (REGwrite & (0x80000000 >> i))
            	draw_puts("1");
            else
            	draw_puts("0");
        }
        draw_puts(" : ");
        //-Render Nibbles
        for (i = 0; i < 4; i++)
        {
            setcolor(255,255,255);
            if (i == (REGsetbit - 16))
            	setcolor(0,255,0);
            sprintf(console_text, "%1X", ((REGwrite / (0x10000000 >> (i * 4))) & 0xF));
            draw_puts(console_text);
        }
        
        setcolor(255,255,255);
        sprintf(console_text, "\n\n\n\n     %08X:%08X", REGselected, REGread);
        draw_puts(console_text);
        
        //When quitting
    	if (menumode != 2)
        {
            clear_draw();
  
	    setcolor(0,255,0);
  	    draw_puts("\f\n\n    leotest\n    ----------------------------------------\n");
        }
    }
    else if (menumode == 3)
    {
    	//BATTERY BACKED 64DD RAM
    	//Button Management
    	if (newbutton & Z_TRIG)
        {
            //READ RAM
            for (i = 0; i < 256; i++)
            {
              WriteLeoReg(ASIC_DATA, (i << 24));
              WriteLeoReg(ASIC_CMD, 0x001C0000);
              //Waste Time.
              for (j = 0; j < 1000; j++)
              	ReadLeoReg(ASIC_STATUS);
              blockData[i] = (ReadLeoReg(ASIC_DATA) & 0x00FF0000) >> 16;
            }
        }
        else if (newbutton & A_BUTTON)
        {
            //Zero
            bzero(blockData, 19720);
        }
        else if (newbutton & L_TRIG)
        {
            //Load Back from SRAM
            SramDma(OS_READ, blockData, SRAM_OFFSET_DDRAM, SRAM_SIZE_DDRAM);
        }
        else if (newbutton & R_TRIG)
        {
            //Save to SRAM
            SramDma(OS_WRITE, blockData, SRAM_OFFSET_DDRAM, SRAM_SIZE_DDRAM);
        }
        else if (newbutton & START_BUTTON)
        {
            for (i = 0; i < 256; i++)
            {
              WriteLeoReg(ASIC_DATA, (i << 24) | (blockData[i] << 16));
              WriteLeoReg(ASIC_CMD, 0x001D0000);
              //Waste Time.
              while (ReadLeoReg(ASIC_DATA) != 0);
            }
        }
        else if (newbutton & B_BUTTON)
        {
            menumode = 0;
        }
        
        //Render
        setcolor(255,255,255);
        draw_puts("\f\n\n\n\n\n\n          64DD INTERNAL RAM DUMP\n    Press Z to dump / Press A to zero it / Press START to write\n    Press L to load from SRAM / R to save to SRAM\n\n    ");
        
        for (i = 1; i <= 256; i++)
        {
          sprintf(console_text, " %02X", blockData[(i - 1)]);
          draw_puts(console_text);
          if (i % 16 == 0)
             draw_puts("\n    ");
        }
        
        //quit
        if (menumode != 3)
        {
            clear_draw();
  
	    setcolor(0,255,0);
  	    draw_puts("\f\n\n    leotest\n    ----------------------------------------\n");
        }
    }
    else if (menumode == 4)
    {
    	//Button Management
    	if (newbutton & B_BUTTON)
        {
            menumode = 0;
        }
    
    	//quit
        if (menumode != 4)
        {
            clear_draw();
  
	    setcolor(0,255,0);
  	    draw_puts("\f\n\n    leotest\n    ----------------------------------------\n");
        }
    }
  }
}
