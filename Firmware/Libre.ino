#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <GxEPD2_BW.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>

#define BTN_NEXT 16
#define BTN_PREVIOUS 15
#define BTN_UP 4
#define BTN_DOWN 2
#define BTN_HOME 17

#define EPD_CS 10
#define EPD_DC 9
#define EPD_RST 8
#define EPD_BUSY 7

#define SD_CS 5
#define SD_SCK 12
#define SD_MISO 13
#define SD_MOSI 11

GxEPD2_BW<GxEPD2_420, GxEPD2_420::HEIGHT> display(
  GxEPD2_420(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY)
);

SPIClass sdSPI(FSPI);

struct Button {
    unit8_t pin;
    bool lastState;
};

Button buttons[] = {
    {BTN_NEXT, true},
    {BTN_PREVIOUS, true},
    {BTN_UP, true},
    {BTN_DOWN, true},
    {BTN_HOME, true},
};

enum ButtonEvent {
    NONE,
    NEXT,
    PREVIOUS,
    UP,
    DOWN,
    HOME,
};

ButtonEvent readButton(){
    const uint8_t pins[] = {
        BTN_UP, BTN_DOWN, BTN_HOME, BTN_NEXT, BTN_PREVIOUS
    };

    for (int i=0; i<5; i++){
        bool state = digitalRead(pins[i]);
         if (state == LOW && buttons[i].lastState == HIGH) {
      delay(25);
      if (digitalRead(pins[i]) == LOW) {
        buttons[i].lastState = LOW;
        return (ButtonEvent)(i + 1);
      }
    }
    if (state == HIGH) {
      buttons[i].lastState = HIGH;
    }
  }
  return NONE;
}

const int MAX_BOOKS = 100;
String books[MAX_BOOKS];
int bookCount = 0;
int selectedBook = 0;

bool isBookFile(const String &name) {
    String lower = name;
    lower.toLowerCase()

    return lower.endsWith(".pdf") ||
           lower.endsWith(".epub") ||
           lower.endsWith(".txt");
           lower.endsWith(".text");
}

void scanBooks() {
    bookCount = 0;

    File root = SD.open("/" );
    if (!root){
        return;
    }
    File file = root.openNextFile();

    while(file && bookCount < MAX_BOOKS) {
        if(file.isDirectory()){
            String name = String(file.name());
            
            if (isBookFile(name)){
                books[bookCount++] = name;
            }
        }  
      file.close();
      file = root.openNextFile();
    }
    root.close();

  if (selectedBook >= bookCount) {
    selectedBook = max(0, bookCount - 1);
  }
}

float readButtonVoltage(){
    const float ADC_REFERENCE = 3.3f;
    const float DIVIDER_RATIO = 2.0f;

    unit16_t raw = analogRead(BATTERY_ADC);
    float vadc = (raw / 4095.0f) * ADC_REFERENCE;
    return vadc * DIVIDER_RATIO;
}

int batteryPercent(float voltage){

  if (voltage >= 4.20) return 100;
  if (voltage >= 4.10) return 90;
  if (voltage >= 4.00) return 75;
  if (voltage >= 3.90) return 60;
  if (voltage >= 3.80) return 45;
  if (voltage >= 3.70) return 30;
  if (voltage >= 3.60) return 15;
  if (voltage >= 3.50) return 5;
  return 0;
}

void drawHeader(const char *title){
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(GxEPD_BLACK);
  display.setCursor(8, 20);
  display.print(title);

  display.drawLine(5, 27, display.width() - 5, 27, GxEPD_BLACK);
}

void drawBattery(){
    float v = readBatteryVoltage();
    int p = batteryPercent(v);

    String text = String(p)+"%";
    display.setFont(&FreeMonoBold19pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(display.width() - 55, 20);
    display.print(text);
}

void showMessage(const String &line1, const String &line2 = ""){
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);

        display.setFont(&FreeSansBold12pt7b);
        display.setTextColor(GxEPD_BLACK);
        display.setCursor(10, 40);
        display.print(line1);

        if (line2.length()) {
            display.setCursor(10, 65);
            display.print(line2);
        }
    } while (display.nextPage());
}

void drawLibrary(){
    display.setFullWindow();
    display.firstPage();

    do{
        display.fillScreen(GxEPD_WHITE);

        drawHeader("DREAMXV LIBRE");
        drawBattery();

        if (bookCount == 0) {
            display.setFont(&FreeMonoBold9pt7b);
            display.setCursor(10, 65);
            display.print("No books found.");
            display.setCursor(10, 90);
            display.print("Copy PDF/TXT to MicroSD.");
        } else {
            display.setFont(&FreeMonoBold9pt7b);

            int start = max(0, selectedBook - 4);
            int end = min(bookCount, start + 8);

            for (int i = start; i < end; i++) {
                int y = 50 + ((i - start) * 28);

                if (i == selectedBook) {
                    display.fillRect(5, y - 17, display.width() - 10, 23, GxEPD_BLACK);
                    display.setTextColor(GxEPD_WHITE);
                } else {
                    display.setTextColor(GxEPD_BLACK);
                }

                String name = books[i];
                if (name.length() > 37) {
                    name = name.substring(0, 34) + "...";
                }
                
                display.setCursor(10, y);
                display.print(name);
            }
        }
    } while (display.nextPage());
}

void showTextPage(File &file, uint32_t startPosition) {
  const int linesPerPage = 16;
  const int charsPerLine = 48;

  file.seek(startPosition);

  String lines[linesPerPage];
  int lineCount = 0;

  while (file.available() && lineCount < linesPerPage) {
    String line = file.readStringUntil('\n');
    line.replace("\r", "");

    while (line.length() > 0 && lineCount < linesPerPage) {
      if (line.length() <= charsPerLine) {
        lines[lineCount++] = line;
        line = "";
      } else {
        lines[lineCount++] = line.substring(0, charsPerLine);
        line = line.substring(charsPerLine);
      }
    }
  }

  display.setFullWindow();
  display.firstPage();

  do {
    display.fillScreen(GxEPD_WHITE);
    display.setFont(&FreeMonoBold9pt7b);
    display.setTextColor(GxEPD_BLACK);

    for (int i = 0; i < lineCount; i++) {
      display.setCursor(5, 20 + (i * 18));
      display.print(lines[i]);
    }
  } while (display.nextPage());
}

void readTextBook(const String &path) {
  File file = SD.open(path, FILE_READ);

  if (!file) {
    showMessage("Cannot open file");
    delay(1000);
    return;
  }

  uint32_t position = 0;

  showTextPage(file, position);

  while (true) {
    ButtonEvent event = readButton();

    if (event == HOME || event == PREVIOUS) {
      break;
    }

    if (event == NEXT) {
      if (position + 1 < file.size()) {

        position += 700;
        if (position >= file.size()) {
          position = file.size() > 700 ? file.size() - 700 : 0;
        }
        showTextPage(file, position);
      }
    }

    if (event == UP) {
      if (position >= 700) {
        position -= 700;
        showTextPage(file, position);
      }
    }

    delay(10);
  }

  file.close();
}

void renderPdfPage(const String &path, int pageNumber) {

  showMessage("PDF selected:", path);
  delay(1500);
  showMessage("PDF renderer", "not installed");
  delay(1500);
}

void openBook() {
  if (bookCount == 0) return;

  String path = books[selectedBook];
  String lower = path;
  lower.toLowerCase();

  if (lower.endsWith(".txt") || lower.endsWith(".text")) {
    readTextBook(path);
  } else if (lower.endsWith(".pdf")) {
    renderPdfPage(path, 1);
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_PREVIOUS, INPUT_PULLUP);
  pinMode(BTN_NEXT, INPUT_PULLUP);
  pinMode(BTN_HOME, INPUT_PULLUP);

  pinMode(BATTERY_ADC, INPUT);
  analogReadResolution(12);

  display.init(115200);
  display.setRotation(1);

  showMessage("DREAMXV LIBRE", "Starting...");
  delay(800);

  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

  if (!SD.begin(SD_CS, sdSPI)) {
    showMessage("MicroSD error", "Check card/wiring");
    delay(1500);
  } else {
    scanBooks();
  }

  drawLibrary();
}

void loop() {
  ButtonEvent event = readButton();

  switch (event) {
    case UP:
      if (bookCount > 0) {
        selectedBook--;
        if (selectedBook < 0) {
          selectedBook = bookCount - 1;
        }
        drawLibrary();
      }
      break;

    case DOWN:
      if (bookCount > 0) {
        selectedBook++;
        if (selectedBook >= bookCount) {
          selectedBook = 0;
        }
        drawLibrary();
      }
      break;

    case NEXT:
      openBook();
      drawLibrary();
      break;

    case PREVIOUS:

      scanBooks();
      drawLibrary();
      break;

    case HOME:
      scanBooks();
      selectedBook = 0;
      drawLibrary();
      break;

    case NONE:
      break;
  }

  delay(10);
}
