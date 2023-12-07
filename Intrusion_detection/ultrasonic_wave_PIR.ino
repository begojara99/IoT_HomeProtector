// PIR motion sensor settings
const int MOTION_PIN = 36;
volatile bool motion_detected;
// Ultrasonic sensor settings
const int TRIG_PIN = 26;
const int ECHO_PIN = 25;
const int ULTRA_INTERVAL = 3000;
const int ULTRA_DEVIATION = 20;
long old_distance, new_distance;
unsigned long ultra_old_trigger;
unsigned long ultra_new_trigger;
volatile bool ultra_detected;
bool ultra_set;

// PIR motion sensor interrupt handler
void IRAM_ATTR detectMotion() {
  if ((ultra_new_trigger - ultra_old_trigger) > ULTRA_INTERVAL) {
    motion_detected = true;
  }
}

void setup() {
  Serial.begin(115200);
  // PIR motion sensor setup
  // MOTION_PIN mode INPUT_PULLUP
  pinMode(MOTION_PIN, INPUT_PULLUP);
  // Set MOTION_PIN as interrupt, assign interrupt function and set RISING mode
  attachInterrupt(digitalPinToInterrupt(MOTION_PIN), detectMotion, RISING);
  // Ultrasonic sensor setup
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
}

void loop() {
  if (motion_detected || ultra_detected) {
    Serial.println("Sensor detected!");
    motion_detected = false;
    ultra_detected = false;
    ultra_set = true;
    ultra_old_trigger = millis();
  }
  ultra_new_trigger = millis();
  if ((ultra_new_trigger - ultra_old_trigger) > ULTRA_INTERVAL) {
    // Triggering by 10us pulse
    digitalWrite(TRIG_PIN, LOW); // Trigger low for 2us
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH); // Trigger high for 10us
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    long duration, distance;
    // Getting duration for echo pulse
    duration = pulseIn(ECHO_PIN, HIGH);
    // sound speed = 340m/s = 34/1000cm/us
    // distance = duration * 34/1000 * 1/2
    distance = duration * 17 / 1000;
    Serial.println(distance);
    old_distance = new_distance;
    new_distance = distance;
    // Prevent redundant ultrasonic sensor detection
    if (ultra_set) {
      ultra_set = false;
      // Set the new distance to old distance
      old_distance = new_distance;
    }
    // Check if ultrasonic sensor detcted objects
    if (abs(new_distance - old_distance) > ULTRA_DEVIATION) {
      ultra_detected = true;
      ultra_old_trigger = millis();
    }
  }
}
