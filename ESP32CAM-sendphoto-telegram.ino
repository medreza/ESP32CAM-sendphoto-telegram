#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <SPIFFS.h>
#include <FS.h>


// uncomment to select camera model
//#define CAMERA_MODEL_WROVER_KIT
//#define CAMERA_MODEL_ESP_EYE
//#define CAMERA_MODEL_M5STACK_PSRAM
//#define CAMERA_MODEL_M5STACK_WIDE
#define CAMERA_MODEL_AI_THINKER

#include "camera_pins.h"


const char* ssid = "<your ssid>"; // WiFi SSID name
const char* password = "<your password>"; // WiFi password

#define BOTtoken "<your bot token>"  // Telegram bot token (Get it from the BotFather)
#define FILE_PHOTO "/photo.jpg"

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

long botLastUpdate = 0; 
long botUpdateCheckInterval = 10000; // bot check update interval in miliseconds
int sensorPin = 2; // vibration sensor pin
bool standbyOn = false;
File file;
long measurement;


void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();
  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    ESP.restart();
  }
  else {
    delay(500);
    Serial.println("SPIFFS mounted successfully");
  }
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  //init with high specs to pre-allocate larger buffers
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  #if defined(CAMERA_MODEL_ESP_EYE)
    pinMode(13, INPUT_PULLUP);
    pinMode(14, INPUT_PULLUP);
  #endif
  // camera module init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
  sensor_t * s = esp_camera_sensor_get();
  //initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);//flip it back
    s->set_brightness(s, 1);//up the blightness just a bit
    s->set_saturation(s, -2);//lower the saturation
  }
  //drop down frame size for higher initial frame rate
  s->set_framesize(s, FRAMESIZE_SVGA);
  #if defined(CAMERA_MODEL_M5STACK_WIDE)
    s->set_vflip(s, 1);
    s->set_hmirror(s, 1);
  #endif
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("Connected to WiFi");
  Serial.print("Got IP address of '");
  Serial.print(WiFi.localIP());
  Serial.println("'");
}


void loop() {
    // check bot message update every interval
    if (millis() > botLastUpdate + botUpdateCheckInterval)  {
      Serial.println("Checking bot message update...");
      if (countBotMessageUpdate() >= 1) {
        checkStandbyStatus();
      }
      botLastUpdate = millis();
    }
   while (!standbyOn) {
     if (countBotMessageUpdate() >= 1) {
        Serial.println("last msg recv: " + String(bot.last_message_received));
        Serial.println("chat_id: " + String(bot.messages[0].chat_id));
        if (bot.messages[0].text == "standby" || bot.messages[0].text == "Standby") {
          bot.sendMessage(bot.messages[0].chat_id, "Standby mode activated! Camera and alarm are standby!", "");
          standbyOn = true;
          Serial.println("message update left: " + String(countBotMessageUpdate()));
          botLastUpdate = millis();
        }
        checkStandbyStatus();
     } 
   }
    measurement = vibration();
    delay(50);
    Serial.println("Sensor value: " + String(measurement));
    if (measurement > 50){
      Serial.println("!!! Vibration detected !!! Ringing alarm..");
      digitalWrite(4, HIGH);
      capturePhotoSaveSpiffs();
      delay(3000);
      readAndSendPhoto(SPIFFS, "/photo.jpg", 0);
      delay(3000);
      digitalWrite(4, LOW);
      Serial.println("Remaining bot message update: " + String(countBotMessageUpdate()));
      botLastUpdate = millis();
    }
}



// Count remaining bot message update. Returns zero if there is no message update.
int countBotMessageUpdate() {
    return bot.getUpdates(bot.last_message_received + 1);
}


// check if photo capture was successful
bool checkPhoto( fs::FS &fs ) {
  File f_pic = fs.open( FILE_PHOTO );
  unsigned int pic_sz = f_pic.size();
  return ( pic_sz > 100 );
}


// capture photo and save it to SPIFFS
void capturePhotoSaveSpiffs( void ) {
  camera_fb_t * fb = NULL; 
  bool ok = 0; // flag indicating if the picture has been taken correctly
  do {
    // take a photo with the camera
    Serial.println("Taking photo...");
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      return;
    }
    // create photo file
    Serial.printf("Picture file name: %s\n", FILE_PHOTO);
    file = SPIFFS.open(FILE_PHOTO, FILE_WRITE);
    // write captured photo to file
    if (!file) {
      Serial.println("Failed to open file in writing mode");
    }
    else {
      file.write(fb->buf, fb->len); // payload (image), payload length
      Serial.print("The picture has been saved in ");
      Serial.print(FILE_PHOTO);
      Serial.print(" - Size: ");
      Serial.print(file.size());
      Serial.println(" bytes");
    }
    // close the file
    file.close();
    esp_camera_fb_return(fb);
    // check if file has been correctly saved in SPIFFS
    ok = checkPhoto(SPIFFS);
  } while ( !ok );
}


// read saved photo file in SPIFFS and send it to the Telegram bot
void readAndSendPhoto(fs::FS &fs, const char * path, int i){
   Serial.printf("Reading and sending file: %s\r\n", path);
   file = fs.open(path);
    if(!file || file.isDirectory()){
        Serial.println("- failed to open file for reading");
        return;
    }
   // send photo file to Telegram
     String sent = bot.sendPhotoByBinary(bot.messages[i].chat_id, "image/jpeg", file.size(),
            isMoreDataAvailable,
            getNextByte);
        if (sent) {
          Serial.println("Photo was successfully sent");
        } else {
          Serial.println("Failed to send photo :(");
        }
    file.close();
}


bool isMoreDataAvailable(){
  return file.available();
}


byte getNextByte(){
  return file.read();
}


long vibration(){
  long measurement=pulseIn(sensorPin, HIGH);  //wait for the pin to get HIGH and returns measurement
  return measurement;
}


void checkStandbyStatus() {
    if (bot.messages[0].text == "status" || bot.messages[0].text == "Status") {
      if (standbyOn){
        bot.sendMessage(bot.messages[0].chat_id, "Camera and alarm are standby.", "");
      } else {
        bot.sendMessage(bot.messages[0].chat_id, "Camera and alarm are NOT standby. Please send 'standby' or 'Standby' command to activate standby mode.", "");
      }
      Serial.println("Remaining bot message update: " + String(bot.getUpdates(bot.last_message_received + 1)));
      botLastUpdate = millis();
  }
}
