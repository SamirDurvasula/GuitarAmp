/**
 * This is a very small example that shows how to use
 * === OUTPUT COMPARE  ===
/*
 * File:        PWM example
 * Author:      Bruce Land
 * Target PIC:  PIC32MX250F128B
 */

////////////////////////////////////
// clock AND protoThreads configure!
// You MUST check this file!
#include "config.h"
// threading library
#include "pt_cornell_1_2.h"

// Configure digital potentiometer to be writeable
#define Digipot_config_chan_A 0b0000000000000000
// === thread structures ============================================
// thread control structs
// note that UART input and output are threads
static struct pt pt_cmd, pt_input, pt_output, pt_DMA_output ;

volatile SpiChannel spiChn = SPI_CHANNEL1 ;
volatile int spiClkDiv = 4 ; //run spi at 10 MHz


// === Command Thread ======================================================
// The serial interface
//Primary use of microcontroller is to control all of the various circuits 
//used in the circuit (literal controller).
static char cmd[16]; 
static int value;

static PT_THREAD (protothread_cmd(struct pt *pt))
{
    PT_BEGIN(pt);
      while(1) {
          
            // send the prompt via DMA to serial
            sprintf(PT_send_buffer,"cmd>");
            // by spawning a print thread
            PT_SPAWN(pt, &pt_DMA_output, PT_DMA_PutSerialBuffer(&pt_DMA_output) );
 
          //spawn a thread to handle terminal input
            // the input thread waits for input
            // -- BUT does NOT block other threads
            // string is returned in "PT_term_buffer"
            PT_SPAWN(pt, &pt_input, PT_GetSerialBuffer(&pt_input) );
            // returns when the thread dies
            // in this case, when <enter> is pushed
            // now parse the string
             sscanf(PT_term_buffer, "%s %d", cmd, &value);
         

             
              if (cmd[0]=='d' ) {
                //set mux to DSP chip
                mPORTAClearBits(BIT_4);
                mPORTBClearBits(BIT_13);
                mPORTASetBits(BIT_4);
                mPORTBClearBits(BIT_3 | BIT_7 | BIT_8 | BIT_9);
                 
                 if(((value << 6) & (0x0040))){
                     mPORTBSetBits(BIT_3);
                 }
                     //select sound effect
                 mPORTBSetBits(value << 6);
                 }
             if (cmd[0] == 's'){ //distortion
                 //set mux to Distortion
                mPORTAClearBits(BIT_4);
                mPORTBClearBits(BIT_13);
             }
             if (cmd[0] == 'n'){ //normal: only tone stack
                 //set mux to channel 0
                mPORTAClearBits(BIT_4);
                mPORTBClearBits(BIT_13);
                mPORTBSetBits(BIT_13);
             }
             //pots
             if (cmd[0] == 't'){ //treble
                // CS low to start transaction
                mPORTBClearBits(BIT_4); // start transaction
                deliverSPICh1Datum(value);
                // CS high
                mPORTBSetBits(BIT_4); // end transaction
             }
             if (cmd[0] == 'b'){ //bass
                // CS low to start transaction
                mPORTAClearBits(BIT_0); // start transaction
                deliverSPICh1Datum(value);
                // CS high
                mPORTASetBits(BIT_0); // end transaction
            }
            if (cmd[0] == 'v'){ //level
                // CS low to start transaction
                    mPORTAClearBits(BIT_2); // start transaction
                    deliverSPICh1Datum(value);                              
                     // CS high
                    mPORTASetBits(BIT_2); // end transaction
            }
            if (cmd[0] == 'm'){ //mid
                // CS low to start transaction
                mPORTAClearBits(BIT_3); // start transaction
                deliverSPICh1Datum(value);
                // CS high
                mPORTASetBits(BIT_3); // end transaction
            }
                          
            // never exit while
      } // END WHILE(1)
  PT_END(pt);
} // thread 3

//instead of writing SPI code repeatedly, made a function.
//Used various chip selects on the same channel to change
//values of various different digital potentiometers
void deliverSPICh1Datum(int datum){
    // test for ready
     while (TxBufFullSPI1());
     // write to spi2
     WriteSPI1(Digipot_config_chan_A | datum);
    // test for done
    while (SPI1STATbits.SPIBUSY); // wait for end of transaction
     // CS high
    // mPORTBSetBits(BIT_4); // end transaction
}

// === Main  ======================================================

int main(void)
{

  // === config the uart, DMA, SPI ===========
  PT_setup();

  // == SPI ==
  //enable SPI at 10 MHz clock to meet digital potentiometer specifications
  SpiChnOpen(spiChn, SPI_OPEN_ON | SPI_OPEN_MODE16 | SPI_OPEN_MSTEN | SPI_OPEN_CKE_REV , spiClkDiv);
  
  // === setup system wide interrupts  ====================
  INTEnableSystemMultiVectoredInt();
    
  // === set up i/o port pin ===============================
  //Port B bits, 3,7,8, and 9 are used to select digital output
  //Port B bit 4 is used as chip select for treble digital potentiometer
  //Port B bit 13 is used as a select signal for output effect selection multiplexer
  //Additional functionality would use the TFT to display which output was being
  //selected
  mPORTBSetPinsDigitalOut(BIT_4 | BIT_3|BIT_7 | BIT_8 | BIT_9 |BIT_13);    //Set port as output
 
  //Port A Bits 0,2,and 3 are used to configure digital potentiometers (chip selects). Port A bit 4 is used
  //for multiplexing whether to have input from Digital effector chip, distortion,
  //or pure tonestack sound
  mPORTASetPinsDigitalOut(BIT_0 | BIT_2 | BIT_3 | BIT_4);
  // === now the threads ===================================
  
  // init the threads
  PT_INIT(&pt_cmd);
  PT_INIT(&pt_time);
  
  //==Digipot spi stuff
   // SCK2 is pin 26 
    // SDO1 (MOSI) is in PPS output group 1, could be connected to RB5 which is pin 14
    PPSOutput(2, RPB5, SDO1);
    // control CS for DAC
    //mPORTBSetPinsDigitalOut(BIT_4); //CS
    mPORTBSetBits(BIT_4 | BIT_6);
    //===
    
    mPORTASetBits(BIT_0 | BIT_2 | BIT_3 | BIT_4); //CS pins active high
    mPORTAClearBits(BIT_4);
    mPORTBClearBits(BIT_13);
    mPORTBSetBits(BIT_13);
    
    
  // schedule the threads
  while(1) {
      //cmd used as command center for all effects
    PT_SCHEDULE(protothread_cmd(&pt_cmd));
  }
} // main