#include <math.h>
#include <string.h>
#include "ch.h"
#include "hal.h"
#include "chprintf.h"
#include "nanovna.h"

#define __DRAW_Z__



#define SWAP(x,y) { int z=x; x = y; y = z; }

static void cell_draw_marker_info(int m, int n, int w, int h);
void frequency_string(char *buf, size_t len, int32_t freq);
void markmap_all_markers(void);
uint16_t cell_drawstring_8x8_var(int w, int h, char *str, int x, int y, uint16_t fg, uint8_t invert);
uint16_t cell_drawstring_size(int w, int h, const char *str, int x, int y, uint16_t fg, uint16_t bg, uint8_t size);

void request_to_draw_cells_behind_biginfo(void);

//#define GRID_COLOR 0x0863
//uint16_t grid_color = 0x1084;

/* indicate dirty cells */
static uint16_t markmap[2][8];
static uint16_t current_mappage = 0;

static int32_t fgrid = 50000000;
static int16_t grid_offset;
static int16_t grid_width;

int area_width = AREA_WIDTH_NORMAL;
int area_height = HEIGHT;

#define GRID_RECTANGULAR (1<<0)
#define GRID_SMITH       (1<<1)
#define GRID_ADMIT       (1<<2)
#define GRID_POLAR       (1<<3)


#define CELLWIDTH 32
#define CELLHEIGHT 32

/*
 * CELL_X0[27:31] cell position
 * CELL_Y0[22:26]
 * CELL_N[10:21] original order
 * CELL_X[5:9] position in the cell
 * CELL_Y[0:4]
 */
static uint32_t trace_index[TRACE_COUNT][POINT_COUNT];

#define INDEX(x, y, n) \
  ((((x)&0x03e0UL)<<22) | (((y)&0x03e0UL)<<17) | (((n)&0x0fffUL)<<10)  \
 | (((x)&0x1fUL)<<5) | ((y)&0x1fUL))

#define CELL_X(i) (int)((((i)>>5)&0x1f) | (((i)>>22)&0x03e0))
#define CELL_Y(i) (int)(((i)&0x1f) | (((i)>>17)&0x03e0))
#define CELL_N(i) (int)(((i)>>10)&0xfff)

#define CELL_X0(i) (int)(((i)>>22)&0x03e0)
#define CELL_Y0(i) (int)(((i)>>17)&0x03e0)

#define CELL_P(i, x, y) (((((x)&0x03e0UL)<<22) | (((y)&0x03e0UL)<<17)) == ((i)&0xffc00000UL))


void update_grid(void)
{
  int32_t gdigit = 100000000;
  int32_t fstart, fspan;
  int32_t grid;
  if (frequency1 > 0) {
    fstart = frequency0;
    fspan = frequency1 - frequency0;
  } else {
    fspan = -frequency1;
    fstart = frequency0 - fspan/2;
  }
  
  while (gdigit > 100) {
    grid = 5 * gdigit;
    if (fspan / grid >= 4)
      break;
    grid = 2 * gdigit;
    if (fspan / grid >= 4)
      break;
    grid = gdigit;
    if (fspan / grid >= 4)
      break;
    gdigit /= 10;
  }
  fgrid = grid;

  grid_offset = (WIDTH-1) * ((fstart % fgrid) / 100) / (fspan / 100);
  grid_width = (WIDTH-1) * (fgrid / 100) / (fspan / 1000);

  force_set_markmap();
  redraw_request |= REDRAW_FREQUENCY;
}

static int circle_inout(int x, int y, int r)
{
  int d = x*x + y*y - r*r;
  if (d <= -r)
    return 1;
  if (d > r)
    return -1;
  return 0;
}


#define P_CENTER_X 146
#define P_CENTER_Y 116
#define P_RADIUS 116

static int polar_grid(int x, int y)
{
  int c = config.grid_color;
  int d;

  // offset to center
  x -= P_CENTER_X;
  y -= P_CENTER_Y;

  // outer circle
  d = circle_inout(x, y, P_RADIUS);
  if (d < 0) return 0;
  if (d == 0) return c;

  // vertical and horizontal axis
  if (x == 0 || y == 0)
    return c;

  d = circle_inout(x, y, P_RADIUS / 5);
  if (d == 0) return c;
  if (d > 0) return 0;

  d = circle_inout(x, y, P_RADIUS * 2 / 5);
  if (d == 0) return c;
  if (d > 0) return 0;

  // cross sloping lines
  if (x == y || x == -y)
    return c;

  d = circle_inout(x, y, P_RADIUS * 3 / 5);
  if (d == 0) return c;
  if (d > 0) return 0;

  d = circle_inout(x, y, P_RADIUS * 4 / 5);
  if (d == 0) return c;
  return 0;
}

/*
 * Constant Resistance circle: (u - r/(r+1))^2 + v^2 = 1/(r+1)^2
 * Constant Reactance circle:  (u - 1)^2 + (v-1/x)^2 = 1/x^2
 */
static int smith_grid(int x, int y)
{
  int c = config.grid_color;
  int d;

  // offset to center
  x -= P_CENTER_X;
  y -= P_CENTER_Y;
  
  // outer circle
  d = circle_inout(x, y, P_RADIUS);
  if (d < 0)
    return 0;
  if (d == 0)
    return c;
  
  // horizontal axis
  if (y == 0)
    return c;

  // shift circle center to right origin
  x -= P_RADIUS;

  // Constant Reactance Circle: 2j : R/2 = 58
  if (circle_inout(x, y+58, 58) == 0)
    return c;
  if (circle_inout(x, y-58, 58) == 0)
    return c;

  // Constant Resistance Circle: 3 : R/4 = 29
  d = circle_inout(x+29, y, 29);
  if (d > 0) return 0;
  if (d == 0) return c;

  // Constant Reactance Circle: 1j : R = 116
  if (circle_inout(x, y+116, 116) == 0)
    return c;
  if (circle_inout(x, y-116, 116) == 0)
    return c;

  // Constant Resistance Circle: 1 : R/2 = 58
  d = circle_inout(x+58, y, 58);
  if (d > 0) return 0;
  if (d == 0) return c;

  // Constant Reactance Circle: 1/2j : R*2 = 232
  if (circle_inout(x, y+232, 232) == 0)
    return c;
  if (circle_inout(x, y-232, 232) == 0)
    return c;

  // Constant Resistance Circle: 1/3 : R*3/4 = 87
  if (circle_inout(x+87, y, 87) == 0)
    return c;
  return 0;
}

//static int smith_grid2(int x, int y, float scale)
//{
//  int c = config.grid_color;
//  int d;
//
//  // offset to center
//  x -= P_CENTER_X;
//  y -= P_CENTER_Y;
//  
//  // outer circle
//  d = circle_inout(x, y, P_RADIUS);
//  if (d < 0)
//    return 0;
//  if (d == 0)
//    return c;
//
//  // shift circle center to right origin
//  x -= P_RADIUS * scale;
//
//  // Constant Reactance Circle: 2j : R/2 = 58
//  if (circle_inout(x, y+58*scale, 58*scale) == 0)
//    return c;
//  if (circle_inout(x, y-58*scale, 58*scale) == 0)
//    return c;
//#if 0
//  // Constant Resistance Circle: 3 : R/4 = 29
//  d = circle_inout(x+29*scale, y, 29*scale);
//  if (d > 0) return 0;
//  if (d == 0) return c;
//  d = circle_inout(x-29*scale, y, 29*scale);
//  if (d > 0) return 0;
//  if (d == 0) return c;
//#endif
//
//  // Constant Reactance Circle: 1j : R = 116
//  if (circle_inout(x, y+116*scale, 116*scale) == 0)
//    return c;
//  if (circle_inout(x, y-116*scale, 116*scale) == 0)
//    return c;
//
//  // Constant Resistance Circle: 1 : R/2 = 58
//  d = circle_inout(x+58*scale, y, 58*scale);
//  if (d > 0) return 0;
//  if (d == 0) return c;
//  d = circle_inout(x-58*scale, y, 58*scale);
//  if (d > 0) return 0;
//  if (d == 0) return c;
//
//  // Constant Reactance Circle: 1/2j : R*2 = 232
//  if (circle_inout(x, y+232*scale, 232*scale) == 0)
//    return c;
//  if (circle_inout(x, y-232*scale, 232*scale) == 0)
//    return c;
//
//#if 0
//  // Constant Resistance Circle: 1/3 : R*3/4 = 87
//  d = circle_inout(x+87*scale, y, 87*scale);
//  if (d > 0) return 0;
//  if (d == 0) return c;
//  d = circle_inout(x+87*scale, y, 87*scale);
//  if (d > 0) return 0;
//  if (d == 0) return c;
//#endif
//
//  // Constant Resistance Circle: 0 : R
//  d = circle_inout(x+P_RADIUS*scale, y, P_RADIUS*scale);
//  if (d > 0) return 0;
//  if (d == 0) return c;
//  d = circle_inout(x-P_RADIUS*scale, y, P_RADIUS*scale);
//  if (d > 0) return 0;
//  if (d == 0) return c;
//
//  // Constant Resistance Circle: -1/3 : R*3/2 = 174
//  d = circle_inout(x+174*scale, y, 174*scale);
//  if (d > 0) return 0;
//  if (d == 0) return c;
//  d = circle_inout(x-174*scale, y, 174*scale);
//  //if (d > 0) return 0;
//  if (d == 0) return c;
//  return 0;
//}

static const int cirs[][4] = {
  { 0, 58/2, 58/2, 0 },    // Constant Reactance Circle: 2j : R/2 = 58
  { 29/2, 0, 29/2, 1 },    // Constant Resistance Circle: 3 : R/4 = 29
  { 0, 116/2, 116/2, 0 },  // Constant Reactance Circle: 1j : R = 116
  { 58/2, 0, 58/2, 1 },    // Constant Resistance Circle: 1 : R/2 = 58
  { 0, 232/2, 232/2, 0 },  // Constant Reactance Circle: 1/2j : R*2 = 232
  { 87/2, 0, 87/2, 1 },    // Constant Resistance Circle: 1/3 : R*3/4 = 87
  { 0, 464/2, 464/2, 0 },  // Constant Reactance Circle: 1/4j : R*4 = 464
  { 116/2, 0, 116/2, 1 },  // Constant Resistance Circle: 0 : R
  { 174/2, 0, 174/2, 1 },  // Constant Resistance Circle: -1/3 : R*3/2 = 174
  { 0, 0, 0, 0 } // sentinel
};  

static int smith_grid3(int x, int y)
{
  int c = config.grid_color;
  int d;

  // offset to center
  x -= P_CENTER_X;
  y -= P_CENTER_Y;
  
  // outer circle
  d = circle_inout(x, y, P_RADIUS);
  if (d < 0)
    return 0;
  if (d == 0)
    return c;

  // shift circle center to right origin
  x -= P_RADIUS /2;

  int i;
  for (i = 0; cirs[i][2]; i++) {
    d = circle_inout(x+cirs[i][0], y+cirs[i][1], cirs[i][2]);
    if (d == 0)
      return c;
    if (d > 0 && cirs[i][3])
      return 0;
    d = circle_inout(x-cirs[i][0], y-cirs[i][1], cirs[i][2]);
    if (d == 0)
      return c;
    if (d > 0 && cirs[i][3])
      return 0;
  }
  return 0;
}

//static int rectangular_grid(int x, int y)
//{
//  int c = config.grid_color;
//  //#define FREQ(x) (((x) * (fspan / 1000) / (WIDTH-1)) * 1000 + fstart)
//  //int32_t n = FREQ(x-1) / fgrid;
//  //int32_t m = FREQ(x) / fgrid;
//  //if ((m - n) > 0)
//  //if (((x * 6) % (WIDTH-1)) < 6)
//  //if (((x - grid_offset) % grid_width) == 0)
//  if (x == 0 || x == WIDTH-1)
//    return c;
//  if ((y % GRIDY) == 0)
//    return c;
//  if ((((x + grid_offset) * 10) % grid_width) < 10)
//    return c;
//  return 0;
//}

static int rectangular_grid_x(int x)
{
  int c = config.grid_color;
  if (x < 0)
    return 0;
  if (x == 0 || x == WIDTH)
    return c;
  if ((((x + grid_offset) * 10) % grid_width) < 10)
    return c;
  return 0;
}

static int rectangular_grid_y(int y)
{
  int c = config.grid_color;
  if (y < 0)
    return 0;
  if ((y % GRIDY) == 0)
    return c;
  return 0;
}

//static int set_strut_grid(int x)
//{
//  uint16_t *buf = spi_buffer;
//  int y;
//
//  for (y = 0; y < HEIGHT; y++) {
//    int c = rectangular_grid(x, y);
//    c |= smith_grid(x, y);
//    *buf++ = c;
//  }
//  return y;
//}
//
//static void draw_on_strut(int v0, int d, int color)
//{
//  int v;
//  int v1 = v0 + d;
//  if (v0 < 0) v0 = 0;
//  if (v1 < 0) v1 = 0;
//  if (v0 >= HEIGHT) v0 = HEIGHT-1;
//  if (v1 >= HEIGHT) v1 = HEIGHT-1;
//  if (v0 == v1) {
//    v = v0; d = 2;
//  } else if (v0 < v1) {
//    v = v0; d = v1 - v0 + 1;
//  } else {
//    v = v1; d = v0 - v1 + 1;
//  }
//  while (d-- > 0)
//    spi_buffer[v++] |= color;
//}

/*
 * calculate log10(abs(gamma))
 */ 
static float logmag(float *v)
{
  return log10f(v[0]*v[0] + v[1]*v[1]) * 10;
}

/*
 * calculate phase[-2:2] of coefficient
 */ 
static float phase(float *v)
{
  return 2 * atan2f(v[1], v[0]) / M_PI * 90;
}

/*
 * calculate group_delay = -deltaAngle(gamma) / (deltaf * 360)
 */ 
static float group_delay(float gamma[POINT_COUNT][2], uint32_t* freq, int count, int index)
{
    float *v, *w;
    float deltaf;
    if (index == count-1) {
        deltaf = freq[index] - freq[index-1];
        v = gamma[index-1];
        w = gamma[index];
    }
    else {
        deltaf = freq[index+1] - freq[index];
        v = gamma[index];
        w = gamma[index+1];
    }
    // w = w[0]/w[1]
    // v = v[0]/v[1]
    // atan(w)-atan(v) = atan((w-v)/(1+wv))
    float r = w[0]*v[1] - w[1]*v[0];
    float i = w[0]*v[0] + w[1]*v[1];
    return atan2f(r, i) / (2 * M_PI * deltaf);
}

/*
 * calculate abs(gamma)
 */
static float linear(float *v)
{
  return - sqrtf(v[0]*v[0] + v[1]*v[1]);
}

/*
 * calculate vswr; (1+gamma)/(1-gamma)
 */ 
static float swr(float *v)
{
  float x = sqrtf(v[0]*v[0] + v[1]*v[1]);
  if (x > 1)
    return INFINITY;
  return (1 + x)/(1 - x);
}

static float resitance(float *v) {
  float z0 = 50;
  float d = z0 / ((1-v[0])*(1-v[0])+v[1]*v[1]);
  float zr = ((1+v[0])*(1-v[0]) - v[1]*v[1]) * d;
  return zr;
}

static float reactance(float *v) {
  float z0 = 50;
  float d = z0 / ((1-v[0])*(1-v[0])+v[1]*v[1]);
  float zi = 2*v[1] * d;
  return zi;
}

#define RADIUS ((HEIGHT-1)/2)
static void cartesian_scale(float re, float im, int *xp, int *yp, float scale)
{
  //float scale = 4e-3;
  int x = (int)(re * RADIUS * scale);
  int y = (int)(im * RADIUS * scale);
  if (x < -RADIUS) x = -RADIUS;
  if (y < -RADIUS) y = -RADIUS;
  if (x > RADIUS) x = RADIUS;
  if (y > RADIUS) y = RADIUS;
  *xp = WIDTH/2 + x;
  *yp = HEIGHT/2 - y;
}


static uint32_t trace_into_index(
    int x, int t, int i, 
    float coeff[POINT_COUNT][2], 
    uint32_t freq[POINT_COUNT], 
    int point_count)
{
  int y = 0;
  float v = 0;
  float refpos = 8 - get_trace_refpos(t);
  float scale = 1 / get_trace_scale(t);
  switch (trace[t].type) {
  case TRC_LOGMAG:
    v = refpos - logmag(coeff[i]) * scale;
    break;
  case TRC_PHASE:
    v = refpos - phase(coeff[i]) * scale;
    break;
  case TRC_DELAY:
    v = refpos - group_delay(coeff, freq, point_count, i) * scale;
    break;
  case TRC_LINEAR:
    v = refpos + linear(coeff[i]) * scale;
    break;
  case TRC_SWR:
    v = refpos+ (1 - swr(coeff[i])) * scale;
    break;
  case TRC_REAL:
    v = refpos - coeff[i][0] * scale;
    break;
  case TRC_IMAG:
    v = refpos - coeff[i][1] * scale;
    break;
  case TRC_R:
    v = refpos - resitance(coeff[i]) * scale;
    break;
  case TRC_X:
    v = refpos - reactance(coeff[i]) * scale;
    break;
  case TRC_SMITH:
  //case TRC_ADMIT:
  case TRC_POLAR:
    cartesian_scale(coeff[i][0], coeff[i][1], &x, &y, scale);
    return INDEX(x +CELLOFFSETX, y, i);
    break;
  }
  if (v < 0) v = 0;
  if (v > 8) v = 8;
  y = (int)(v * GRIDY);
  return INDEX(x +CELLOFFSETX, y, i);
}

static int string_value_with_prefix(char *buf, int len, float val, char unit)
{
  char prefix;
  int n;
  if (val < 0) {
    val = -val;
    *buf++ = '-';
    len--;
  }
  if (val < 1e-12) {
    prefix = 'f';
    val *= 1e15;
  } else if (val < 1e-9) {
    prefix = 'p';
    val *= 1e12;
  } else if (val < 1e-6) {
    prefix = 'n';
    val *= 1e9;
  } else if (val < 1e-3) {
    prefix = S_MICRO[0];
    val *= 1e6;
  } else if (val < 1) {
    prefix = 'm';
    val *= 1e3;
  } else if (val < 1e3) {
    prefix = 0;
  } else if (val < 1e6) {
    prefix = 'k';
    val /= 1e3;
  } else if (val < 1e9) {
    prefix = 'M';
    val /= 1e6;
  } else {
    prefix = 'G';
    val /= 1e9;
  }

  if (val < 10) {
    n = chsnprintf(buf, len, "%.2f", val);
  } else if (val < 100) {
    n = chsnprintf(buf, len, "%.1f", val);
  } else {
    n = chsnprintf(buf, len, "%d", (int)val);
  }

  if (prefix)
    buf[n++] = prefix;
  if (unit)
    buf[n++] = unit;
  buf[n] = '\0';
  return n;
}


#define PI2 6.283184

static void gamma2imp(char *buf, int len, const float coeff[2], uint32_t frequency)
{
  // z = (gamma+1)/(gamma-1) * z0
  float z0 = 50;
  float d = z0 / ((1-coeff[0])*(1-coeff[0])+coeff[1]*coeff[1]);
  float zr = ((1+coeff[0])*(1-coeff[0]) - coeff[1]*coeff[1]) * d;
  float zi = 2*coeff[1] * d;
  int n;

  n = string_value_with_prefix(buf, len, zr, S_OHM[0]);
  buf[n++] = ' ';

  if (zi < 0) {
    float c = -1 / (PI2 * frequency * zi);
    string_value_with_prefix(buf+n, len-n, c, 'F');
  } else {
    float l = zi / (PI2 * frequency);
    string_value_with_prefix(buf+n, len-n, l, 'H');
  }
}

static void gamma2resistance(char *buf, int len, const float coeff[2])
{
  float z0 = 50;
  float d = z0 / ((1-coeff[0])*(1-coeff[0])+coeff[1]*coeff[1]);
  float zr = ((1+coeff[0])*(1-coeff[0]) - coeff[1]*coeff[1]) * d;
  string_value_with_prefix(buf, len, zr, S_OHM[0]);
}

static void gamma2reactance(char *buf, int len, const float coeff[2])
{
  float z0 = 50;
  float d = z0 / ((1-coeff[0])*(1-coeff[0])+coeff[1]*coeff[1]);
  float zi = 2*coeff[1] * d;
  string_value_with_prefix(buf, len, zi, S_OHM[0]);
}

static void trace_get_value_string(
    int t, char *buf, int len,
    int i, float coeff[POINT_COUNT][2], 
    uint32_t freq[POINT_COUNT], 
    int point_count)
{
  float v;
  switch (trace[t].type) {
  case TRC_LOGMAG:
    v = logmag(coeff[i]);
    if (v == -INFINITY)
      chsnprintf(buf, len, "-INF dB");
    else
      chsnprintf(buf, len, "%.2fdB", v);
    break;
  case TRC_PHASE:
    v = phase(coeff[i]);
    chsnprintf(buf, len, "%.3f" S_DEGREE, v);
    break;
  case TRC_DELAY:
    v = group_delay(coeff, freq, point_count, i);
    string_value_with_prefix(buf, len, v, 's');
    break;
  case TRC_LINEAR:
    v = linear(coeff[i]);
    chsnprintf(buf, len, "%.3f", v);
    break;
  case TRC_SWR:
    v = swr(coeff[i]);
    chsnprintf(buf, len, "%.2f", v);
    break;
  case TRC_SMITH:
    gamma2imp(buf, len, coeff[i], freq[i]);
    break;
  case TRC_REAL:
    chsnprintf(buf, len, "%.3f", coeff[i][0]);
    break;
  case TRC_IMAG:
    chsnprintf(buf, len, "%.3fj", coeff[i][1]);
    break;
  case TRC_R:
    gamma2resistance(buf, len, coeff[i]);
    break;
  case TRC_X:
    gamma2reactance(buf, len, coeff[i]);
    break;
  //case TRC_ADMIT:
  case TRC_POLAR:
    chsnprintf(buf, len, "%.3f %.3fj", coeff[i][0], coeff[i][1]);
    break;
  }
}

void trace_get_info(int t, char *buf, int len)
{
  const char *type = get_trace_typename(t);
  int n;
  switch (trace[t].type) {
  case TRC_LOGMAG:
    chsnprintf(buf, len, "%s %ddB/", type, (int)get_trace_scale(t));
    break;
  case TRC_PHASE:
    chsnprintf(buf, len, "%s %d" S_DEGREE "/", type, (int)get_trace_scale(t));
    break;
  case TRC_SMITH:
  //case TRC_ADMIT:
  case TRC_POLAR:
    chsnprintf(buf, len, "%s %.1fFS", type, get_trace_scale(t));
    break;
  default:
    n = chsnprintf(buf, len, "%s ", type);
    string_value_with_prefix(buf+n, len-n, get_trace_scale(t), '/');
    break;
  }
}

static float time_of_index(int idx) {
   return 1.0 / (float)(frequencies[1] - frequencies[0]) / (float)FFT_SIZE * idx;
}

static float distance_of_index(int idx) {
#define SPEED_OF_LIGHT 299792458
   float distance = ((float)idx * (float)SPEED_OF_LIGHT) / ( (float)(frequencies[1] - frequencies[0]) * (float)FFT_SIZE * 2.0);
   return distance * (velocity_factor / 100.0);
}


static inline void mark_map(int x, int y)
{
  if (y >= 0 && y < 8 && x >= 0 && x < 16)
    markmap[current_mappage][y] |= 1<<x;
}

static inline int is_mapmarked(int x, int y)
{
  uint16_t bit = 1<<x;
  return (markmap[0][y] & bit) || (markmap[1][y] & bit);
}

static inline void markmap_upperarea(void)
{
  markmap[current_mappage][0] |= 0xffff;
}

static inline void swap_markmap(void)
{
  current_mappage = 1 - current_mappage;
}

static inline void clear_markmap(void)
{
  memset(markmap[current_mappage], 0, sizeof markmap[current_mappage]);
}

inline void force_set_markmap(void)
{
  memset(markmap[current_mappage], 0xff, sizeof markmap[current_mappage]);
}

static void mark_cells_from_index(void)
{
  int t;
  /* mark cells between each neighber points */
  for (t = 0; t < TRACE_COUNT; t++) {
    if (!trace[t].enabled)
      continue;
    int x0 = CELL_X(trace_index[t][0]);
    int y0 = CELL_Y(trace_index[t][0]);
    int m0 = x0 >> 5;
    int n0 = y0 >> 5;
    int i;
    mark_map(m0, n0);
    for (i = 1; i < sweep_points; i++) {
      int x1 = CELL_X(trace_index[t][i]);
      int y1 = CELL_Y(trace_index[t][i]);
      int m1 = x1 >> 5;
      int n1 = y1 >> 5;
      while (m0 != m1 || n0 != n1) {
        if (m0 == m1) {
          if (n0 < n1) n0++; else n0--;
        } else if (n0 == n1) {
          if (m0 < m1) m0++; else m0--;
        } else {
          int x = (m0 < m1) ? (m0 + 1)<<5 : m0<<5;
          int y = (n0 < n1) ? (n0 + 1)<<5 : n0<<5;
          int sgn = (n0 < n1) ? 1 : -1;
          if (sgn*(y-y0)*(x1-x0) < sgn*(x-x0)*(y1-y0)) {
            if (m0 < m1) m0++;
            else m0--;
          } else {
            if (n0 < n1) n0++;
            else n0--;
          }
        }
        mark_map(m0, n0);
      }
      x0 = x1;
      y0 = y1;
      m0 = m1;
      n0 = n1;
    }
  }
}

void plot_into_index(float measured[2][POINT_COUNT][2])
{
  int i, t;
  for (i = 0; i < sweep_points; i++) {
    int x = i * (WIDTH-1) / (sweep_points-1);
    for (t = 0; t < TRACE_COUNT; t++) {
      if (!trace[t].enabled)
        continue;
      int n = trace[t].channel;
      trace_index[t][i] = trace_into_index(
        x, t, i,
        measured[n], frequencies, sweep_points);
    }
  }
#if 0
  for (t = 0; t < TRACE_COUNT; t++)
    if (trace[t].enabled && trace[t].polar)
      quicksort(trace_index[t], 0, sweep_points);
#endif

  mark_cells_from_index();
  markmap_all_markers();
}

//static const uint8_t INSIDE = 0x00;
static const uint8_t LEFT   = 0x01;
static const uint8_t RIGHT  = 0x02;
static const uint8_t BOTTOM = 0x04;
static const uint8_t TOP    = 0x08;

static inline uint8_t _compute_outcode(int w, int h, int x, int y)
{
    uint8_t code = 0;
    if (x < 0) {
        code |= LEFT;
    } else
    if (x > w) {
        code |= RIGHT;
    }
    if (y < 0) {
        code |= BOTTOM;
    } else
    if (y > h) {
        code |= TOP;
    }
    return code;
}

static void cell_drawline(int w, int h, int x0, int y0, int x1, int y1, int c)
{
    uint8_t outcode0 = _compute_outcode(w, h, x0, y0);
    uint8_t outcode1 = _compute_outcode(w, h, x1, y1);

    if (outcode0 & outcode1) {
        // this line is out of requested area. early return
        return;
    }

    if (x0 > x1) {
        SWAP(x0, x1);
        SWAP(y0, y1);
    }

    int dx = x1 - x0;
    int dy = y1 - y0;
    int sy = dy > 0 ? 1 : -1;
    int e = 0;

    dy *= sy;

    if (dx >= dy) {
        e = dy * 2 - dx;
        while (x0 != x1) {
            if (y0 >= 0 && y0 < h && x0 >= 0 && x0 < w)  spi_buffer[y0*w+x0] |= c;
            x0++;
            e += dy * 2;
            if (e >= 0) {
                e -= dx * 2;
                y0 += sy;
            }
        }
        if (y0 >= 0 && y0 < h && x0 >= 0 && x0 < w)  spi_buffer[y0*w+x0] |= c;
    } else {
        e = dx * 2 - dy;
        while (y0 != y1) {
            if (y0 >= 0 && y0 < h && x0 >= 0 && x0 < w)  spi_buffer[y0*w+x0] |= c;
            y0 += sy;
            e += dx * 2;
            if (e >= 0) {
                e -= dy * 2;
                x0++;
            }
        }
        if (y0 >= 0 && y0 < h && x0 >= 0 && x0 < w)  spi_buffer[y0*w+x0] |= c;
    }
}

//static int search_index_range(int x, int y, uint32_t index[POINT_COUNT], int *i0, int *i1)
//{
//  int i, j;
//  int head = 0;
//  int tail = sweep_points;
//  i = 0;
//  x &= 0x03e0;
//  y &= 0x03e0;
//  while (head < tail) {
//    i = (head + tail) / 2;
//    if (x < CELL_X0(index[i]))
//      tail = i+1;
//    else if (x > CELL_X0(index[i]))
//      head = i;
//    else if (y < CELL_Y0(index[i]))
//      tail = i+1;
//    else if (y > CELL_Y0(index[i]))
//      head = i;
//    else
//      break;
//  }
//
//  if (x != CELL_X0(index[i]) || y != CELL_Y0(index[i]))
//    return FALSE;
//    
//  j = i;
//  while (j > 0 && x == CELL_X0(index[j-1]) && y == CELL_Y0(index[j-1]))
//    j--;
//  *i0 = j;
//  j = i;
//  while (j < 100 && x == CELL_X0(index[j+1]) && y == CELL_Y0(index[j+1]))
//    j++;
//  *i1 = j;
//  return TRUE;
//}

static int search_index_range_x(int x, uint32_t index[POINT_COUNT], int *i0, int *i1)
{
  int i, j;
  int head = 0;
  int tail = sweep_points;
  x &= 0x03e0;
  i = 0;
  while (head < tail) {
    i = (head + tail) / 2;
    if (x == CELL_X0(index[i]))
      break;
    else if (x < CELL_X0(index[i])) {
      if (tail == i+1)
        break;
      tail = i+1;      
    } else {
      if (head == i)
        break;
      head = i;
    }
  }

  if (x != CELL_X0(index[i]))
    return FALSE;

  j = i;
  while (j > 0 && x == CELL_X0(index[j-1]))
    j--;
  *i0 = j;
  j = i;
  while (j < 100 && x == CELL_X0(index[j+1]))
    j++;
  *i1 = j;
  return TRUE;
}

static void draw_refpos(int w, int h, int x, int y, int c)
{
  // draw triangle
  int i, j;
  if (y < -3 || y > 32 + 3)
    return;
  for (j = 0; j < 3; j++) {
    int j0 = 6 - j*2;
    for (i = 0; i < j0; i++) {
      int x0 = x + i-5;
      int y0 = y - j;
      int y1 = y + j;
      if (y0 >= 0 && y0 < h && x0 >= 0 && x0 < w)
        spi_buffer[y0*w+x0] = c;
      if (j != 0 && y1 >= 0 && y1 < h && x0 >= 0 && x0 < w)
        spi_buffer[y1*w+x0] = c;
    }
  }
}


static void cell_draw_refpos(int m, int n, int w, int h)
{
  int x0 = m * CELLWIDTH;
  int y0 = n * CELLHEIGHT;
  int t;
  for (t = 0; t < TRACE_COUNT; t++) {
    if (!trace[t].enabled)
      continue;
    if (trace[t].type == TRC_SMITH || trace[t].type == TRC_POLAR)
      continue;
    int x = 0 - x0 +CELLOFFSETX;
    int y = 8*GRIDY - (int)(get_trace_refpos(t) * GRIDY) - y0;
    if (x > -5 && x < w && y >= -3 && y < h+3)
      draw_refpos(w, h, x, y, config.trace_color[t]);
  }
}

static void draw_marker(int w, int h, int x, int y, int c, int ch)
{
  int i, j;
  for (j = 10; j >= 0; j--) {
    int j0 = j / 2;
    for (i = -j0; i <= j0; i++) {
      int x0 = x + i;
      int y0 = y - j;
      int cc = c;
      if (j <= 9 && j > 2 && i >= -1 && i <= 3) {
        uint8_t bits = x5x7_bits[(ch * 7) + (9-j)];
        if (bits & (0x80>>(i+1)))
          cc = 0;
      }
      if (y0 >= 0 && y0 < h && x0 >= 0 && x0 < w)
        spi_buffer[y0*w+x0] = cc;
    }
  }
}

void marker_position(int m, int t, int *x, int *y)
{
    uint32_t index = trace_index[t][markers[m].index];
    *x = CELL_X(index);
    *y = CELL_Y(index);
}

int search_nearest_index(int x, int y, int t)
{
  uint32_t *index = trace_index[t];
  int min_i = -1;
  int min_d = 1000;
  int i;
  for (i = 0; i < sweep_points; i++) {
    int16_t dx = x - CELL_X(index[i]) - OFFSETX;
    int16_t dy = y - CELL_Y(index[i]) - OFFSETY;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    if (dx > 20 || dy > 20)
      continue;
    int d = dx*dx + dy*dy;
    if (d < min_d) {
      min_i = i;
    }
  }

  return min_i;
}

static void cell_draw_markers(int m, int n, int w, int h)
{
  int x0 = m * CELLWIDTH;
  int y0 = n * CELLHEIGHT;
  int t, i;
  for (i = 0; i < MARKER_COUNT; i++) {
    if (!markers[i].enabled)
      continue;
    for (t = 0; t < TRACE_COUNT; t++) {
      if (!trace[t].enabled)
        continue;
      uint32_t index = trace_index[t][markers[i].index];
      int x = CELL_X(index) - x0;
      int y = CELL_Y(index) - y0;
      if (x > -6 && x < w+6 && y >= 0 && y < h+12)
        draw_marker(w, h, x, y, config.trace_color[t], '1' + i);
    }
  }
}

static void markmap_marker(int marker)
{
    int t;
    if (!markers[marker].enabled)
        return;
    for (t = 0; t < TRACE_COUNT; t++) {
        if (!trace[t].enabled)
            continue;
        uint32_t index = markers[marker].index;
        if (index >= POINT_COUNT)
            continue;
        index = trace_index[t][index];
        int x = CELL_X(index);
        int y = CELL_Y(index);
        int m = x>>5;
        int n = y>>5;
        mark_map(m, n);
        if ((x&31) < 6)
            mark_map(m-1, n);
        if ((x&31) > 32-6)
            mark_map(m+1, n);
        if ((y&31) < 12) {
            mark_map(m, n-1);
            if ((x&31) < 6)
                mark_map(m-1, n-1);
            if ((x&31) > 32-6)
                mark_map(m+1, n-1);
        }
    }
}

static void markmap_all_markers(void)
{
  int i;
  for (i = 0; i < MARKER_COUNT; i++) {
    if (!markers[i].enabled)
      continue;
    markmap_marker(i);
  }
  markmap_upperarea();
}


static void draw_cell(int m, int n)
{
  int x0 = m * CELLWIDTH;
  int y0 = n * CELLHEIGHT;
  int x0off = x0 - CELLOFFSETX;
  int w = CELLWIDTH;
  int h = CELLHEIGHT;
  int x, y;
  int i0, i1;
  int i;
  int t;
  char buf[24];

  if (x0off + w > area_width)
    w = area_width - x0off;
  if (y0 + h > area_height)
    h = area_height - y0;
  if (w <= 0 || h <= 0)
    return;

  chMtxLock(&mutex_ili9341); // [protect spi_buffer]
  uint16_t grid_mode = 0;
  for (t = 0; t < TRACE_COUNT; t++) {
    if (!trace[t].enabled)
      continue;

    if (trace[t].type == TRC_SMITH)
      grid_mode |= GRID_SMITH;
    //else if (trace[t].type == TRC_ADMIT)
    //  grid_mode |= GRID_ADMIT;
    else if (trace[t].type == TRC_POLAR)
      grid_mode |= GRID_POLAR;
    else
      grid_mode |= GRID_RECTANGULAR;
  }

  PULSE;
  /* draw grid */
  if (grid_mode & GRID_RECTANGULAR) {
    for (x = 0; x < w; x++) {
      uint16_t c = rectangular_grid_x(x+x0off);
      for (y = 0; y < h; y++)
        spi_buffer[y * w + x] = c;
    }
    for (y = 0; y < h; y++) {
      uint16_t c = rectangular_grid_y(y+y0);
      for (x = 0; x < w; x++)
        if (x+x0off >= 0 && x+x0off <= WIDTH)
          spi_buffer[y * w + x] |= c;
    }
  } else {
    memset(spi_buffer, 0, sizeof spi_buffer);
  }
  if (grid_mode & (GRID_SMITH|GRID_ADMIT|GRID_POLAR)) {
    for (y = 0; y < h; y++) {
      for (x = 0; x < w; x++) {
        uint16_t c = 0;
        if (grid_mode & GRID_SMITH)
          c = smith_grid(x+x0off, y+y0);
        else if (grid_mode & GRID_ADMIT)
          c = smith_grid3(x+x0off, y+y0);
        //c = smith_grid2(x+x0, y+y0, 0.5);
        else if (grid_mode & GRID_POLAR)
          c = polar_grid(x+x0off, y+y0);
        spi_buffer[y * w + x] |= c;
      }
    }
  }
  PULSE;



  /* draw large ch0 infos */
  int cxpos = 15, cypos = 50;
  cxpos -= m * CELLWIDTH - CELLOFFSETX;
  cypos -= n * CELLHEIGHT;
  
  
  
  if ( (biginfo_enabled != FALSE) && (active_marker >= 0) )
  {
    float *coeff = measured[0][ markers[active_marker].index ];
    float v;
    v = swr(coeff);
      
    chsnprintf(buf, sizeof(buf), "CH0 Marker %d:", active_marker + 1);
    cell_drawstring_size(w, h, buf, cxpos, cypos+=30, 0x0000, 0xffff, 3);


    chsnprintf(buf, sizeof(buf), "SWR 1:%.2f", v);
    cell_drawstring_size(w, h, buf, cxpos, cypos+=30, 0xffff, 0x000, 3);

    frequency_string(buf, sizeof(buf), frequencies[ markers[active_marker].index ]);
    cell_drawstring_size(w, h, buf, cxpos, cypos+=30, 0xffff, 0x0000, 3);

    gamma2imp(buf, sizeof(buf), coeff, frequencies[ markers[active_marker].index ]);
    cell_drawstring_size(w, h, buf, cxpos, cypos+=30, 0xffff, 0x0000, 3);
    
    request_to_draw_cells_behind_biginfo();

      
  }


#if 1
  /* draw rectanglar plot */
  for (t = 0; t < TRACE_COUNT; t++) {
    if (!trace[t].enabled)
      continue;
    if (trace[t].type == TRC_SMITH || trace[t].type == TRC_POLAR)
      continue;
    
    if (search_index_range_x(x0, trace_index[t], &i0, &i1)) {
      if (i0 > 0)
        i0--;
      if (i1 < POINT_COUNT-1)
        i1++;
      for (i = i0; i < i1; i++) {
        int x1 = CELL_X(trace_index[t][i]);
        int x2 = CELL_X(trace_index[t][i+1]);
        int y1 = CELL_Y(trace_index[t][i]);
        int y2 = CELL_Y(trace_index[t][i+1]);
        int c = config.trace_color[t];
        cell_drawline(w, h, x1 - x0, y1 - y0, x2 - x0, y2 - y0, c);
      }
    }
  }
#endif
#if 1
  /* draw polar plot */
  for (t = 0; t < TRACE_COUNT; t++) {
    int c = config.trace_color[t];
    if (!trace[t].enabled)
      continue;
    if (trace[t].type != TRC_SMITH && trace[t].type != TRC_POLAR)
      continue;

    for (i = 1; i < sweep_points; i++) {
      //uint32_t index = trace_index[t][i];
      //uint32_t pindex = trace_index[t][i-1];
      //if (!CELL_P(index, x0, y0) && !CELL_P(pindex, x0, y0))
      //  continue;
      int x1 = CELL_X(trace_index[t][i-1]);
      int x2 = CELL_X(trace_index[t][i]);
      int y1 = CELL_Y(trace_index[t][i-1]);
      int y2 = CELL_Y(trace_index[t][i]);
      cell_drawline(w, h, x1 - x0, y1 - y0, x2 - x0, y2 - y0, c);
    }
  }
#endif

  PULSE;
  //draw marker symbols on each trace
  cell_draw_markers(m, n, w, h);
  // draw trace and marker info on the top
  cell_draw_marker_info(m, n, w, h);
  PULSE;

  if (m == 0)
    cell_draw_refpos(m, n, w, h);

  ili9341_bulk(OFFSETX + x0off, OFFSETY + y0, w, h);
  chMtxUnlock(&mutex_ili9341); // [/protect spi_buffer]
}

static void draw_all_cells(bool flush_markmap)
{
  int m, n;
  for (m = 0; m < (area_width+CELLWIDTH-1) / CELLWIDTH; m++)
    for (n = 0; n < (area_height+CELLHEIGHT-1) / CELLHEIGHT; n++) {
      if (is_mapmarked(m, n))
        draw_cell(m, n);
    }

  if (flush_markmap) {
    // keep current map for update
    swap_markmap();
    // clear map for next plotting
    clear_markmap();
  }
}

void draw_all(bool flush)
{
    if (redraw_request & REDRAW_CELLS)
        draw_all_cells(flush);
    if (redraw_request & REDRAW_FREQUENCY)
        draw_frequencies();
    if (redraw_request & REDRAW_CAL_STATUS)
        draw_cal_status();
    redraw_request = 0;
}

void redraw_marker(int marker, int update_info)
{
  // mark map on new position of marker
  markmap_marker(marker);

  // mark cells on marker info
  if (update_info)
    markmap[current_mappage][0] = 0xffff;

  draw_all_cells(TRUE);
}

void request_to_draw_cells_behind_menu(void)
{
  int n, m;
  for (m = 7; m <= 9; m++)
    for (n = 0; n < 8; n++)
      mark_map(m, n);
  redraw_request |= REDRAW_CELLS;
}

void request_to_draw_cells_behind_numeric_input(void)
{
  int n, m;
  for (m = 0; m <= 9; m++)
    for (n = 6; n < 8; n++)
      mark_map(m, n);
  redraw_request |= REDRAW_CELLS;
}




void
request_to_draw_cells_behind_biginfo(void)
{
  int n, m;
  for (m = 0; m <= 9; m++)
    for (n = 2; n < 7; n++)
      mark_map(m, n);
  redraw_request |= REDRAW_CELLS;
}



uint16_t
cell_drawchar_8x8(int w, int h, uint8_t ch, int x, int y, uint16_t fg, uint8_t var, uint8_t invert)
{
  uint8_t bits;
  int c, r;
  if (y <= -7 || y >= h || x <= -5 || x >= w)
    return;
  for(c = 0; c < 7; c++) {
    if ((y + c) < 0 || (y + c) >= h)
      continue;
    bits = x5x7_bits[(ch * 7) + c];
    if (invert)
      bits = ~bits;
    for (r = 0; r < 5; r++) {
      if ((x+r) >= 0 && (x+r) < w && (0x80 & bits)) 
        spi_buffer[(y+c)*w + (x+r)] = fg;
      bits <<= 1;
    }
  }
}



uint16_t
cell_drawchar_size(int w, int h, uint8_t ch, int x, int y, uint16_t fg, uint16_t bg, uint8_t size)
{
  uint8_t bits;
  uint16_t charwidthpx;
  uint8_t cline, ccol;

  ch = x8x8_map_char_table(ch);

  charwidthpx = x8x8_len[ch] * size;
  
  if ( y <= -(8*size) || y >= h || x <= -(charwidthpx) || x >= w )
    return charwidthpx;


  for (cline = 0; cline < (8*size); cline++) 
  {
    if ((y + cline) < 0 || (y + cline) >= h)
      continue;
      
    bits = x8x8_bits[ch][cline/size];
    for (ccol = 0; ccol < charwidthpx; ccol++)     
    {
      if ( (x+ccol) >= 0 && (x+ccol) < w ) 
        spi_buffer[(y+cline)*w + (x+ccol)] = (0x80 & bits) ? fg : bg;
  
      if (ccol % size == (size-1))
        bits <<= 1;
    }
  }

  return charwidthpx;
}



uint16_t
cell_drawstring_8x8(int w, int h, char *str, int x, int y, uint16_t fg, uint8_t invert)
{
  while (*str) {
    cell_drawchar_5x7(w, h, *str, x, y, fg, FALSE);
    x += 5;
    str++;
  }
}

static void cell_drawstring_invert_5x7(int w, int h, char *str, int x, int y, uint16_t fg, int invert)
{
  while (*str) {
    cell_drawchar_5x7(w, h, *str, x, y, fg, invert);
    x += 5;
    str++;
  }
}



uint16_t
cell_drawstring_size(int w, int h, const char *str, int x, int y, uint16_t fg, uint16_t bg, uint8_t size)
{
  unsigned char clength = 0;
  uint16_t strwidthpx = 0;
 
  while (*str) 
  {
    clength = cell_drawchar_size(w, h, *str, x, y, fg, bg, size);
    x += clength;
    strwidthpx += clength;
    str++;
  }

  return strwidthpx;

}



static void
cell_draw_marker_info(int m, int n, int w, int h)
{
  char buf[24];
  int t;
  if (n != 0)
    return;
  if (active_marker < 0)
    return;
  int idx = markers[active_marker].index;
  int j = 0;
  for (t = 0; t < TRACE_COUNT; t++) {
    if (!trace[t].enabled)
      continue;
    int xpos = 1 + (j%2)*146;
    int ypos = 1 + (j/2)*7;
    xpos -= m * CELLWIDTH -CELLOFFSETX;
    ypos -= n * CELLHEIGHT;
    chsnprintf(buf, sizeof buf, "CH%d", trace[t].channel);
    cell_drawstring_invert_5x7(w, h, buf, xpos, ypos, config.trace_color[t], t == uistat.current_trace);
    xpos += 20;
    trace_get_info(t, buf, sizeof buf);
    cell_drawstring_5x7(w, h, buf, xpos, ypos, config.trace_color[t]);
    xpos += 64;
    trace_get_value_string(
        t, buf, sizeof buf,
        idx, measured[trace[t].channel], frequencies, sweep_points);
    cell_drawstring_5x7(w, h, buf, xpos, ypos, config.trace_color[t]);
    j++;
  }    
  j += j&1;
  
  // LEFT
  int ypos = 1 + (j/2)*7;
  ypos -= n * CELLHEIGHT;
  if (electrical_delay != 0) {
    // draw electrical delay
    int xpos = 21;
    xpos -= m * CELLWIDTH -CELLOFFSETX;
    chsnprintf(buf, sizeof buf, "Edelay");
    cell_drawstring_5x7(w, h, buf, xpos, ypos, 0xffff);
    xpos += 7 * 5;
    int n = string_value_with_prefix(buf, sizeof buf, electrical_delay * 1e-12, 's');
    cell_drawstring_5x7(w, h, buf, xpos, ypos, 0xffff);
    xpos += n * 5 + 5;
    float light_speed_ps = 299792458e-12; //(m/ps)
    string_value_with_prefix(buf, sizeof buf, electrical_delay * light_speed_ps * velocity_factor / 100.0, 'm');
    cell_drawstring_5x7(w, h, buf, xpos, ypos, 0xffff);
    ypos += 7;
  }

#ifdef __DRAW_Z__  
  {
#define ZCOLOR RGBHEX(0x00ffff)
    // draw Z
    int xpos = 1 + 2 * 5;
    xpos -= m * CELLWIDTH -CELLOFFSETX;
    cell_drawchar_5x7(w, h, 'Z', xpos, ypos, ZCOLOR, 0);
    xpos += 2 * 5;
    float re = measured[0][idx][0];
    float im = measured[0][idx][1];
    float d = 50.0 / ((1-re)*(1-re)+im*im);
    float rs = ((1+re)*(1-re) - im*im) * d;
    float xs = 2*im * d;
    int len = chsnprintf(buf, sizeof buf, "%.1f", rs);
    cell_drawstring_5x7(w, h, buf, xpos, ypos, ZCOLOR);
    xpos += 5 * len + 5;
    cell_drawchar_5x7(w, h, xs >= 0 ? '+':'-', xpos, ypos, ZCOLOR, 0);
    xpos += 5 + 5;
    len = chsnprintf(buf, sizeof buf, "%.1f", xs >= 0 ? xs:-xs);
    cell_drawstring_5x7(w, h, buf, xpos, ypos, ZCOLOR);
    xpos += 5 * len;
    cell_drawchar_5x7(w, h, 'j', xpos, ypos, ZCOLOR, 0);
  }
#endif

  // RIGHT
  // draw marker frequency
  int xpos = 192;
  ypos = 1 + (j/2)*7;
  xpos -= m * CELLWIDTH -CELLOFFSETX;
  ypos -= n * CELLHEIGHT;
  chsnprintf(buf, sizeof buf, "%d:", active_marker + 1);
  xpos += 5;
  cell_drawstring_5x7(w, h, buf, xpos, ypos, 0xffff);
  xpos += 14;
  if ((domain_mode & DOMAIN_MODE) == DOMAIN_FREQ) {
    frequency_string(buf, sizeof buf, frequencies[idx]);
  } else {
    //chsnprintf(buf, sizeof buf, "%d ns %.1f m", (uint16_t)(time_of_index(idx) * 1e9), distance_of_index(idx));
    int n = string_value_with_prefix(buf, sizeof buf, time_of_index(idx), 's');  
    buf[n++] = ' ';
    string_value_with_prefix(&buf[n], sizeof buf-n, distance_of_index(idx), 'm');
  }
  cell_drawstring_5x7(w, h, buf, xpos, ypos, 0xffff);

  // draw marker delta
  if (previous_marker >= 0 && active_marker != previous_marker && markers[previous_marker].enabled) {
    int idx0 = markers[previous_marker].index;
    xpos = 192;
    xpos -= m * CELLWIDTH -CELLOFFSETX;
    ypos += 7;
    chsnprintf(buf, sizeof buf, "d%d:", previous_marker+1);
    cell_drawstring_5x7(w, h, buf, xpos, ypos, 0xffff);
    xpos += 19;
    if ((domain_mode & DOMAIN_MODE) == DOMAIN_FREQ) {
      frequency_string(buf, sizeof buf, frequencies[idx] - frequencies[idx0]);
    } else {
      //chsnprintf(buf, sizeof buf, "%d ns %.1f m", (uint16_t)(time_of_index(idx) * 1e9 - time_of_index(idx0) * 1e9),
      //                                            distance_of_index(idx) - distance_of_index(idx0));
      int n = string_value_with_prefix(buf, sizeof buf, time_of_index(idx) - time_of_index(idx0), 's');
      buf[n++] = ' ';
      string_value_with_prefix(&buf[n], sizeof buf - n, distance_of_index(idx) - distance_of_index(idx0), 'm');
    }
    cell_drawstring_5x7(w, h, buf, xpos, ypos, 0xffff);
  }
}

static void frequency_string(char *buf, size_t len, int32_t freq)
{
  if (freq < 0) {
    freq = -freq;
    *buf++ = '-';
    len -= 1;
  }
  if (freq < 1000) {
    chsnprintf(buf, len, "%d Hz", (int)freq);
  } else if (freq < 1000000) {
    chsnprintf(buf, len, "%d.%03d kHz",
             (int)(freq / 1000),
             (int)(freq % 1000));
  } else {
    chsnprintf(buf, len, "%d.%03d %03d MHz",
             (int)(freq / 1000000),
             (int)((freq / 1000) % 1000),
             (int)(freq % 1000));
  }
}

void draw_frequencies(void)
{
  char buf[24];
  if ((domain_mode & DOMAIN_MODE) == DOMAIN_FREQ) {
      if (frequency1 > 0) {
        int start = frequency0;
        int stop = frequency1;
        strcpy(buf, "START ");
        frequency_string(buf+6, 24-6, start);
        strcat(buf, "    ");
        ili9341_drawstring_5x7(buf, OFFSETX, 233, 0xffff, 0x0000);
        strcpy(buf, "STOP ");
        frequency_string(buf+5, 24-5, stop);
        strcat(buf, "    ");
        ili9341_drawstring_5x7(buf, 205, 233, 0xffff, 0x0000);
      } else if (frequency1 < 0) {
        int fcenter = frequency0;
        int fspan = -frequency1;
        strcpy(buf, "CENTER ");
        frequency_string(buf+7, 24-7, fcenter);
        strcat(buf, "    ");
        ili9341_drawstring_5x7(buf, OFFSETX, 233, 0xffff, 0x0000);
        strcpy(buf, "SPAN ");
        frequency_string(buf+5, 24-5, fspan);
        strcat(buf, "    ");
        ili9341_drawstring_5x7(buf, 205, 233, 0xffff, 0x0000);
      } else {
        int fcenter = frequency0;
        chsnprintf(buf, 24, "CW %d.%03d %03d MHz    ",
                   (int)(fcenter / 1000000),
                   (int)((fcenter / 1000) % 1000),
                   (int)(fcenter % 1000));
        ili9341_drawstring_5x7(buf, OFFSETX, 233, 0xffff, 0x0000);
        chsnprintf(buf, 24, "                             ");
        ili9341_drawstring_5x7(buf, 205, 233, 0xffff, 0x0000);
      }
  } else {
      strcpy(buf, "START 0s        ");
      ili9341_drawstring_5x7(buf, OFFSETX, 233, 0xffff, 0x0000);

      strcpy(buf, "STOP ");
      chsnprintf(buf+5, 24-5, "%d ns", (uint16_t)(time_of_index(POINT_COUNT-1) * 1e9));
      strcat(buf, "          ");
      ili9341_drawstring_5x7(buf, 205, 233, 0xffff, 0x0000);
  }
}

void draw_cal_status(void)
{
  int x = 0;
  int y = 100;
#define YSTEP 7
  ili9341_fill(0, y, 10, 6*YSTEP, 0x0000);
  if (cal_status & CALSTAT_APPLY) {
    char c[3] = "C0";
    c[1] += lastsaveid;
    if (cal_status & CALSTAT_INTERPOLATED)
      c[0] = 'c';
    else if (active_props == &current_props)
      c[1] = '*';
    ili9341_drawstring_5x7(c, x, y, 0xffff, 0x0000);
    y += YSTEP;
  }

  if (cal_status & CALSTAT_ED) {
    ili9341_drawstring_5x7("D", x, y, 0xffff, 0x0000);
    y += YSTEP;
  }
  if (cal_status & CALSTAT_ER) {
    ili9341_drawstring_5x7("R", x, y, 0xffff, 0x0000);
    y += YSTEP;
  }
  if (cal_status & CALSTAT_ES) {
    ili9341_drawstring_5x7("S", x, y, 0xffff, 0x0000);
    y += YSTEP;
  }
  if (cal_status & CALSTAT_ET) {
    ili9341_drawstring_5x7("T", x, y, 0xffff, 0x0000);
    y += YSTEP;
  }
  if (cal_status & CALSTAT_EX) {
    ili9341_drawstring_5x7("X", x, y, 0xffff, 0x0000);
    y += YSTEP;
  }
}

// voltage to percent table based on discharge time measurements
static uint8_t v2p[] = {
    (3000 - 3000 + 5)/10,   //   0%
    (3364 - 3000 + 5)/10,   //   5%
    (3468 - 3000 + 5)/10,   //  10%
    (3530 - 3000 + 5)/10,   //  15%
    (3625 - 3000 + 5)/10,   //  20%
    (3690 - 3000 + 5)/10,   //  25%
    (3725 - 3000 + 5)/10,   //  30%
    (3750 - 3000 + 5)/10,   //  35%
    (3770 - 3000 + 5)/10,   //  40%
    (3797 - 3000 + 5)/10,   //  45%
    (3814 - 3000 + 5)/10,   //  50%
    (3831 - 3000 + 5)/10,   //  55%
    (3850 - 3000 + 5)/10,   //  60%
    (3874 - 3000 + 5)/10,   //  65%
    (3901 - 3000 + 5)/10,   //  70%
    (3928 - 3000 + 5)/10,   //  75%
    (3952 - 3000 + 5)/10,   //  80%
    (3973 - 3000 + 5)/10,   //  85%
    (3988 - 3000 + 5)/10,   //  90%
    (4014 - 3000 + 5)/10,   //  95%
    (4171 - 3000 + 5)/10,   // 100%
};

// convert vbat [mV] to battery percent
uint8_t vbat2percent(int16_t vbat)
{
    int i, v;
    if (vbat < 3000) 
        return 0;
    v = (vbat - 3000 + 5) / 10;
    if (v < v2p[0])
        return 0;
    for (i=0; i < sizeof(v2p)-1; i++) {
        if (v < v2p[i+1]) {
            return i*5 + 5*((vbat-3000) - v2p[i]*10)/(v2p[i+1]*10 - v2p[i]*10);
        }
    }
    return 100;
}

void draw_battery_status(void)
{
    chMtxLock(&mutex_ili9341); // [protect spi_buffer]
    int w = 10, h = 14;
    int x = 0, y = 0;
    uint16_t *buf = spi_buffer;
    uint8_t vbati = vbat2percent(vbat);
    if (vbati > 100) vbati = 100;
    uint16_t colo = vbati < 10 ? RGBHEX(0xff0000) : 
        vbati < 25 ? RGBHEX(0xffd000) : RGBHEX(0xd0d0d0);
    uint16_t colf = vbati < 10 ? RGBHEX(0xff0000) :
        vbati < 25 ? RGBHEX(0xffd000) : RGBHEX(0x1fe300);
            
    memset(spi_buffer, 0, w * h * 2);

    // battery head
    x = 3;
    buf[y * w + x++] = colo;
    buf[y * w + x++] = colo;
    buf[y * w + x++] = colo;
    buf[y * w + x++] = colo;

    y++;
    x = 3;
    for (int i = 0; i < 4; i++)
        buf[y * w + x++] = colo;

    y++;
    x = 1;
    for (int i = 0; i < 8; i++)
        buf[y * w + x++] = colo;

    y++;
    x = 1;
    buf[y * w + x++] = colo;
    x += 6;
    buf[y * w + x++] = colo;
    
    uint8_t n = vbati / 12;
    if (!n && vbati >= 10) n = 1;
    n = 8 - n;
    for (int j = 0; j < 8; j++) {
        y++;
        x = 1;
        if (j < n) {
            buf[y * w + x++] = colo;
            x += 6;
            buf[y * w + x++] = colo;
        } else {
            buf[y * w + x++] = colo;
            x++;
            for (int i = 0; i < 4; i++)
                buf[y * w + x++] = colf;
            x++;
            buf[y * w + x++] = colo;
        }
    }

    y++;
    x = 1;
    buf[y * w + x++] = colo;
    x += 6;
    buf[y * w + x++] = colo;
    
    // battery footer
    y++;
    x = 1;
    for (int i = 0; i < 8; i++)
        buf[y * w + x++] = colo;

    ili9341_bulk(0, 1, w, h);
    chMtxUnlock(&mutex_ili9341); // [/protect spi_buffer]
}

void draw_pll_lock_error(void)
{
    int y = 1+7*2;
    ili9341_fill(0, y, 10, 4*7, RGBHEX(0x000000));
    y += 4;
    ili9341_fill(1, y, 8, 3*7+4, RGBHEX(0xff0000));
    y += 2;
    ili9341_drawchar_5x7('P', 3, y, RGBHEX(0x000000), RGBHEX(0xff0000));
    y+=7;
    ili9341_drawchar_5x7('L', 3, y, RGBHEX(0x000000), RGBHEX(0xff0000));
    y+=7;
    ili9341_drawchar_5x7('L', 3, y, RGBHEX(0x000000), RGBHEX(0xff0000));
}

void request_to_redraw_grid(void)
{
  force_set_markmap();
  redraw_request |= REDRAW_CELLS;
}

void redraw_frame(void)
{
  ili9341_fill(0, 0, 320, 240, 0);
  draw_frequencies();
  draw_cal_status();
}

void plot_init(void)
{
  force_set_markmap();
}
