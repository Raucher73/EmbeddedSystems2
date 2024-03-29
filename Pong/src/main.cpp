// Pong
// Bibliotheken einbinden
#include <Arduino.h>
#include <WiFi.h>
#include <AsyncUDP.h>
#include <FS.h>
#include <FastLED.h>
#include <alpha.h>

//--------------------------------------------------------------------------------
// Einstellungen, welche vorgenommen werden können
#define heigth_one_led_mat 16     // Höhe einer einzelnen verbauten LED-Matte
#define count_led_mat 25          // Anzahl der verbauten LED-Matten
#define x_coordinations_length 80 // Anzahl der LEDs auf der x-Achse
#define y_coordinations_length 80 // Anzahl der LEDs auf der y-Achse
#define brightness_led 25         // Helligkeit der Matte einstellen
#define paddle_length 16          // Länge der Schläger
#define paddle_heigth 2           // Höhe der Schläger
#define ball_length 16            // Größe des Balls
#define max_score 5               // Maximal erreichbarer Score
//--------------------------------------------------------------------------------

// Globale Variablen
struct Coordinations
{
  int x;
  int y;
};

Coordinations led;
Coordinations gyro;
Coordinations paddle1[paddle_length];
Coordinations paddle2[paddle_length];
Coordinations ball[ball_length];

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
int stateGame = INIT;

// Colors
#define BLACK 0
#define RED 1
#define GREEN 2
#define WHITE 3

// Game - Pong
// Ball
#define RIGHT_UP 0
#define RIGHT_DOWN 1
#define LEFT_UP 2
#define LEFT_DOWN 3
int directionBall = RIGHT_UP;
int ballLength = ball_length;

// Paddle
#define RIGHT 0
#define LEFT 1
#define NONE 2
#define BACK 3

int directionPaddle1 = NONE;
int directionPaddle2 = NONE;
int paddleLength = paddle_length;
int paddleHeight = paddle_heigth;

// Score
int score1 = 0, score2 = 0;

// Funktionen vordefinieren
void receiveUDPMessage(int udpPort);
void breakString(String output);
void sendData(int x_coord_led, int y_coord_led, int status);
void initializeGame();
void initializeBall();
void movePaddle();
void moveBall();
void checkCollision();
void drawingField();
void score();
void gameOver();
bool waitReady();
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

  // Startwerte für Spiel setzen
  stateGame = SCORE;
  randomSeed(analogRead(0));

  // UDP-Nachricht abfangen, funktioniert asynchron
  receiveUDPMessage(udpPort);
}

void loop()
{
  breakString(udp_message);

  switch (stateGame)
  {
  case INIT:
    // Startwerte setzen
    initializeGame();

    // Spielfeldrand zeichnen
    drawingField();

    // Warten auf bereit
    if (waitReady())
    {
      // Richtung von Ball bestimmen und Ball zeichnen
      initializeBall();
      stateGame = PLAY;
    };
    break;

  case PLAY:
    // Paddle bewegen, Ball bewegen, und auf Kollission prüfen
    movePaddle();
    moveBall();
    checkCollision();
    break;

  case SCORE:
    // Wenn Score auf einer Seite 10 ist, soll Spiel abgebrochen werden
    if (score1 == max_score || score2 == max_score)
    {
      stateGame = GAME_OVER;
    }
    else
    {
      score();
      FastLED.show();
      delay(2000);
      stateGame = INIT;
    }
    break;

  case GAME_OVER:
    FastLED.clear();
    gameOver();
    FastLED.show();
    delay(2000);
    stateGame = INIT;
    score1 = 0;
    score2 = 0;
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

bool waitReady()
{
  movePaddle();
  if (directionPaddle1 == BACK && directionPaddle2 == BACK)
  {
    return 1;
  }
  else
  {
    return 0;
  }
};

void drawingField()
{
  // Feldbegrenzung zeichnen
  for (int i = 0; i < X_MAX; i++)
  {
    sendData(0, i, WHITE);
    sendData(X_MAX - 1, i, WHITE);
  }
}

void initializeGame()
{
  // lokale Variablen
  int startpositionPaddle1X = (X_MAX / 2) - (paddleLength / 2) -1;
  int startpositionPaddle1Y = ballLength + 1;
  int startpositionPaddle2X = (X_MAX / 2) - (paddleLength / 2) -1;
  int startpositionPaddle2Y = Y_MAX - ballLength - paddleHeight - 1;

  // Spielfeld leeren
  FastLED.clear();

  // Paddle1 zeichnen
  for (int i = 0; i < paddleLength; i++)
  {
    for (int j = 0; j < paddleHeight; j++)
    {
      paddle1[i].x = i + startpositionPaddle1X;
      paddle1[j].y = j + startpositionPaddle1Y;
      sendData(paddle1[i].x, paddle1[j].y, RED);
    }
  }

  // Paddle2 zeichnen
  for (int i = 0; i < paddleLength; i++)
  {
    for (int j = 0; j < paddleHeight; j++)
    {
      paddle2[i].x = i + startpositionPaddle2X;
      paddle2[j].y = j + startpositionPaddle2Y;
      sendData(paddle2[i].x, paddle2[j].y, RED);
    }
  }
}

void initializeBall()
{
  int startpositionBallX = random(2, X_MAX-2);
  int startpositionBallY = (Y_MAX / 2) - 1;

  // Ball zeichnen
  for (int i = 0; i < ballLength; i++)
  {
    for (int j = 0; j < ballLength; j++)
    {
      ball[i].x = i + startpositionBallX;
      ball[j].y = j + startpositionBallY;
      sendData(ball[i].x, ball[j].y, GREEN);
    }
  }
  directionBall = random(0, 4);
}

void moveBall()
{
  // lokale Variablen
  int directionX;
  int directionY;
  int step = 1;

  // alte Ballposition ausschalten (LEDs)
  for (int i = 0; i < ballLength; i++)
  {
    for (int j = 0; j < ballLength; j++)
    {
      sendData(ball[i].x, ball[j].y, BLACK);
    }
  }

  // Richtung des Balls definieren
  switch (directionBall)
  {
  case RIGHT_UP:
    directionX = step;
    directionY = step;
    break;

  case RIGHT_DOWN:
    directionX = step;
    directionY = -step;
    break;

  case LEFT_UP:
    directionX = -step;
    directionY = step;
    break;

  case LEFT_DOWN:
    directionX = -step;
    directionY = -step;
    break;
  }

  // neue Ball Position berechnen
  for (int i = 0; i < ballLength; i++)
  {
    ball[i].x = ball[i].x + directionX;
    ball[i].y = ball[i].y + directionY;
  }

  // neue Ball Position einschalten (LEDs)
  for (int i = 0; i < ballLength; i++)
  {
    for (int j = 0; j < ballLength; j++)
    {
      sendData(ball[i].x, ball[j].y, GREEN);
    }
  }
}

void movePaddle()
{
  // lokale Variablen
  int step = 2;

  // Paddle1 nach rechts bewegen
  if (directionPaddle1 == RIGHT && paddle1[paddleLength - 1].x != X_MAX - 1 - step)
  {
    // letzten LEDs auschalten
    for (int i = 0; i < step; i++)
    {
      sendData(paddle1[i].x, paddle1[0].y, BLACK);
      sendData(paddle1[i].x, paddle1[1].y, BLACK);
    }

    // Paddle1 bewegen
    for (int i = 0; i < paddleLength; i++)
    {
      paddle1[i].x = paddle1[i].x + step;
      sendData(paddle1[i].x, paddle1[0].y, RED);
      sendData(paddle1[i].x, paddle1[1].y, RED);
    }
  }
  // Paddle1 nach links bewegen
  if (directionPaddle1 == LEFT && paddle1[0].x != step)
  {
    // letzten LEDs auschalten
    for (int i = 1; i < step + 1; i++)
    {
      sendData(paddle1[paddleLength - i].x, paddle1[0].y, BLACK);
      sendData(paddle1[paddleLength - i].x, paddle1[1].y, BLACK);
    }

    // Paddle1 bewegen
    for (int i = 0; i < paddleLength; i++)
    {
      paddle1[i].x = paddle1[i].x - step;
      sendData(paddle1[i].x, paddle1[0].y, RED);
      sendData(paddle1[i].x, paddle1[1].y, RED);
    }
  }

  // Paddle2 nach rechts bewegen
  if (directionPaddle2 == RIGHT && paddle2[paddleLength - 1].x != X_MAX - 1 - step)
  {
    // letzten LEDs auschalten
    for (int i = 0; i < step; i++)
    {
      sendData(paddle2[i].x, paddle2[0].y, BLACK);
      sendData(paddle2[i].x, paddle2[1].y, BLACK);
    }

    // Paddle2 bewegen
    for (int i = 0; i < paddleLength; i++)
    {
      paddle2[i].x = paddle2[i].x + step;
      sendData(paddle2[i].x, paddle2[0].y, RED);
      sendData(paddle2[i].x, paddle2[1].y, RED);
    }
  }

  // Paddle2 nach links bewegen
  if (directionPaddle2 == LEFT && paddle2[0].x != step)
  {
    // letzten LEDs auschalten
    for (int i = 1; i < step + 1; i++)
    {
      sendData(paddle2[paddleLength - i].x, paddle2[0].y, BLACK);
      sendData(paddle2[paddleLength - i].x, paddle2[1].y, BLACK);
    }

    // Paddle2 bewegen
    for (int i = 0; i < paddleLength; i++)
    {
      paddle2[i].x = paddle2[i].x - step;
      sendData(paddle2[i].x, paddle2[0].y, RED);
      sendData(paddle2[i].x, paddle2[1].y, RED);
    }
  }
}

void checkCollision()
{
  // Check ob Ball gegen Wand geprallt ist
  if ((ball[0].x < 2) || (ball[1].x > X_MAX-3))
  {
    switch (directionBall)
    {
    case RIGHT_UP:
      directionBall = LEFT_UP;
      break;
    case RIGHT_DOWN:
      directionBall = LEFT_DOWN;
      break;
    case LEFT_UP:
      directionBall = RIGHT_UP;
      break;
    case LEFT_DOWN:
      directionBall = RIGHT_DOWN;
      break;
    }
  }

  // Check ob Ball gegen Paddle1 geprallt ist
  for (int i = 0; i < paddleLength - 1; i++)
  {
    if (ball[0].y <= paddle1[1].y + 1 && ball[0].x == paddle1[i].x)
    {
      switch (directionBall)
      {
      case LEFT_DOWN:
        directionBall = LEFT_UP;
        break;

      case RIGHT_DOWN:
        directionBall = RIGHT_UP;
        break;
      }
    }
  }

  // Check ob Ball gegen Paddle2 geprallt ist
  for (int i = 0; i < paddleLength; i++)
  {
    if (ball[1].y >= paddle2[0].y - 1 && ball[0].x == paddle2[i].x)
    {
      switch (directionBall)
      {
      case LEFT_UP:
        directionBall = LEFT_DOWN;
        break;

      case RIGHT_UP:
        directionBall = RIGHT_DOWN;
        break;
      }
    }
  }

  // Score erhöhen nache Punkteerfolg
  if (ball[0].y < paddle1[0].y)
  {
    score2++;
    stateGame = SCORE;
  }

  if (ball[1].y > paddle2[1].y)
  {
    score1++;
    stateGame = SCORE;
  }
}

void score()
{
  // lokale Variablen
  int counter1 = 0;
  int counter2 = 0;
  int startpointX1 = 38;
  int startpointY1 = 16;
  int startpointX2 = 38;
  int startpointY2 = 57;

  // Score anzeigen
  for (int i = startpointY1 + 7; i > startpointY1; i--)
  {
    for (int j = startpointX1; j < startpointX1 + 4; j++)
    {
      sendData(j, i, digitMatrix[score1][counter1]);
      counter1++;
    }
  }

  for (int i = startpointY2; i < startpointY2 + 7; i++)
  {
    for (int j = startpointX2 + 4; j > startpointX2; j--)
    {
      sendData(j, i, digitMatrix[score2][counter2]);
      counter2++;
    }
  }
}

void gameOver()
{
  // lokale Variablen
  int startpointX = 32;
  int startpointY1 = 29;
  int startpointY2 = 29;
  int counter = 0;

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

  // Paddle1: Richtung bestimmen durch Gyro
  if (id_gyro == "g1")
  {
    if (gyro.y == 1)
    {
      directionPaddle1 = RIGHT;
    }
    if (gyro.y == -1)
    {
      directionPaddle1 = LEFT;
    }
    if (gyro.y == 0)
    {
      directionPaddle1 = NONE;
    }
    if (gyro.x == 1)
    {
      directionPaddle1 = BACK;
    }
  }

  // Paddle2: Richtung bestimmen durch Gyro
  if (id_gyro == "g2")
  {
    if (gyro.y == -1)
    {
      directionPaddle2 = RIGHT;
    }
    if (gyro.y == 1)
    {
      directionPaddle2 = LEFT;
    }
    if (gyro.y == 0)
    {
      directionPaddle2 = NONE;
    }
    if (gyro.x == 1)
    {
      directionPaddle2 = BACK;
    }
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
  }
}

void receiveUDPMessage(int udpPort)
{
  // Wenn Nachricht an UDP dann speichern
  if (udp.listen(udpPort))
  {
    Serial.print("UDP Listening on IP: ");
    Serial.println(WiFi.localIP());
    udp.onPacket([](AsyncUDPPacket packet)
                 { udp_message = (const char *)packet.data(); });
  }
}