#include <FastLED.h>
#define     LIN_OUT             1           // Use the linear output function
#define     FHT_N               256         // Number of FFT bins
#include <FHT.h> 
#define COLOR_ORDER RGB
#define BRIGHTNESS 250
#define STRIPS 2
#define LEDS_PER_STRIP 150
CRGB  leds[STRIPS][LEDS_PER_STRIP];
int topAverages[50];
int topVariances[50];
int topZScores[50];
float bassScore;
float midScore;
float highScore;

int sensorPin = A0;    // select the input pin for the potentiometer
float mean = 400;
float variance = 30;
int reading = 0;
int zScore;


void setup() {
  int data[128];
//  index = data;
  delay(1000);
  FastLED.addLeds<WS2812, 8, GRB>(leds[0], LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
  FastLED.addLeds<WS2812, 9, GRB>(leds[1], LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
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
  
  for (uint8_t px = 0; px < 50; px++) {
     setLED(px, bassScore, midScore, highScore);
  }
  
  
  Serial.println(fht_lin_out[2]);
  FastLED.show();
  //Fire(55,120,15);
//  for(int i = 0; i < LEDS_PER_STRIP; i = i + 1){
//    doFHT();
//    setLED(leds[0], i, 255, 0, 0);
//    FastLED[0].showLeds(BRIGHTNESS);
//    FastLED[1].showLeds(BRIGHTNESS);
//    //delay(1);
//  }
//  for(int i = 0; i < LEDS_PER_STRIP; i = i + 1){
//    doFHT();
//    setLED(leds[0], i, 0, 0, 0);
//    FastLED[0].showLeds(BRIGHTNESS);
//    FastLED[1].showLeds(BRIGHTNESS);
//    //delay(1);
//  }
  delay(20);
}

inline void computeRanges(){
  bassScore = 0;
  for(int i = 0; i < 5; i++){
    bassScore += topAverages[i];
  }
  bassScore = bassScore / 5;
  
  midScore = 0;
  for( int i = 5; i < 25; i++){
    midScore += topAverages[i];
  }
  midScore = midScore / 20;
  
  highScore = 0;
  
  for(int i = 25; i < 50; i++){
    highScore += topAverages[i];
  }
  highScore = highScore / 20;
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
  mean = mean*0.9 + reading*0.1;
  variance = variance * 0.9 + abs(reading - mean)*0.1;
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

const TProgmemPalette16 myRedWhiteBluePalette_p PROGMEM =
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
    CRGB::Gray,
    CRGB::Gray
};
