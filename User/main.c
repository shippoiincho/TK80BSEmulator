#include "debug.h"
#include "i8080.h"

#include "tk80rom.h"

#include "usb_host_config.h"

/* Global Variable */

uint8_t tk80ram[16384];
uint8_t tk80ppi[4];     // i8255 on TK80
uint8_t tk80keypad[4];  // keypad data on TK80
uint8_t bsvram[512];
uint8_t bsppi[4];       // i8255 on TK80BS
uint8_t basicmode=0;    // select BASIC rom Level 1 or 2

uint8_t kbmode=0;  // 0=tk80,1=bs
uint8_t kbmodify=0; // kana status
uint8_t lastkeycode=0;

#define NTSC_COUNT 3050 // = 63.56 us / 48MHz
#define NTSC_HSYNC 225  // =  4.7  us / 48MHz
#define NTSC_VSYNC 2825 // = NTSC_COUNT - NTSC_HSYNC
#define NTSC_SCAN_DELAY 0 // Delay for video signal generation
#define NTSC_SCAN_START 40 // Display start line

#define NTSC_X_PIXELS 256
#define NTSC_Y_PIXELS 192

#define NTSC_PRESCALER SPI_BaudRatePrescaler_16

#define NTSC_X_CHARS (NTSC_X_PIXELS/8)
#define NTSC_Y_CHARS (NTSC_Y_PIXELS/8)

#define USE_USB_KEYBOARD

// enable option rom and patch for COMPATIBLE ROM
// CAUTION: COMPATIBLE BASIC DOES NOT WORK WELL ON THIS EMULATOR
//#define USE_COMPATIBLE_ROM

volatile uint16_t ntsc_line;
volatile uint8_t ntsc_blank = 0;

uint8_t *scandata[2];

i8080 cpu;
volatile unsigned long cycle_count;
volatile uint8_t stepflag=0;
volatile uint8_t stepmode=0;
volatile uint8_t breakflag=0;

#define RX_BUFFER_LEN 64
volatile uint8_t rxbuff[RX_BUFFER_LEN];
uint8_t rxptr = 0;
uint32_t lastptr = RX_BUFFER_LEN;

const uint8_t tk80funckey[] = { 0x02 ,0x01, 0x40 , 0x80 , 0x04, 0x10 , 0x08 , 0x20 };

const uint8_t leddata[]={ 0x5c,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x27};

const uint8_t usbhidcode[] = {
        // 0x00
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
        0x41,0x00,0xc1,0x00,  // A
        0x42,0x00,0xba,0x00,  // B
        0x43,0x00,0xbf,0x00,  // C
        0x44,0x00,0xbc,0x00,  // D
        0x45,0x00,0xb2,0x00,  // E
        0x46,0x00,0xca,0x00,  // F
        0x47,0x00,0xb7,0x00,  // G
        0x48,0x00,0xb8,0x00,  // H
        0x49,0x00,0xc6,0x00,  // I
        0x4a,0x00,0xcf,0x00,  // J
        0x4b,0x00,0xc9,0x00,  // K
        0x4c,0x00,0xd8,0x00,  // L
        // 0x10
        0x4d,0x00,0xd3,0x00,  // M
        0x4e,0x00,0xd0,0x00,  // N
        0x4f,0x00,0xd7,0x00,  // O
        0x50,0x00,0xbe,0x00,  // P
        0x51,0x00,0xc0,0x00,  // Q
        0x52,0x00,0xbd,0x00,  // R
        0x53,0x00,0xc4,0x00,  // S
        0x54,0x00,0xb6,0x00,  // T
        0x55,0x00,0xc5,0x00,  // U
        0x56,0x00,0xcb,0x00,  // V
        0x57,0x00,0xc3,0x00,  // W
        0x58,0x00,0xbb,0x00,  // X
        0x59,0x00,0xcd,0x00,  // Y
        0x5a,0x00,0xd2,0x00,  // Z
        0x31,0x21,0xc7,0x00,  // 1
        0x32,0x22,0xcc,0x00,  // 2
        // 0x20
        0x33,0x23,0xb1,0xa7,  // 3
        0x34,0x24,0xb3,0xa9,  // 4
        0x35,0x25,0xb4,0xaa,  // 5
        0x36,0x26,0xb5,0xab,  // 6
        0x37,0x27,0xd4,0xac,  // 7
        0x38,0x28,0xd5,0xad,  // 8
        0x39,0x29,0xd6,0xae,  // 9
        0x30,0x00,0xdc,0xa6,  // 0
        0x0a,0x0a,0x0a,0x0a,  // Enter
        0x00,0x00,0x00,0x00,  // Escape
        0x08,0x08,0x08,0x08,  // Backspace
        0x00,0x00,0x00,0x00,  // Tab
        0x20,0x20,0x20,0x20,  // Space
        0x2d,0x3d,0xce,0x00,  // -
        0x7e,0x00,0xcd,0x00,  // ^
        0x40,0x00,0xde,0x00,  // @
        // 0x30
        0x5b,0x00,0xdf,0xa2,  // [
        0x5d,0x00,0xd1,0xa3,  // ]
        0x00,0x00,0x00,0x00,  //
        0x3b,0x2b,0xda,0x00,  // ;
        0x3a,0x2a,0xb9,0x00,  // :
        0x00,0x00,0x00,0x00,
        0x2c,0x3c,0xc8,0xa4,  // ,
        0x2e,0x3e,0xd9,0xa1,  // .
        0x2f,0x3f,0xd2,0xa5,  // /
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
        // 0x40
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
        0x7f,0x7f,0x7f,0x7f,  // Del
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
};

static inline uint8_t usart_getch() {

    uint8_t ch;
    uint32_t currptr;

    currptr = DMA_GetCurrDataCounter(DMA1_Channel6);

    if (currptr == lastptr) {
        return 0;
    }

    ch = rxbuff[rxptr];
    lastptr--;
    if (lastptr == 0) {
        lastptr = RX_BUFFER_LEN;
    }

    rxptr++;
    if (rxptr >= RX_BUFFER_LEN)
        rxptr = 0;

    return ch;

}

uint8_t memread(void *userdata, uint16_t addr) {

    uint32_t currptr;

    if (addr < 0x300) {
        return tk80rom[addr];
#ifdef USE_COMPATIBLE_ROM
    } else if ((addr >= 0x0c00) && (addr < 0x1a00)) {
        return bsext_rom[addr - 0xc00];
#endif
    } else if ((addr >= 0x7df8) && (addr < 0x7e00)) {

        switch (addr) {
        // BS USART

        case 0x7df8:
            return usart_getch();

        case 0x7df9:

            currptr = DMA_GetCurrDataCounter(DMA1_Channel6);

            if (currptr!=lastptr) {
                return 0x7;
            } else {
                return 0x5;
            }

        // BS PPI
        case 0x7dfc:
            bsppi[2]&=0xdf;
            return bsppi[0];
        case 0x7dfe:
            return bsppi[2];
        default:
            return 0;
        }
    } else if ((addr >= 0x7e00) && (addr < 0x8000)) {
        return bsvram[addr - 0x7e00];
    } else if ((addr >= 0x8000) && (addr < 0xc000)) {
        return tk80ram[addr - 0x8000];
    } else if ((addr >= 0xd000) && (addr < 0xf000)) {
#ifndef USE_COMPATIBLE_ROM
        if(basicmode==0) {
#endif
            return l2basic_rom[addr - 0xd000];
#ifndef USE_COMPATIBLE_ROM
        } else {
            if(addr>=0xe000) {
                return l1basic_rom[addr - 0xe000];
            } else {
                return 0;
            }
        }
#endif
    } else if (addr >= 0xf000) {
        return bsmonitor_rom[addr - 0xf000];
    }

    return 0;

}

void memwrite(void *userdata,uint16_t addr,uint8_t data){

    uint8_t bit;

    if((addr>=0x7df8)&&(addr<0x7e00)) {
        // BS USART
        switch(addr) {
        case 0x7df8:
            printf("%c",data);
            break;

        // BS PPI
        case 0x7dfe:
            bsppi[2]=data;
            break;
        case 0x7dff:
            if((data&0x80)==0) {
                 bit=1<<((data>>1)&7);
                 if((data&1)==0) {
                     bsppi[2]&= ~bit;
                 } else {
                     bsppi[2]|=bit;
                 }
             }

        }
    } else if((addr>=0x7e00)&&(addr<0x8000)) {
        bsvram[addr-0x7e00]=data;
    } else if((addr>=0x8000)&&(addr<0xc000)) {
        tk80ram[addr-0x8000]=data;
    }
}


uint8_t ioread(void *userdata,uint8_t addr){

    switch(addr&3) {
    case 0:

        if((tk80ppi[2]&0x10)==0) { return tk80keypad[0]; }
        if((tk80ppi[2]&0x20)==0) { return tk80keypad[1]; }
        if((tk80ppi[2]&0x40)==0) { return tk80keypad[2]; }

        return 0xff;

    case 2:
        return tk80ppi[2];

    default:
        return 0xff;

    }

    return 0xff;

}

void iowrite(void *userdata,uint8_t addr,uint8_t data){

    uint8_t bit;

    tk80ppi[addr&3]=data;
    if((addr&3)==2) {   // Port C
        if((data&2)==0) {  // Buzzer (bit 1)
            GPIO_WriteBit(GPIOB, GPIO_Pin_9, Bit_RESET);
        } else {
            GPIO_WriteBit(GPIOB, GPIO_Pin_9, Bit_SET);
        }
    }
    if((addr&3)==3) {   // Port C bit operation
        if((data&0x80)==0) {
            bit=1<<((data>>1)&7);
            if((data&1)==0) {
                tk80ppi[2]&= ~bit;
                if(bit==2) {   // Buzzer (bit 1)
                    GPIO_WriteBit(GPIOB, GPIO_Pin_9, Bit_RESET);
                }
            } else {
                tk80ppi[2]|=bit;
                if(bit==2) {  // Buzzer (bit 1)
                    GPIO_WriteBit(GPIOB, GPIO_Pin_9, Bit_SET);
                }
            }
        }

    }

}

// TVout for CH32V203

void video_init() {

    TIM_OCInitTypeDef TIM_OCInitStructure = { 0 };
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure = { 0 };
    GPIO_InitTypeDef GPIO_InitStructure = { 0 };
    SPI_InitTypeDef SPI_InitStructure = { 0 };
    NVIC_InitTypeDef NVIC_InitStructure = { 0 };

    RCC_APB2PeriphClockCmd(
    RCC_APB2Periph_GPIOA | RCC_APB2Periph_SPI1 | RCC_APB2Periph_TIM1, ENABLE);

    // PC11:Sync

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init( GPIOA, &GPIO_InitStructure);

    // PC7: Video (SPI1)

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init( GPIOA, &GPIO_InitStructure);

    // Initalize TIM1

    TIM_TimeBaseInitStructure.TIM_Period = NTSC_COUNT;
    TIM_TimeBaseInitStructure.TIM_Prescaler = 1;                // Presclaer = 0
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit( TIM1, &TIM_TimeBaseInitStructure);

    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM2;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = NTSC_HSYNC;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC4Init( TIM1, &TIM_OCInitStructure);

    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM2;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = NTSC_HSYNC * 2 - NTSC_SCAN_DELAY; // 9.4usec - delay
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC3Init( TIM1, &TIM_OCInitStructure);

    TIM_CtrlPWMOutputs(TIM1, ENABLE);
    TIM_OC3PreloadConfig( TIM1, TIM_OCPreload_Disable);
    TIM_OC4PreloadConfig( TIM1, TIM_OCPreload_Disable);
    TIM_ARRPreloadConfig( TIM1, ENABLE);
    TIM_Cmd( TIM1, ENABLE);

    // Initialize SPI1

    SPI_InitStructure.SPI_Direction = SPI_Direction_1Line_Tx;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_High;
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = NTSC_PRESCALER; // 6MHz
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial = 7;
    SPI_Init( SPI1, &SPI_InitStructure);

    SPI_I2S_DMACmd(SPI1, SPI_I2S_DMAReq_Tx, ENABLE);
    SPI_Cmd(SPI1, ENABLE);

    // NVIC

    NVIC_InitStructure.NVIC_IRQChannel = TIM1_CC_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    TIM_ITConfig(TIM1, TIM_IT_CC3, ENABLE);

    // Init VRAM

    scandata[0] = malloc(NTSC_X_CHARS + 1);
    scandata[1] = malloc(NTSC_X_CHARS + 1);

    scandata[0][NTSC_X_CHARS] = 0;
    scandata[1][NTSC_X_CHARS] = 0;

    //

}

void video_wait_vsync() {

    while(ntsc_blank==1);
    while(ntsc_blank==0);

}

//

void DMA_Tx_Init(DMA_Channel_TypeDef *DMA_CHx, u32 ppadr, u32 memadr,
        u16 bufsize) {
    DMA_InitTypeDef DMA_InitStructure = { 0 };

    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    DMA_DeInit(DMA_CHx);

    DMA_InitStructure.DMA_PeripheralBaseAddr = ppadr;
    DMA_InitStructure.DMA_MemoryBaseAddr = memadr;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;
    DMA_InitStructure.DMA_BufferSize = bufsize;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority = DMA_Priority_VeryHigh;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA_CHx, &DMA_InitStructure);

}


void TIM1_CC_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

void TIM1_CC_IRQHandler(void) {

    TIM_ClearFlag(TIM1, TIM_FLAG_CC3);
    uint8_t char_x, char_y, slice_y, ch, ddat;
#ifdef USE_USB_KEYBOARD
    uint8_t index;
    uint8_t hub_port;
    uint8_t intf_num, in_num;
#endif

    ntsc_line++;

    // VSYNC/HSYNC slection for next scanline

    if ((ntsc_line == 3) || (ntsc_line == 4) || (ntsc_line == 5)) { // VSYNC : ntsc_line : 4-6
        TIM_SetCompare4(TIM1, NTSC_VSYNC);
        //    TIM1->CH4CVR = NTSC_VSYNC;
    } else {
        TIM_SetCompare4(TIM1, NTSC_HSYNC);
        //    TIM1->CH4CVR = NTSC_HSYNC;
    }

    // Video Out

    if ((ntsc_line >= NTSC_SCAN_START)
            && (ntsc_line < (NTSC_SCAN_START + NTSC_Y_PIXELS))) { // video out
        ntsc_blank = 0;
        DMA_Tx_Init(DMA1_Channel3, (u32) (&SPI1->DATAR + 1),
                (u32) scandata[ntsc_line % 2], NTSC_X_CHARS + 1);
        DMA_Cmd(DMA1_Channel3, ENABLE);
    } else {
        ntsc_blank = 1;
    }

    // Redner fonts for next scanline

    if ((ntsc_line >= NTSC_SCAN_START - 1)
            && (ntsc_line < (NTSC_SCAN_START + NTSC_Y_PIXELS - 1))) {

        char_y = (ntsc_line + 1 - NTSC_SCAN_START) / 8;
        slice_y = (ntsc_line + 1 - NTSC_SCAN_START) % 8;

        if (char_y < 16) {

            for (char_x = 0; char_x < NTSC_X_CHARS; char_x++) {
                ch = bsvram[char_x + char_y * NTSC_X_CHARS];

                if((bsppi[2]&1)==0) {
                    ddat = bsfont_rom[ch * 8 + slice_y];
                } else {
                    ddat = bsfont_rom[2048 + ch * 8 + slice_y];
                }

                if((bsppi[2]&2)==0) {
                    ddat=~ddat;
                }

                scandata[(ntsc_line + 1) % 2][char_x] = ddat;

            }
        } else if(char_y==23) {
            // tk80 and keyboard status line

            if ((tk80ppi[2] & 0x80) != 0) {
                scandata[(ntsc_line + 1) % 2][0] =
                        tk80font[tk80ram[0x3f8 + 0] *8 + slice_y];
                scandata[(ntsc_line + 1) % 2][1] =
                        tk80font[tk80ram[0x3f8 + 1] *8 + slice_y];
                scandata[(ntsc_line + 1) % 2][2] =
                        tk80font[tk80ram[0x3f8 + 2] *8 + slice_y];
                scandata[(ntsc_line + 1) % 2][3] =
                        tk80font[tk80ram[0x3f8 + 3] *8 + slice_y];

                scandata[(ntsc_line + 1) % 2][5] =
                        tk80font[tk80ram[0x3f8 + 4] *8 + slice_y];
                scandata[(ntsc_line + 1) % 2][6] =
                        tk80font[tk80ram[0x3f8 + 5] *8 + slice_y];
                scandata[(ntsc_line + 1) % 2][7] =
                        tk80font[tk80ram[0x3f8 + 6] *8 + slice_y];
                scandata[(ntsc_line + 1) % 2][8] =
                        tk80font[tk80ram[0x3f8 + 7] *8 + slice_y];
            }

#ifndef USE_COMPATIBLE_ROM
            if(basicmode==0) {
                scandata[(ntsc_line + 1) % 2][19] = bsfont_rom[0x0c*8 + slice_y];
                scandata[(ntsc_line + 1) % 2][20] = bsfont_rom[0x32*8 + slice_y];
            } else {
                scandata[(ntsc_line + 1) % 2][19] = bsfont_rom[0x0c*8 + slice_y];
                scandata[(ntsc_line + 1) % 2][20] = bsfont_rom[0x31*8 + slice_y];
            }
#endif

            if(stepmode==1) {
                scandata[(ntsc_line + 1) % 2][22] = bsfont_rom[0x13*8 + slice_y];
                scandata[(ntsc_line + 1) % 2][23] = bsfont_rom[0x14*8 + slice_y];
                scandata[(ntsc_line + 1) % 2][24] = bsfont_rom[0x05*8 + slice_y];
                scandata[(ntsc_line + 1) % 2][25] = bsfont_rom[0x10*8 + slice_y];
            } else {
                scandata[(ntsc_line + 1) % 2][22] = bsfont_rom[0x01*8 + slice_y];
                scandata[(ntsc_line + 1) % 2][23] = bsfont_rom[0x15*8 + slice_y];
                scandata[(ntsc_line + 1) % 2][24] = bsfont_rom[0x14*8 + slice_y];
                scandata[(ntsc_line + 1) % 2][25] = bsfont_rom[0x0F*8 + slice_y];
            }

            if(kbmode==0) {
                // TK
                scandata[(ntsc_line + 1) % 2][27] = bsfont_rom[0x14*8 + slice_y];
                scandata[(ntsc_line + 1) % 2][28] = bsfont_rom[0x0b*8 + slice_y];
            } else {
                // BS
                scandata[(ntsc_line + 1) % 2][27] = bsfont_rom[0x02*8 + slice_y];
                scandata[(ntsc_line + 1) % 2][28] = bsfont_rom[0x13*8 + slice_y];
            }

            if((kbmode==1)&&(kbmodify==1)) {
                scandata[(ntsc_line + 1) % 2][30] = bsfont_rom[0x76*8 + slice_y];
                scandata[(ntsc_line + 1) % 2][31] = bsfont_rom[0x45*8 + slice_y];
            }

        } else {
            for (char_x = 0; char_x < NTSC_X_CHARS; char_x++) {
                scandata[(ntsc_line + 1) % 2][char_x] = 0;
            }
        }
    }

    if (ntsc_line > 262) {
        ntsc_line = 0;
    }

    cycle_count=cpu.cyc;
    if(stepmode==0) {
        while((cpu.cyc-cycle_count)<110) { //  63.5us / 2048kHz  = 130 cycles * wait (4/5)
            //        while((cpu.cyc-cycle_count)<1) { //  63.5us / 2048kHz  = 130 cycles * wait (4/5)

            i8080_step(&cpu);

        }
    } else {

        while((cpu.cyc-cycle_count)<10) {

            if(cpu.iff==1) {
                stepflag++;
                if(stepflag>2) {       // Interrupt (RST 7) next op fetch cycle
                    i8080_interrupt(&cpu, 0xff);
                }
            } else {
                stepflag=0;
            }
            i8080_step(&cpu);
        }
    }

#ifdef USE_USB_KEYBOARD
    /* USB HID Device Input Endpoint Timing */
    if( RootHubDev.bStatus >= ROOT_DEV_SUCCESS )
    {
        index = RootHubDev.DeviceIndex;
        if( RootHubDev.bType == USB_DEV_CLASS_HID )
        {
            for( intf_num = 0; intf_num < HostCtl[ index ].InterfaceNum; intf_num++ )
            {
                for( in_num = 0; in_num < HostCtl[ index ].Interface[ intf_num ].InEndpNum; in_num++ )
                {
                    HostCtl[ index ].Interface[ intf_num ].InEndpTimeCount[ in_num ]++;
                }
            }
        }
        else if( RootHubDev.bType == USB_DEV_CLASS_HUB )
        {
            HostCtl[ index ].Interface[ 0 ].InEndpTimeCount[ 0 ]++;
            for( hub_port = 0; hub_port < RootHubDev.bPortNum; hub_port++ )
            {
                if( RootHubDev.Device[ hub_port ].bStatus >= ROOT_DEV_SUCCESS )
                {
                    index = RootHubDev.Device[ hub_port ].DeviceIndex;

                    if( RootHubDev.Device[ hub_port ].bType == USB_DEV_CLASS_HID )
                    {
                        for( intf_num = 0; intf_num < HostCtl[ index ].InterfaceNum; intf_num++ )
                        {
                            for( in_num = 0; in_num < HostCtl[ index ].Interface[ intf_num ].InEndpNum; in_num++ )
                            {
                                HostCtl[ index ].Interface[ intf_num ].InEndpTimeCount[ in_num ]++;
                            }
                        }
                    }
                }
            }
        }
    }

#endif

}

#ifdef USE_USB_KEYBOARD

void usb_getkey( uint8_t index, uint8_t intf_num, uint8_t *pbuf, uint16_t len ) {

        uint8_t  i;
        uint8_t  value;
        uint8_t  bit_pos = 0x00;
        uint8_t  modifyer=0;
        uint8_t  kbshift;
        uint8_t  keycode;
        uint8_t  jiscode;

//        for(i=0;i<len;i++) {
//            printf("%x ",pbuf[i]);
//        }
//        printf("\n\r");
//        key_break_check=0;
        breakflag=0;

        if ((len == 8) && (pbuf[1] != 0x1)) {

            modifyer=pbuf[0];

            tk80keypad[0]=255;
            tk80keypad[1]=255;
            tk80keypad[2]=255;
            tk80keypad[3]=255;

            if (kbmode == 0) {
                // TK80 mode
                // Check all keycode

                bsppi[0]=0;
                bsppi[2]&=0xdf;

                for(int i=2;i<8;i++)  {

                    keycode = pbuf[i];

                    if((keycode>=0x04)&&(keycode<0x0a)) { // A..F
                        bit_pos=1<<(keycode-0x02);
                        tk80keypad[1]&= ~bit_pos;
                    }
                    if((keycode>=0x1e)&&(keycode<0x25)) { // 1..7
                        bit_pos=1<<(keycode-0x1d);
                        tk80keypad[0]&= ~bit_pos;
                    }

                    if(keycode==0x25) { // 8
                        tk80keypad[1]&= 0xfe;
                    }

                    if(keycode==0x26) { // 9
                        tk80keypad[1]&= 0xfd;
                    }

                    if(keycode==0x27) { // 0
                        tk80keypad[0]&= 0xfe;
                    }

                    if((keycode>=0x3a)&&(keycode<0x42)) { // Function Keys
                        tk80keypad[2]&= ~tk80funckey[keycode-0x3a];
                    }

                    if(keycode==0x45) { // F12=reset
                        cpu.pc=0;
                    }

                    if(keycode==0x44) { // F11=AUTO/STEP toggle
                        if(stepmode==0) {
                            stepmode=1;
                        } else {
                            stepmode=0;
                        }
                    }

                    if(keycode==0x42) { // F9=L2/L1 BASIC ROM toggle
                          if(basicmode==0) {
                              basicmode=1;
                          } else {
                              basicmode=0;
                          }
                      }

                    if(keycode==0x29) { // Escape
                        kbmode=1;
                    }
                }

            } else {
                // BS mode
                // Check only first keycode

                bsppi[0]=0;
                bsppi[2]&=0xdf;

                keycode = pbuf[2];
                if (keycode != 0) {

                    if((modifyer & 0x22)!=0) {
                        kbshift=1;
                    } else {
                        kbshift=0;
                    }

                    if((keycode>=0x3a)&&(keycode<0x42)) { // Function Keys as TK-80 Pad

                        tk80keypad[2]&= ~tk80funckey[keycode-0x3a];

                    } else if(keycode<0x50) {
                        jiscode=usbhidcode[keycode*4 + kbmodify *2 + kbshift];

                        if(keycode!=lastkeycode) {
                            if(keycode==0x48) {  // Break

                                if(cpu.iff==1) {
                                    i8080_interrupt(&cpu, 0xff);  // RST38
                                }
                                lastkeycode=keycode;

                            } else if(jiscode!=0) {
                                bsppi[0]=jiscode;
                                bsppi[2]|=0x20;
                                lastkeycode=keycode;
                            }
                        }
                    }

//                    if(keycode==0x45) { // F12=reset
//                        cpu.pc=0;
//                    }

                    if(keycode==0x88) { // Hira/Kata
                        if(keycode!=lastkeycode) {
                            if(kbmodify==0) {
                                kbmodify=1;
                            } else {
                                kbmodify=0;
                            }
                            lastkeycode=keycode;
                        }
                    }

                    if(keycode==0x89) { // Yen
                        if(kbshift==0) {
                            if(keycode!=lastkeycode) {
                                if(kbmodify==0) {
                                    bsppi[0]=0x5c;
                                } else {
                                    bsppi[0]=0xb0;
                                }
                                bsppi[2]|=0x20;
                                lastkeycode=keycode;
                            }
                        }
                    }

                    if(keycode==0x87) { // Backslash
                         if(kbshift==0) {
                             if(keycode!=lastkeycode) {
                                 if(kbmodify==1) {
                                     bsppi[0]=0xdb;
                                     bsppi[2]|=0x20;
                                 }
                                 lastkeycode=keycode;
                             }
                         }
                     }

                    if(keycode==0x29) { // Escape
                        kbmode=0;
                    }

                } else {
                    lastkeycode=0;
                }
            }
        }


        value = HostCtl[ index ].Interface[ intf_num ].SetReport_Value;

        for( i = HostCtl[ index ].Interface[ intf_num ].LED_Usage_Min; i <= HostCtl[ index ].Interface[ intf_num ].LED_Usage_Max; i++ )
        {
            if( i == 0x01 )
            {
                if( memchr( pbuf, DEF_KEY_NUM, len ) )
                {
                    HostCtl[ index ].Interface[ intf_num ].SetReport_Value ^= ( 1 << bit_pos );
                }
            }
            else if( i == 0x02 )
            {
                if( memchr( pbuf, DEF_KEY_CAPS, len ) )
                {
                    HostCtl[ index ].Interface[ intf_num ].SetReport_Value ^= ( 1 << bit_pos );
                }
            }
            else if( i == 0x03 )
            {
                if( memchr( pbuf, DEF_KEY_SCROLL, len ) )
                {
                    HostCtl[ index ].Interface[ intf_num ].SetReport_Value ^= ( 1 << bit_pos );
                }
            }

            bit_pos++;
        }

        if( value != HostCtl[ index ].Interface[ intf_num ].SetReport_Value )
        {
            HostCtl[ index ].Interface[ intf_num ].SetReport_Flag = 1;
        }
        else
        {
            HostCtl[ index ].Interface[ intf_num ].SetReport_Flag = 0;
        }

}

/*********************************************************************
 * @fn      USBH_MainDeal
 *
 * @brief   Provide a simple enumeration process for USB devices and
 *          obtain keyboard and mouse data at regular intervals.
 *
 * @return  none
 */
void usb_keyboard( void )
{
    uint8_t  s;
    uint8_t  index;
    uint8_t  hub_port;
    uint8_t  hub_dat;
    uint8_t  intf_num, in_num;
    uint16_t len;
#if DEF_DEBUG_PRINTF
    uint16_t i;
#endif

    s = USBFSH_CheckRootHubPortStatus( RootHubDev.bStatus ); // Check USB device connection or disconnection
    if( s == ROOT_DEV_CONNECTED )
    {
        DUG_PRINTF( "USB Port Dev In.\r\n" );

        /* Set root device state parameters */
        RootHubDev.bStatus = ROOT_DEV_CONNECTED;
        RootHubDev.DeviceIndex = DEF_USBFS_PORT_INDEX * DEF_ONE_USB_SUP_DEV_TOTAL;

        s = USBH_EnumRootDevice( ); // Simply enumerate root device
        if( s == ERR_SUCCESS )
        {
            if( RootHubDev.bType == USB_DEV_CLASS_HID ) // Further enumerate it if this device is a HID device
            {
                DUG_PRINTF("Root Device Is HID. ");

                s = USBH_EnumHidDevice( RootHubDev.DeviceIndex, RootHubDev.bEp0MaxPks );
                DUG_PRINTF( "Further Enum Result: " );
                if( s == ERR_SUCCESS )
                {
                    DUG_PRINTF( "OK\r\n" );

                    /* Set the connection status of the device  */
                    RootHubDev.bStatus = ROOT_DEV_SUCCESS;
                }
                else if( s != ERR_USB_DISCON )
                {
                    DUG_PRINTF( "Err(%02x)\r\n", s );

                    RootHubDev.bStatus = ROOT_DEV_FAILED;
                }
            }
            else if( RootHubDev.bType == USB_DEV_CLASS_HUB )
            {
                DUG_PRINTF("Root Device Is HUB. ");

                s = USBH_EnumHubDevice( );
                DUG_PRINTF( "Further Enum Result: " );
                if( s == ERR_SUCCESS )
                {
                    DUG_PRINTF( "OK\r\n" );

                    /* Set the connection status of the device  */
                    RootHubDev.bStatus = ROOT_DEV_SUCCESS;
                }
                else if( s != ERR_USB_DISCON )
                {
                    DUG_PRINTF( "Err(%02x)\r\n", s );

                    RootHubDev.bStatus = ROOT_DEV_FAILED;
                }
            }
            else // Detect that this device is a NON-HID device
            {
                DUG_PRINTF( "Root Device Is " );
                switch( RootHubDev.bType )
                {
                    case USB_DEV_CLASS_STORAGE:
                        DUG_PRINTF("Storage. ");
                        break;
                    case USB_DEV_CLASS_PRINTER:
                        DUG_PRINTF("Printer. ");
                        break;
                    case DEF_DEV_TYPE_UNKNOWN:
                        DUG_PRINTF("Unknown. ");
                        break;
                }
                DUG_PRINTF( "End Enum.\r\n" );

                RootHubDev.bStatus = ROOT_DEV_SUCCESS;
            }
        }
        else if( s != ERR_USB_DISCON )
        {
            /* Enumeration failed */
            DUG_PRINTF( "Enum Fail with Error Code:%x\r\n",s );
            RootHubDev.bStatus = ROOT_DEV_FAILED;
        }
    }
    else if( s == ROOT_DEV_DISCONNECT )
    {
        DUG_PRINTF( "USB Port Dev Out.\r\n" );

        /* Clear parameters */
        index = RootHubDev.DeviceIndex;
        memset( &RootHubDev.bStatus, 0, sizeof( ROOT_HUB_DEVICE ) );
        memset( &HostCtl[ index ].InterfaceNum, 0, sizeof( HOST_CTL ) );
    }

    /* Get the data of the HID device connected to the USB host port */
    if( RootHubDev.bStatus >= ROOT_DEV_SUCCESS )
    {
        index = RootHubDev.DeviceIndex;

        if( RootHubDev.bType == USB_DEV_CLASS_HID )
        {
            for( intf_num = 0; intf_num < HostCtl[ index ].InterfaceNum; intf_num++ )
            {
                for( in_num = 0; in_num < HostCtl[ index ].Interface[ intf_num ].InEndpNum; in_num++ )
                {
                    /* Get endpoint data based on the interval time of the device */
                    if( HostCtl[ index ].Interface[ intf_num ].InEndpTimeCount[ in_num ] >= HostCtl[ index ].Interface[ intf_num ].InEndpInterval[ in_num ] )
                    {
                        HostCtl[ index ].Interface[ intf_num ].InEndpTimeCount[ in_num ] %= HostCtl[ index ].Interface[ intf_num ].InEndpInterval[ in_num ];

                        /* Get endpoint data */
                        s = USBFSH_GetEndpData( HostCtl[ index ].Interface[ intf_num ].InEndpAddr[ in_num ],
                                                &HostCtl[ index ].Interface[ intf_num ].InEndpTog[ in_num ], Com_Buf, &len );
                        if( s == ERR_SUCCESS )
                        {
#if DEF_DEBUG_PRINTF
                            for( i = 0; i < len; i++ )
                            {
                                DUG_PRINTF( "%02x ", Com_Buf[ i ] );
                            }
                            DUG_PRINTF( "\r\n" );
#endif

                            /* Handle keyboard lighting */
                            if( HostCtl[ index ].Interface[ intf_num ].Type == DEC_KEY )
                            {
//                                KB_AnalyzeKeyValue( index, intf_num, Com_Buf, len );
                                usb_getkey( index, intf_num, Com_Buf, len );

                                if( HostCtl[ index ].Interface[ intf_num ].SetReport_Flag )
                                {
                                    KB_SetReport( index, RootHubDev.bEp0MaxPks, intf_num );
                                }
                            }
                        }
                        else if( s == ERR_USB_DISCON )
                        {
                            break;
                        }
                        else if( s == ( USB_PID_STALL | ERR_USB_TRANSFER ) )
                        {
                            /* USB device abnormal event */
                            DUG_PRINTF("Abnormal\r\n");

                            /* Clear endpoint */
                            USBFSH_ClearEndpStall( RootHubDev.bEp0MaxPks, HostCtl[ index ].Interface[ intf_num ].InEndpAddr[ in_num ] | 0x80 );
                            HostCtl[ index ].Interface[ intf_num ].InEndpTog[ in_num ] = 0x00;

                            /* Judge the number of error */
                            HostCtl[ index ].ErrorCount++;
                            if( HostCtl[ index ].ErrorCount >= 10 )
                            {
                                /* Re-enumerate the device and clear the endpoint again */
                                memset( &RootHubDev.bStatus, 0, sizeof( struct _ROOT_HUB_DEVICE ) );
                                s = USBH_EnumRootDevice( );
                                if( s == ERR_SUCCESS )
                                {
                                    USBFSH_ClearEndpStall( RootHubDev.bEp0MaxPks, HostCtl[ index ].Interface[ intf_num ].InEndpAddr[ in_num ] | 0x80 );
                                    HostCtl[ index ].ErrorCount = 0x00;

                                    RootHubDev.bStatus = ROOT_DEV_CONNECTED;
                                    RootHubDev.DeviceIndex = DEF_USBFS_PORT_INDEX * DEF_ONE_USB_SUP_DEV_TOTAL;

                                    memset( &HostCtl[ index ].InterfaceNum, 0, sizeof( struct __HOST_CTL ) );
                                    s = USBH_EnumHidDevice( index, RootHubDev.bEp0MaxPks );
                                    if( s == ERR_SUCCESS )
                                    {
                                        RootHubDev.bStatus = ROOT_DEV_SUCCESS;
                                    }
                                    else if( s != ERR_USB_DISCON )
                                    {
                                        RootHubDev.bStatus = ROOT_DEV_FAILED;
                                    }
                                }
                                else if( s != ERR_USB_DISCON )
                                {
                                    RootHubDev.bStatus = ROOT_DEV_FAILED;
                                }
                            }
                        }
                    }
                }

                if( s == ERR_USB_DISCON )
                {
                    break;
                }
            }
        }
        else if( RootHubDev.bType == USB_DEV_CLASS_HUB )
        {
           /* Query port status change */
           if( HostCtl[ index ].Interface[ 0 ].InEndpTimeCount[ 0 ] >= HostCtl[ index ].Interface[ 0 ].InEndpInterval[ 0 ] )
           {
               HostCtl[ index ].Interface[ 0 ].InEndpTimeCount[ 0 ] %= HostCtl[ index ].Interface[ 0 ].InEndpInterval[ 0 ];

               /* Select HUB port */
               USBFSH_SetSelfAddr( RootHubDev.bAddress );
               USBFSH_SetSelfSpeed( RootHubDev.bSpeed );

               /* Get HUB interrupt endpoint data */
               s = USBFSH_GetEndpData( HostCtl[ index ].Interface[ 0 ].InEndpAddr[ 0 ], &HostCtl[ index ].Interface[ 0 ].InEndpTog[ 0 ], Com_Buf, &len );
               if( s == ERR_SUCCESS )
               {
                   hub_dat = Com_Buf[ 0 ];
                   DUG_PRINTF( "Hub Int Data:%02x\r\n", hub_dat );

                   for( hub_port = 0; hub_port < RootHubDev.bPortNum; hub_port++ )
                   {
                       /* HUB Port PreEnumate Step 1: C_PORT_CONNECTION */
                       s = HUB_Port_PreEnum1( ( hub_port + 1 ), &hub_dat );
                       if( s == ERR_USB_DISCON )
                       {
                           hub_dat &= ~( 1 << ( hub_port + 1 ) );

                           /* Clear parameters */
                           memset( &HostCtl[ RootHubDev.Device[ hub_port ].DeviceIndex ], 0, sizeof( HOST_CTL ) );
                           memset( &RootHubDev.Device[ hub_port ].bStatus, 0, sizeof( HUB_DEVICE ) );
                           continue;
                       }

                       /* HUB Port PreEnumate Step 2: Set/Clear PORT_RESET */
                       Delay_Ms( 100 );  //
                       s = HUB_Port_PreEnum2( ( hub_port + 1 ), &hub_dat );
                       if( s == ERR_USB_CONNECT )
                       {
                           /* Set parameters */
                           RootHubDev.Device[ hub_port ].bStatus = ROOT_DEV_CONNECTED;
                           RootHubDev.Device[ hub_port ].bEp0MaxPks = DEFAULT_ENDP0_SIZE;
                           RootHubDev.Device[ hub_port ].DeviceIndex = DEF_USBFS_PORT_INDEX * DEF_ONE_USB_SUP_DEV_TOTAL + hub_port + 1;
                       }
                       else
                       {
                           hub_dat &= ~( 1 << ( hub_port + 1 ) );
                       }

                       /* Enumerate HUB Device */
                       if( RootHubDev.Device[ hub_port ].bStatus == ROOT_DEV_CONNECTED )
                       {
                           /* Check device speed */
                           RootHubDev.Device[ hub_port ].bSpeed = HUB_CheckPortSpeed( ( hub_port + 1 ), Com_Buf );
                           DUG_PRINTF( "Dev Speed:%x\r\n", RootHubDev.Device[ hub_port ].bSpeed );

                           /* Select the specified port */
                           USBFSH_SetSelfAddr( RootHubDev.Device[ hub_port ].bAddress );
                           USBFSH_SetSelfSpeed( RootHubDev.Device[ hub_port ].bSpeed );
                           if( RootHubDev.bSpeed != USB_LOW_SPEED )
                           {
                               USBOTG_H_FS->HOST_CTRL &= ~USBFS_UH_LOW_SPEED;
                           }

                           /* Enumerate the USB device of the current HUB port */
                           DUG_PRINTF("Enum_HubDevice\r\n");
                           s = USBH_EnumHubPortDevice( hub_port, &RootHubDev.Device[ hub_port ].bAddress, \
                                                       &RootHubDev.Device[ hub_port ].bType );
                           if( s == ERR_SUCCESS )
                           {
                               if( RootHubDev.Device[ hub_port ].bType == USB_DEV_CLASS_HID )
                               {
                                   DUG_PRINTF( "HUB port%x device is HID! Further Enum:\r\n", hub_port );

                                   /* Perform HID class enumeration on the current device */
                                   s = USBH_EnumHidDevice( RootHubDev.Device[ hub_port ].DeviceIndex, \
                                                           RootHubDev.Device[ hub_port ].bEp0MaxPks );
                                   if( s == ERR_SUCCESS )
                                   {
                                       RootHubDev.Device[ hub_port ].bStatus = ROOT_DEV_SUCCESS;
                                       DUG_PRINTF( "OK!\r\n" );
                                   }
                               }
                               else // Detect that this device is a Non-HID device
                               {
                                   DUG_PRINTF( "HUB port%x device is ", hub_port );
                                   switch( RootHubDev.Device[ hub_port ].bType )
                                   {
                                       case USB_DEV_CLASS_STORAGE:
                                           DUG_PRINTF("storage!\r\n");
                                           break;
                                       case USB_DEV_CLASS_PRINTER:
                                           DUG_PRINTF("printer!\r\n");
                                           break;
                                       case USB_DEV_CLASS_HUB:
                                           DUG_PRINTF("printer!\r\n");
                                           break;
                                       case DEF_DEV_TYPE_UNKNOWN:
                                           DUG_PRINTF("unknown!\r\n");
                                           break;
                                   }
                                   RootHubDev.Device[ hub_port ].bStatus = ROOT_DEV_SUCCESS;
                               }
                           }
                           else
                           {
                               RootHubDev.Device[ hub_port ].bStatus = ROOT_DEV_FAILED;
                               DUG_PRINTF( "HUB Port%x Enum Err!\r\n", hub_port );
                           }
                       }
                   }
               }
           }

           /* Get HUB port HID device data */
           for( hub_port = 0; hub_port < RootHubDev.bPortNum; hub_port++ )
           {
               if( RootHubDev.Device[ hub_port ].bStatus == ROOT_DEV_SUCCESS )
               {
                   index = RootHubDev.Device[ hub_port ].DeviceIndex;

                   if( RootHubDev.Device[ hub_port ].bType == USB_DEV_CLASS_HID )
                   {
                       for( intf_num = 0; intf_num < HostCtl[ index ].InterfaceNum; intf_num++ )
                       {
                           for( in_num = 0; in_num < HostCtl[ index ].Interface[ intf_num ].InEndpNum; in_num++ )
                           {
                               /* Get endpoint data based on the interval time of the device */
                               if( HostCtl[ index ].Interface[ intf_num ].InEndpTimeCount[ in_num ] >= HostCtl[ index ].Interface[ intf_num ].InEndpInterval[ in_num ] )
                               {
                                   HostCtl[ index ].Interface[ intf_num ].InEndpTimeCount[ in_num ] %= HostCtl[ index ].Interface[ intf_num ].InEndpInterval[ in_num ];

                                   /* Select HUB device port */
                                   USBFSH_SetSelfAddr( RootHubDev.Device[ hub_port ].bAddress );
                                   USBFSH_SetSelfSpeed( RootHubDev.Device[ hub_port ].bSpeed );
                                   if( RootHubDev.bSpeed != USB_LOW_SPEED )
                                   {
                                       USBOTG_H_FS->HOST_CTRL &= ~USBFS_UH_LOW_SPEED;
                                   }

                                   /* Get endpoint data */
                                   s = USBFSH_GetEndpData( HostCtl[ index ].Interface[ intf_num ].InEndpAddr[ in_num ], \
                                                           &HostCtl[ index ].Interface[ intf_num ].InEndpTog[ in_num ], Com_Buf, &len );
                                   if( s == ERR_SUCCESS )
                                   {
#if DEF_DEBUG_PRINTF
                                       for( i = 0; i < len; i++ )
                                       {
                                           DUG_PRINTF( "%02x ", Com_Buf[ i ] );
                                       }
                                       DUG_PRINTF( "\r\n" );
#endif

                                       if( HostCtl[ index ].Interface[ intf_num ].Type == DEC_KEY )
                                       {
//                                           KB_AnalyzeKeyValue( index, intf_num, Com_Buf, len );
                                           usb_getkey( index, intf_num, Com_Buf, len );

                                           if( HostCtl[ index ].Interface[ intf_num ].SetReport_Flag )
                                           {
                                               KB_SetReport( index, RootHubDev.Device[ hub_port ].bEp0MaxPks, intf_num );
                                           }
                                       }
                                   }
                                   else if( s == ERR_USB_DISCON )
                                   {
                                       break;
                                   }
                               }
                           }

                           if( s == ERR_USB_DISCON )
                           {
                               break;
                           }
                       }
                   }
               }
           }
        }
    }
}
#endif

void USART_CFG(void) {
    GPIO_InitTypeDef GPIO_InitStructure = { 0 };
    USART_InitTypeDef USART_InitStructure = { 0 };

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    /* USART2 TX-->A.2   RX-->A.3 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate = 115200;
//    USART_InitStructure.USART_BaudRate = 1200;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl =
    USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;

    USART_Init(USART2, &USART_InitStructure);
    USART_DMACmd(USART2, USART_DMAReq_Rx, ENABLE);
    USART_Cmd(USART2, ENABLE);

}


void DMA_Rx_Init(DMA_Channel_TypeDef *DMA_CHx, u32 ppadr, u32 memadr,
        u16 bufsize) {
    DMA_InitTypeDef DMA_InitStructure = { 0 };

    RCC_AHBPeriphClockCmd( RCC_AHBPeriph_DMA1, ENABLE);

    DMA_DeInit(DMA_CHx);

    DMA_InitStructure.DMA_PeripheralBaseAddr = ppadr;
    DMA_InitStructure.DMA_MemoryBaseAddr = memadr;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
    DMA_InitStructure.DMA_BufferSize = bufsize;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
    DMA_InitStructure.DMA_Priority = DMA_Priority_VeryHigh;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA_CHx, &DMA_InitStructure);

    DMA_Cmd(DMA_CHx, ENABLE);

}



void beep_init(void) {
    GPIO_InitTypeDef GPIO_InitStructure = { 0 };

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

}

/*********************************************************************
 * @fn      main
 *
 * @brief   Main program.
 *
 * @return  none
 */
int main(void)
{

    uint32_t stepflag=0;

    // Init emulator

#ifdef USE_COMPATIBLE_ROM
    // Patch for TK-80BS compatible basic
    tk80ram[0x3dd]=0xc3;
    tk80ram[0x3de]=0x51;
    tk80ram[0x3df]=0x01;

    tk80ram[0x804]=0x1a;
    tk80ram[0x805]=0xbf;
#endif

    i8080_init(&cpu);

    cpu.read_byte = memread;
    cpu.write_byte = memwrite;
    cpu.port_in = ioread;
    cpu.port_out = iowrite;

    // run Systick timer

    SysTick->CNT = 0;
    SysTick->CTLR |= (1 << 0);

    Delay_Init();

//  Peripheral setup

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

    USART_CFG();
//
    DMA_Rx_Init( DMA1_Channel6, (u32) &USART2->DATAR, (u32) &rxbuff,
    RX_BUFFER_LEN);

#ifdef USE_USB_KEYBOARD
    USBFS_RCC_Init( );
    USBFS_Host_Init( ENABLE );
    memset( &RootHubDev.bStatus, 0, sizeof( ROOT_HUB_DEVICE ) );
    memset( &HostCtl[ DEF_USBFS_PORT_INDEX * DEF_ONE_USB_SUP_DEV_TOTAL ].InterfaceNum, 0, DEF_ONE_USB_SUP_DEV_TOTAL * sizeof( HOST_CTL ) );
#endif

    beep_init();

    video_init();

//      USART_Printf_Init(115200);
//      printf("SystemClk:%d\r\n", SystemCoreClock);
//      printf("ChipID:%08x\r\n", DBGMCU_GetCHIPID());

//#ifdef DEBUG_TK80

      // DEBUG

//      for(int i=0;i<TK80EXAMPLE1_BYTES;i++) {
//          tk80ram[0x200+i]=tk80example1[i];
//      }

//      for(int i=0;i<TK80EXAMPLE4_BYTES;i++) {
//          tk80ram[0x200+i]=tk80example4[i];
//      }

//#endif

    while(1)
    {

        video_wait_vsync();

        usb_keyboard();

    }
}
