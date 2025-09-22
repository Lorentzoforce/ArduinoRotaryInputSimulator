// 旋转编码器读取和串口输出程序
// Rotary encoder reading and serial output program
// 用于 Arduino Uno，检测旋转角度
// For Arduino Uno, detects rotation angle

// 定义引脚
// Define pins
const int clkPin = 2;  // CLK 连接到 D2（中断引脚） // CLK connected to D2 (interrupt pin)
const int dtPin = 3;   // DT 连接到 D3              // DT connected to D3
const int swPin = 4;   // SW 连接到 D4（按钮，可选） // SW connected to D4 (button, optional)

// 旋转编码器参数
// Rotary encoder parameters
const int stepsPerRevolution = 20; // 每圈步数，需根据编码器规格调整（常见 20 或 24） // Steps per revolution, adjust based on encoder spec (commonly 20 or 24)
const float degreesPerStep = 360.0 / stepsPerRevolution; // 每步角度 // Degrees per step

// 全局变量
// Global variables
volatile int encoderPos = 0; // 编码器位置计数 // Encoder position counter
int lastClkState;           // 保存 CLK 上次状态 // Last state of CLK
float totalAngle = 0.0;     // 累计旋转角度 // Accumulated rotation angle
bool angleChanged = false;   // 标记角度是否变化 // Flag to indicate angle change

void setup() {
  // 设置引脚模式
  // Set pin modes
  pinMode(clkPin, INPUT);
  pinMode(dtPin, INPUT);
  pinMode(swPin, INPUT_PULLUP); // 按钮使用上拉电阻 // Use internal pull-up resistor for button

  // 初始化串口
  // Initialize serial communication
  Serial.begin(9600);
  Serial.println("Rotary Encoder Initialized");

  // 读取初始 CLK 状态
  // Read initial CLK state
  lastClkState = digitalRead(clkPin);

  // 设置中断，CLK 引脚变化时触发
  // Attach interrupt on CLK pin change
  attachInterrupt(digitalPinToInterrupt(clkPin), updateEncoder, CHANGE);
}

void loop() {
  // 检测按钮按下（重置角度）
  // Check if button is pressed (reset angle)
  if (digitalRead(swPin) == LOW) {
    encoderPos = 0; // 重置计数 // Reset position count
    totalAngle = 0.0;
    Serial.println("angle reset"); // 输出归零角度 // Output reset angle
    angleChanged = true;
    delay(200); // 防抖 // Debounce delay
  }

  // 计算当前角度
  // Calculate current angle
  float currentAngle = encoderPos * degreesPerStep;

  // 仅在角度变化时输出，减少串口冗余
  // Only output when angle changes to reduce serial output
  if (currentAngle != totalAngle || angleChanged) {
    totalAngle = currentAngle;
    angleChanged = false;

    // 输出角度
    // Output angle
    Serial.print("angle=");
    Serial.println(totalAngle); // 直接输出角度，负数表示逆时针，正数表示顺时针 // Output angle, negative for counterclockwise, positive for clockwise
  }

  delay(100); // 控制输出频率 // Control output rate
}

// 中断服务函数，检测编码器变化
// Interrupt service routine to detect encoder changes
void updateEncoder() {
  int clkState = digitalRead(clkPin);
  int dtState = digitalRead(dtPin);

  // 检测 CLK 变化
  // Detect change on CLK
  if (clkState != lastClkState) {
    // 根据 DT 和 CLK 状态判断方向
    // Determine direction based on DT and CLK state
    if (dtState != clkState) {
      encoderPos--; // 顺时针，正角度 // Clockwise, positive angle
    } else {
      encoderPos++; // 逆时针，负角度 // Counter-clockwise, negative angle
    }
    angleChanged = true; // 标记角度变化 // Mark angle as changed
  }
  lastClkState = clkState; // 更新上次状态 // Update last CLK state
}