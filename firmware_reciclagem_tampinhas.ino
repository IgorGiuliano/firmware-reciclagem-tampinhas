
/*
 #
 #  Firmware Projeto Integrador V - Versão 0.02
 #  Autor: Igor Giuliano
 #  ESP 32, ILI9488, A9488, TCS34725, NEMA 17 STEPPER MOTOR
 #  Bibliotecas:  https://github.com/knolleary/pubsubclient
 #                https://github.com/bblanchon/ArduinoJson
 #                https://github.com/tzapu/WiFiManager
 #  Configuraçôes da IDE Arduino: version:  2.0.0-rc5
 #                                boards:   https://arduino.esp8266.com/stable/package_esp8266com_index.json
 #  
 */

#include "constants.h"
#include "credentials.h"
#include "Adafruit_TCS34725.h"
#include "BasicStepperDriver.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <WiFiManager.h>
#define MQTT_MAX_PACKET_SIZE 256
#define MQTT_KEEPALIVE 240
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <Servo.h>

#include <SPI.h>
#include <TFT_eSPI.h>  // Hardware-specific library

WiFiClientSecure espClient;
WiFiManager wifiManager;
PubSubClient mqttClient(espClient);

Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_614MS, TCS34725_GAIN_1X);

BasicStepperDriver stepper(MOTOR_STEPS, DIR, STEP);

Servo servo;

TFT_eSPI tft = TFT_eSPI();

char numberBuffer[NUM_LEN + 1] = "";
bool continue_screen_flow = false;
uint8_t numberIndex = 0;
char keyLabel[13][5] = { "CLR", "DEL", "OK", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0" };
uint16_t keyColor[13] = { TFT_RED, TFT_DARKGREY, TFT_DARKGREEN,
                          TFT_BLUE, TFT_BLUE, TFT_BLUE,
                          TFT_BLUE, TFT_BLUE, TFT_BLUE,
                          TFT_BLUE, TFT_BLUE, TFT_BLUE,
                          TFT_BLUE };
TFT_eSPI_Button key[15];

char startButtonLabel[2][5] = { "SIM", "NÃO" };
uint16_t startButtonColor[13] = { TFT_GREEN, TFT_RED };
TFT_eSPI_Button startButton[15];

int tampinhasVermelhas = 0;
int tampinhasVerdes = 0;

void setup() {
  uint16_t calData[5] = { 285, 3669, 244, 3569, 7 };
  tft.init();
  tft.setRotation(1);
  tft.setTouch(calData);

  servo.attach(SERVO_PIN);

  Serial.begin(BAUD_RATE);
  Serial.println();

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);

  Wire.begin(SDA_PIN, SCL_PIN);
  if (tcs.begin()) {
    Serial.println("Found sensor");
    tft.drawCentreString("TCS34725 FOUND", tft.width() / 2, tft.height() / 2, 2);
  } else {
    Serial.println("No TCS34725 found ... check your connections");
    tft.drawCentreString("No TCS34725 found ... check your connections", tft.width() / 2, tft.height() / 2, 2);
    while (1)
      ;
  }

  Serial.println("Initiated");
  Serial.println("WIFI config init");
  wifiManager.setAPCallback(configModeCallback);
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  if (!wifiManager.autoConnect("ESP32", "123asd123")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    ESP.restart();
    delay(5000);
  }

  espClient.setInsecure();
  tft.drawCentreString("WIFI config done", tft.width() / 2, tft.height() / 2, 2);
  Serial.println("WIFI config done");

  Serial.println('MQTT config init');
  mqttClient.setServer(IOTHUB_URL, IOTHUB_PORT);
  mqttClient.setCallback(callback);
  connectMqtt();
  Serial.println("MQTT config done");
  tft.drawCentreString("MQTT config done", tft.width() / 2, tft.height() / 2, 2);

  Serial.println("Testing servo motor");
  servo.write(30);
  delay(1000);
  servo.write(80);
  delay(1000);
  servo.write(130);
  delay(1000);
  servo.write(80);

  tft.drawCentreString("Servo motor test done", tft.width() / 2, tft.height() / 2, 2);

  stepper.begin(RPM, MICROSTEPS);
  stepper.rotate(-90);
  delay(1000);
  stepper.rotate(-90);
  delay(1000);
  stepper.rotate(-90);
  delay(1000);
  stepper.rotate(-90);
  tft.drawCentreString("Stepper motor test done", tft.width() / 2, tft.height() / 2, 2);

  drawInitializeScreen();
  delay(1000);
}

void loop() {
  tampinhasVermelhas = 0;
  tampinhasVerdes = 0;
  tft.fillScreen(TFT_DARKGREY);
  bool finish = false;
  while (!finish) {
    numberIndex = 0;
    numberBuffer[numberIndex] = 0;
    drawAwaitScreen();
    if (!continue_screen_flow) {
      continue_screen_flow = true;
      finish = true;
      continue;
    }
    drawCPFInputScreen();
    if (!continue_screen_flow) {
      continue_screen_flow = true;
      finish = true;
      continue;
    }
    drawStartCountScreen();
    if (!continue_screen_flow) {
      continue_screen_flow = true;
      finish = true;
      continue;
    }
    drawCountScreen();
    drawFinishCountScreen();
  }
}

void drawInitializeScreen() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawCentreString("A maquina inicializou com sucesso", tft.width() / 2, tft.height() / 2, 2);
}

void drawAwaitScreen() {
  tft.fillRect(0, 0, 480, 320, TFT_DARKGREY);
  tft.setTextFont(0);
  tft.setTextColor(TFT_GREEN, TFT_DARKGREY);
  tft.setTextSize(5);
  tft.drawCentreString("ECOCAPS", tft.width() / 2, tft.height() / 2 - 100, 1);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.setTextSize(3);
  tft.drawCentreString("Toque para inserir o CPF", tft.width() / 2, tft.height() / 2, 1);

  continue_screen_flow = false;
  while (!continue_screen_flow) {
    uint16_t t_x = 0, t_y = 0;
    bool pressed = tft.getTouch(&t_x, &t_y);
    if (pressed) {
      continue_screen_flow = true;
    }
  }
}

void status(const char* msg) {
  tft.setTextPadding(240);
  tft.setCursor(STATUS_X, STATUS_Y);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.setTextFont(0);
  tft.setTextDatum(TC_DATUM);
  tft.setTextSize(1);
  tft.drawString(msg, STATUS_X, STATUS_Y);
}

void drawCPFInputScreen() {
  tft.fillRect(0, 0, 480, 320, TFT_DARKGREY);

  // VOLTAR
  tft.fillRect(2, 1, 50, 50, TFT_BLUE);
  tft.drawRect(2, 1, 50, 50, TFT_WHITE);
  tft.setTextFont(0);
  tft.setCursor(2, 1);
  tft.setTextColor(TFT_WHITE, TFT_BLUE);
  tft.setTextSize(3);
  tft.drawString("<-", 5, 10);

  // CAMPO
  tft.fillRect(DISP_X, DISP_Y, DISP_W, DISP_H, TFT_BLACK);
  tft.drawRect(DISP_X, DISP_Y, DISP_W, DISP_H, TFT_WHITE);

  // DESENHA TECLADO
  for (uint8_t row = 0; row < 5; row++) {
    if (row == 4) {
      for (uint8_t col = 0; col < 3; col++) {
        uint8_t b = col + row * 3;

        if (b < 3) tft.setFreeFont(LABEL1_FONT);
        else tft.setFreeFont(LABEL2_FONT);
        if (col == 1) {
          uint8_t b = col - 1 + row * 3;
          key[b].initButton(&tft, KEY_X + col * (KEY_W + KEY_SPACING_X),
                            KEY_Y + row * (KEY_H + KEY_SPACING_Y),
                            KEY_W, KEY_H, TFT_WHITE, keyColor[b], TFT_WHITE,
                            keyLabel[b], KEY_TEXTSIZE);
          key[b].drawButton();
        }
      }
    } else {
      for (uint8_t col = 0; col < 3; col++) {
        uint8_t b = col + row * 3;

        if (b < 3) tft.setFreeFont(LABEL1_FONT);
        else tft.setFreeFont(LABEL2_FONT);

        key[b].initButton(&tft, KEY_X + col * (KEY_W + KEY_SPACING_X),
                          KEY_Y + row * (KEY_H + KEY_SPACING_Y),
                          KEY_W, KEY_H, TFT_WHITE, keyColor[b], TFT_WHITE,
                          keyLabel[b], KEY_TEXTSIZE);
        key[b].drawButton();
      }
    }
  }

  continue_screen_flow = false;
  while (!continue_screen_flow) {

    uint16_t t_x = 0, t_y = 0;
    bool pressed = tft.getTouch(&t_x, &t_y);
    if ((t_x > 2) && (t_x < (2 + 50))) {
      if ((t_x > 2) && (t_x <= (2 + 50))) {
        numberBuffer[numberIndex] = 0;
        if (numberIndex > 0) {
          numberIndex--;
          numberBuffer[numberIndex] = 0;
        }
        break;
      }
    }

    for (uint8_t b = 0; b < 15; b++) {
      if (pressed && key[b].contains(t_x, t_y)) {
        key[b].press(true);
      } else {
        key[b].press(false);
      }
    }

    for (uint8_t b = 0; b < 15; b++) {

      if (b < 3) tft.setFreeFont(LABEL1_FONT);
      else tft.setFreeFont(LABEL2_FONT);

      if (key[b].justReleased()) key[b].drawButton();

      if (key[b].justPressed()) {
        key[b].drawButton(true);

        if (b >= 3) {
          if (numberIndex < NUM_LEN) {
            numberBuffer[numberIndex] = keyLabel[b][0];
            numberIndex++;
            numberBuffer[numberIndex] = 0;  // zero terminate
          }
          status("");  // Clear the old status
        }

        if (b == 1) {
          numberBuffer[numberIndex] = 0;
          if (numberIndex > 0) {
            numberIndex--;
            numberBuffer[numberIndex] = 0;
          }
          status("");
        }

        if (b == 2) {
          Serial.println("CPF captado");
          Serial.println(numberBuffer);
          continue_screen_flow = true;
        }

        if (b == 0) {
          status("APAGADO");
          numberIndex = 0;
          numberBuffer[numberIndex] = 0;
        }

        tft.setTextDatum(TL_DATUM);
        tft.setFreeFont(&FreeSans18pt7b);
        tft.setTextColor(DISP_TCOLOR);

        int xwidth = tft.drawString(numberBuffer, DISP_X + 4, DISP_Y + 12);
        tft.fillRect(DISP_X + 4 + xwidth, DISP_Y + 1, DISP_W - xwidth - 5, DISP_H - 2, TFT_BLACK);

        delay(10);
      }
    }

    delay(100);
  }
}

void drawStartCountScreen() {
  tft.fillRect(0, 0, 480, 320, TFT_DARKGREY);
  tft.setTextFont(0);
  tft.setTextColor(TFT_GREEN, TFT_DARKGREY);
  tft.setTextSize(3);
  tft.drawCentreString("INICIAR A CONTAGEM?", tft.width() / 2, tft.height() / 2 - 100, 1);

  for (uint8_t col = 0; col < 2; col++) {
    startButton[col].initButton(&tft, 160 + col * (160), 160, 100, 50, TFT_WHITE, startButtonColor[col], TFT_WHITE, startButtonLabel[col], 1);
    startButton[col].drawButton();
  }

  continue_screen_flow = false;
  bool getOut = false;
  while (!continue_screen_flow) {
    uint16_t t_x = 0, t_y = 0;
    bool pressed = tft.getTouch(&t_x, &t_y);
    if (pressed) {
      for (uint8_t b = 0; b < 2; b++) {
        if (pressed && startButton[b].contains(t_x, t_y)) {
          startButton[b].press(true);
        } else {
          startButton[b].press(false);
        }
      }

      for (uint8_t b = 0; b < 2; b++) {
        if (startButton[b].justReleased()) startButton[b].drawButton();

        if (startButton[b].justPressed()) {
          startButton[b].drawButton(true);

          if (b == 0) {
            continue_screen_flow = true;
          }

          if (b == 1) {
            numberIndex = 0;
            numberBuffer[numberIndex] = 0;
            getOut = true;
          }
        }
      }

      if (getOut) {
        break;
      }
    }
  }
}

void drawCountScreen() {
  tft.fillRect(0, 0, 480, 320, TFT_DARKGREY);

  tft.setTextSize(2);
  tft.setTextColor(TFT_GREEN, TFT_DARKGREY);
  tft.drawCentreString("TOQUE PARA ENCERRAR A CONTAGEM", tft.width() / 2, tft.height() / 2 - 140, 1);

  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.drawCentreString("Quantidade de tampinhas de cada cor:", tft.width() / 2, tft.height() / 2 - 100, 1);

  tft.setTextSize(3);
  tft.fillRect(0, 100, 240, 220, TFT_RED);
  tft.drawRect(0, 100, 240, 220, TFT_WHITE);
  tft.setTextColor(TFT_BLACK, TFT_RED);

  tft.fillRect(240, 100, 240, 220, TFT_GREEN);
  tft.drawRect(240, 100, 240, 220, TFT_WHITE);

  unsigned long currenttime = millis();
  continue_screen_flow = false;
  while (!continue_screen_flow) {
    tft.setTextColor(TFT_BLACK, TFT_RED);
    char vermelho[5];
    sprintf(vermelho, "%d", tampinhasVermelhas);
    tft.drawCentreString(vermelho, 120, 220, 1);
    tft.setTextColor(TFT_BLACK, TFT_GREEN);
    char verde[5];
    sprintf(verde, "%d", tampinhasVerdes);
    tft.drawCentreString(verde, 360, 220, 1);
    float r, g, b;
    uint16_t t_x = 0, t_y = 0;
    bool pressed = tft.getTouch(&t_x, &t_y);
    if (pressed) {
      continue_screen_flow = true;
      Serial.println("A");
      break;
    }

    if (pressed) {
      continue_screen_flow = true;
      if (tampinhasVermelhas != 0) {
        sendData(numberBuffer, "RED", tampinhasVermelhas);
      }
      if (tampinhasVerdes != 0) {
        sendData(numberBuffer, "GREEN", tampinhasVerdes);
      }

      break;
    }

    stepper.rotate(-90);
    delay(1000);
    tcs.getRGB(&r, &g, &b);
    if ((r > g) && (g > (r / 2))) {
      Serial.println("É marrom");
      turnServoOrigin();
      delay(2000);
    } else if ((g > r) && (g > b)) {
      Serial.println("É verde");
      tampinhasVerdes++;
      turnServoLeft();
      delay(2000);
    } else if ((r > g) && (r > b)) {
      tampinhasVermelhas++;
      Serial.println("É vermelho");
      turnServoRight();
      delay(2000);
    }
    delay(1000);
  }
}


void drawFinishCountScreen() {
  tft.fillRect(0, 0, 480, 320, TFT_DARKGREY);
  tft.setTextFont(0);
  tft.setTextColor(TFT_GREEN, TFT_DARKGREY);
  tft.setTextSize(5);
  tft.drawCentreString("ECOCAPS", tft.width() / 2, tft.height() / 2 - 100, 1);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.setTextSize(3);
  tft.drawCentreString("Obrigado por colaborar!", tft.width() / 2, tft.height() / 2, 1);
  clearCPF();

  delay(5000);
}

void configModeCallback(WiFiManager* myWiFiManager) {
  Serial.println("Modo de configuração");
  Serial.println(WiFi.softAPIP());
  Serial.println(myWiFiManager->getConfigPortalSSID());
}

void saveConfigCallback() {
  Serial.println("Configuração Salva");
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.printf("\nMessage arrived: ");
  for (int i = 0; i < length; i++) {

    Serial.print((char)payload[i]);
  }
  Serial.println("---");
}

void connectMqtt() {

  Serial.printf("\nTrying to connect to MQTT server: %s\n", IOTHUB_URL);


  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection... ");

    if (mqttClient.connect(IOTHUB_DEVICE_NAME, IOTHUB_USER, IOTHUB_SAS_TOKEN)) {

      Serial.printf("\nMQTT connected at: %s", IOTHUB_URL);
      mqttClient.subscribe("devices/nodemcu-0001/messages/devicebound/#");
      Serial.printf("\n Connected to MQTT server:\t%s", IOTHUB_URL);

    } else {

      Serial.print("failed, state=");
      Serial.print(mqttClient.state());
      Serial.println("Trying again in 10 seconds.");

      delay(10000);
    }
  }
}

void turnServoLeft() {

  servo.write(30);
}

void turnServoRight() {
  servo.write(130);
}

void turnServoOrigin() {
  servo.write(80);
}

void clearCPF() {
  numberBuffer[numberIndex] = 0;
  if (numberIndex > 0) {
    numberIndex--;
    numberBuffer[numberIndex] = 0;
  }
}

void sendData(char* userCpf, char* capColor, int quantity) {
  DynamicJsonDocument doc(1024);
  char JSONbuffer[1024];
  doc["machine_name"] = MACHINE_NAME;
  doc["cap_color"] = capColor;
  doc["quantity"] = quantity;
  doc["user_cpf"] = userCpf;
  serializeJson(doc, JSONbuffer);

  mqttClient.publish("devices/nodemcu-0001/messages/events/", JSONbuffer);
}