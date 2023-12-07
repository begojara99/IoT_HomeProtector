const int sampleWindow = 50; // Sample window width in mS (50 mS = 20Hz)
const int ledPin = 16; // LED 핀
unsigned int sample;
bool isExceeding = false;
unsigned long startTime = 0;
unsigned long cooldownStartTime = 0;
const unsigned long cooldownDuration = 5000; 

void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT); 

void loop() {
  unsigned long currentTime = millis();

  sample = analogRead(36); // 아날로그 핀
  Serial.println(sample);

  if (sample == 0) {
    
    if (!isExceeding) {
      cooldownStartTime = currentTime; // Start cooldown
      isExceeding = true;
    }
    digitalWrite(ledPin, HIGH);

  } 
  if (currentTime - cooldownStartTime >= cooldownDuration) {
      digitalWrite(ledPin, LOW); // Turn off LED after cooldown
      isExceeding = false; // Reset 
    } 
}
