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
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include "Adafruit_Thermal.h"

// Include the Edge Impulse FOMO model converted to an Arduino library:
#include <Dental_Model_Classifier_inferencing.h>

// Define the required parameters to run an inference with the Edge Impulse model.
#define EI_CAMERA_RAW_FRAME_BUFFER_COLS   1280
#define EI_CAMERA_RAW_FRAME_BUFFER_ROWS   960

#define CAPTURED_IMAGE_BUFFER_COLS        320
#define CAPTURED_IMAGE_BUFFER_ROWS        320

static uint8_t *ei_camera_capture_out = NULL;

// Define the dental model category (class) names and color codes:
const char *classes[] = {"Cast", "Failed", "Implant"};
uint32_t color_codes[] = {ILI9341_GREEN, ILI9341_MAGENTA, ILI9341_ORANGE};

// Include graphics (color bitmaps):
#include "dental.c"

// Include icons for the thermal printer.
#include "dental_logo.h"

// Define camera settings:
int               g_pict_id = 0;
int               g_width   = CAPTURED_IMAGE_BUFFER_COLS;
int               g_height  = CAPTURED_IMAGE_BUFFER_ROWS;
CAM_IMAGE_PIX_FMT g_img_fmt = CAM_IMAGE_PIX_FMT_YUV422;
CAM_WHITE_BALANCE g_wb      = CAM_WHITE_BALANCE_AUTO;
CAM_COLOR_FX      g_cfx     = CAM_COLOR_FX_NONE;
int               g_divisor = 7;

// Define the camera error object.
CamErr err;

// Define the thermal printer object passing commands through Spresense's hardware serial port (Serial2).
Adafruit_Thermal printer(&Serial2);

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
int predicted_class = -1;
int b_b_x, b_b_y, b_b_w, b_b_h;

void setup(){
  Serial.begin(115200);

  pinMode(button_B, INPUT_PULLUP);
  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);

  // Initialize the TFT LCD Touch Screen (ILI9341):
  tft.begin();
  tft.setRotation(TFT_ROTATION);
  tft.fillScreen(ILI9341_NAVY);
  tft.setTextColor(ILI9341_WHITE);  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("Initializing...");

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

  // Initialize the hardware serial (Serial2).
  Serial2.begin(9600);
  // Initialize the thermal printer.  
  printer.begin();
  
  adjustColor(0,0,1);
  sleep(2);
}

void loop(){
  // Set the default color.
  adjustColor(1,0,1);
  
  // If the control button (B) is pressed, run the Edge Impulse FOMO model to classify dental casts.
  if(!digitalRead(button_B)) run_inference_to_make_predictions();
}

void run_inference_to_make_predictions(){
  // Summarize the Edge Impulse FOMO model inference settings (from model_metadata.h):
  ei_printf("\nInference settings:\n");
  ei_printf("\tImage resolution: %dx%d\n", EI_CLASSIFIER_INPUT_WIDTH, EI_CLASSIFIER_INPUT_HEIGHT);
  ei_printf("\tFrame size: %d\n", EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE);
  ei_printf("\tNo. of classes: %d\n", sizeof(ei_classifier_inferencing_categories) / sizeof(ei_classifier_inferencing_categories[0]));
  
  // Take a picture with the given still picture settings.
  CamImage img = theCamera.takePicture();
  
  if(img.isAvailable()){
    // Pause video stream and print errors, if any.
    adjustColor(1,1,0);
    Serial.println("\nPausing streaming...\n");
    err = theCamera.startStreaming(false, CamCB);
    if(err != CAM_ERR_SUCCESS) printError(err);

    // Resize the currently captured image depending on the given FOMO model.
    CamImage res_img;
    img.resizeImageByHW(res_img, EI_CLASSIFIER_INPUT_WIDTH, EI_CLASSIFIER_INPUT_HEIGHT);
    Serial.printf("Captured Image Resolution: %d / %d\nResized Image Resolution: %d / %d", img.getWidth(), img.getHeight(), res_img.getWidth(), res_img.getHeight());

    // Convert the resized (sample) image data format to GRAYSCALE so as to run inferences with the model.
    res_img.convertPixFormat(CAM_IMAGE_PIX_FMT_GRAY);
    Serial.print("\nResized Image Format: ");
    Serial.println((res_img.getPixFormat() == CAM_IMAGE_PIX_FMT_GRAY) ? "GRAYSCALE" : "ERROR");

    // Run inference:
    ei::signal_t signal;
    ei_camera_capture_out = res_img.getImgBuff();
    // Create a signal object from the resized and converted sample image.
    signal.total_length = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT;
    signal.get_data = &ei_camera_cutout_get_data;
    // Run the classifier:
    ei_impulse_result_t result = { 0 };
    EI_IMPULSE_ERROR _err = run_classifier(&signal, &result, false);
    if(_err != EI_IMPULSE_OK){
      ei_printf("ERR: Failed to run classifier (%d)\n", err);
      return;
    }

    // Print the inference timings on the serial monitor.
    ei_printf("\nPredictions (DSP: %d ms., Classification: %d ms., Anomaly: %d ms.): \n",
        result.timing.dsp, result.timing.classification, result.timing.anomaly);

    // Obtain the object detection results and bounding boxes for the detected labels (classes). 
    bool bb_found = result.bounding_boxes[0].value > 0;
    for(size_t ix = 0; ix < EI_CLASSIFIER_OBJECT_DETECTION_COUNT; ix++){
      auto bb = result.bounding_boxes[ix];
      if(bb.value == 0) continue;
      // Print the detected bounding box measurements on the serial monitor.
      ei_printf("    %s (", bb.label);
      ei_printf_float(bb.value);
      ei_printf(") [ x: %u, y: %u, width: %u, height: %u ]\n", bb.x, bb.y, bb.width, bb.height);
      b_b_x = bb.x; b_b_y =  bb.y; b_b_w = bb.width; b_b_h = bb.height;
      // Get the predicted label (class).
      if(bb.label == "cast") predicted_class = 0;
      if(bb.label == "failed") predicted_class = 1;
      if(bb.label == "implant") predicted_class = 2;
      Serial.print("\nPredicted Class: "); Serial.println(predicted_class);
    }
    if(!bb_found) ei_printf("    No objects found!\n");

    // Detect anomalies, if any:
    #if EI_CLASSIFIER_HAS_ANOMALY == 1
      ei_printf("Anomaly: ");
      ei_printf_float(result.anomaly);
      ei_printf("\n");
    #endif    

    // If the Edge Impulse FOMO model predicted a label (class) successfully:
    if(predicted_class != -1){
      // Scale the detected bounding box.
      int box_scale_x = tft.width() / EI_CLASSIFIER_INPUT_WIDTH;
      b_b_x = b_b_x * box_scale_x;
      b_b_w = b_b_w * box_scale_x * 16;
      if((b_b_w + b_b_x) > (tft.width() - 10)) b_b_w = tft.width() - b_b_x - 10;
      int box_scale_y = tft.height() / EI_CLASSIFIER_INPUT_HEIGHT;
      b_b_y = b_b_y * box_scale_y;
      b_b_h = b_b_h * box_scale_y * 16;
      if((b_b_h + b_b_y) > (tft.height() - 10)) b_b_h = tft.height() - b_b_y - 10;
      
      // Display the predicted label (class) and the detected bounding box on the ILI9341 TFT screen.
      for(int i=0; i<5; i++){
        tft.drawRect(b_b_x+i, b_b_y+i, b_b_w-(2*i), b_b_h-(2*i), color_codes[predicted_class]);
      }
      int c_x = 10, c_y = 10, r_x = 120, r_y = 40, r = 3, offset = 6;
      tft.drawRGBBitmap(10, c_y+r_y+10, (uint16_t*)(dental.pixel_data), (int16_t)dental.width, (int16_t)dental.height);
      tft.fillRoundRect(c_x, c_y, r_x, r_y, r, ILI9341_WHITE);
      tft.fillRoundRect(c_x+offset, c_y+offset, r_x-(2*offset), r_y-(2*offset), r, color_codes[predicted_class]);
      tft.setTextColor(ILI9341_WHITE); tft.setTextSize(2);
      tft.setCursor(c_x+(2*offset), c_y+(2*offset));
      tft.printf(classes[predicted_class]);

      // Print the predicted label (class) information via the thermal printer.
      print_thermal(predicted_class);

      // Clear the predicted class (label).
      predicted_class = -1;
    }
     
    sleep(10);
    
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

void print_thermal(int _class){
  printer.printBitmap(80, 80, dental_logo);
  printer.boldOn();
  printer.justify('R');
  printer.setSize('L');
  printer.println(classes[_class]);
  if(_class == 0){
    printer.boldOff();
    printer.justify('L');
    printer.setSize('M');
    printer.println("Dental Casts:\n");
    printer.setSize('S');
    printer.println("Big Central");
    printer.println("Antagonist");
    printer.println("Orthodontic");
    printer.println("Prognathous");
    printer.println("Strange Inf.");
    printer.println("Strange Sup.");
  }
  printer.feed(5);
  printer.setDefault(); // Restore printer to defaults.
}

int ei_camera_cutout_get_data(size_t offset, size_t length, float *out_ptr){
  // Convert the given image data (buffer) to the out_ptr format required by the Edge Impulse FOMO model.
  size_t bytes_left = length;
  size_t out_ptr_ix = 0;
  // read byte for byte
  while(bytes_left != 0){
    // grab the value and convert to r/g/b
    uint8_t pixel = ei_camera_capture_out[offset];
    uint8_t r, g, b;
    mono_to_rgb(pixel, &r, &g, &b);
    // then convert to out_ptr format
    float pixel_f = (r << 16) + (g << 8) + b;
    out_ptr[out_ptr_ix] = pixel_f;
    // and go to the next pixel
    out_ptr_ix++;
    offset++;
    bytes_left--;
  }
  return 0;
}

static inline void mono_to_rgb(uint8_t mono_data, uint8_t *r, uint8_t *g, uint8_t *b){
  uint8_t v = mono_data;
  *r = *g = *b = v;
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
