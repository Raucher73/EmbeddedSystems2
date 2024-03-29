// Snake
#include <Arduino.h>
#include <WiFi.h>
#include <AsyncUDP.h>
#include <FS.h>
#include <FastLED.h>
#include <alpha.h>

//--------------------------------------------------------------------------------
// Einstellungen, welche vorgenommen werden können
#define heigth_one_led_mat      16  // Höhe einer einzelnen verbauten LED-Matte (LEDs)
#define count_led_mat           25  // Anzahl der verbauten LED-Matten
#define x_coordinations_length  80  // Anzahl der LEDs auf der x-Achse
#define y_coordinations_length  80  // Anzahl der LEDs auf der y-Achse
#define brightness_led          25  // Helligkeit der Matte einstellen
#define snake_length            100 // Länge der Schlange einstellen
//--------------------------------------------------------------------------------


// Globale Variablen
struct Coordinations
{
  int x;
  int y;
  int status;
};

Coordinations led;
Coordinations gyro;
Coordinations food;
Coordinations snake[snake_length];

// Wifi
#define WIFI_SSID "Access-Point LED_Spielmatte"
#define WIFI_PASS "LED_Spielmatte"
AsyncUDP udp;
String udp_message;
unsigned int udpPort = 8888;
IPAddress remoteUDPIP(255, 255, 255, 255);

// LED-Matrix
#define LED_PIN 12
#define COLOR_ORDER GRB
#define CHIPSET WS2812B
const int kMatrixWidth = heigth_one_led_mat;
const int kMatrixHeight = heigth_one_led_mat * count_led_mat;
#define NUM_LEDS (kMatrixWidth * kMatrixHeight)
int X_MAX = x_coordinations_length;
int Y_MAX = y_coordinations_length;
int BRIGHTNESS = brightness_led;
const bool kMatrixSerpentineLayout = true;
const bool kMatrixVertical = false;
CRGB leds_plus_safety_pixel[NUM_LEDS + 1];
CRGB *const leds(leds_plus_safety_pixel + 1);

// Statemachine
#define INIT 0
#define PLAY 1
#define SCORE 2
#define GAME_OVER 3
int stateGame;

// Colors
#define BLACK   0
#define RED     1
#define GREEN   2
#define WHITE   3
#define BLUE    4
#define ORANGE  5

// Game - Snake
#define UP 0
#define DOWN 1
#define LEFT 2
#define RIGHT 3
#define NONE 5
int snakeLength;
int direction = RIGHT;

// Funktionen
void receiveUDPMessage(int udpPort);
void breakString(String output);
void sendData(int x_coord_led, int y_coord_led, int status);
void initializeGame();
void spawnFood();
void updateGame();
void moveSnake();
void checkWallCollision();
void checkFood();
void drawingField();
void gameOver();
bool checkSnakeCollision(Coordinations head);
uint16_t XY(int x, int y);

void setup()
{
  // Serielle Schnittstelle öffnen
  Serial.begin(9600);

  // LED-Matrix initialisieren
  FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalSMD5050);
  FastLED.setBrightness(BRIGHTNESS);

  // Access-Point für Gyros setzen
  Serial.println("Setting AP (Access Point)…");
  WiFi.softAP(WIFI_SSID, WIFI_PASS);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  // Startwerte für Game setzen
  randomSeed(analogRead(0));
  stateGame = INIT;

  // UDP Nachricht abfangen, funktioniert asynchron
  receiveUDPMessage(udpPort);
}

void loop()
{
  breakString(udp_message);

  switch (stateGame)
  {
  case INIT:
    // Spielfeldrand zeichnen
    drawingField();

    // Startwerte setzen und zeichnen
    initializeGame();
    spawnFood();
    stateGame = PLAY;
    break;

  case PLAY:
    // Schlange bewegen, auf Kollision prüfen und prüfen ob gefressen wurde
    if (direction != NONE)
    {
      moveSnake();
      checkWallCollision();
      checkFood();
    }
    break;

  case GAME_OVER:
    gameOver();
    FastLED.show();
    delay(1500);
    stateGame = INIT;
    break;
  }

  FastLED.show();
}

uint16_t XY(int x, int y)
{
  int16_t i;

  if (kMatrixSerpentineLayout == false)
  {
    if (kMatrixVertical == false)
    {
      i = (y * kMatrixWidth) + x;
    }
    else
    {
      i = kMatrixHeight * (kMatrixWidth - (x + 1)) + y;
    }
  }

  if (kMatrixSerpentineLayout == true)
  {
    if (kMatrixVertical == false)
    {
      if (y & 0x01)
      {
        // Odd rows run backwards
        int8_t reverseX = (kMatrixWidth - 1) - x;
        i = (y * kMatrixWidth) + reverseX;
      }
      else
      {
        // Even rows run forwards
        i = (y * kMatrixWidth) + x;
      }
    }
    else
    { // vertical positioning
      if (x & 0x01)
      {
        i = kMatrixHeight * (kMatrixWidth - (x + 1)) + y;
      }
      else
      {
        i = kMatrixHeight * (kMatrixWidth - x) - (y + 1);
      }
    }
  }

  return i;
}

void drawingField()
{
  // Spielfeld begrenzung zeichnen
  for (int i = 0; i < X_MAX; i++)
  {
    sendData(0, i, WHITE);
    sendData(X_MAX - 1, i, WHITE);
  }
  for (int i = 0; i < Y_MAX; i++)
  {
    sendData(i, 0, WHITE);
    sendData(i, Y_MAX - 1, WHITE);
  }
}

void initializeGame()
{
  // Spielfeld leeren
  for (int i = 1; i < X_MAX - 2; i++)
  {
    for (int j = 1; j < Y_MAX - 2; j++)
    {
      sendData(i, j, BLACK);
    }
  }
  snakeLength = 1;

  // Startwerte festlegen
  snake[0].x = 40;
  snake[0].y = 40;
  snake[0].status = 5;
  sendData(snake[0].x, snake[0].y, RED);
}

void spawnFood()
{
  // Position von Essen erzeugen
  food.x = random(2, X_MAX - 2);
  food.y = random(2, Y_MAX - 2);
  food.status = 2;

  // Sicherstellen das Essen nicht auf Schlange spawnt
  for (int i = 0; i < snakeLength; i++)
  {
    if (food.x == snake[i].x && food.y == snake[i].y)
    {
      spawnFood();
      return;
    }
  }

  // Essen auf Matrix dartstellen
  sendData(food.x, food.y, food.status);
}

void moveSnake()
{
  Coordinations head;

    led.x = snake[snakeLength - 1].x;
    led.y = snake[snakeLength - 1].y;
      
  head = snake[0];
  switch (direction)
      {
      case UP:
        head.y--;
        break;
      case DOWN:
        head.y++;
        break;
      case LEFT:
        head.x--;
        break;
      case RIGHT:
        head.x++;
        break;
      }

  if (checkSnakeCollision(head)){
    return;
  }

  for (int i = snakeLength - 1; i > 0; i--)
  {
    snake[i].x = snake[i - 1].x;
    snake[i].y = snake[i - 1].y;
    snake[i].status = snake[i - 1].status;
    sendData(snake[i].x, snake[i].y, snake[i].status);
  }

  snake[0] = head;
  
  sendData(snake[0].x, snake[0].y, RED);
  sendData(led.x, led.y, BLACK);
}

bool checkSnakeCollision(Coordinations head)
{
  // Check ob Schlange sich selber gefressen hat
  bool isCollision = false;
  for (int i = 1; i < snakeLength; i++)
  {

    if (head.x == snake[i].x && head.y == snake[i].y)
    {
      stateGame = GAME_OVER;
      isCollision = true;
      break;
    }
  }
  return isCollision;
}

void checkWallCollision()
{
  // Check ob Schlange gegen Wand geprallt ist
  if (snake[0].x < 1 || snake[0].x > X_MAX - 1 || snake[0].y < 1 || snake[0].y > Y_MAX - 1)
  {
    stateGame = GAME_OVER;
  }
}

void checkFood()
{
  // Check ob Schlange gegessen hat
  if (snake[0].x == food.x && snake[0].y == food.y)
  {
    snakeLength++;
    spawnFood();
  }
}

void gameOver()
{
  // lokale Variablen
  int startpointX = 32;
  int startpointY1 = 29;
  int startpointY2 = 29;
  int counter = 0;

  FastLED.clear();

  // GAME schreiben
  for (int z = 0; z < 4; z++)
  {
    for (int i = startpointX; i < startpointX + 7; i++)
    {
      for (int j = startpointY1; j < startpointY1 + 4; j++)
      {
        sendData(i, j, alphaMatrix[z][counter]);
        counter++;
      }
    }
    startpointY1 = startpointY1 + 6;
    counter = 0;
  }

  // OVER schreiben
  startpointX = startpointX + 9;

  for (int z = 4; z < 8; z++)
  {
    for (int i = startpointX; i < startpointX + 7; i++)
    {
      for (int j = startpointY2; j < startpointY2 + 4; j++)
      {
        sendData(i, j, alphaMatrix[z][counter]);
        counter++;
      }
    }
    startpointY2 = startpointY2 + 6;
    counter = 0;
  }
}

void breakString(String output)
{
  // lokale Variablen
  String id_gyro;

  // String von Gyrosensor zerteilen
  id_gyro = output.substring(0, output.indexOf(','));
  output = output.substring(output.indexOf(',') + 1, output.length() + 1);
  gyro.x = output.substring(0, output.indexOf(',')).toFloat();
  output = output.substring(output.indexOf(',') + 1, output.length() + 1);
  gyro.y = output.substring(0, output.indexOf(';')).toFloat();
  output = output.substring(output.indexOf(';') + 1, output.length());

  Serial.println("ID:" + id_gyro + ", X: " + String(gyro.x) + ", Y: " + String(gyro.y));

  // Richtung von Schlange bestimmen
  if (gyro.x == 1)
  {
    direction = RIGHT;
  }
  if (gyro.x == -1)
  {
    direction = LEFT;
  }
  if (gyro.y == -1)
  {
    direction = UP;
  }
  if (gyro.y == 1)
  {
    direction = DOWN;
  }
  if (gyro.x == 0 && gyro.y == 0)
  {
    direction = NONE;
  }
}

void sendData(int x_coord_led, int y_coord_led, int status)
{
  // Umrechnung der Koordinaten in Koordinaten für Matrix
  int x = x_coord_led % heigth_one_led_mat;
  int y = Y_MAX * (x_coord_led / heigth_one_led_mat) + y_coord_led;

  // Unterschiedliche Farben für Status
  switch (status)
  {
  case BLACK:
    leds[XY(x, y)] = CRGB::Black;
    break;

  case RED:
    leds[XY(x, y)] = CRGB::Red;
    break;

  case GREEN:
    leds[XY(x, y)] = CRGB::Green;
    break;

  case WHITE:
    leds[XY(x, y)] = CRGB::White;
    break;

  case BLUE:
    leds[XY(x, y)] = CRGB::Blue;
    break;

  case ORANGE:
    leds[XY(x, y)] = CRGB::Orange;
    break;
  }
}

void receiveUDPMessage(int udpPort)
{
  if (udp.listen(udpPort))
  {
    // Wenn Nachricht an UDP dann speichern
    Serial.print("UDP Listening on IP: ");
    Serial.println(WiFi.localIP());
    udp.onPacket([](AsyncUDPPacket packet)
                 { udp_message = (const char *)packet.data(); });
  }
}