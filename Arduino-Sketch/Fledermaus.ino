/******************************************************************************************
 * Fledermausdetektor für den C-Turm      C-Hack/2020
 * 
 * Hardware: Feather M0 Proto Board mit ARM M0 und 350ksps ADC
 *           MEMS Mikrofon (ELV MEMS-1)  Knowles SPU0410LR5H 
 *           60dB Verstärkung mit TS912 Rail-to-Rail OpAmp
 *           
 * Software: ADC-DMA Funktionen (siehe unten)
 *           FFT Library
 *           
 * Return-Daten: Entweder detektierte Hochfrequenz (nur Frequenzangabe)
 *               Oder Roh FFT Daten
 *               Oder Roih Sampledaten
 *               
 ******************************************************************************************/
  int dataMode = '1';      // 0 = raw, 1 = FFT, 2 = Frequenzen

/*===========================================================================================
 * Adafruit FFT Library für SAMD21xx Prozessoren (ARM M0)
 *===========================================================================================*/
 #include "Adafruit_ZeroFFT.h"

/*=========================================================================================== 
 *  ADCDMA
 *  
 * analog A1
 *   
 * Nach http://www.atmel.com/Images/Atmel-42258-ASF-Manual-SAM-D21_AP-Note_AT07627.pdf pg 73
 * Urheber: https://github.com/manitou48/ZERO/blob/master/adcdma.ino 
 * 
 * Sampling Rate = 500ksps
 * Bins: Bin[HWORD/2] = 250kHz;
 * Umrechnung von bin Nr auf Frequenz: f(binNr) = 250 / HWORDS * binNr    [in kHz]
 *==========================================================================================*/
 
#define ADCPIN A1            // ADC Input Pin
#define HWORDS 2048          // Anzahl der Samples
uint16_t adcbuf[HWORDS];     // Datenarray (16bit Daten)
int16_t signal[HWORDS];
float bin, maxBin;


typedef struct {
    uint16_t btctrl;
    uint16_t btcnt;
    uint32_t srcaddr;
    uint32_t dstaddr;
    uint32_t descaddr;
} dmacdescriptor ;
volatile dmacdescriptor wrb[12] __attribute__ ((aligned (16)));
dmacdescriptor descriptor_section[12] __attribute__ ((aligned (16)));
dmacdescriptor descriptor __attribute__ ((aligned (16)));


static uint32_t chnl = 0;  // DMA channel
volatile uint32_t dmadone;

void DMAC_Handler() {
    // interrupts DMAC_CHINTENCLR_TERR DMAC_CHINTENCLR_TCMPL DMAC_CHINTENCLR_SUSP
    uint8_t active_channel;

    // disable irqs ?
    __disable_irq();
    active_channel =  DMAC->INTPEND.reg & DMAC_INTPEND_ID_Msk; // get channel number
    DMAC->CHID.reg = DMAC_CHID_ID(active_channel);
    dmadone = DMAC->CHINTFLAG.reg;
    DMAC->CHINTFLAG.reg = DMAC_CHINTENCLR_TCMPL; // clear
    DMAC->CHINTFLAG.reg = DMAC_CHINTENCLR_TERR;
    DMAC->CHINTFLAG.reg = DMAC_CHINTENCLR_SUSP;
    __enable_irq();
}


void dma_init() {
    // probably on by default
    PM->AHBMASK.reg |= PM_AHBMASK_DMAC ;
    PM->APBBMASK.reg |= PM_APBBMASK_DMAC ;
    NVIC_EnableIRQ( DMAC_IRQn ) ;

    DMAC->BASEADDR.reg = (uint32_t)descriptor_section;
    DMAC->WRBADDR.reg = (uint32_t)wrb;
    DMAC->CTRL.reg = DMAC_CTRL_DMAENABLE | DMAC_CTRL_LVLEN(0xf);
}

void adc_dma(void *rxdata,  size_t hwords) {
    uint32_t temp_CHCTRLB_reg;

    DMAC->CHID.reg = DMAC_CHID_ID(chnl);
    DMAC->CHCTRLA.reg &= ~DMAC_CHCTRLA_ENABLE;
    DMAC->CHCTRLA.reg = DMAC_CHCTRLA_SWRST;
    DMAC->SWTRIGCTRL.reg &= (uint32_t)(~(1 << chnl));
    temp_CHCTRLB_reg = DMAC_CHCTRLB_LVL(0) |
      DMAC_CHCTRLB_TRIGSRC(ADC_DMAC_ID_RESRDY) | DMAC_CHCTRLB_TRIGACT_BEAT;
    DMAC->CHCTRLB.reg = temp_CHCTRLB_reg;
    DMAC->CHINTENSET.reg = DMAC_CHINTENSET_MASK ; // enable all 3 interrupts
    dmadone = 0;
    descriptor.descaddr = 0;
    descriptor.srcaddr = (uint32_t) &ADC->RESULT.reg;
    descriptor.btcnt =  hwords;
    descriptor.dstaddr = (uint32_t)rxdata + hwords*2;   // end address
    descriptor.btctrl =  DMAC_BTCTRL_BEATSIZE_HWORD | DMAC_BTCTRL_DSTINC | DMAC_BTCTRL_VALID;
    memcpy(&descriptor_section[chnl],&descriptor, sizeof(dmacdescriptor));

    // start channel
    DMAC->CHID.reg = DMAC_CHID_ID(chnl);
    DMAC->CHCTRLA.reg |= DMAC_CHCTRLA_ENABLE;
}

static __inline__ void ADCsync() __attribute__((always_inline, unused));
static void   ADCsync() {
  while (ADC->STATUS.bit.SYNCBUSY == 1); //Just wait till the ADC is free
}


void adc_init(){
  analogRead(ADCPIN);  // do some pin init  pinPeripheral()
  ADC->CTRLA.bit.ENABLE = 0x00;             // Disable ADC
  ADCsync();
  //ADC->REFCTRL.bit.REFSEL = ADC_REFCTRL_REFSEL_INTVCC0_Val; //  2.2297 V Supply VDDANA
  //ADC->INPUTCTRL.bit.GAIN = ADC_INPUTCTRL_GAIN_1X_Val;      // Gain select as 1X
  ADC->INPUTCTRL.bit.GAIN = ADC_INPUTCTRL_GAIN_DIV2_Val;  // default
  ADC->REFCTRL.bit.REFSEL = ADC_REFCTRL_REFSEL_INTVCC1_Val;
  ADCsync();    //  ref 31.6.16
  ADC->INPUTCTRL.bit.MUXPOS = g_APinDescription[ADCPIN].ulADCChannelNumber;
  ADCsync();
  ADC->AVGCTRL.reg = 0x00 ;       //no averaging
  ADC->SAMPCTRL.reg = 0x00;  ; //sample length in 1/2 CLK_ADC cycles
  ADCsync();
  ADC->CTRLB.reg = ADC_CTRLB_PRESCALER_DIV16 | ADC_CTRLB_FREERUN | ADC_CTRLB_RESSEL_10BIT;
  ADCsync();
  ADC->CTRLA.bit.ENABLE = 0x01;
  ADCsync();
}



void setup(){
  int timeout;
  Serial.begin(115200);
  analogWriteResolution(10);
  analogWrite(A0,64);   // test with DAC 
  pinMode(LED_BUILTIN, OUTPUT);
  adc_init();
  dma_init();

  while (1==1) {
    timeout = 2000;
    
    if (Serial.available()) dataMode = Serial.read();
  
    digitalWrite(LED_BUILTIN, HIGH);
    adc_dma(adcbuf,HWORDS);           // Start des Samplings
    while((!dmadone) && (timeout>0)) {   // Warten, bis Sampling und DMA fertig (timeout notwendig)
      delay(1);
      timeout--;                  // Warten, bis Sampling und DMA fertig
    }
    digitalWrite(LED_BUILTIN, LOW);

    for (int i=0; i<HWORDS; i++) {    // Schreiben der Samples
      if (dataMode == '0') Serial.println(adcbuf[i]);
    }
 
    for (int i=0; i<HWORDS; i++) signal[i] = adcbuf[i]-512;     // DC Wert abziehen

    ZeroFFT(signal, HWORDS);          // FFT rechnen 
    for (int i=0; i<HWORDS/2; i++) {  // Schreiben der FFT Bins
      if (dataMode == '1') Serial.println(signal[i]);
    }
    if (dataMode == '1') Serial.println(32000);
    
    if (dataMode == '2') {
      Serial.print("Echo bei ");
      for (int i=0; i<HWORDS/2; i++) {
        if (signal[i]>10) {
          bin = i;
          maxBin = HWORDS;
          Serial.print(250000*bin/maxBin);
          Serial.print(" Hz, ");  
        }
      }
      Serial.println("."); 
    }
    delay(100);
  }
}


void loop() {
}
