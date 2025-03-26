         /////////////////////////////////////////////  
        //    AI-driven Dental Model Classifier    //
       //            w/ Edge Impulse              //
      //             ---------------             //
     //             (Sony Spresense)            //           
    //             by Kutluhan Aktar           // 
   //                                         //
  /////////////////////////////////////////////

//
// Via Spresense; collect dental cast images on an SD card to train an object detection model, display video stream, and run the model directly.
//
// For more information:
// https://www.theamplituhedron.com/projects/AI_driven_Dental_Model_Classifier_w_Edge_Impulse
//
//
// Connections
// Sony Spresense (w/ Extension Board) :  
//                                2.8'' 240x320 TFT LCD Touch Screen (ILI9341)
// D7   --------------------------- CS 
// D8   --------------------------- RESET 
// D9   --------------------------- D/C
// MOSI --------------------------- SDI (MOSI)
// SCK  --------------------------- SCK 
// 3.3V --------------------------- LED 
// MISO --------------------------- SDO(MISO) 
//                                Tiny (Embedded) Thermal Printer
// TX   --------------------------- RX
// RX   --------------------------- TX
// GND  --------------------------- GND
//                                Control Button (A)
// D2   --------------------------- +
//                                Control Button (B)
// D4   --------------------------- +
//                                Control Button (C)
// D14  --------------------------- +
//                                Keyes 10mm RGB LED Module (140C05)
// D3   --------------------------- R
// D5   --------------------------- G
// D6   --------------------------- B  


// Include the required libraries:
#include <Camera.h>
#include <SDHCI.h>
#include <RTC.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

// Include graphics (color bitmaps):
#include "data_collect.c"

// Define camera settings:
int               g_pict_id = 0;
int               g_width   = CAM_IMGSIZE_QUADVGA_H;
int               g_height  = CAM_IMGSIZE_QUADVGA_V;
CAM_IMAGE_PIX_FMT g_img_fmt = CAM_IMAGE_PIX_FMT_JPG;
CAM_WHITE_BALANCE g_wb      = CAM_WHITE_BALANCE_FLUORESCENT;
CAM_COLOR_FX      g_cfx     = CAM_COLOR_FX_NONE;
int               g_divisor = 7;

// Define the camera error object.
CamErr err;

// Initialize the SD class.
SDClass  theSD;

// Define the required pins for the 240x320 TFT LCD Touch Screen (ILI9341):
#define TFT_CS   7
#define TFT_RST  8
#define TFT_DC   9

// Use hardware SPI (on Spresense, SCK, MISO, MOSI) and the above for DC/CS/RST.
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

// Define the control button pins:
#define button_A   2
#define button_B   4
#define button_C   14

// Define the RGB LED pins:
#define redPin     3
#define greenPin   5
#define bluePin    6

// Define the data holders:
#define TFT_ROTATION 1

void setup(){
  Serial.begin(115200);

  pinMode(button_A, INPUT_PULLUP);
  pinMode(button_B, INPUT_PULLUP);
  pinMode(button_C, INPUT_PULLUP);
  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);

  // Initialize the RTC timer and set the date and time as the compiled date and time.
  RTC.begin();
  RtcTime compiledDateTime(__DATE__, __TIME__);
  RTC.setTime(compiledDateTime);

  // Initialize the TFT LCD Touch Screen (ILI9341):
  tft.begin();
  tft.setRotation(TFT_ROTATION);
  tft.fillScreen(ILI9341_NAVY);
  tft.setTextColor(ILI9341_WHITE);  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("Initializing...");

  // Check the connection status between Spresense and the SD card.
  while(!theSD.begin()){
    Serial.println("Insert SD card.");
    adjustColor(1,0,0);
    sleep(1);
  }
  Serial.println("SD card is detected successfully!\n");

  // Initialize the camera and print errors, if any.
  /* begin() without parameters means that
   * number of buffers = 1, 30FPS, QVGA, YUV 4:2:2 format */
  Serial.println("Camera initializing...");
  err = theCamera.begin();
  if(err != CAM_ERR_SUCCESS) printError(err);

  // Start video stream and print errors, if any.
  Serial.println("Starting streaming...");
  err = theCamera.startStreaming(true, CamCB);
  if(err != CAM_ERR_SUCCESS) printError(err);

  // Set the Auto white balance parameter and print errors, if any.
  Serial.println("Setting the Auto white balance parameter...");
  err = theCamera.setAutoWhiteBalanceMode(g_wb);
  if(err != CAM_ERR_SUCCESS) printError(err);
 
  // Set the still picture parameters and print errors, if any.
  Serial.println("Setting the still picture parameters...\n");
  err = theCamera.setStillPictureImageFormat(g_width, g_height, g_img_fmt, g_divisor);
  if(err != CAM_ERR_SUCCESS) printError(err);

  adjustColor(0,0,1);
  sleep(2);
}

void loop(){
  // Set the default color.
  adjustColor(1,0,1);
  // Save the recently captured picture to the SD card, named according to the selected class.
  if(!digitalRead(button_A)) takePicture(0);
  if(!digitalRead(button_B)) takePicture(1);
  if(!digitalRead(button_C)) takePicture(2);
}

void CamCB(CamImage img){
  // Check whether the img instance is available or not.
  if (img.isAvailable()){    
    // Convert the image data format to RGB565 so as to display images on the ILI9341 TFT screen.
    img.convertPixFormat(CAM_IMAGE_PIX_FMT_RGB565);
    /* You can use image data directly by using getImgSize() and getImgBuff().
     * for displaying image to a display, etc. */
    tft.drawRGBBitmap(0, 0, (uint16_t *)img.getImgBuff(), 320, 240);
    Serial.print("Image data size => "); Serial.print(img.getImgSize(), DEC); Serial.print(" , ");
    Serial.print("Image buffer address => "); Serial.println((unsigned long)img.getImgBuff(), HEX);
  }else{
    Serial.println("Failed to get video stream image!");
  }
}

void takePicture(int _class){
  char filename[30] = {0};
  // Take a picture with the given still picture settings.
  CamImage img = theCamera.takePicture();
  if(img.isAvailable()){
    // Pause video stream and print errors, if any.
    adjustColor(1,1,0);
    Serial.println("\nPausing streaming...\n");
    err = theCamera.startStreaming(false, CamCB);
    if(err != CAM_ERR_SUCCESS) printError(err);
    // Get the current date and time.
    RtcTime rtc;
    rtc = RTC.getTime();
    // Define the file name. 
    sprintf(filename, "%d_D_%04d.%02d.%02d__%02d.%02d.%02d.%s", _class, rtc.year(), rtc.month(), rtc.day(), rtc.hour(), rtc.minute(), rtc.second(), "JPG");
    // If the same file name exists, remove it in advance to prevent file appending.
    theSD.remove(filename);
    // Save the recently captured picture to the SD card.
    File myFile = theSD.open(filename, FILE_WRITE);
    myFile.write(img.getImgBuff(), img.getImgSize());
    myFile.close();
    Serial.println("Image captured successfully!");
    Serial.print("Selected Class: "); Serial.println(_class);
    Serial.printf("Name: %s\n", filename);
    Serial.printf("Resolution: %dx%d\n", img.getWidth(), img.getHeight());
    Serial.printf("Memory Size: %.2f / %.2f [KB]\n", img.getImgSize() / 1024.0, img.getImgBuffSize() / 1024.0);
    // Display the recently saved image information on the ILI9341 TFT screen.
    int c_x = 10, c_y = 100, r_x = 300, r_y = 120, r = 10, offset = 10, l = 15;
    tft.drawRGBBitmap(10, 10, (uint16_t*)(data_collect.pixel_data), (int16_t)data_collect.width, (int16_t)data_collect.height);
    tft.fillRoundRect(c_x, c_y, r_x, r_y, r, ILI9341_WHITE);
    tft.fillRoundRect(c_x+offset, c_y+offset, r_x-(2*offset), r_y-(2*offset), r, ILI9341_DARKGREEN);
    tft.setTextColor(ILI9341_WHITE); tft.setTextSize(1);
    tft.setCursor(c_x+(2*offset), c_y+(2*offset));
    tft.printf("Name: %s\n", filename);
    tft.setCursor(c_x+(2*offset), c_y+(2*offset)+l);
    tft.printf("Resolution: %dx%d\n", img.getWidth(), img.getHeight());
    tft.setCursor(c_x+(2*offset), c_y+(2*offset)+(2*l));
    tft.printf("Selected Class: %d", _class);
    sleep(5);
    // Resume video stream and print errors, if any.
    adjustColor(0,1,0);
    sleep(2);
    Serial.println("\nResuming streaming...\n");
    err = theCamera.startStreaming(true, CamCB);
    if(err != CAM_ERR_SUCCESS) printError(err);
  }else{
    Serial.println("Failed to take a picture!");
    adjustColor(1,0,0);
    sleep(2);
  }
}

void printError(enum CamErr err){
  adjustColor(1,0,0);
  sleep(2);
  Serial.print("Error: ");
  switch(err){
    case CAM_ERR_NO_DEVICE:             Serial.println("No Device");                      break;
    case CAM_ERR_ILLEGAL_DEVERR:        Serial.println("Illegal device error");           break;
    case CAM_ERR_ALREADY_INITIALIZED:   Serial.println("Already initialized");            break;
    case CAM_ERR_NOT_INITIALIZED:       Serial.println("Not initialized");                break;
    case CAM_ERR_NOT_STILL_INITIALIZED: Serial.println("Still picture not initialized");  break;
    case CAM_ERR_CANT_CREATE_THREAD:    Serial.println("Failed to create thread");        break;
    case CAM_ERR_INVALID_PARAM:         Serial.println("Invalid parameter");              break;
    case CAM_ERR_NO_MEMORY:             Serial.println("No memory");                      break;
    case CAM_ERR_USR_INUSED:            Serial.println("Buffer already in use");          break;
    case CAM_ERR_NOT_PERMITTED:         Serial.println("Operation not permitted");        break;
    default:
      break;
  }
}

void adjustColor(int r, int g, int b){
  digitalWrite(redPin, (1-r));
  digitalWrite(greenPin, (1-g));
  digitalWrite(bluePin, (1-b));
}
