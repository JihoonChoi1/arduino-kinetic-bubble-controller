#include <Adafruit_NeoPixel.h>
// for cos() function
#include <math.h> 
#include <Adafruit_TiCoServo.h>


// ==========================================
// 하드웨어 핀 및 기본 설정
// ==========================================
#define LED_PIN    13   // 네오픽셀 데이터 선(DI)이 연결된 아두이노 핀 번호
#define SENSOR_PIN A0    // for LED 
#define SENSOR2_PIN A5   // for MOTOR
#define SOUND_PIN  A2    // for BUBBLES
#define RELAY_PIN  2 
#define MOTOR_PIN  9
#define NUMPIXELS  60    // 연결된 네오픽셀 LED의 총 개수

// Creating NeoPixel object. (NumOfPixels, Pin Number, LED Type Setting)
Adafruit_NeoPixel pixels(NUMPIXELS, LED_PIN, NEO_GRBW + NEO_KHZ800);

// 360 Motor object
Adafruit_TiCoServo myServo;


// 1. 센서 민감도: 이 값보다 센서값이 크면 '손이 닿았다'고 판단.
const int SENSOR_THRESHOLD = 300;     

// 2. 페이드 인 속도: 쓰다듬을 때 파란색으로 변하는 속도 (클수록 빠름)
const float FADE_IN_SPEED = 1.0;      

// 3. 페이드 아웃 속도: 손을 뗐을 때 하얀색으로 돌아오는 속도 (클수록 빠름)
const float FADE_OUT_SPEED = 0.3;     

// 4. 유예 시간(ms): 손이 잠깐 떨어져도 '쓰다듬는 중'으로 봐주는 시간 (부드러운 연결용)
const long maintenanceTime = 800;

// 5. 숨쉬기 대기 시간(ms): 손을 가만히 대고 몇 초가 지나야 숨쉬기 모드가 될지 결정 (5초)
const long HOLD_TIME_REQ = 5000;

// 6. 밝기 회복 속도: 숨쉬기 모드(어두움)에서 일반 모드(밝기 50)로 돌아올 때의 속도
const float BRIGHTNESS_RECOVER_SPEED = 0.5; 

// [모터 관련]
const int BASE_MOTOR_SPEED = 70;  // 평상시 속도 (천천히 돎)
const int STOP_MOTOR_SPEED = 75;  // 정지 속도

// 👇 [여기가 중요!] 감지 범위를 최대로 넓히기 위해 값을 낮췄습니다.
// 50 정도면 약 70~80cm 거리부터 반응하기 시작합니다.
// (만약 아무것도 없는데 혼자 느려지면 60이나 70으로 조금 올리세요)
const int MOTOR_THRESHOLD = 50;  

// 모터가 완전히 멈추는 거리 (값이 클수록 가까운 거리)
const int MAX_DISTANCE_VAL = 800;

// 7. 목표 색상 (파란색 계열) 설정
const int targetR = 135;
const int targetG = 206;
const int targetB = 250;


const int RELAY_ON = LOW;
const int RELAY_OFF = HIGH; 

const int SOUND_THRESHOLD = 300; 
const long BLOW_HOLD_TIME = 500;

// ==========================================


// ==========================================
// 🧠 [상태 변수] - 아두이노가 현재 상황을 기억하는 변수들
// ==========================================
// 현재 색상 진행도 (0.0 = 완전 흰색 ~ 255.0 = 완전 파란색)
float currentStep = 0.0;          

// 현재 LED 전체의 밝기 (기본 50.0 / 숨쉬기 모드일 때 변함)
float currentBrightness = 200.0;

float defaultBrightness = 200.0;

const float breathRange = 190.0;

// 마지막으로 센서가 '물리적으로' 감지된 시각 (유예 시간 계산용)
unsigned long lastDetectedTime = 0; 

unsigned long lastBlowTime = 0;

// ==========================================
// 🏳️ [플래그 변수] - 현재 상태를 True/False로 판단
// ==========================================
// [논리적 터치]: 유예 시간을 포함하여 "쓰다듬는 중"이라고 판단되면 True
bool isLogicalTouching = false; 

// [물리적 터치]: 지금 당장 센서 값이 임계값을 넘었으면 True
bool isPhysicalTouch = false;


// ==========================================
// ⏱️ [엄격한 타이머] - 5초 홀딩 감지용
// ==========================================
// 손이 처음 닿은 시각 (한 번이라도 떨어지면 리셋됨)
unsigned long strictHoldStartTime = 0;
// 손을 떼지 않고 유지한 총 시간
unsigned long strictDuration = 0;


// 함수 미리 알리기 (컴파일러 오류 방지용)
void calculateStateVariables(unsigned long currentTime, int sensorValue);
void applyVisualEffects(unsigned long currentTime);
void updateColor(int step);
void controlMotorSpeed();
void controlBubbleMachine(unsigned long currentTime);


// ========================================================
// 1. SETUP: 전원이 켜지면 딱 한 번 실행되는 설정
// ========================================================
void setup() {
  pixels.begin();           
  pixels.setBrightness(defaultBrightness);

  Serial.begin(9600);  

  myServo.attach(MOTOR_PIN);  
  myServo.write(BASE_MOTOR_SPEED);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, RELAY_OFF);

  lastBlowTime = millis() - BLOW_HOLD_TIME - 1000;
}



// ========================================================
// 2. LOOP: 전원이 꺼질 때까지 무한 반복되는 메인 로직
// ========================================================
void loop() {
  // 현재 시간(흐른 시간)과 센서 값을 읽어옵니다.
  unsigned long currentTime = millis();
  int sensorValue = analogRead(SENSOR_PIN);
  
  // [1단계] 현재 상황 판단: 손이 있는지, 얼마나 있었는지 계산합니다.
  calculateStateVariables(currentTime, sensorValue);

  // [2단계] 효과 결정: 계산된 상황에 맞춰 색상(currentStep)과 밝기를 조절합니다.
  applyVisualEffects(currentTime);
    
  // [3단계] 최종 출력: 결정된 밝기와 색상을 실제 LED에 적용합니다.
  pixels.setBrightness((int)currentBrightness); // 밝기 적용
  updateColor((int)currentStep);                // 색상 적용
  
  controlMotorSpeed();

  controlBubbleMachine(currentTime);
  // 너무 빠른 반복을 막기 위해 0.01초 대기
  delay(10);
}


void controlBubbleMachine(unsigned long currentTime) {
int soundValue = analogRead(SOUND_PIN);

  if (soundValue > SOUND_THRESHOLD) {
    lastBlowTime = currentTime;
  }
  if (currentTime - lastBlowTime < BLOW_HOLD_TIME) {
    digitalWrite(RELAY_PIN, RELAY_ON); 
    
  } else {
    digitalWrite(RELAY_PIN, RELAY_OFF); 
  }
}


// ========================================================
// 👇 [변경된 모터 제어 함수]
// ========================================================
void controlMotorSpeed() {
  // 1. 2번 센서(A1) 값 읽기
  int distValue = analogRead(SENSOR2_PIN);
  
  int newSpeed = BASE_MOTOR_SPEED; // 일단 기본 속도로 설정

  // 2. 물체가 감지 범위(Threshold)보다 가까이 왔을때
  if (distValue > MOTOR_THRESHOLD) {
    // 거리에 따라 속도를 '기본(70)'에서 '정지(75)' 사이로 변환 (Map 함수)
    // 값이 클수록(가까울수록) -> 75(정지)에 가까워짐
    newSpeed = map(distValue, MOTOR_THRESHOLD, MAX_DISTANCE_VAL, BASE_MOTOR_SPEED, STOP_MOTOR_SPEED);
    
    // 혹시 계산된 값이 75를 넘어가면(반대 회전) 안 되니까 꽉 잡음
    newSpeed = constrain(newSpeed, BASE_MOTOR_SPEED, STOP_MOTOR_SPEED); 
  }

  // 3. 모터에 적용
  myServo.write(newSpeed);
}

// ========================================================
// 👇 함수 정의 (세부 기능을 담당하는 부하 직원들)
// ========================================================

/**
 * [함수 1] 상태 계산
 * 센서 값과 시간을 분석해서 '터치 중인지', '얼마나 오래 대고 있었는지'를 판단합니다.
 */
void calculateStateVariables(unsigned long currentTime, int sensorValue) {
  
  // A. [물리적 터치 판단]
  // 센서 값이 설정한 기준(300)보다 크면 '물리적으로 닿았다'고 봅니다.
  isPhysicalTouch = (sensorValue > SENSOR_THRESHOLD);

  // B. [논리적 터치 판단] (유예 시간 적용)
  if (isPhysicalTouch) {
    // 실제로 손이 있으면 당당하게 터치 중!
    isLogicalTouching = true;
    lastDetectedTime = currentTime; // 마지막 감지 시간 갱신
  } else {
    // 손이 없더라도, 마지막 감지 후 0.8초(maintenanceTime)가 안 지났으면 '터치 중'으로 쳐줍니다.
    if (currentTime - lastDetectedTime < maintenanceTime) isLogicalTouching = true; 
    else isLogicalTouching = false; // 시간 다 됐으면 터치 끝!
  }

  // C. [엄격한 홀딩 타이머 계산]
  if (isPhysicalTouch) {
    // 손이 닿아있다면 시간을 잽니다.
    if (strictHoldStartTime == 0) {
      strictHoldStartTime = currentTime; // 카운트 시작점 기록
    }
    // 현재 시간 - 시작 시간 = 누르고 있는 시간(duration)
    strictDuration = currentTime - strictHoldStartTime;
  } else {
    // 손이 한 순간이라도 떨어지면 타이머를 즉시 0으로 리셋합니다. (봐주기 없음)
    strictHoldStartTime = 0; 
    strictDuration = 0; 
  }
}

// 2. 효과 적용 함수
void applyVisualEffects(unsigned long currentTime) {
  
  if (isLogicalTouching) { // 쓰다듬기 중
    
    // 2-A. [숨쉬기 모드 발동 체크]
    if (strictDuration > HOLD_TIME_REQ) { 
      
      // [숨쉬기 모드]
      currentStep = 255; 
      
      // 1. 숨쉬기 모드 진입 후 흐른 시간 계산
      unsigned long breathingTime = strictDuration - HOLD_TIME_REQ;
      
      float amplitude = breathRange / 2.0;

      float offset = defaultBrightness - amplitude;
      // 2. cos 함수 적용 (최대 밝기 50에서 시작하여 내려감)
      // breathingTime이 0일 때 cos(0)=1 이므로 (1 * 20) + 30 = 50
      float breath = (cos(breathingTime / 1000.0 * PI) * amplitude) + offset; 
      
      currentBrightness = breath;
      
    } else {

      if (currentBrightness < defaultBrightness) {
        currentBrightness += BRIGHTNESS_RECOVER_SPEED;
        if (currentBrightness > defaultBrightness) currentBrightness = defaultBrightness;
      }
      // (만약 현재 밝기가 설정값보다 크면 낮춰줌 - 설정 변경 시 대응)
      else if (currentBrightness > defaultBrightness) {
         currentBrightness = defaultBrightness;
      }
      
      currentStep += FADE_IN_SPEED;
      if (currentStep > 255) currentStep = 255;
    }
    
  } else { 
    if (currentBrightness < defaultBrightness) {
      currentBrightness += BRIGHTNESS_RECOVER_SPEED;
      if (currentBrightness > defaultBrightness) currentBrightness = defaultBrightness;
    }
    else if (currentBrightness > defaultBrightness) {
       currentBrightness = defaultBrightness;
    }
    
    currentStep -= FADE_OUT_SPEED;
    if (currentStep < 0) currentStep = 0;
  }
}

/**
 * [함수 3] 색상 업데이트
 * 0~255 사이의 step 값을 받아 실제 LED 색상(RGBW)으로 변환하고 켬.
 */
void updateColor(int step) {
  // map 함수: step 값에 비례하여 두 가지 색을 섞습니다.
  
  // 흰색(W): step이 커질수록 255 -> 0으로 줄어듭니다. (Fade Out)
  int w = map(step, 0, 255, 255, 0);       
  
  // 파란색(RGB): step이 커질수록 0 -> 목표값으로 늘어납니다. (Fade In)
  int r = map(step, 0, 255, 0, targetR);   
  int g = map(step, 0, 255, 0, targetG);
  int b = map(step, 0, 255, 0, targetB);

  // 모든 LED(0번부터 59번까지)에 같은 색을 칠합니다.
  for(int i=0; i<NUMPIXELS; i++) {
    pixels.setPixelColor(i, pixels.Color(r, g, b, w));
  }
  
  // 아두이노에게 "이제 불을 켜!"라고 전송합니다.
  pixels.show();
}