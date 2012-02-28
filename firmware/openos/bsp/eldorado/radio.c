/**
\brief Eldorado-specific definition of the "radio" bsp module.

\author Fabien Chraim <chraim@eecs.berkeley.edu>, February 2012.
*/

#include "PE_Types.h"
#include "string.h"
#include "radio.h"
#include "eldorado.h"
#include "spi.h"
#include "radiotimer.h"

//=========================== defines =========================================

//=========================== variables =======================================

typedef struct {
   radiotimer_capture_cbt    startFrameCb;
   radiotimer_capture_cbt    endFrameCb;
} radio_vars_t;

radio_vars_t radio_vars;

//=========================== prototypes ======================================

void    radio_spiWriteReg(uint8_t reg_addr, uint16_t reg_setting);
uint16_t radio_spiReadReg(uint8_t reg_addr);
void    radio_spiWriteTxFifo(uint8_t* bufToWrite, uint8_t lenToWrite);
void    radio_spiReadRxFifo(uint8_t* bufRead, uint8_t length);

//=========================== public ==========================================

void radio_init() {
   //start fabien code:
   MC13192_RESET_PULLUP = 0;//pullup resistor on the RST pin
   MC13192_RESET_PORT = 1;//specify direction for RST pin
   MC13192_CE = 1;  //chip enable (SPI)   
   MC13192_ATTN = 1; //modem attenuation (used to send it to sleep mode for example)
   MC13192_RTXEN = 0;//to enable RX or TX
   MC13192_CE_PORT = 1;
   MC13192_ATTN_PORT = 1;
   MC13192_RTXEN_PORT = 1;

   MC13192_RESET = 1;//turn radio ON
   
   
   //PA: put the external PA in bypass mode:
    EN_PA1_DIR = 0;
    EN_PA2_DIR = 0;
    EN_PA1 = 0;
    EN_PA2 = 1;
   
   //init routine:
	/* Please refer to document MC13192RM for hidden register initialization */
   SPIDrvWrite(0x11,0x20FF);   /* Eliminate Unlock Conditions due to L01 */
   SPIDrvWrite(0x1B,0x8000);   /* Disable TC1. */
   SPIDrvWrite(0x1D,0x8000);   /* Disable TC2. */
   SPIDrvWrite(0x1F,0x8000);   /* Disable TC3. */
   SPIDrvWrite(0x21,0x8000);   /* Disable TC4. */
   SPIDrvWrite(0x07,0x0E00);   /* Enable CLKo in Doze */
   SPIDrvWrite(0x0C,0x0300);   /* IRQ pull-up disable. */
   (void)SPIDrvRead(0x25);           /* Sets the reset indicator bit */
   SPIDrvWrite(0x04,0xA08D);   /* New cal value */
   SPIDrvWrite(0x08,0xFFF7);   /* Preferred injection */
   SPIDrvWrite(0x05,0x8351);   /* Acoma, TC1, Doze, ATTN masks, LO1, CRC */
   SPIDrvWrite(0x06,0x4720);   /* CCA, TX, RX, energy detect */
 
    /* Read the status register to clear any undesired IRQs. */
   (void)SPIDrvRead(0x24);           /* Clear the status register, if set */
   gu8RTxMode = IDLE_MODE;     /* Update global to reflect MC13192 status */
	
	//end init routine
   
   //end fabien code

   // clear variables
   memset(&radio_vars,0,sizeof(radio_vars_t));
   
 

   //busy wait until radio status is TRX_OFF
   while((radio_spiReadReg(RG_TRX_STATUS) & 0x1F) != TRX_OFF);
   
   //radiotimer_start(0xffff);//poipoi
}

void radio_startTimer(uint16_t period) {
   radiotimer_start(period);
}

void radio_setOverflowCb(radiotimer_compare_cbt cb) {
   radiotimer_setOverflowCb(cb);
}

void radio_setCompareCb(radiotimer_compare_cbt cb) {
   radiotimer_setCompareCb(cb);
}

void radio_setStartFrameCb(radiotimer_capture_cbt cb) {
   radio_vars.startFrameCb = cb;
}

void radio_setEndFrameCb(radiotimer_capture_cbt cb) {
   radio_vars.endFrameCb = cb;
}

void radio_reset() {
   PTDD_PTDD3 = 0;
   PTDD_PTDD3 = 1;//goes back to idle mode
}

void radio_setFrequency(uint8_t frequency) {
   // configure the radio to the right frequecy
   radio_spiWriteReg(LO1_IDIV_ADDR,0x0f95);
   radio_spiWriteReg(LO1_NUM_ADDR,frequency-11);//in this radio they index the channels from 0 to 15
}

void radio_rfOn() {
   //poipoi
   //to leave doze mode, assert ATTN then deassert it
   MC13192_ATTN = 0;
   MC13192_ATTN = 1;
}

void radio_loadPacket(uint8_t* packet, uint8_t len) {
   // load packet in TXFIFO
   radio_spiWriteTxFifo(packet,len);
}

void radio_txEnable() {
   // turn on radio's PLL
     //read status reg
   uint16_t stat_reg = radio_spiReadReg(MODE_ADDR);
   stat_reg &= 0xfff8;
   stat_reg |= TX_MODE;
   //turn off LNA
   MC13192_LNA_CTRL = LNA_OFF;
   //turn on PA
   MC13192_PA_CTRL = PA_ON;
   // put radio in reception mode
   radio_spiWriteReg(MODE_ADDR, stat_reg);
   while((radio_spiReadReg(STATUS_ADDR) & 0x8000)); // busy wait until pll locks
}

void radio_txNow() {
   // send packet by assterting the RTXEN pin
   MC13192_RTXEN = 1;
   
   
   // The AT86RF231 does not generate an interrupt when the radio transmits the
   // SFD, which messes up the MAC state machine. The danger is that, if we leave
   // this funtion like this, any radio watchdog timer will expire.
   // Instead, we cheat an mimick a start of frame event by calling
   // ieee154e_startOfFrame from here. This also means that software can never catch
   // a radio glitch by which #radio_txEnable would not be followed by a packet being
   // transmitted (I've never seen that).
   //poipoiieee154e_startOfFrame(ieee154etimer_getCapturedTime());
}

void radio_rxEnable() {
   //read status reg
   uint16_t stat_reg = radio_spiReadReg(MODE_ADDR);
   stat_reg &= 0xfff8;
   stat_reg |= RX_MODE;
   //turn on LNA
   MC13192_LNA_CTRL = LNA_ON;
   //turn off PA
   MC13192_PA_CTRL = PA_OFF;
   // put radio in reception mode
   radio_spiWriteReg(MODE_ADDR, stat_reg);
   MC13192_RTXEN = 1;//assert the signal to put radio in rx mode.
   while((radio_spiReadReg(STATUS_ADDR) & 0x8000)); // busy wait until pll locks
}

void radio_rxNow() {
   // nothing to do
}

void radio_getReceivedFrame(uint8_t* bufRead, uint8_t* lenRead, uint8_t maxBufLen) {
   uint8_t len;
   uint8_t junk;
   len = radio_spiReadReg(RX_PKT_LEN);//Read the RX packet length 
   len &= 0x007F;          /* Mask out all but the RX packet length */
   junk = SPI1S;
   junk = SPI1D;//as specified in the generated radio code
  
   if (len>2 && len<=127) {
      // retrieve the whole packet (including 1B SPI address, 1B length, the packet, 1B LQI)
      radio_spiReadRxFifo(bufRead,len);
   }
}

void radio_rfOff() {// turn radio off
   uint16_t stat_reg;
   
   MC13192_RTXEN = 0;//go back to idle
   /* The state to enter is "Acoma Doze Mode" because CLKO is made available at any setting
   //now put the radio in idle mode
   //read status reg
   stat_reg = radio_spiReadReg(MODE_ADDR);
   stat_reg &= 0xfff8;
   stat_reg |= IDLE_MODE;
   radio_spiWriteReg(MODE_ADDR, stat_reg);  
   //turn PA off
   MC13192_PA_CTRL = PA_OFF;
   //turn LNA off
   MC13192_LNA_CTRL = LNA_OFF;
   
   //while((radio_spiReadReg(RG_TRX_STATUS) & 0x1F) != TRX_OFF); // busy wait until done */ //revisit this function REVIEW poipoi
   
}

//=========================== private =========================================

void radio_spiWriteReg(uint8_t reg_addr, uint16_t reg_setting) {
   uint8_t u8TempValue=0;

  MC13192DisableInterrupts();   // Necessary to prevent double SPI access 
  AssertCE();                   // Enables MC13192 SPI
   
  SPI1D = reg_addr & 0x3F;      // Write the command   
  while (!(SPI1S_SPRF));        // busywait
  u8TempValue = SPI1D;          //Clear receive data register. SPI entirely 

  SPI1D = ((UINT8)(u16Content >> 8));    /* Write MSB */       
  while (!(SPI1S_SPRF));         
  u8TempValue = SPI1D;         
  
  
  SPI1D = ((UINT8)(u16Content & 0x00FF));    /* Write LSB */
  while (!(SPI1S_SPRF)); 
  u8TempValue = SPI1D;
  
  DeAssertCE();
  MC13192RestoreInterrupts();
}

uint16_t radio_spiReadReg(uint8_t reg_addr) {
   uint8_t u8TempValue=0;
   uint16_t  u16Data=0;            /* u16Data[0] is MSB, u16Data[1] is LSB */


    MC13192DisableInterrupts(); /* Necessary to prevent double SPI access */
    AssertCE();                 /* Enables MC13192 SPI */
    
    
    SPI1D = ((u8Addr & 0x3f) | 0x80);      // Write the command   
    while (!(SPI1S_SPRF));        // busywait
    u8TempValue = SPI1D;          //Clear receive data register. SPI entirely 
    
   
    SPISendChar(reg_addr );      // Dummy write.
    while (!(SPI1S_SPRF));        // busywait
    ((UINT8*)u16Data)[0] = SPI1D;               /* MSB */
    
    
    SPISendChar(reg_addr );      // Dummy write.
    while (!(SPI1S_SPRF));        // busywait
    ((UINT8*)u16Data)[1] = SPI1D;               /* LSB */
    
    
    DeAssertCE();                     /* Disables MC13192 SPI */
    MC13192RestoreInterrupts();       /* Restore MC13192 interrupt status */
    return u16Data;
}

void radio_spiWriteTxFifo(uint8_t* bufToWrite, uint8_t  lenToWrite) {
     uint8_t junk;
     radio_spiWriteReg(TX_PKT_LEN, lenToWrite);/* Update the TX packet length field (2 extra bytes for CRC) */
     junk = SPI1S;
     junk = SPI1D;//as specified in the generated radio code
     
     //now check if we have an even number of bytes and update accordingly (the radio requires an even number)
     if(lenToWrite&0x01)
       lenToWrite +=1;
     
     uint8_t bufToWrite2[lenToWrite+1];//recreate packet with TX_PKT address
     bufToWrite2[0] = TX_PKT;
     for(junk=0;junk<lenToWrite;junk++)
       bufToWrite2[junk+1] = bufToWrite[junk];
       
     
     uint8_t spi_rx_buffer[lenToWrite+1];              // 1B SPI address + length	   
	   spi_txrx(bufToWrite,
	            lenToWrite+1,
	            SPI_BUFFER,
	            spi_rx_buffer,
	            sizeof(spi_rx_buffer),
	            SPI_FIRST,
	            SPI_LAST);
                              
}

void radio_spiReadRxFifo(uint8_t* bufRead, uint16_t length) {

  uint8_t junk;
  bool removeOneByte = 0;
  
  //now check if we have an even number of bytes and update accordingly (the radio requires an even number)
     if(length&0x01){
      length +=1;
      removeOneByte = 1;   //need to remove the MSB of the last word
     }

  uint8_t spi_tx_buffer[length+1+2];//1byte for the address+2junk bytes+length
  spi_tx_buffer[0] = RX_PKT | 0x80;  /* SPI RX ram data register */
  
  spi_txrx(spi_tx_buffer,
            length+1+2,
            SPI_BUFFER,
            bufRead,
            length+1+2,
            SPI_FIRST,
            SPI_LAST);
   //now remove the first three bytes because they're junk        
   bufRead = bufRead + 3; //or should it be (&bufRead) = *bufRead + 3??? poipoi
   if(removeOneByte){
    bufRead(length-1) = bufread(length);
    length-=1;
    //refer to the datasheet to know why this is needed (even number of bytes)    
   }
}

//=========================== callbacks =======================================

//=========================== interrupt handlers ==============================

//#pragma vector = PORT1_VECTOR
//__interrupt void PORT1_ISR (void) {
interrupt void IRQIsr(void){ //REVIEW poipoi prototype: interrupt void IRQIsr(void); maybe in eldorado.h

   uint16_t capturedTime;
   uint16_t  irq_status;
   // clear interrupt flag
   IRQSC |= 0x04;
   // capture the time
   capturedTime = radiotimer_getCapturedTime();
   // reading IRQ_STATUS causes IRQ_RF (P1.6) to go low
   irq_status = radio_spiReadReg(STATUS_ADDR);
   
   if((irq_status&RX_IRQ_MASK)!=0){
     if (radio_vars.endFrameCb!=NULL) {
		 MC13192_RTXEN = 0;//deassert the signal to put radio in idle mode.
            // call the callback
            radio_vars.endFrameCb(capturedTime);
            // make sure CPU restarts after leaving interrupt
            __bic_SR_register_on_exit(CPUOFF);
         }
   
   }
   
   if((irq_status&TX_IRQ_MASK)!=0){
     if (radio_vars.endFrameCb!=NULL) {
		 MC13192_RTXEN = 0;//deassert the signal to put radio in idle mode.
            // call the callback
            radio_vars.endFrameCb(capturedTime);
            // make sure CPU restarts after leaving interrupt
            __bic_SR_register_on_exit(CPUOFF);
         }
   }
   
   //WARNING! We need to find a fix for this!!! We can't capture time because the radio does not provide an interrupt on RX_Start
   /*   case AT_IRQ_RX_START:
         if (radio_vars.startFrameCb!=NULL) {
            // call the callback
            radio_vars.startFrameCb(capturedTime);
            // make sure CPU restarts after leaving interrupt
            __bic_SR_register_on_exit(CPUOFF);
         }
         break;
  */
}