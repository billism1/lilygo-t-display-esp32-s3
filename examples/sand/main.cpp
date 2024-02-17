#include <Arduino.h>
#include <TFT_eSPI.h> // Need to use the modified TFT library provided in this project.
#include <pin_config.h>
#include <OneButton.h>

#define top_button_pin PIN_BUTTON_1
#define bottom_button_pin PIN_BUTTON_2

OneButton top_button(top_button_pin, true);
OneButton bottom_button(bottom_button_pin, true);

static const int16_t ROWS = 170;
static const int16_t COLS = 320;

int16_t BACKGROUND_COLOR = TFT_BLACK;

TFT_eSPI tft = TFT_eSPI();

byte red = 31;
byte green = 0;
byte blue = 0;
byte colorState = 0;
int16_t color = red << 11;
unsigned long colorChangeTime = 0;

int16_t gravity = 1;

int16_t inputWidth = 10;
int16_t inputX = 160; // COLS / 2;
int16_t inputY = 30;
// int16_t inputY = 85;  // ROWS / 2;

int16_t **grid;
int16_t **nextGrid;
int16_t **lastGrid;

int16_t **velocityGrid;
int16_t **nextVelocityGrid;
int16_t **lastVelocityGrid;

int16_t **stateGrid;
int16_t **nextStateGrid;
int16_t **lastStateGrid;

static const int16_t GRID_STATE_NONE = 0;
static const int16_t GRID_STATE_NEW = 1;
static const int16_t GRID_STATE_FALLING = 2;
static const int16_t GRID_STATE_COMPLETE = 3;

long lastMillis = 0;
int fps = 0;
char fpsStringBuffer[16];

bool withinCols(int16_t value)
{
  return value >= 0 && value <= COLS - 1;
}

bool withinRows(int16_t value)
{
  return value >= 0 && value <= ROWS - 1;
}

// bool isWithinInputArea(int16_t x, int16_t y)
// {
//   long halfInputWidthPlusThree = (inputWidth / 2) + 3;
//   return x > (inputX - halfInputWidthPlusThree) && x < (inputX + halfInputWidthPlusThree) && y > (inputY - halfInputWidthPlusThree) && y < (inputY + halfInputWidthPlusThree);
// }

// Color changing state machine
void setNextColor()
{
  switch (colorState)
  {
  case 0:
    green += 2;
    if (green == 64)
    {
      green = 63;
      colorState = 1;
    }
    break;
  case 1:
    red--;
    if (red == 255)
    {
      red = 0;
      colorState = 2;
    }
    break;
  case 2:
    blue++;
    if (blue == 32)
    {
      blue = 31;
      colorState = 3;
    }
    break;
  case 3:
    green -= 2;
    if (green == 255)
    {
      green = 0;
      colorState = 4;
    }
    break;
  case 4:
    red++;
    if (red == 32)
    {
      red = 31;
      colorState = 5;
    }
    break;
  case 5:
    blue--;
    if (blue == 255)
    {
      blue = 0;
      colorState = 0;
    }
    break;
  }

  color = red << 11 | green << 5 | blue;

  if (color == BACKGROUND_COLOR)
    color++;
}

void topButtonLongPress()
{
  inputX -= 5;

  // Wrap, if needed
  if (inputX < 0)
    inputX = COLS;
}

void topButtonClick()
{
  inputX += 5;

  // Wrap, if needed
  if (inputX > COLS)
    inputX = 0;
}

void bottomButtonDoubleClick()
{
  inputY -= 5;

  // Wrap, if needed
  if (inputY < 0)
    inputY = ROWS;
}

void bottomButtonClick()
{
  inputY += 5;

  // Wrap, if needed
  if (inputY > ROWS)
    inputY = 0;
}

void setup()
{
  // Serial.begin(9600);
  //   Serial.println("Hello, starting...");

  pinMode(top_button_pin, INPUT_PULLUP);
  pinMode(bottom_button_pin, INPUT_PULLUP);

  top_button.attachDuringLongPress(topButtonLongPress);
  top_button.attachClick(topButtonClick);
  top_button.attachDoubleClick(topButtonClick);
  bottom_button.attachDuringLongPress(bottomButtonDoubleClick);
  bottom_button.attachClick(bottomButtonClick);
  bottom_button.attachDoubleClick(bottomButtonClick);

  // Serial.println("Creating grids...");
  grid = new int16_t *[ROWS];
  nextGrid = new int16_t *[ROWS];
  lastGrid = new int16_t *[ROWS];

  velocityGrid = new int16_t *[ROWS];
  nextVelocityGrid = new int16_t *[ROWS];
  lastVelocityGrid = new int16_t *[ROWS];

  stateGrid = new int16_t *[ROWS];
  nextStateGrid = new int16_t *[ROWS];
  lastStateGrid = new int16_t *[ROWS];

  // Serial.println("Clearing grids...");
  for (int16_t i = 0; i < ROWS; ++i)
  {
    grid[i] = new int16_t[COLS];
    nextGrid[i] = new int16_t[COLS];
    lastGrid[i] = new int16_t[COLS];

    velocityGrid[i] = new int16_t[COLS];
    nextVelocityGrid[i] = new int16_t[COLS];
    lastVelocityGrid[i] = new int16_t[COLS];

    stateGrid[i] = new int16_t[COLS];
    nextStateGrid[i] = new int16_t[COLS];
    lastStateGrid[i] = new int16_t[COLS];

    for (int16_t j = 0; j < COLS; ++j)
    {
      grid[i][j] = BACKGROUND_COLOR;
      nextGrid[i][j] = BACKGROUND_COLOR;
      lastGrid[i][j] = BACKGROUND_COLOR;

      velocityGrid[i][j] = 0;
      nextVelocityGrid[i][j] = 0;
      lastVelocityGrid[i][j] = 0;

      stateGrid[i][j] = GRID_STATE_NONE;
      nextStateGrid[i][j] = GRID_STATE_NONE;
      lastStateGrid[i][j] = GRID_STATE_NONE;
    }
  }

  colorChangeTime = millis() + 1000;

  pinMode(PIN_POWER_ON, OUTPUT);
  digitalWrite(PIN_POWER_ON, HIGH);

  tft.begin();
  tft.setRotation(3); // Landscape orientation
  tft.fillScreen(TFT_BLACK);

  delay(2000);
}

// #error  "Error! Please make sure <User_Setups/Setup206_LilyGo_T_Display_S3.h> is selected in <TFT_eSPI/User_Setup_Select.h>"
// #error  "Error! Please make sure <User_Setups/Setup206_LilyGo_T_Display_S3.h> is selected in <TFT_eSPI/User_Setup_Select.h>"
// #error  "Error! Please make sure <User_Setups/Setup206_LilyGo_T_Display_S3.h> is selected in <TFT_eSPI/User_Setup_Select.h>"
// #error  "Error! Please make sure <User_Setups/Setup206_LilyGo_T_Display_S3.h> is selected in <TFT_eSPI/User_Setup_Select.h>"

void resetAdjacentPixels(int16_t x, int16_t y)
{
  // Row above
  if (withinRows(y - 1))
  {
    if (nextStateGrid[y - 1][x - 1] == GRID_STATE_COMPLETE)
    {
      nextStateGrid[y - 1][x - 1] = GRID_STATE_FALLING;
      nextVelocityGrid[y - 1][x - 1] = 1;
    }
    if (nextStateGrid[y - 1][x] == GRID_STATE_COMPLETE)
    {
      nextStateGrid[y - 1][x] = GRID_STATE_FALLING;
      nextVelocityGrid[y - 1][x] = 1;
    }
    if (nextStateGrid[y - 1][x + 1] == GRID_STATE_COMPLETE)
    {
      nextStateGrid[y - 1][x + 1] = GRID_STATE_FALLING;
      nextVelocityGrid[y - 1][x + 1] = 1;
    }
  }

  // Current row
  if (nextStateGrid[y][x - 1] == GRID_STATE_COMPLETE)
  {
    nextStateGrid[y][x - 1] = GRID_STATE_FALLING;
    nextVelocityGrid[y][x - 1] = 1;
  }
  if (nextStateGrid[y][x + 1] == GRID_STATE_COMPLETE)
  {
    nextStateGrid[y][x + 1] = GRID_STATE_FALLING;
    nextVelocityGrid[y][x + 1] = 1;
  }

  // Row below
  if (withinRows(y + 1))
  {
    if (nextStateGrid[y + 1][x - 1] == GRID_STATE_COMPLETE)
    {
      nextStateGrid[y + 1][x - 1] = GRID_STATE_FALLING;
      nextVelocityGrid[y + 1][x - 1] = 1;
    }
    if (nextStateGrid[y + 1][x] == GRID_STATE_COMPLETE)
    {
      nextStateGrid[y + 1][x] = GRID_STATE_FALLING;
      nextVelocityGrid[y + 1][x] = 1;
    }
    if (nextStateGrid[y + 1][x + 1] == GRID_STATE_COMPLETE)
    {
      nextStateGrid[y + 1][x + 1] = GRID_STATE_FALLING;
      nextVelocityGrid[y + 1][x + 1] = 1;
    }
  }
}

void loop()
{
  // Serial.println("Looping...");

  // delay(50);

  // Get frame rate.
  unsigned long currentMillis = millis();
  fps = 1000 / (currentMillis - lastMillis);
  sprintf(fpsStringBuffer, "fps: %lu", fps);
  lastMillis = currentMillis;

  top_button.tick();
  bottom_button.tick();

  // Display frame rate
  tft.fillRect(1, 1, 55, 10, BACKGROUND_COLOR);
  tft.setTextColor(TFT_WHITE, BACKGROUND_COLOR);
  tft.drawString(fpsStringBuffer, 1, 1);

  // Randomly add an area of pixels
  int16_t halfInputWidth = inputWidth / 2;
  for (int16_t i = -halfInputWidth; i <= halfInputWidth; ++i)
  {
    for (int16_t j = -halfInputWidth; j <= halfInputWidth; ++j)
    {
      if (random(100) < 50)
      {
        int16_t col = inputX + i;
        int16_t row = inputY + j;

        if (withinCols(col) && withinRows(row) &&
            (stateGrid[row][col] == GRID_STATE_NONE || stateGrid[row][col] == GRID_STATE_COMPLETE))
        {
          grid[row][col] = color;
          velocityGrid[row][col] = 1;
          stateGrid[row][col] = GRID_STATE_NEW;
        }
      }
    }
  }

  // Change the color of the pixels over time
  if (colorChangeTime < millis())
  {
    colorChangeTime = millis() + 750;
    setNextColor();
  }

  // DEBUG
  // DEBUG
  // Serial.println("Array Values:");
  // for (int16_t i = 0; i < ROWS; ++i)
  // {
  //   for (int16_t j = 0; j < COLS; ++j)
  //   {
  //     Serial.printf("|%u|", stateGrid[i][j]);
  //   }
  //   Serial.println("");
  // }
  // DEBUG
  // DEBUG

  // unsigned long beforeDraw = millis();
  // unsigned long pixelsDrawn = 0;
  // Draw the pixels
  for (int16_t i = 0; i < ROWS; i = ++i)
  {
    for (int16_t j = 0; j < COLS; j = ++j)
    {
      if (lastGrid == NULL || lastGrid[i][j] != grid[i][j])
      {
        tft.drawPixel(j, i, grid[i][j]);
        // pixelsDrawn++;
      }
    }
  }
  // unsigned long afterDraw = millis();
  // unsigned long drawTime = afterDraw - beforeDraw;
  // Serial.printf("%lu pixels drawn in %lu ms.\r\n", pixelsDrawn, drawTime);

  // Serial.println("Clearing next grids...");
  // Clear the grids for the next frame of animation
  for (int16_t i = 0; i < ROWS; ++i)
  {
    for (int16_t j = 0; j < COLS; ++j)
    {
      nextGrid[i][j] = BACKGROUND_COLOR;
      nextVelocityGrid[i][j] = 0;
      nextStateGrid[i][j] = GRID_STATE_NONE;
    }
  }

  // Serial.println("Checking every cell...");
  // unsigned long beforeCheckEveryCell = millis();
  // unsigned long pixelsChecked = 0;
  // Check every pixel to see which need moving, and move them.
  for (int16_t i = 0; i < ROWS; ++i)
  {
    for (int16_t j = 0; j < COLS; ++j)
    {
      // This nexted loop is where the bulk of the computations occur.
      // Tread lightly in here, and check as few pixels as needed.

      // Get the state of the current pixel.
      int16_t pixelColor = grid[i][j];
      int16_t pixelVelocity = velocityGrid[i][j];
      int16_t pixelState = stateGrid[i][j];

      bool moved = false;

      // If the current pixel has landed, no need to keep checking for its next move.
      if (pixelState != GRID_STATE_NONE && pixelState != GRID_STATE_COMPLETE)
      {
        // pixelsChecked++;

        int16_t newPos = int16_t(i + pixelVelocity);
        for (int16_t y = newPos; y > i; y--)
        {
          if (!withinRows(y))
          {
            continue;
          }

          int16_t belowState = stateGrid[y][j];
          int16_t belowNextState = nextStateGrid[y][j];

          int16_t direction = 1;
          if (random(100) < 50)
          {
            direction *= -1;
          }

          int16_t belowStateA = -1;
          int16_t belowNextStateA = -1;
          int16_t belowStateB = -1;
          int16_t belowNextStateB = -1;

          if (withinCols(j + direction))
          {
            belowStateA = stateGrid[y][j + direction];
            belowNextStateA = nextStateGrid[y][j + direction];
          }
          if (withinCols(j - direction))
          {
            belowStateB = stateGrid[y][j - direction];
            belowNextStateB = nextStateGrid[y][j - direction];
          }

          if (belowState == GRID_STATE_NONE && belowNextState == GRID_STATE_NONE)
          {
            // This pixel will go straight down.
            nextGrid[y][j] = pixelColor;
            nextVelocityGrid[y][j] = pixelVelocity + gravity;
            nextStateGrid[y][j] = GRID_STATE_FALLING;
            moved = true;
            break;
          }
          else if (belowStateA == GRID_STATE_NONE && belowNextStateA == GRID_STATE_NONE)
          {
            // This pixel will fall to side A (right)
            nextGrid[y][j + direction] = pixelColor;
            nextVelocityGrid[y][j + direction] = pixelVelocity + gravity;
            nextStateGrid[y][j + direction] = GRID_STATE_FALLING;
            moved = true;
            break;
          }
          else if (belowStateB == GRID_STATE_NONE && belowNextStateB == GRID_STATE_NONE)
          {
            // This pixel will fall to side B (left)
            nextGrid[y][j - direction] = pixelColor;
            nextVelocityGrid[y][j - direction] = pixelVelocity + gravity;
            nextStateGrid[y][j - direction] = GRID_STATE_FALLING;
            moved = true;
            break;
          }
        }
      }

      if (moved)
        resetAdjacentPixels(j, i);

      if (pixelState != GRID_STATE_NONE && !moved)
      {
        nextGrid[i][j] = grid[i][j];
        nextVelocityGrid[i][j] = velocityGrid[i][j] + gravity;
        if (pixelState == GRID_STATE_NEW)
          nextStateGrid[i][j] = GRID_STATE_FALLING;
        else if (pixelState == GRID_STATE_FALLING && pixelVelocity > 2)
          nextStateGrid[i][j] = GRID_STATE_COMPLETE;
        else
          nextStateGrid[i][j] = pixelState; // should be GRID_STATE_COMPLETE
      }
    }
  }
  // unsigned long afterCheckEveryCell = millis();
  // unsigned long checkCellTime = afterCheckEveryCell - beforeCheckEveryCell;
  // Serial.printf("%lu pixels checked in %lu ms.\r\n", pixelsChecked, checkCellTime);

  // Swap the grid pointers.

  lastGrid = grid;
  lastVelocityGrid = velocityGrid;
  lastStateGrid = stateGrid;

  grid = nextGrid;
  velocityGrid = nextVelocityGrid;
  stateGrid = nextStateGrid;

  nextGrid = lastGrid;
  nextVelocityGrid = lastVelocityGrid;
  nextStateGrid = lastStateGrid;
}
