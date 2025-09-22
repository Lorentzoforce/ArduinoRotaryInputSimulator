// 滑动变阻器角度读取和串口输出程序
// Potentiometer angle reading and serial output program
// 用于 Arduino Uno，读取旋转型滑动变阻器角度
// For Arduino Uno, reads rotary potentiometer angle

// 定义引脚
// Define pins
const int potPin = A0; // 滑动变阻器连接到 A0 // Potentiometer connected to A0
//Connect the other two terminals of potentiometer to 5V and ground

// 角度参数
// Angle parameters
const int minAngle = -150; // 逆时针极限角度 // Counterclockwise limit angle
const int maxAngle = 150;  // 顺时针极限角度 // Clockwise limit angle
const int adcMin = 0;     // ADC 最小值 // Minimum ADC value
const int adcMax = 1023;  // ADC 最大值 // Maximum ADC value
const bool REVERSE_DIRECTION = true; // 方向反转开关，true 为反转 // Direction reverse switch, true to reverse

// 全局变量
// Global variables
float currentAngle = 0.0; // 当前角度 // Current angle
float lastAngle = 0.0;   // 上次角度 // Last angle

void setup() {
  // 初始化串口
  // Initialize serial communication
  Serial.begin(9600);
  Serial.println("Potentiometer Angle Initialized");
}

void loop() {
  // 读取滑动变阻器值
  // Read potentiometer value
  int adcValue = analogRead(potPin);

  // 将 ADC 值映射到角度 (-150° 到 +150°)，根据 REVERSE_DIRECTION 调整方向
  // Map ADC value to angle (-150° to +150°), adjust direction based on REVERSE_DIRECTION
  currentAngle = map(adcValue, adcMin, adcMax, minAngle, maxAngle) / 1.0;
  if (REVERSE_DIRECTION) {
    currentAngle = -currentAngle; // 反转方向 // Reverse direction
  }

  // 仅在角度变化时输出，减少串口冗余
  // Only output when angle changes to reduce serial output
  if (abs(currentAngle - lastAngle) > 0.1) { // 增加小幅变化过滤 // Add small change filtering
    lastAngle = currentAngle;

    // 输出角度
    // Output angle
    Serial.print("angle=");
    Serial.println(currentAngle); // 负数表示逆时针（或根据 REVERSE_DIRECTION 调整），正数表示顺时针 // Negative for counterclockwise (or adjusted by REVERSE_DIRECTION), positive for clockwise
  }

  delay(100); // 控制输出频率 // Control output rate
}