#include "FastLED.h"
FASTLED_USING_NAMESPACE;

#include "GradientPalettes.h"

#include "application.h"

SYSTEM_THREAD(ENABLED);

// allow us to use itoa() in this scope
extern char* itoa(int a, char* buffer, unsigned char radix);

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

#define ONE_DAY_MILLIS (24 * 60 * 60 * 1000)

#define NUM_LEDS 50

CRGB leds[NUM_LEDS];

typedef uint8_t (*SimplePattern)();
typedef SimplePattern SimplePatternList[];
typedef struct { SimplePattern drawFrame;  String name; } PatternAndName;
typedef PatternAndName PatternAndNameList[];

// List of patterns to cycle through.  Each is defined as a separate function below.

const PatternAndNameList patterns = {
  { pride,                  "Pride" },
  { colorWaves,             "Color Waves" },
  { rainbowTwinkles,        "Rainbow Twinkles" },
  { snowTwinkles,           "Snow Twinkles" },
  { cloudTwinkles,          "Cloud Twinkles" },
  { incandescentTwinkles,   "Incandescent Twinkles" },
  { rainbow,                "Rainbow" },
  { rainbowWithGlitter,     "Rainbow With Glitter" },
  { rainbowSolid,           "Solid Rainbow" },
  { confetti,               "Confetti" },
  { sinelon,                "Sinelon" },
  { bpm,                    "Beat" },
  { juggle,                 "Juggle" },
  { fire,                   "Fire" },
  { water,                  "Water" },
  { showSolidColor,         "Solid Color" }
};

// variables exposed via Particle cloud API (Spark Core is limited to 10)
int brightness = 32;
int patternCount = ARRAY_SIZE(patterns);
int patternIndex = 0;
String patternName = "Pride";
int power = 1;
char variableValue[32] = "";

// variables exposed via the variableValue variable, via Particle Cloud API
int r = 0;
int g = 0;
int b = 255;

uint8_t gHue = 0; // rotating "base color" used by many of the patterns

CRGB solidColor = CRGB(r, g, b);

int offlinePin = D7;

CRGBPalette16 IceColors_p = CRGBPalette16(CRGB::Black, CRGB::Blue, CRGB::Aqua, CRGB::White);

uint8_t paletteIndex = 0;

// List of palettes to cycle through.
CRGBPalette16 palettes[] = {
  RainbowColors_p,
  RainbowStripeColors_p,
  CloudColors_p,
  OceanColors_p,
  ForestColors_p,
  HeatColors_p,
  LavaColors_p,
  PartyColors_p,
  IceColors_p,
};

uint8_t paletteCount = ARRAY_SIZE(palettes);

CRGBPalette16 currentPalette(CRGB::Black);
CRGBPalette16 targetPalette = palettes[paletteIndex];

// ten seconds per color palette makes a good demo
// 20-120 is better for deployment
#define SECONDS_PER_PALETTE 20

void setup() {
    FastLED.addLeds<WS2811, TX>(leds, NUM_LEDS);
    FastLED.setCorrection(Typical8mmPixel);
    FastLED.setBrightness(brightness);
    FastLED.setDither(false);
    fill_solid(leds, NUM_LEDS, solidColor);
    FastLED.show();

    // Serial.begin(9600);

    // load settings from EEPROM
    brightness = EEPROM.read(0);
    if(brightness < 1)
      brightness = 1;
    else if(brightness > 255)
      brightness = 255;

    FastLED.setBrightness(brightness);
    FastLED.setDither(brightness < 255);

    patternIndex = EEPROM.read(1);
    if(patternIndex < 0)
      patternIndex = 0;
    else if (patternIndex >= patternCount)
      patternIndex = patternCount - 1;

    r = EEPROM.read(2);
    g = EEPROM.read(3);
    b = EEPROM.read(4);

    if(r == 0 && g == 0 && b == 0) {
      r = 0;
      g = 0;
      b = 255;
    }

    solidColor = CRGB(r, b, g);

    Particle.function("patternIndex", setPatternIndex); // sets the current pattern index, changes to the pattern with the specified index
    Particle.function("pNameCursor", movePatternNameCursor); // moves the pattern name cursor to the specified index, allows getting a list of pattern names
    Particle.function("variable", setVariable); // sets the value of a variable, args are name:value
    Particle.function("varCursor", moveVariableCursor);

    Particle.variable("power", power);
    Particle.variable("brightness", brightness);
    Particle.variable("patternCount", patternCount);
    Particle.variable("patternIndex", patternIndex);
    Particle.variable("patternName", patternName);
    Particle.variable("variable", variableValue);
}

void loop() {
    if(power < 1) {
        fill_solid(leds, NUM_LEDS, CRGB::Black);
        FastLED.show();
        FastLED.delay(15);
        return;
    }

    uint8_t delay = patterns[patternIndex].drawFrame();

    // send the 'leds' array out to the actual LED strip
    FastLED.show();

    // insert a delay to keep the framerate modest
    FastLED.delay(delay);

    // blend the current palette to the next
    EVERY_N_MILLISECONDS(40) {
        nblendPaletteTowardPalette(currentPalette, targetPalette, 16);
    }

    EVERY_N_MILLISECONDS( 40 ) { gHue++; } // slowly cycle the "base color" through the rainbow

    // slowly change to a new palette
    EVERY_N_SECONDS(SECONDS_PER_PALETTE) {
      paletteIndex++;
      if (paletteIndex >= paletteCount) paletteIndex = 0;
      targetPalette = palettes[paletteIndex];
    };
}

uint8_t variableCursor = 0;

int moveVariableCursor(String args)
{
    if(args.startsWith("pwr")) {
        itoa(power, variableValue, 10);
        return power;
    }
    else if (args.startsWith("brt")) {
        itoa(brightness, variableValue, 10);
        return brightness;
    }
    else if (args.startsWith("r")) {
        itoa(r, variableValue, 10);
        return r;
    }
    else if (args.startsWith("g")) {
        itoa(g, variableValue, 10);
        return g;
    }
    else if (args.startsWith("b")) {
        itoa(b, variableValue, 10);
        return b;
    }

    return 0;
}

int setVariable(String args) {
    if(args.startsWith("pwr:")) {
        return setPower(args.substring(4));
    }
    else if (args.startsWith("brt:")) {
        return setBrightness(args.substring(4));
    }
    else if (args.startsWith("r:")) {
        r = parseByte(args.substring(2));
        solidColor.r = r;
        EEPROM.write(2, r);
        patternIndex = patternCount - 1;
        return r;
    }
    else if (args.startsWith("g:")) {
        g = parseByte(args.substring(2));
        solidColor.g = g;
        EEPROM.write(3, g);
        patternIndex = patternCount - 1;
        return g;
    }
    else if (args.startsWith("b:")) {
        b = parseByte(args.substring(2));
        solidColor.b = b;
        EEPROM.write(4, b);
        patternIndex = patternCount - 1;
        return b;
    }

    return -1;
}

int setPower(String args) {
    power = args.toInt();
    if(power < 0)
        power = 0;
    else if (power > 1)
        power = 1;

    return power;
}

int setBrightness(String args)
{
    brightness = args.toInt();
    if(brightness < 1)
        brightness = 1;
    else if(brightness > 255)
        brightness = 255;

    FastLED.setBrightness(brightness);
    FastLED.setDither(brightness < 255);

    EEPROM.write(0, brightness);

    return brightness;
}

byte parseByte(String args) {
    int c = args.toInt();
    if(c < 0)
        c = 0;
    else if (c > 255)
        c = 255;

    return c;
}

int setPatternIndex(String args)
{
    patternIndex = args.toInt();
    if(patternIndex < 0)
        patternIndex = 0;
    else if (patternIndex >= patternCount)
        patternIndex = patternCount - 1;

    EEPROM.write(1, patternIndex);

    return patternIndex;
}

int movePatternNameCursor(String args)
{
    int index = args.toInt();
    if(index < 0)
        index = 0;
    else if (index >= patternCount)
        index = patternCount - 1;

    patternName = patterns[index].name;

    return index;
}

uint8_t rainbow()
{
  // FastLED's built-in rainbow generator
  fill_rainbow( leds, NUM_LEDS, gHue, 255 / NUM_LEDS);
  return 8;
}

uint8_t rainbowWithGlitter()
{
  // built-in FastLED rainbow, plus some random sparkly glitter
  rainbow();
  addGlitter(80);
  return 8;
}

uint8_t rainbowSolid()
{
  fill_solid(leds, NUM_LEDS, CHSV(gHue, 255, 255));
  return 8;
}

uint8_t confetti()
{
  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy( leds, NUM_LEDS, 10);
  int pos = random16(NUM_LEDS);
  leds[pos] += CHSV( gHue + random8(64), 200, 255);
  return 8;
}

uint8_t sinelon()
{
  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy( leds, NUM_LEDS, 20);
  int pos = beatsin16(13,0,NUM_LEDS);
  leds[pos] += CHSV( gHue, 255, 192);
  return 8;
}

uint8_t bpm()
{
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t BeatsPerMinute = 62;
  uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);
  for( int i = 0; i < NUM_LEDS; i++) { //9948
    leds[i] = ColorFromPalette(currentPalette, gHue+(i*2), beat-gHue+(i*10));
  }

  return 8;
}

uint8_t juggle() {
  // eight colored dots, weaving in and out of sync with each other
  fadeToBlackBy( leds, NUM_LEDS, 20);
  byte dothue = 0;
  for( int i = 0; i < 8; i++) {
    leds[beatsin16(i+7,0,NUM_LEDS - 1)] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }

  return 8;
}

uint8_t fire() {
    heatMap(HeatColors_p, true);

    return 30;
}

uint8_t water() {
    heatMap(IceColors_p, false);

    return 30;
}

uint8_t showSolidColor() {
    fill_solid(leds, NUM_LEDS, solidColor);

    return 30;
}

// This function draws rainbows with an ever-changing,
// widely-varying set of parameters.
uint8_t pride()
{
  static uint16_t sPseudotime = 0;
  static uint16_t sLastMillis = 0;
  static uint16_t sHue16 = 0;

  uint8_t sat8 = beatsin88( 87, 220, 250);
  uint8_t brightdepth = beatsin88( 341, 96, 224);
  uint16_t brightnessthetainc16 = beatsin88( 203, (25 * 256), (40 * 256));
  uint8_t msmultiplier = beatsin88(147, 23, 60);

  uint16_t hue16 = sHue16;//gHue * 256;
  uint16_t hueinc16 = beatsin88(113, 1, 3000);

  uint16_t ms = millis();
  uint16_t deltams = ms - sLastMillis ;
  sLastMillis  = ms;
  sPseudotime += deltams * msmultiplier;
  sHue16 += deltams * beatsin88( 400, 5,9);
  uint16_t brightnesstheta16 = sPseudotime;

  for( uint16_t i = 0 ; i < NUM_LEDS; i++) {
    hue16 += hueinc16;
    uint8_t hue8 = hue16 / 256;

    brightnesstheta16  += brightnessthetainc16;
    uint16_t b16 = sin16( brightnesstheta16  ) + 32768;

    uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
    uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
    bri8 += (255 - brightdepth);

    CRGB newcolor = CHSV( hue8, sat8, bri8);

    uint16_t pixelnumber = i;
    pixelnumber = (NUM_LEDS-1) - pixelnumber;

    nblend( leds[pixelnumber], newcolor, 64);
  }

  return 15;
}

uint8_t radialPaletteShift()
{
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    // leds[i] = ColorFromPalette( currentPalette, gHue + sin8(i*16), brightness);
    leds[i] = ColorFromPalette(currentPalette, i + gHue, 255, LINEARBLEND);
  }

  return 8;
}

void heatMap(CRGBPalette16 palette, bool up) {
    fill_solid(leds, NUM_LEDS, CRGB::Black);

    // Add entropy to random number generator; we use a lot of it.
    random16_add_entropy(random(256));

    uint8_t cooling = 55;
    uint8_t sparking = 120;

    // Array of temperature readings at each simulation cell
    static const uint8_t halfLedCount = NUM_LEDS / 2;
    static byte heat[2][halfLedCount];

    byte colorindex;

    for(uint8_t x = 0; x < 2; x++) {
        // Step 1.  Cool down every cell a little
        for( int i = 0; i < halfLedCount; i++) {
          heat[x][i] = qsub8( heat[x][i],  random8(0, ((cooling * 10) / halfLedCount) + 2));
        }

        // Step 2.  Heat from each cell drifts 'up' and diffuses a little
        for( int k= halfLedCount - 1; k >= 2; k--) {
          heat[x][k] = (heat[x][k - 1] + heat[x][k - 2] + heat[x][k - 2] ) / 3;
        }

        // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
        if( random8() < sparking ) {
          int y = random8(7);
          heat[x][y] = qadd8( heat[x][y], random8(160,255) );
        }

        // Step 4.  Map from heat cells to LED colors
        for( int j = 0; j < halfLedCount; j++) {
            // Scale the heat value from 0-255 down to 0-240
            // for best results with color palettes.
            colorindex = scale8(heat[x][j], 240);

            CRGB color = ColorFromPalette(palette, colorindex);

            if(up) {
                if(x == 0) {
                    leds[(halfLedCount - 1) - j] = color;
                }
                else {
                    leds[halfLedCount + j] = color;
                }
            }
            else {
                if(x == 0) {
                    leds[j] = color;
                }
                else {
                    leds[(NUM_LEDS - 1) - j] = color;
                }
            }
        }
    }
}

// scale the brightness of all pixels down
void dimAll(byte value)
{
  for (int i = 0; i < NUM_LEDS; i++){
    leds[i].nscale8(value);
  }
}

void addGlitter( uint8_t chanceOfGlitter)
{
  if( random8() < chanceOfGlitter) {
    leds[ random16(NUM_LEDS) ] += CRGB::White;
  }
}

///////////////////////////////////////////////////////////////////////

// Forward declarations of an array of cpt-city gradient palettes, and
// a count of how many there are.  The actual color palette definitions
// are at the bottom of this file.
extern const TProgmemRGBGradientPalettePtr gGradientPalettes[];
extern const uint8_t gGradientPaletteCount;

// Current palette number from the 'playlist' of color palettes
uint8_t gCurrentPaletteNumber = 0;

CRGBPalette16 gCurrentPalette( CRGB::Black);
CRGBPalette16 gTargetPalette( gGradientPalettes[0] );


uint8_t colorWaves()
{
  EVERY_N_SECONDS( SECONDS_PER_PALETTE ) {
    gCurrentPaletteNumber = addmod8( gCurrentPaletteNumber, 1, gGradientPaletteCount);
    gTargetPalette = gGradientPalettes[ gCurrentPaletteNumber ];
  }

  EVERY_N_MILLISECONDS(40) {
    nblendPaletteTowardPalette( gCurrentPalette, gTargetPalette, 16);
  }

  colorwaves( leds, NUM_LEDS, gCurrentPalette);

  return 20;
}


// This function draws color waves with an ever-changing,
// widely-varying set of parameters, using a color palette.
void colorwaves( CRGB* ledarray, uint16_t numleds, CRGBPalette16& palette)
{
  static uint16_t sPseudotime = 0;
  static uint16_t sLastMillis = 0;
  static uint16_t sHue16 = 0;

  // uint8_t sat8 = beatsin88( 87, 220, 250);
  uint8_t brightdepth = beatsin88( 341, 96, 224);
  uint16_t brightnessthetainc16 = beatsin88( 203, (25 * 256), (40 * 256));
  uint8_t msmultiplier = beatsin88(147, 23, 60);

  uint16_t hue16 = sHue16;//gHue * 256;
  uint16_t hueinc16 = beatsin88(113, 300, 1500);

  uint16_t ms = millis();
  uint16_t deltams = ms - sLastMillis ;
  sLastMillis  = ms;
  sPseudotime += deltams * msmultiplier;
  sHue16 += deltams * beatsin88( 400, 5,9);
  uint16_t brightnesstheta16 = sPseudotime;

  for( uint16_t i = 0 ; i < numleds; i++) {
    hue16 += hueinc16;
    uint8_t hue8 = hue16 / 256;
    uint16_t h16_128 = hue16 >> 7;
    if( h16_128 & 0x100) {
      hue8 = 255 - (h16_128 >> 1);
    } else {
      hue8 = h16_128 >> 1;
    }

    brightnesstheta16  += brightnessthetainc16;
    uint16_t b16 = sin16( brightnesstheta16  ) + 32768;

    uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
    uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
    bri8 += (255 - brightdepth);

    uint8_t index = hue8;
    //index = triwave8( index);
    index = scale8( index, 240);

    CRGB newcolor = ColorFromPalette( palette, index, bri8);

    uint16_t pixelnumber = i;
    pixelnumber = (numleds-1) - pixelnumber;

    nblend( ledarray[pixelnumber], newcolor, 128);
  }
}

// Alternate rendering function just scrolls the current palette
// across the defined LED strip.
void palettetest( CRGB* ledarray, uint16_t numleds, const CRGBPalette16& gCurrentPalette)
{
  static uint8_t startindex = 0;
  startindex--;
  fill_palette( ledarray, numleds, startindex, (256 / NUM_LEDS) + 1, gCurrentPalette, 255, LINEARBLEND);
}

uint8_t cloudTwinkles()
{
  gCurrentPalette = CloudColors_p; // Blues and whites!
  colortwinkles();
  return 20;
}

uint8_t rainbowTwinkles()
{
  gCurrentPalette = RainbowColors_p; // Blues and whites!
  colortwinkles();
  return 20;
}

uint8_t snowTwinkles()
{
  CRGB w(85,85,85), W(CRGB::White);

  gCurrentPalette = CRGBPalette16( W,W,W,W, w,w,w,w, w,w,w,w, w,w,w,w );
  colortwinkles();
  return 20;
}

uint8_t incandescentTwinkles()
{
  CRGB l(0xE1A024);

  gCurrentPalette = CRGBPalette16( l,l,l,l, l,l,l,l, l,l,l,l, l,l,l,l );
  colortwinkles();
  return 20;
}

#define STARTING_BRIGHTNESS 64
#define FADE_IN_SPEED       32
#define FADE_OUT_SPEED      20
#define DENSITY            255

enum { GETTING_DARKER = 0, GETTING_BRIGHTER = 1 };

void colortwinkles()
{
  // Make each pixel brighter or darker, depending on
  // its 'direction' flag.
  brightenOrDarkenEachPixel( FADE_IN_SPEED, FADE_OUT_SPEED);

  // Now consider adding a new random twinkle
  if( random8() < DENSITY ) {
    int pos = random16(NUM_LEDS);
    if( !leds[pos]) {
      leds[pos] = ColorFromPalette( gCurrentPalette, random8(), STARTING_BRIGHTNESS, NOBLEND);
      setPixelDirection(pos, GETTING_BRIGHTER);
    }
  }
}

void brightenOrDarkenEachPixel( fract8 fadeUpAmount, fract8 fadeDownAmount)
{
 for( uint16_t i = 0; i < NUM_LEDS; i++) {
    if( getPixelDirection(i) == GETTING_DARKER) {
      // This pixel is getting darker
      leds[i] = makeDarker( leds[i], fadeDownAmount);
    } else {
      // This pixel is getting brighter
      leds[i] = makeBrighter( leds[i], fadeUpAmount);
      // now check to see if we've maxxed out the brightness
      if( leds[i].r == 255 || leds[i].g == 255 || leds[i].b == 255) {
        // if so, turn around and start getting darker
        setPixelDirection(i, GETTING_DARKER);
      }
    }
  }
}

CRGB makeBrighter( const CRGB& color, fract8 howMuchBrighter)
{
  CRGB incrementalColor = color;
  incrementalColor.nscale8( howMuchBrighter);
  return color + incrementalColor;
}

CRGB makeDarker( const CRGB& color, fract8 howMuchDarker)
{
  CRGB newcolor = color;
  newcolor.nscale8( 255 - howMuchDarker);
  return newcolor;
}

// Compact implementation of
// the directionFlags array, using just one BIT of RAM
// per pixel.  This requires a bunch of bit wrangling,
// but conserves precious RAM.  The cost is a few
// cycles and about 100 bytes of flash program memory.
uint8_t  directionFlags[ (NUM_LEDS+7) / 8];

bool getPixelDirection( uint16_t i) {
  uint16_t index = i / 8;
  uint8_t  bitNum = i & 0x07;

  uint8_t  andMask = 1 << bitNum;
  return (directionFlags[index] & andMask) != 0;
}

void setPixelDirection( uint16_t i, bool dir) {
  uint16_t index = i / 8;
  uint8_t  bitNum = i & 0x07;

  uint8_t  orMask = 1 << bitNum;
  uint8_t andMask = 255 - orMask;
  uint8_t value = directionFlags[index] & andMask;
  if( dir ) {
    value += orMask;
  }
  directionFlags[index] = value;
}
