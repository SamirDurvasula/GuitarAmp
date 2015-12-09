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

// A-channel, 1x, active
#define DAC_config_chan_A 0b0000000000000000
// === thread structures ============================================
// thread control structs

//print lock
static struct pt_sem print_sem ;

// note that UART input and output are threads
static struct pt pt_cmd, pt_time, pt_input, pt_output, pt_DMA_output ;

// system 1 second interval tick
int sys_time_seconds ;

//The actual period of the wave
int generate_period = 2000 ;
int pwm_on_time = 500 ;
//print state variable
int printing=0 ;

volatile SpiChannel spiChn = SPI_CHANNEL1 ;
volatile int spiClkDiv = 4 ; //run directly at peripheral bus clk freq?

// == Timer 2 ISR =====================================================
// just toggles a pin for timeing strobe
void __ISR(_TIMER_2_VECTOR, ipl2) Timer2Handler(void)
{
    // generate a trigger strobe for timing other events
    mPORTBSetBits(BIT_0);
    // clear the timer interrupt flag
    mT2ClearIntFlag();
    mPORTBClearBits(BIT_0);
}


// === Command Thread ======================================================
// The serial interface
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
            // returns when the thead dies
            // in this case, when <enter> is pushed
            // now parse the string
             sscanf(PT_term_buffer, "%s %d", cmd, &value);
          
             if (cmd[0]=='p' ) {
                /*
                 generate_period = value;
                 // update the timer period
                 WritePeriod2(generate_period);
                 // update the pulse start/stop
                 SetPulseOC2(generate_period>>2, generate_period>>1);
                 */
            }
             
             //if (cmd[0]=='d' ) {
             //    pwm_on_time = value ;
               //  SetDCOC3PWM(pwm_on_time);
             //} //  
             
            /* if (cmd[0]=='t' ) {
                 sprintf(PT_send_buffer,"%d ", sys_time_seconds);
                // by spawning a print thread
                PT_SPAWN(pt, &pt_DMA_output, PT_DMA_PutSerialBuffer(&pt_DMA_output) );
             } //  */
              if (cmd[0]=='d' ) {
                //set mux to DSP chip
                mPORTBClearBits(BIT_12 | BIT_13); 
                mPORTBSetBits(BIT_12);
                mPORTBClearBits(BIT_3 | BIT_7 | BIT_8 | BIT_9);
                 
                 if(((value << 6) & (0x0040))){
                     mPORTBSetBits(BIT_3);
                 }
                     //select sound effect
                 mPORTBSetBits(value << 6);
                /* 
                 //generate_period = value;
                 // update the timer period
                 //WritePeriod2(generate_period);
                // update the pulse start/stop
                 //SetPulseOC2(generate_period>>2, generate_period>>1);
             
                 */ }
             if (cmd[0] == 's'){ //distortion
                 //set mux to Distortion
                mPORTBClearBits(BIT_12 | BIT_13); 
                mPORTBSetBits(BIT_13);
             }
             if (cmd[0] == 'n'){ //normal: only tone stack
                 //set mux to channel 0
                mPORTBClearBits(BIT_12 | BIT_13); 
             }
             //pots
             if (cmd[0] == 't'){ 
                // CS low to start transaction
                mPORTBClearBits(BIT_4); // start transaction
                deliverSPICh1Datum(value);
                // CS high
                mPORTBSetBits(BIT_4); // end transaction
             }
             if (cmd[0] == 'b'){ 
                // CS low to start transaction
                mPORTAClearBits(BIT_0); // start transaction
                deliverSPICh1Datum(value);
                // CS high
                mPORTASetBits(BIT_0); // end transaction
            }
            if (cmd[0] == 'm'){ 
                // CS low to start transaction
                mPORTAClearBits(BIT_2); // start transaction
                deliverSPICh1Datum(value);
                // CS high
                mPORTASetBits(BIT_2); // end transaction
            }
            if (cmd[0] == 'v'){ //gain/volume
                // CS low to start transaction
                mPORTAClearBits(BIT_3); // start transaction
                deliverSPICh1Datum(value);
                // CS high
                mPORTASetBits(BIT_3); // end transaction
            }
            if (cmd[0] == 'l'){ //left/right 
                // CS low to start transaction
                mPORTAClearBits(BIT_4); // start transaction
                deliverSPICh1Datum(value);
                // CS high
                mPORTASetBits(BIT_4); // end transaction
            }
             
             
            // never exit while
      } // END WHILE(1)
  PT_END(pt);
} // thread 3

// === One second Thread ======================================================
// update a 1 second tick counter
static PT_THREAD (protothread_time(struct pt *pt))
{
    PT_BEGIN(pt);

      while(1) {
            // yield time 1 second
            PT_YIELD_TIME_msec(1000) ;
            sys_time_seconds++ ;
            // NEVER exit while
      } // END WHILE(1)

  PT_END(pt);
} // thread 4

void deliverSPICh1Datum(int datum){
    // CS low to start transaction
    // mPORTBClearBits(BIT_4); // start transaction
    // test for ready
     while (TxBufFullSPI1());
     // write to spi2
     WriteSPI1(DAC_config_chan_A | datum);
    // test for done
    while (SPI1STATbits.SPIBUSY); // wait for end of transaction
     // CS high
    // mPORTBSetBits(BIT_4); // end transaction
}

// === Main  ======================================================

int main(void)
{
/*    
  // === Config timer and output compares to make pulses ========
  // set up timer2 to generate the wave period
  OpenTimer2(T2_ON | T2_SOURCE_INT | T2_PS_1_1, generate_period);
  ConfigIntTimer2(T2_INT_ON | T2_INT_PRIOR_2);
  mT2ClearIntFlag(); // and clear the interrupt flag

  // set up compare3 for PWM mode
  OpenOC3(OC_ON | OC_TIMER2_SRC | OC_PWM_FAULT_PIN_DISABLE , pwm_on_time, pwm_on_time); //
  // OC3 is PPS group 4, map to RPB9 (pin 18)
  //PPSOutput(4, RPB9, OC3);

  // set pulse to go high at 1/4 of the timer period and drop again at 1/2 the timer period
  OpenOC2(OC_ON | OC_TIMER2_SRC | OC_CONTINUE_PULSE, generate_period>>1, generate_period>>2);
  // OC2 is PPS group 2, map to RPB5 (pin 14)
  PPSOutput(2, RPB5, OC2);
*/
  // === config the uart, DMA, vref, timer5 ISR ===========
  PT_setup();

  // == SPI ==
  SpiChnOpen(spiChn, SPI_OPEN_ON | SPI_OPEN_MODE16 | SPI_OPEN_MSTEN | SPI_OPEN_CKE_REV , spiClkDiv);
  
  // === setup system wide interrupts  ====================
  INTEnableSystemMultiVectoredInt();
    
  // === set up i/o port pin ===============================
  mPORTBSetPinsDigitalOut(BIT_0 | BIT_4 | BIT_3| BIT_6 | BIT_7 | BIT_8 | BIT_9 |  BIT_12 | BIT_13);    //Set port as output
 
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
    mPORTBClearBits(BIT_10 | BIT_12);
    
    
  // schedule the threads
  while(1) {
    PT_SCHEDULE(protothread_cmd(&pt_cmd));
    PT_SCHEDULE(protothread_time(&pt_time));
  }
} // main