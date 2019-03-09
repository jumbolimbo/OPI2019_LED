#include <FastLED.h>
#define     LIN_OUT             1           // Use the linear output function
#define     FHT_N               256         // Number of FFT bins
#include <FHT.h> 
#define COLOR_ORDER RGB
#define BRIGHTNESS 200
#define STRIPS 2
#define LEDS_PER_STRIP 450
CRGB  leds[STRIPS][LEDS_PER_STRIP];
int topAverages[50];
int topVariances[50];
int topZScores[50];
float volumeScore;
float bassScore = 40;
float midScore = 36;
float highScore = 32;
float budget = 128;

float volumeScoreMean = 40 + 36 + 32;
float bassScoreMean = 40;
float midScoreMean = 36;
float highScoreMean = 32;

int sensorPin = A0;    // select the input pin for the potentiometer
float mean = 400;
float variance = 30;
int reading = 0;
int zScore;

extern const TProgmemPalette16 capAmerica_p PROGMEM;
extern CRGBPalette16 hulk_p;

void setup() {
  delay(1000);
  FastLED.addLeds<WS2812B, 8, GRB>(leds[0], LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
  FastLED.addLeds<WS2812B, 9, GRB>(leds[1], LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
  Serial.begin(9600);
  
  TIMSK0 = 0; // turn off timer0 for lower jitter
  ADCSRA = 0xe5; // set the adc to free running mode
  ADMUX = 0x40; // use adc0
  DIDR0 = 0x01; // turn off the digital input for adc0
}

void loop() {
  
  doFHT();
  computeFFTMoving();
  fht_lin_out[0] = 0;
  fht_lin_out[1] = 0;
  fht_lin_out[2] = 0;
  computeRanges();
  computeRangeMoving();
  Serial.print(bassScoreMean);
  Serial.print(" , ");
  Serial.print(midScoreMean);
  Serial.print(" , ");
  Serial.println(highScoreMean);
  bassScore = budget*(bassScore/volumeScore);
  midScore = budget*(midScore/volumeScore);
  highScore = budget*(highScore/volumeScore);

  if(budget < 16 && volumeScoreMean > 0){
    volumeScoreMean = volumeScoreMean*0.99+0.01;
  } else if (budget > 512){
    volumeScoreMean = volumeScoreMean*1.01 - 0.01;
  }
   static uint8_t startIndex = 0;
    startIndex = startIndex + 1;
  
  
  wholeStripEffect(0,250);
  //FillLEDsFromPaletteColors(0,450, startIndex, capAmerica_p, NOBLEND);
  
  
  FastLED.show();
  delay(10);
}
inline void wholeStripEffect(int low, int high){
  for (uint8_t px = low; px < high; px+= 2) {
     setLED(px, max(min(bassScore,255),0), max(min(midScore,255),0), max(min(highScore,255),0));
  }
}


void FillLEDsFromPaletteColors(int low, int high, uint8_t colorIndex, CRGBPalette16 palette, TBlendType blending)
{
    uint8_t brightness = 255;
    
    for( int i = low; i < high; i++) {
        leds[0][i] = ColorFromPalette( palette, colorIndex, brightness, blending);
        colorIndex += 1;
    }
}

inline void computeRanges(){
  bassScore = 0;
  for(int i = 5; i < 10; i++){
    bassScore += topAverages[i];
  }
  bassScore = bassScore / 10;
  
  midScore = 0;
  for( int i = 10; i < 20; i++){
    midScore += topAverages[i];
  }
  midScore = midScore / 10;
  
  highScore = 0;
  
  for(int i = 20; i < 50; i++){
    highScore += topAverages[i];
  }
  highScore = highScore / 10;

  volumeScore = highScore + midScore + bassScore;
  
}

inline void computeRangeMoving(){
  budget = 0.5 * 128 + 6*(volumeScore - volumeScoreMean) + 0.5 * budget;
  bassScoreMean = bassScoreMean*0.9999 + bassScore*0.0001;
  midScoreMean = midScoreMean*0.9999 + midScore*0.0001;
  highScoreMean = highScoreMean*0.9999 + highScore*0.0001;
  volumeScoreMean = volumeScoreMean*0.99 + volumeScore*0.01;
}


inline void readAnalog(int updates){
  for (int i = 0 ; i < updates ; i++) { 
  // Wait for ADC to be ready - ADIF bit is set
    while (!(ADCSRA & 0x10)); 
            
    ADCSRA = 0xf5;      // Restart the ADC (start another conversion)
            
    byte m = ADCL;      // Get the ADC data word
    byte j = ADCH;
    int k = (j << 8) | m;   // Convert to an int
    k -= 0x0200;            // then a signed int
    k <<= 6;                // and finally a 16-bit signed int
            
    fht_input[i] = k;   // Insert into FHT input array
  }
}

inline void doFHT(){
  cli();                  // Disable interrupts
    
  // Acquire FHT input - `FHT_N` number of samples
  readAnalog(FHT_N);
        
    // Do the FHT:
    fht_window();       // Window the data for better frequency response
    fht_reorder();      // Reorder the data before doing the FHT
    fht_run();          // Process the data in the FHT
    fht_mag_lin();      // Take the output of the FHT    
    sei();        
}

inline void computeFFTMoving(){
  for(int i = 0; i < 50; i++){
    topAverages[i] = topAverages[i]*0.7 + fht_lin_out[i]*0.3;
    topVariances[i] = topVariances[i] * 0.7 + abs(fht_lin_out[i] - topAverages[i])*0.3;
    topZScores[i] = abs( (fht_lin_out[i] - topAverages[i])/topVariances[i]);
  }
}

inline void computeMoving(){
  readAnalog(1);
  reading = fht_input[0];
  mean = mean*0.9999 + reading*0.0001;
  variance = variance * 0.9999 + abs(reading - mean)*0.0001;
  zScore = abs( (reading - mean)/variance);
}


inline void setLED(int led, uint8_t r, uint8_t g, uint8_t b){
  leds[0][led].r = r;
  leds[0][led].g = g;
  leds[0][led].b = b;
  leds[1][led].r = r;
  leds[1][led].g = g;
  leds[1][led].b = b;
}

inline void setLED(CRGB strip[], int led, uint8_t r, uint8_t g, uint8_t b){
  strip[led].r = r;
  strip[led].g = g;
  strip[led].b = b;
}

const TProgmemPalette16 capAmerica_p PROGMEM =
{
    CRGB::Red,
    CRGB::Gray, // 'white' is too bright compared to red and blue
    CRGB::Blue,
    CRGB::Black,
    
    CRGB::Red,
    CRGB::Gray,
    CRGB::Blue,
    CRGB::Black,
    
    CRGB::Red,
    CRGB::Red,
    CRGB::Gray,
    CRGB::Gray,
    CRGB::Blue,
    CRGB::Blue,
    CRGB::Black,
    CRGB::Black
};

CRGB purple = CHSV( HUE_PURPLE, 255, 255);
    CRGB green  = CHSV( HUE_GREEN, 255, 255);
    CRGB black  = CRGB::Black;
    
CRGBPalette16 hulk_p = CRGBPalette16(
                                   green,  green,  black,  black,
                                   purple, purple, black,  black,
                                   green,  green,  black,  black,
                                   purple, purple, black,  black );
