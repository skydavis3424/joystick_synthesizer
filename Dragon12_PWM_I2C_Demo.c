/* File: Dragon12_PWM_I2C_DEMO.c
 * Copy: Copyright(c) 2018, Skylar Davis
 * Vers: 1.0.4, 11/5/2018, scd, Add expanded, non-linearized joystick;
 * Vers: 1.0.3, 10/20/2018, scd, Add I2C functionality, expanded joystick functionality
 * Vers: 1.0.2, 10/5/2018, scd, Add Joystick functionality
 * Vers: 1.0.1, 9/27/2018, scd, Add LTC 1661 functionality
 * Vers: 1.0.0, 9/14/2018, scd, Add PWM functionality
 */

#include <hidef.h>                        /* common defines and macros */
#include "derivative.h"                   /* derivative-specific definitions */

void initPWM(void);                       /* init PWM ports function header */
void initDAC(void);                       /* init DAC ports function header */
void initATD(void);                       /* init ATD ports function header */
void playSound(long);                     /* play sound function header */
void createVoltage(int, int);             /* create voltage function header */
void delay(void);                         /* debounce function header */
int readJoystick(void);                   /* read joystick function header */
long joyToIncrement(long);                /* convert joystick value to freq value */
void initI2C(void);                       /* init I2C interfacing and serial timer */
long readClock(void);                     /* read status of the clock */
int getOctave(long)                       /* get musical octave of toneFreq */
       
void main(void) {

  /* INPUT VARIABLES - SET BY READ AND OBSERVED BY THINK */
  int btnDown;                            
  
  /* STATE VARIABLES - SET AND OBSERVED BY THINK */
  int btnCount;                           
  int btnState;
  int oldBtnState;
  int loopCount;
  int isPlaying;
  long timeKept;
  long timeStamp;
  long timeStop;
  
  /* ACTION VARIABLES - SET BY THINK AND OBSERVED BY DO */
  long analogValue;
  long oldToneFreq;
  long toneFreq;
  int octave;
  
  /* INITIALIZATION - CODE THAT MUST EXECUTE ONE TIME */
  btnDown = 0;
  btnCount = 0;
  btnState = 0;
  oldBtnState = 0;
  toneFreq = 0;
  loopCount = 0;
  isPlaying = 1;
  oldToneFreq = 2500 * 100000;
  time = 0;
  stamp = 0;
  stop = 0;
  initPWM();
  initDAC();
  initATD();
  initI2C();

  /* START OF MAJOR LOOP */
  while(1)  {

    /* READ CLOCK */
    timeKept = readClock();

    /* CREATE VOLTAGE AT START OF 'READ' */
    createVoltage(300, 9);
    
    /* READ (GET CURRENT STATE OF THE BUTTON) */
    if (PTH == 0b00000000)  {
      btnDown = 1;
    } else {
      btnDown = 0;
    }
    
    /* READ (GET CURRENT VALUE OF THE JOYSTICK) */
    analogValue = readJoystick();
    octave = getOctave(oldToneFreq);
    toneFreq = oldToneFreq + (joyToIncrement(analogValue, octave) * 100);
    if (toneFreq >= 500000000)  {
      toneFreq = 500000000;
    } else if (toneFreq <= 10000000)  {
      toneFreq = 10000000;
    } 
    
    /* CREATE VOLTAGE AT START OF 'THINK' */
    createVoltage(700, 9);

    /* THINK (INCREMENT THE COUNT UP OR DOWN) */
    if (btnDown == 1)  {
      btnCount++;
      if (btnCount > 6) btnCount = 6;
    } else  {
      btnCount--;
      if (btnCount < 0) btnCount = 0;
    }
    
    /* THINK (STATE MACHINE TO DEBOUNCE BUTTON) */
    switch(btnState)  {
      case 0: if (btnCount > 4)  {
        btnState = 1;
        timeStamp = timeKept;
        timeStop = timeStamp + 60;
      } break;
      case 1: if (btnCount < 2)  {
        btnState = 0;
      } break;
    }
    
    /* THINK (COUNT LOOPS IN EACH STATE AS A SAFETY MECHANISM) */
    if (btnState == oldBtnState) loopCount++;
    else {
      oldBtnState = btnState;
      loopCount = 0;
    }

    /* CREATE VOLTAGE AT START OF 'DO' */
    createVoltage(1023, 9);

    /* DO (PLAY SOUND, QUIT PLAYING IF TIME IS UP) */
    if (timeStamp != 0)  {
      if (timeKept >= timeStop)  {
         playSound(0);
         while(1);
      }
    }

    playSound(toneFreq);
    oldToneFreq = toneFreq;
    delay();

    /* END OF MAJOR LOOP */ 
  }
}

/* NAME: initPWM
 * DESC: initializes relevant ports for IO and PWM
 * ARGU: none
 */
void initPWM()  {
  PWMCLK = 0b00000000;                    /* select clock A for channels 4-5 */
  PWMPOL = 0b00110000;                    /* set polarity for channels 4-5 high */
  PWMCAE = 0b00000000;                    /* set channels 4-5 as left aligned */
  PWMCTL = 0b01000000;                    /* concatenate channels 4-5 */                                       
  DDRH = 0b11111110;                      /* set DDRH for SW5 input */ 
}

/* NAME: initDAC
 * DESC: initializes relevant ports for LTC 1661 DAC
 * ARGU: none
 */
 void initDAC()  {
  byte x;                                 /* flag variable */
  SPI0CR1 = 0b01010000;                   /* enable SPI, master mode */
  SPI0CR2 = 0b00000010;                   /* disable mode fault enable bit, clock operates normally in stop mode */
  SPI0BR = 0b00100000;                    /* set baud rate to 6 MHz */
  
  while (SPI0SR & 0x80)  {
    x = SPI0DR;                           /* wait for flag, clear flag */
  }
  
  DDRM = 0b01000000;                      /* set bit 6 of DDRM as output */
  PTM = 0b01000000;                       /* set bit 6 of PTM high */
 }
 
/* NAME: initATD
* DESC: initializes relevant ports for ATD coverter 
* ARGU: none
*/
void initATD()  {
   ATD0CTL2 = 0xC0;                 /* normal ATD operation, clear flag if set */
   ATD0CTL3 = 0x08;                 /* limit one coversion */
   ATD0CTL4 = 0xFF;                 /* 10 bit operation mode, set ATD clock prescaler */
   ATD0CTL5 = 0x86;                 /* set conversion type and ATD channel 6 */
  }

/* NAME: playSound
 * DESC: generates a tone at a given frequency using PWM
 * ARGU: frequency in units of 1/100000 Hz
 */
void playSound(long freq)  {
    int scale;
    long count;
    long period = 24000000 / (freq / 100000);
    PWMPRCLK = 0b00000000;
    
    if (period > 65536)  {
      count = period;
      while (count > 65536)  {
        PWMPRCLK = PWMPRCLK + 1;
        if (PWMPRCLK >= 7)  {
          PWMPRCLK = 7;
        }
        switch(PWMPRCLK)  {
          case 1: scale = 2;
          break;
          case 2: scale = 4;
          break;
          case 3: scale = 8;
          break;
          case 4: scale = 16;
          break;
          case 5: scale = 32;
          break;
          case 6: scale = 64;
          break;
          case 7: scale = 128;
          break;
        }
        count = count - 65536;
      }
      period = (24000000 / (scale)) / (freq / 100000);
    }
    
    PWMPER4 = period>>8;                  /* set period for channel 4 */
    PWMPER5 = period;                     /* set period for channel 5 */
    PWMDTY4 = PWMPER4 / 2;                /* ensure 50% dc for channel 4 */                
    PWMDTY5 = PWMPER5 / 2;                /* ensure 50% dc for channel 5 */
    PWME = 0b00100000;                    /* enable PWM channels 4 & 5 */
}

/* NAME: createVoltage
 * DESC: LTC 1661 creates a voltage to be measured with oscilloscope
 * ARGU: int volts, int cmd
 */
 void createVoltage(int volts, int cmd)  {
  byte x;
  volts<<=2;
  volts |= (cmd<<12);
  PTM &= 0b10111111;
  SPI0DR = (byte)(volts>>8);
  
  while ((SPI0SR & 0x80) == 0);
  x = SPI0DR;
  SPI0DR = (byte) (volts & 0b11111111);
  while ((SPI0SR & 0b10000000) == 0);
  x = SPI0DR;
  PTM |= 0b01000000;
}

/* NAME: debounce
 * DESC: delays for approx. 20 ms
 * ARGU: none
 */
void delay()  {
  long delay = 0;
  while (delay < 2500)  {
    delay++;
  }
}

/* NAME: readJoystick
 * DESC: reads the value of the joystick
 * ARGU: none
 */
 int readJoystick()  {
   ATD0CTL5 = 0x86;                       /* right justification for results, select ATD channel 6 */
   while(!(ATD0STAT0 & 0x80));            /* wait for conversion to complete, raise the flag, &= 0b00001000 */
   return ATD0DR0;                        /* return joystick value (0-256) */
 }
 
/* NAME: joyToFreq
 * DESC: converts joystick value to freq value used as arg to playSound function
 * ARGU: none
 */
  long joyToIncrement(long analog, int oct)  {
    int increment = 0;
    analog = analog - 128;
    
    if (analog >= 110)  {
      increment = 5000 * oct;
    } else if (analog < 110 && analog >= 90)  {
      increment = 2500 * oct;
    } else if (analog < 90 && analog >= 70)  {
      increment = 1250 * oct;
    } else if (analog < 70 && analog >= 50)  {
      increment = 625 * oct;
    } else if (analog < 50 && analog >= 30)  {
      increment = 300 * oct;
    } else if (analog < 30 && analog >= -30)  {
      increment = 0;
    } else if (analog < -30 && analog >= -50)  {
      increment = -300 * oct;
    } else if (analog < -50 && analog >= -70)  {
      increment = -625 * oct;
    } else if (analog < -70 && analog >= -90)  {
      increment = -1250 * oct;
    } else if (analog < -90 && analog >= -110)  {
      increment = -2500 * oct;
    } else if (analog < -110)  {
      increment = -5000 * oct;
    }
    
    return increment;
  }
  
  void initI2C()  {
    IBCR = 0x80;                      /* enable I2C module */
    IBFD = 0x1F;                      /* set to 100 kHz bit rate */
    
    IBCR = 0x80 + 0x10 + 0x20;
    IBDR = 0b11010000;
    while(!(IBSR & 0x02));
    IBSR |= 0x02;                     /* clear flag */
    IBDR = 0;                         /* write to RTC */
    while(!(IBSR & 0x02));
    IBSR |= 0x02;                     /* clear flag */
    IBDR = 0;                         /* set seconds */
    while(!(IBSR & 0x02));
    IBSR |= 0x02;                     /* clear flag */
    IBDR = 0;                         /* set minutes */
    while(!(IBSR & 0x02));
    IBSR |= 0x02;                     /* clear flag */
    IBDR = 0;                         /* set hours */
    while(!(IBSR & 0x02));
    IBSR |= 0x02;                     /* clear flag */
    IBDR = 1;                         /* set days  */
    while(!(IBSR & 0x02));
    IBSR |= 0x02;                     /* clear flag */
    IBDR = 1;                         /* set date */
    while(!(IBSR & 0x02));
    IBSR |= 0x02;                     /* clear flag */
    IBDR = 1;                         /* set months */
    while(!(IBSR & 0x02));
    IBSR |= 0x02;                     /* clear flag */
    IBDR = 0;                         /* set years */
    while(!(IBSR & 0x02));
    IBSR |= 0x02;                     /* clear flag */
    IBDR = 0b00000000;                /* set control */
    while(!(IBSR & 0x02));
    IBSR |= 0x02;                     /* clear flag */                        
    IBCR = 0b10000000;                /* generate stop condition */
  }
 
 long readClock()  {
    long time = 0;
    int dummy = 0;
    byte seconds = 0;
    byte totalSec = 0;
    byte tenSeconds = 0;
    byte minutes = 0;
    int totalMin = 0;
    byte tenMinutes = 0;
    byte hours = 0;
    long totalHours = 0;
    IBSR |= 0x02;
    
    IBCR = 0b10110000;                /* enable I2C, master mode, acknowledge signal will be sent, restart cycle */
    IBDR = 0b11010000;                /* set slave address */
    
    while(!(IBSR & 0x02));            /* wait for flag */
    IBSR |= 0x02;                     /* clear flag */                                        
    IBDR = 0;                         /* set address for seconds */                      
                            
    while(!(IBSR & 0x02));            /* wait for flag */
    IBSR |= 0x02;                     /* clear flag */                      
    
    IBCR = 0b10110100;                /* start writing to clock (transmit) */
    IBDR = 0b11010001;                /* set slave address */
    
    while(!(IBSR & 0x02));            /* wait for flag */
    IBSR |= 0x02;                     /* clear flag */                    
    IBCR = 0b10100000;                /* start writing to clock (receive) */
    
    dummy = IBDR;                     /* write to dummy to initiate clock */
    
    while(!(IBSR & 0x02));            /* wait for flag */
    IBSR |= 0x02;                     /* clear flag */
    seconds = IBDR;                   /* read seconds */
    
    while(!(IBSR & 0x02));            /* wait for flag */
    IBSR |= 0x02;                     /* clear flag */
    IBCR |= 0x08;                     /* ignore read after this one */
    minutes = IBDR;                   /* read minutes */
    
    while(!(IBSR & 0x02));            /* wait for flag */
    IBSR |= 0x02;                     /* clear flag */
    hours = IBDR;                     /* read hours */
    
    while(!(IBSR & 0x02));            /* wait for flag */
    IBSR |= 0x02;                     /* clear flag */
    IBCR = 0b10000000;                /* generate stop condition */
   
    // convert bcd values to long, return time //
    tenSeconds = seconds >> 4;
    tenSeconds = tenSeconds * 10;
    seconds = seconds << 4;
    seconds = seconds >>4;
    totalSec = tenSeconds + seconds;
  
    tenMinutes = minutes >> 4;
    tenMinutes = tenMinutes * 600; 
    minutes = minutes << 4;
    minutes = minutes >> 4;
    minutes = minutes * 60;
    totalMin = minutes + tenMinutes;
    
    hours = hours << 4;
    hours = hours >> 4;
    totalHours = hours * 3600; 
  
    time = totalSec + totalMin + totalHours; 
    
    return time; 
}

int getOctave(long freq)  {
  int octave = 0;
  long actualFreq = freq / 100000;
  if (actualFreq > 80)  {
    octave = 2;
  } else if (actualFreq > 130)  {
    octave = 3;
  } else if (actualFreq > 260)  {
    octave = 4;
  } else if (actualFreq > 520)  {
    octave = 5;
  } else if (actualFreq > 1040)  {
    octave = 6;
  } else if (actualFreq > 2070)  {
    octave = 7;
  } else if (actualFreq > 4100)  {
    octave = 8;
  }

  return octave;
}
  
  