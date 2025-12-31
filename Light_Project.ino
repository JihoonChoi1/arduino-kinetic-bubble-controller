#include <Adafruit_NeoPixel.h>
// for cos() function
#include <math.h> 
#include <Adafruit_TiCoServo.h>


// ==========================================
// Hardware Pins and Basic Settings
// ==========================================
#define LED_PIN    13    // Arduino pin connected to NeoPixel data line (DI)
#define SENSOR_PIN A0    // Sensor for LED control (Touch/Distance)
#define SENSOR2_PIN A5   // Sensor for Motor control (Speed)
#define SOUND_PIN  A2    // Sensor for Bubble Machine (Sound)
#define RELAY_PIN  2     // Pin for Relay Module
#define MOTOR_PIN  9     // Pin for Servo Motor
#define NUMPIXELS  60    // Total number of connected NeoPixel LEDs

// Create NeoPixel object (NumOfPixels, Pin Number, LED Type Setting)
Adafruit_NeoPixel pixels(NUMPIXELS, LED_PIN, NEO_GRBW + NEO_KHZ800);

// Create 360-degree Continuous Rotation Servo object
Adafruit_TiCoServo myServo;


// 1. Sensor Sensitivity: Values above this are considered 'touch'.
const int SENSOR_THRESHOLD = 300;     

// 2. Fade In Speed: Speed of turning blue when stroking (Higher is faster)
const float FADE_IN_SPEED = 1.0;      

// 3. Fade Out Speed: Speed of returning to white when hand is removed (Higher is faster)
const float FADE_OUT_SPEED = 0.3;     

// 4. Maintenance Time (ms): Grace period to consider it 'still touching' even if signal drops briefly
const long maintenanceTime = 800;

// 5. Hold Time Requirement (ms): Time required to hold hand still to trigger 'Breathing Mode' (5 sec)
const long HOLD_TIME_REQ = 5000;

// 6. Brightness Recovery Speed: Speed of returning to normal brightness from breathing mode
const float BRIGHTNESS_RECOVER_SPEED = 0.5; 

// [Motor Settings]
const int BASE_MOTOR_SPEED = 70;  // Base speed (Rotating slowly)
const int STOP_MOTOR_SPEED = 75;  // Stop speed

// [IMPORTANT] Lowered value to maximize detection range.
// Around 50 detects from ~70-80cm.
// (If it slows down alone without objects, increase to 60 or 70)
const int MOTOR_THRESHOLD = 50;  

// Distance where motor stops completely (Higher value = closer distance)
const int MAX_DISTANCE_VAL = 800;

// 7. Target Color Settings (Blue tones)
const int targetR = 135;
const int targetG = 206;
const int targetB = 250;

// Relay logic (Active Low/High check)
const int RELAY_ON = LOW;
const int RELAY_OFF = HIGH; 

// Sound Sensor Settings
const int SOUND_THRESHOLD = 300; 
const long BLOW_HOLD_TIME = 500;

// ==========================================


// ==========================================
// [State Variables] - Variables to remember current status
// ==========================================
// Current color progress (0.0 = Pure White ~ 255.0 = Full Blue)
float currentStep = 0.0;          

// Current global brightness (Base 200.0 / Changes in Breathing Mode)
float currentBrightness = 200.0;

// Default max brightness for breathing mode
float defaultBrightness = 200.0;

// Breathing depth range
const float breathRange = 190.0;

// Last time sensor was 'physically' triggered (for grace period calculation)
unsigned long lastDetectedTime = 0; 

// Last time bubble machine was triggered
unsigned long lastBlowTime = 0;

// ==========================================
// [Flag Variables] - True/False status flags
// ==========================================
// [Logical Touch]: True if considered 'touching' including grace period
bool isLogicalTouching = false; 

// [Physical Touch]: True if raw sensor value currently exceeds threshold
bool isPhysicalTouch = false;


// ==========================================
// [Strict Timer] - For detecting 5-second hold
// ==========================================
// Time when hand first touched (Resets if hand leaves)
unsigned long strictHoldStartTime = 0;
// Total duration held without releasing
unsigned long strictDuration = 0;


// Function Prototypes (To prevent compiler errors)
void calculateStateVariables(unsigned long currentTime, int sensorValue);
void applyVisualEffects(unsigned long currentTime);
void updateColor(int step);
void controlMotorSpeed();
void controlBubbleMachine(unsigned long currentTime);


// ========================================================
// 1. SETUP: Runs once when power is turned on
// ========================================================
void setup() {
  pixels.begin();           
  pixels.setBrightness(defaultBrightness);

  Serial.begin(9600);  

  myServo.attach(MOTOR_PIN);  
  myServo.write(BASE_MOTOR_SPEED);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, RELAY_OFF);

  // Initialize timer to avoid triggering bubbles on boot
  lastBlowTime = millis() - BLOW_HOLD_TIME - 1000;
}



// ========================================================
// 2. LOOP: Main logic running continuously
// ========================================================
void loop() {
  // Read current time and sensor value
  unsigned long currentTime = millis();
  int sensorValue = analogRead(SENSOR_PIN);
  
  // [Step 1] Determine State: Calculate if hand is present and for how long
  calculateStateVariables(currentTime, sensorValue);

  // [Step 2] Apply Effects: Adjust color (currentStep) and brightness based on state
  applyVisualEffects(currentTime);
    
  // [Step 3] Output: Apply calculated brightness and color to actual LEDs
  pixels.setBrightness((int)currentBrightness); // Apply brightness
  updateColor((int)currentStep);                // Apply color
  
  controlMotorSpeed();

  controlBubbleMachine(currentTime);
  
  // Brief delay to prevent excessive looping (10ms)
  delay(10);
}


// ========================================================
// 👇 [Bubble Machine Control Function]
// ========================================================
void controlBubbleMachine(unsigned long currentTime) {
  int soundValue = analogRead(SOUND_PIN);

  if (soundValue > SOUND_THRESHOLD) {
    lastBlowTime = currentTime;
  }
  // If within the hold time, keep relay ON
  if (currentTime - lastBlowTime < BLOW_HOLD_TIME) {
    digitalWrite(RELAY_PIN, RELAY_ON); 
    
  } else {
    digitalWrite(RELAY_PIN, RELAY_OFF); 
  }
}


// ========================================================
// 👇 [Motor Control Function]
// ========================================================
void controlMotorSpeed() {
  // 1. Read value from Sensor 2 (A5)
  int distValue = analogRead(SENSOR2_PIN);
  
  int newSpeed = BASE_MOTOR_SPEED; // Set to base speed initially

  // 2. When object is closer than threshold
  if (distValue > MOTOR_THRESHOLD) {
    // Map speed from 'Base' to 'Stop' based on distance
    // Higher value (closer) -> Closer to Stop speed
    newSpeed = map(distValue, MOTOR_THRESHOLD, MAX_DISTANCE_VAL, BASE_MOTOR_SPEED, STOP_MOTOR_SPEED);
    
    // Constrain value to prevent reverse rotation or out of bounds
    newSpeed = constrain(newSpeed, BASE_MOTOR_SPEED, STOP_MOTOR_SPEED); 
  }

  // 3. Apply to motor
  myServo.write(newSpeed);
}

// ========================================================
// 👇 Function Definitions (Helper functions)
// ========================================================

/**
 * [Function 1] Calculate State Variables
 * Analyze sensor and time to determine 'touching' and 'hold duration'
 */
void calculateStateVariables(unsigned long currentTime, int sensorValue) {
  
  // A. [Physical Touch Judgment]
  // If sensor value > threshold, consider it 'physically touched'
  isPhysicalTouch = (sensorValue > SENSOR_THRESHOLD);

  // B. [Logical Touch Judgment] (Grace period applied)
  if (isPhysicalTouch) {
    // If physically touching, it is logically touching
    isLogicalTouching = true;
    lastDetectedTime = currentTime; // Update last detected time
  } else {
    // Even if hand is gone, consider 'touching' if within grace period
    if (currentTime - lastDetectedTime < maintenanceTime) isLogicalTouching = true; 
    else isLogicalTouching = false; // Time is up, touch ended
  }

  // C. [Strict Hold Timer Calculation]
  if (isPhysicalTouch) {
    // If hand is touching, measure duration
    if (strictHoldStartTime == 0) {
      strictHoldStartTime = currentTime; // Record start time
    }
    // Current - Start = Duration held
    strictDuration = currentTime - strictHoldStartTime;
  } else {
    // If hand leaves even for a moment, reset timer to 0 immediately
    strictHoldStartTime = 0; 
    strictDuration = 0; 
  }
}

// 2. Apply Visual Effects Function
void applyVisualEffects(unsigned long currentTime) {
  
  if (isLogicalTouching) { // If stroking (Logical Touch)
    
    // 2-A. [Check for Breathing Mode]
    if (strictDuration > HOLD_TIME_REQ) { 
      
      // [Breathing Mode]
      currentStep = 255; 
      
      // 1. Calculate time elapsed since entering breathing mode
      unsigned long breathingTime = strictDuration - HOLD_TIME_REQ;
      
      float amplitude = breathRange / 2.0;

      float offset = defaultBrightness - amplitude;
      
      // 2. Apply cos function (Starts at max brightness and oscillates)
      float breath = (cos(breathingTime / 1000.0 * PI) * amplitude) + offset; 
      
      currentBrightness = breath;
      
    } else {
      // 2-B. [Normal Stroking Mode]

      // Recover Brightness
      if (currentBrightness < defaultBrightness) {
        currentBrightness += BRIGHTNESS_RECOVER_SPEED;
        if (currentBrightness > defaultBrightness) currentBrightness = defaultBrightness;
      }
      // (If current brightness > default, lower it - handling setting changes)
      else if (currentBrightness > defaultBrightness) {
         currentBrightness = defaultBrightness;
      }
      
      // Fade In Color
      currentStep += FADE_IN_SPEED;
      if (currentStep > 255) currentStep = 255;
    }
    
  } else { 
    // [Hand Removed]
    
    // Recover Brightness
    if (currentBrightness < defaultBrightness) {
      currentBrightness += BRIGHTNESS_RECOVER_SPEED;
      if (currentBrightness > defaultBrightness) currentBrightness = defaultBrightness;
    }
    else if (currentBrightness > defaultBrightness) {
       currentBrightness = defaultBrightness;
    }
    
    // Fade Out Color
    currentStep -= FADE_OUT_SPEED;
    if (currentStep < 0) currentStep = 0;
  }
}

/**
 * [Function 3] Update Color
 * Convert step (0~255) to actual LED color (RGBW) and show
 */
void updateColor(int step) {
  // map function: Blend two colors proportional to step value
  
  // White (W): As step increases, decreases 255 -> 0 (Fade Out)
  int w = map(step, 0, 255, 255, 0);       
  
  // Blue (RGB): As step increases, increases 0 -> Target (Fade In)
  int r = map(step, 0, 255, 0, targetR);   
  int g = map(step, 0, 255, 0, targetG);
  int b = map(step, 0, 255, 0, targetB);

  // Apply same color to all LEDs (0 to 59)
  for(int i=0; i<NUMPIXELS; i++) {
    pixels.setPixelColor(i, pixels.Color(r, g, b, w));
  }
  
  // Send command to Arduino to "Light up now!"
  pixels.show();
}