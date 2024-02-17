#include <Arduino.h>
#include <TFT_eSPI.h> // Need to use the modified TFT library provided in this project.
#include <pin_config.h>
#include "OneButton.h"

#define top_button_pin PIN_BUTTON_1
#define bottom_button_pin PIN_BUTTON_2

static const uint16_t NUM_PARTICLES = 2000;
// static const float PARTICLE_MASS = 3;
static const float PARTICLE_MASS = 5;
// static const float GRAVITY = 0.5;
static const float GRAVITY = 2;
static const uint16_t ROWS = 170;
static const uint16_t COLS = 320;

TFT_eSPI tft = TFT_eSPI();
boolean loadingflag = true;

OneButton top_button(top_button_pin, true);
OneButton bottom_button(bottom_button_pin, true);

template <class T>
class vec2
{
public:
  T x, y;

  vec2() : x(0), y(0) {}
  vec2(T x, T y) : x(x), y(y) {}
  vec2(const vec2 &v) : x(v.x), y(v.y) {}

  vec2 &operator=(const vec2 &v)
  {
    x = v.x;
    y = v.y;
    return *this;
  }

  bool operator==(vec2 &v)
  {
    return x == v.x && y == v.y;
  }

  bool operator!=(vec2 &v)
  {
    return !(x == y);
  }

  vec2 operator+(vec2 &v)
  {
    return vec2(x + v.x, y + v.y);
  }

  vec2 operator-(vec2 &v)
  {
    return vec2(x - v.x, y - v.y);
  }

  vec2 &operator+=(vec2 &v)
  {
    x += v.x;
    y += v.y;
    return *this;
  }

  vec2 &operator-=(vec2 &v)
  {
    x -= v.x;
    y -= v.y;
    return *this;
  }

  vec2 operator+(double s)
  {
    return vec2(x + s, y + s);
  }

  vec2 operator-(double s)
  {
    return vec2(x - s, y - s);
  }

  vec2 operator*(double s)
  {
    return vec2(x * s, y * s);
  }

  vec2 operator/(double s)
  {
    return vec2(x / s, y / s);
  }

  vec2 &operator+=(double s)
  {
    x += s;
    y += s;
    return *this;
  }

  vec2 &operator-=(double s)
  {
    x -= s;
    y -= s;
    return *this;
  }

  vec2 &operator*=(double s)
  {
    x *= s;
    y *= s;
    return *this;
  }

  vec2 &operator/=(double s)
  {
    x /= s;
    y /= s;
    return *this;
  }

  void set(T x, T y)
  {
    this->x = x;
    this->y = y;
  }

  void rotate(double deg)
  {
    double theta = deg / 180.0 * M_PI;
    double c = cos(theta);
    double s = sin(theta);
    double tx = x * c - y * s;
    double ty = x * s + y * c;
    x = tx;
    y = ty;
  }

  vec2 &normalize()
  {
    if (length() == 0)
      return *this;
    *this *= (1.0 / length());
    return *this;
  }

  float dist(vec2 v) const
  {
    // vec2 d(v.x - x, v.y - y);
    // return d.length();
    return sqrt(v.x * x + v.y * y);
  }

  float length() const
  {
    return sqrt(x * x + y * y);
  }

  void truncate(double length)
  {
    double angle = atan2f(y, x);
    x = length * cos(angle);
    y = length * sin(angle);
  }

  vec2 ortho() const
  {
    return vec2(y, -x);
  }

  static float dot(vec2 v1, vec2 v2)
  {
    return v1.x * v2.x + v1.y * v2.y;
  }

  static float cross(vec2 v1, vec2 v2)
  {
    return (v1.x * v2.y) - (v1.y * v2.x);
  }

  float mag() const
  {
    return length();
  }

  float magSq()
  {
    return (x * x + y * y);
  }

  void limit(float max)
  {
    if (magSq() > max * max)
    {
      normalize();
      *this *= max;
    }
  }
};

typedef vec2<float> PVector;
typedef vec2<double> vec2d;

class Boid
{
public:
  PVector location;
  PVector velocity;
  PVector acceleration;
  int hue;

  float mass;
  boolean enabled = true;

  Boid() {}

  Boid(float x, float y)
  {
    acceleration = PVector(0, 0);
    velocity = PVector(randomf(), randomf());
    location = PVector(x, y);
    hue = random(0x0A0A0A0A, 0xFFFFFFFF);
  }

  static float randomf()
  {
    return mapfloat(random(0, 255), 0, 255, -.5, .5);
  }

  static float mapfloat(float x, float in_min, float in_max, float out_min, float out_max)
  {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
  }

  void run(Boid boids[], uint8_t boidCount)
  {
    update();
  }

  // Method to update location
  void update()
  {
    velocity += acceleration;
    location += velocity;
    acceleration *= 0;
  }

  void applyForce(PVector force)
  {
    // We could add mass here if we want A = F / M
    acceleration += force;
  }
};

class Attractor
{
public:
  float mass;
  float G;
  PVector location;

  Attractor()
  {
    location = PVector(COLS / 2, ROWS / 2);
    mass = PARTICLE_MASS;
    G = GRAVITY;
  }

  PVector attract(Boid m)
  {
    PVector force = location - m.location; // Calculate direction of force
    float d = force.mag();                 // Distance between objects
    d = constrain(d, 6.0, 9.0);            // Limiting the distance to eliminate "extreme" results for very close or very far objects
    force.normalize();
    float strength = (G * mass * 2) / (d * d); // Calculate gravitional force magnitude
    force *= strength;                         // Get force vector --> magnitude * direction
    return force;
  }
};

Attractor attractor;
bool loadingFlag = true;

Boid boidss[NUM_PARTICLES]; // this makes the boids
uint16_t x;
uint16_t y;
uint16_t huecounter = 1;

void start()
{
  int direction = random(0, 2);
  if (direction == 0)
    direction = -1;

  uint16_t ROW_CENTER = ROWS / 2;
  uint16_t COL_CENTER = COLS / 2;

  for (int i = 0; i < NUM_PARTICLES; i++)
  {
    Boid boid = Boid(random(1, COLS - 1), random(1, ROWS - 1)); // Full screen
    // Boid boid = Boid(random(COL_CENTER - ROW_CENTER, COL_CENTER + ROW_CENTER), random(1, ROWS - 1)); // Square in middle
    boid.velocity.x = ((float)random(40, 50)) / 14.0;
    boid.velocity.x *= direction;
    boid.velocity.y = ((float)random(40, 50)) / 14.0;
    boid.velocity.y *= direction;
    boid.hue = huecounter;
    huecounter += 0xFABCDE;
    boidss[i] = boid;
  }
}

void topButtonDoubleClick()
{
  attractor.location.x += 20;
}

void topButtonClick()
{
  attractor.location.x -= 20;
}

void bottomButtonDoubleClick()
{
  attractor.location.y += 20;
}

void bottomButtonClick()
{
  attractor.location.y -= 20;
}

void setup()
{
  pinMode(top_button_pin, INPUT_PULLUP);
  pinMode(bottom_button_pin, INPUT_PULLUP);

  top_button.attachDoubleClick(topButtonDoubleClick);
  top_button.attachClick(topButtonClick);
  bottom_button.attachDoubleClick(bottomButtonDoubleClick);
  bottom_button.attachClick(bottomButtonClick);

  pinMode(PIN_POWER_ON, OUTPUT);
  digitalWrite(PIN_POWER_ON, HIGH);
  tft.begin();
  tft.fillScreen(TFT_BLACK);
  delay(2000);
}

// #error  "Error! Please make sure <User_Setups/Setup206_LilyGo_T_Display_S3.h> is selected in <TFT_eSPI/User_Setup_Select.h>"
// #error  "Error! Please make sure <User_Setups/Setup206_LilyGo_T_Display_S3.h> is selected in <TFT_eSPI/User_Setup_Select.h>"
// #error  "Error! Please make sure <User_Setups/Setup206_LilyGo_T_Display_S3.h> is selected in <TFT_eSPI/User_Setup_Select.h>"
// #error  "Error! Please make sure <User_Setups/Setup206_LilyGo_T_Display_S3.h> is selected in <TFT_eSPI/User_Setup_Select.h>"

void loop()
{
  if (loadingFlag)
  {
    loadingFlag = false;
    start();
  }

  top_button.tick();
  bottom_button.tick();

  for (int i = 0; i < NUM_PARTICLES; i++)
  {
    Boid boid = boidss[i];
    tft.drawPixel(boid.location.y, boid.location.x, 0x0000);
    PVector force = attractor.attract(boid);
    boid.applyForce(force);
    boid.update();
    tft.drawPixel(boid.location.y, boid.location.x, boid.hue);
    boidss[i] = boid;
  }
}
