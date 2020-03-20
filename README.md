# ESP32CAM-sendphoto-telegram
Implementation of taking photo from ESP32-CAM OV2640 camera module, saving it to internal SPIFFS, then send it to Telegram Bot. A photo is taken everytime vibration sensor is vibrated, of course.

The code itself is a modification from [Randomnerdtutorial blog](https://randomnerdtutorials.com/esp32-cam-take-photo-save-microsd-card/) where the photo was just saved to a MicroSD card. After some googling, I just remembered that ESP32 (and other ESP development boards) has an internal SPIFFS (SPI Flash File System) which can store files for up to 4MB, thus, you can use this to save you from paying couple bucks for a MicroSD. For sending image to Telegram bot, I used this [awesome Telegram bot API](https://github.com/witnessmenow/Universal-Arduino-Telegram-Bot).

## Installation
#### Telegram Bot
- Create a Telegram Bot by having a chit-chat with the [`@BotFather`](https://t.me/BotFather)
- Get API token of the Telegram bot
- Paste it in `#define BOTtoken "<your bot API token here>"`
#### Arduino libraries
- Install `UniversalTelegramBot` and `ArduinoJson`
#### SSID Config
- Fill your SSID identity:
`const char* ssid = "<your wifi>";` 
`const char* password = "<your password>";`