 /*---------------------------------------------------------------------------------*/
/* --- STC MCU International Limited -------------------------------------
*/
/* --- STC 15 Series MCU A/D Conversion Demo -----------------------
*/
/* --- Mobile: (86)13922805190 --------------------------------------------
*/
/* --- Fax: 86-755-82944243 -------------------------------------------------*/
/* --- Tel: 86-755-82948412 -------------------------------------------------
*/
/* --- Web: www.STCMCU.com --------------------------------------------
*/
/* If you want to use the program or the program referenced in the  ---*/
/* article, please specify in which data and procedures from STC    ---
*/
/*----------------------------------------------------------------------------------*/
#include "stc15.h"
#include "adc.h"

/*----------------------------
Get ADC result - 10 bit
----------------------------*/
uint16_t getADCResult(uint8_t chan)
{
    ADC_CONTR = ADC_POWER | ADC_SPEEDHH | ADC_START | chan;
	_nop_;       //Must wait before inquiry
	while (!(ADC_CONTR & ADC_FLAG));  //Wait complete flag
	ADC_CONTR &= ~ADC_FLAG;           //Close ADC
	return  ADC_RES << 2 | (ADC_RESL & 0b11);  //Return ADC result, 10 bits
}
