// -----------------------------------------------------------------------------
// Spec & Dev of Embedded Computer System, Holon Institute of Thecnology
// LECTURER             : Dr. Avihai Aharon
//------------------------------------------------------------------------------
// PROJECT: Driver and feedback system to DC Motor
//------------------------------------------------------------------------------
// AUTHOR                : Oded Yosef
// -----------------------------------------------------------------------------
// ------------------------Conection's Details----------------------------------
// Vx : GPA0 (ADC0)
// Vy : GPA1 (ADC1)
// PWM: GPA12(PWM0)
// -----------------------------------------------------------------------------
#include <stdio.h>
#include "NUC1xx.h"
#include "LCD_Driver.h"
#include "DrvGPIO.h"
#include "DrvSYS.h"
#include "DrvADC.h"

#define NumOfSam 20                						       //Number Of Sample of the Voltage on the Power Resistor
#define OverLoadCurr 0.75        				   	               //0.75 Amps - Overload on Engine
#define OverLoadDutyCycle 0x7D0     						       //PWM duty cycle in overload state  

// -----------------------------------------------------------------------------
// ------------------------------Function's-------------------------------------
// ---------------------------Initializing PWM----------------------------------
void InitPWM(void)
{
 	/* Step 1. GPIO initial */ 
	SYS->GPAMFP.PWM0_AD13=1;
				
	/* Step 2. Enable and Select PWM clock source*/		
	SYSCLK->APBCLK.PWM01_EN = 1;							//Enable PWM clock
	SYSCLK->CLKSEL1.PWM01_S = 3;							//Select 22.1184Mhz for PWM clock source

	PWMA->PPR.CP01=1;								//Prescaler 0~255, Setting 0 to stop output clock
	PWMA->CSR.CSR0=0;								// PWM clock = clock source/(Prescaler + 1)/divider
				         
	/* Step 3. Select PWM Operation mode */
	PWMA->PCR.CH0MOD=1;								//0:One-shot mode, 1:Auto-load mode
											//CNR and CMR will be auto-cleared after setting CH0MOD form 0 to 1.
	PWMA->CNR0=0xFFFF;
	PWMA->CMR0=0x7D0;

	PWMA->PCR.CH0INV=0;								//Inverter->0:off, 1:on
	PWMA->PCR.CH0EN=1;								//PWM function->0:Disable, 1:Enable
 	PWMA->POE.PWM0=1;								//Output to pin->0:Diasble, 1:Enable
}

// -----------------------------------------------------------------------------
// ---------------------Average for all current samples-------------------------
//*   Function receive arrey with the current sample and return the average   */
float AVRCurr (float Currsample[NumOfSam])
{
	int i;
	float Curr = 0.0;
	for(i=0;i<NumOfSam;i++)
	{
		Curr = Curr + Currsample[i];
	}
	Curr=Curr/NumOfSam;
	return Curr;
}
// -----------------------------------------------------------------------------
// -----------------------------Main Function-----------------------------------
int32_t main (void)
{
  uint16_t Vx, Vy, Vz;                                                      
  float  Vsub, Vsub_temp, Queue_Curr[NumOfSam];
  char TEXT[16];
	int j;	

 	UNLOCKREG();
	SYSCLK->PWRCON.XTL12M_EN = 1;                                             	// enable external clock (12MHz)
	SYSCLK->CLKSEL0.HCLK_S = 0;	 					  	// select external clock (12MHz)
	LOCKREG();
	Initial_panel(); 							  	// initialize LCD pannel
	clr_all_panel(); 							  	// clear LCD panel 
	InitPWM();				     				  	// initialize PWM
	DrvADC_Open(ADC_SINGLE_END, ADC_SINGLE_CYCLE_OP, 0x83, INTERNAL_HCLK, 1); 	// Initialize ADC1 & ADC0 & ADC7
// -----------------------------------------------------------------------------
// -------------------------------While loop------------------------------------	
	while(1)
	{	
		DrvADC_StartConvert();                    			  	// start A/D conversion
    while(DrvADC_IsConversionDone()==FALSE);					  	// wait till conversion is done
		Vx = ADC->ADDR[0].RSLT ;                                          	// Vx <- ADC0 value  
		Vy = ADC->ADDR[1].RSLT ;					  	// Vx <- ADC1 value
		Vz = ADC->ADDR[7].RSLT ;    					        // Vx <- ADC7 value	
		
		//Note 1
		/* 
		Vy minus Vx is the digital value on the resistor that present 
		4095 Voltage level between 0 ~ 3.3V.  so multiply by 3.3(V)
		and 1/0.1(OHM) and divided by 4095 = 0.000805  
		*/	
		Vsub_temp = (Vy-Vx)*0.008058;                                           // See Note 1  
		clr_all_panel();							// Clear LCD
		print_lcd(0, "Oded&Nir Project");					// Print Header
	  
		// -----------------------------------------------------------------------------
		//Note 2
		/* 
		For loop that impliment FIFO
		first in first out and the last current sample
		goes in to Queue
		*/
		for(j=NumOfSam-1; j>0; j--)
		{
			Queue_Curr[j] = Queue_Curr[j-1];                                // Shift right
		}
		Queue_Curr[0] = Vsub_temp;						// last current sample to the Queue
		Vsub = AVRCurr(Queue_Curr);                                             // Vsub is the average of the arrey
		// -----------------------------------------------------------------------------
		// -------------------------------------If--------------------------------------
		//Note 3
		/*
		Chack if the current value is above the over load current
		*/
		if(Vsub > OverLoadCurr && Vz > 500)					// see note 3
		{
			PWMA->CMR0=OverLoadDutyCycle<<4;                      		// Constant middle Duty cycle in overload state
			clr_all_panel();						// clear panel
			print_lcd(0 , "OVERLOAD!");		   		        // Massage on screen
			print_lcd(1 , "PLEASE CHECK IF");				// Massage on screen
			print_lcd(2 , "THE MOTOR IS");					// Massage on screen
			print_lcd(3 , "STUCK");						// Massage on screen
			DrvSYS_Delay(100000);						// Delay
		}			
		else									// Regular state
		PWMA->CMR0=ADC->ADDR[7].RSLT<<4;					// ADC value of the Potentiometer to the PWM output 
		sprintf(TEXT,"Curr: %.2f[A]",Vsub);                                     // Print Current value into "text"
		print_lcd(2, TEXT);							// Print Current value into LCD
		sprintf(TEXT,"PEDAL : %4d",Vz);                                         // Print Potentiometer value into "text"
		print_lcd(3, TEXT);							// Print Potentiometer value into LCD
    		DrvSYS_Delay(100000);							// Delay
		}
	}
}
	// The END [=
	// -----------------------------------------------------------------------------
