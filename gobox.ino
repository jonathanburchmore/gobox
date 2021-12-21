#include <Adafruit_ST77xx.h>
#include <Adafruit_ST7789.h>
#include <Adafruit_ST7735.h>

#include <HT16K33.h>

// TFT
//////////////////////////////////////

#define TFT_CS  10
#define TFT_RST 9
#define TFT_DC  8

Adafruit_ST7735 tft = Adafruit_ST7735( TFT_CS, TFT_DC, TFT_RST );

// 7-Segment Display
//////////////////////////////////////

#define SEG_ID  0x70

HT16K33 seg( SEG_ID );

// Colors
//////////////////////////////////////

// Colors on this TFT are 18bit BGR, so the standard Adafruit GFX colors don't work properly

#define GOBOX_COLOR_WHITE   0B111111111111111111
#define GOBOX_COLOR_BLACK   0B000000000000000000
#define GOBOX_COLOR_RED     0B000000000000111111
#define GOBOX_COLOR_GREEN   0B000000111111000000
#define GOBOX_COLOR_BLUE    0B111111000000000000
#define GOBOX_COLOR_YELLOW  0B000000111111111111

// Display Data
//////////////////////////////////////

enum charging_status { off, noBatt, sleep, mppt, cc, cv, error, start } charging_status = off;

float sol_volts = 0.00;
float sol_amps = 0.00;

float bat_volts = 0.00;
float bat_amps = 0.00;

float ps_volts = 0.00;

float sol_watts = 0.00;

#define MAX_SAMPLES         160

#define GRAPH_HEIGHT        80
#define GRAPH_MAX_SOL_WATTS 100.0
#define GRAPH_MAX_BAT_VOLTS 14.6
#define GRAPH_MIN_BAT_VOLTS 10.0
#define GRAPH_MAX_BAT_AMPS  5.0

int sample_count = 0;
int next_sample = 0;

byte sol_watts_samples[ MAX_SAMPLES ], bat_volts_samples[ MAX_SAMPLES ], bat_amps_samples[ MAX_SAMPLES ];

// Functions
//////////////////////////////////////

void setup()
{
  int i, dir;
  
  Serial.begin( 9600 );

  Serial1.begin( 115200 );
  Serial1.setTimeout( 1000 );

  tft.initR( INITR_BLACKTAB );
  
  tft.setRotation( 1 );
  tft.setTextWrap( false );

  tft.fillScreen( GOBOX_COLOR_BLACK );

  seg.begin();
  Wire.setClock( 100000 );
  seg.displayOn();
}

int constrain_sample_pos( int sample_pos )
{
  if ( sample_pos >= MAX_SAMPLES )
  {
    return sample_pos - MAX_SAMPLES;
  }
  else if ( sample_pos < 0 )
  {
    return MAX_SAMPLES + sample_pos;
  }

  return sample_pos;
}

int sample_y( float percent )
{
  return 20 + ( GRAPH_HEIGHT - constrain( GRAPH_HEIGHT * percent, 0, GRAPH_HEIGHT ) );
}

void read_sample()
{
  String raw_data;
  unsigned int seconds;
  char charging[ 6 ];
  int pwm;
  float max_amps;
  int sol_volts_whole, sol_volts_fract;
  int sol_amps_whole, sol_amps_fract;
  int bat_volts_whole, bat_volts_fract;
  int bat_amps_whole, bat_amps_fract;
  int max_amps_whole, max_amps_fract;
  int ps_volts_whole, ps_volts_fract;
  int sol_watts_whole, sol_watts_fract;

  raw_data = Serial1.readStringUntil( '\r' );   // LLPP has incorrect \n\r line termination
  raw_data.trim();

  if ( raw_data.length() == 0 )
  {
    charging_status = off;

    sol_volts = 0.00;
    sol_amps = 0.00;

    bat_volts = 0.00;
    bat_amps = 0.00;

    ps_volts = 0.00;

    sol_watts = 0.00;
  }
  else
  {
    sscanf( raw_data.c_str(), "%u Charging = %s pwm = %d Vpv = %d.%d Ipv = %d.%d Vbatt = %d.%d Ibatt = %d.%d/%d.%d Vps = %d.%d Wpv = %d.%d",
            &seconds, charging, &pwm, &sol_volts_whole, &sol_volts_fract, &sol_amps_whole, &sol_amps_fract, &bat_volts_whole, &bat_volts_fract,
            &bat_amps_whole, &bat_amps_fract, &max_amps_whole, &max_amps_fract, &ps_volts_whole, &ps_volts_fract, &sol_watts_whole, &sol_watts_fract );

    sol_volts = sol_volts_whole + ( sol_volts_fract / 100.0 );
    sol_amps  = sol_amps_whole + ( sol_amps_fract / 100.0 );
    bat_volts = bat_volts_whole + ( bat_volts_fract / 100.0 );
    bat_amps  = bat_amps_whole + ( bat_amps_fract / 100.0 );
    max_amps  = max_amps_whole + ( max_amps_fract / 100.0 );
    ps_volts  = ps_volts_whole + ( ps_volts_fract / 100.0 );
    sol_watts = sol_watts_whole + ( sol_watts_fract / 100.0 );

    if ( !strcmp( charging, "noBat" ) )       charging_status = noBatt;
    else if ( !strcmp( charging, "sleep" ) )  charging_status = sleep;
    else if ( !strcmp( charging, "mppt" ) )   charging_status = mppt;
    else if ( !strcmp( charging, "cc" ) )     charging_status = cc;
    else if ( !strcmp( charging, "cv" ) )     charging_status = cv;
    else if ( !strcmp( charging, "start" ) )  charging_status = start;
    else                                      charging_status = error;
  }

  sol_watts_samples[ next_sample ]  = sample_y( sol_watts / GRAPH_MAX_SOL_WATTS );
  bat_volts_samples[ next_sample ]  = sample_y( ( bat_volts - GRAPH_MIN_BAT_VOLTS ) / ( GRAPH_MAX_BAT_VOLTS - GRAPH_MIN_BAT_VOLTS ) );
  bat_amps_samples[ next_sample ]   = sample_y( bat_amps / GRAPH_MAX_BAT_AMPS );

  if ( ++next_sample >= MAX_SAMPLES )
  {
    next_sample = 0;
  }

  if ( sample_count < MAX_SAMPLES )
  {
    sample_count++;
  }
}

void draw_header()
{
  tft.setCursor( 0, 0 );

  tft.setTextSize( 2 );
  tft.setTextColor( GOBOX_COLOR_WHITE, GOBOX_COLOR_BLACK );

  switch ( charging_status )
  {
    case off    : tft.print( " PwrPanel Off " );  break;
    case noBatt : tft.print( "  No Battery  " );  break;
    case sleep  : tft.print( "    Sleep     " );  break;
    case mppt   : tft.print( "    MPPT      " );  break;
    case cc     : tft.print( "Charging (CC) " );  break;
    case cv     : tft.print( "Charging (CV) " );  break;
    case error  : tft.print( "    Error     " );  break;
    case start  : tft.print( " Initializing " );  break;
  }
}

void draw_text_data()
{
  tft.setCursor( 0, 104 );

  tft.setTextSize( 1 );
  tft.setTextColor( GOBOX_COLOR_WHITE, GOBOX_COLOR_BLACK );

  // Solar
  tft.print( "Solar " );
  tft.print( sol_volts );
  tft.print( "v " );
  tft.print( sol_amps );
  tft.print( "a " );

  tft.setTextColor( GOBOX_COLOR_YELLOW, GOBOX_COLOR_BLACK );
  tft.print( sol_watts );
  tft.println( "w          " );
  tft.setTextColor( GOBOX_COLOR_WHITE, GOBOX_COLOR_BLACK );

  // Battery
  tft.print( "Battery " );

  tft.setTextColor( GOBOX_COLOR_BLUE, GOBOX_COLOR_BLACK );
  tft.print( bat_volts );
  tft.print( "v " );
  tft.setTextColor( GOBOX_COLOR_WHITE, GOBOX_COLOR_BLACK );

  tft.setTextColor( GOBOX_COLOR_RED, GOBOX_COLOR_BLACK );
  tft.print( bat_amps );
  tft.println( "a          " );
  tft.setTextColor( GOBOX_COLOR_WHITE, GOBOX_COLOR_BLACK );

  // Power Supply
  tft.print( "Power Supply " );
  tft.print( ps_volts );
  tft.println( "v          " );
}

void draw_graph()
{
  int x0, x1;
  int samples_drawn, sample0_pos, sample1_pos;
  
  tft.fillRect( 0, 20, MAX_SAMPLES, GRAPH_HEIGHT, GOBOX_COLOR_BLACK );

  // Since we are using lines, we cannot draw anything until we have two samples
  if ( sample_count < 2 )
  {
    return;
  }

  sample0_pos = constrain_sample_pos( next_sample - sample_count );
  sample1_pos = constrain_sample_pos( next_sample - sample_count + 1 );

  for ( x0 = 0, x1 = 1, samples_drawn = 1; samples_drawn < sample_count; x0++, x1++, samples_drawn++ )
  {
    tft.drawLine( x0, sol_watts_samples[ sample0_pos ], x1, sol_watts_samples[ sample1_pos ], GOBOX_COLOR_YELLOW );
    tft.drawLine( x0, bat_volts_samples[ sample0_pos ], x1, bat_volts_samples[ sample1_pos ], GOBOX_COLOR_BLUE );
    tft.drawLine( x0, bat_amps_samples[ sample0_pos ], x1, bat_amps_samples[ sample1_pos ], GOBOX_COLOR_RED );

    sample0_pos = sample1_pos;
    sample1_pos = constrain_sample_pos( sample1_pos + 1 );
  }
}

void update_voltmeter()
{
  seg.displayFloat( bat_volts, 2 );
}

void loop()
{
  read_sample();
  
  draw_header();
  draw_text_data();
  draw_graph();
  update_voltmeter();
}

#if 0

  charge_data[ 0 ] = 100;

  for ( i = 1, dir = -1; i < 160; i++ )
  {
    if ( charge_data[ i - 1 ] + dir <= 58 || charge_data[ i - 1 ] + dir >= 100 )
    {
      dir = -dir;
    }

    charge_data[ i ] = charge_data[ i - 1 ] + dir;
  }

  for ( i = 0; i < 160; i++ )
  {
    if ( ( i > 20 && i < 55 ) || ( i > 100 && i < 160 ) )
    {
      charge_data[ i ] = -charge_data[ i ];
    }
  }

  
  int datapoint, i;
  static int offset = 0;
  
  seg.displayFloat( 13.41 );

  tft.setCursor( 0, 0 );
  tft.print( "Solar" );
  tft.print( " " );
  tft.print( 60.12 );
  tft.println( "W             " );

  for ( datapoint = 0, i = offset; datapoint < 160; datapoint++, i++ )
  {
    if ( i >= 160 )
    {
      i = 0;
    }

    if ( charge_data[ i ] < 0 )  // Solar Amps
    {
      tft.drawFastVLine( datapoint, 127 + charge_data[ i ], abs( charge_data[ i ] ), GOBOX_COLOR_YELLOW );
      tft.drawFastVLine( datapoint, 27, 100 + charge_data[ i ] + 1, GOBOX_COLOR_BLACK );
    }
    else                  // Power Supply (or 0) Amps
    {
      tft.drawFastVLine( datapoint, 127 - charge_data[ i ], charge_data[ i ], GOBOX_COLOR_RED );
      tft.drawFastVLine( datapoint, 27, 100 - charge_data[ i ] + 1, GOBOX_COLOR_BLACK );
    }
  }

  if ( ++offset >= 160 )
  {
    offset = 0;
  }

  delay( 1000 );
#endif
