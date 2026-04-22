/* @WEBARDU_META
{
  "id": "blink",
  "name": "Blink",
  "description": "LED를 고정 간격으로 점멸합니다.",
  "params": [
    {
      "key": "INTERVAL",
      "label": "Blink Interval (ms)",
      "type": "int",
      "min": 50,
      "max": 5000,
      "default": 500,
      "description": "점멸 간격을 밀리초 단위로 지정"
    }
  ]
}
*/

const int LED_PIN = 13;
int interval = {{INTERVAL}};

void setup() {
  pinMode(LED_PIN, OUTPUT);
}

void loop() {
  digitalWrite(LED_PIN, HIGH);
  delay(interval);
  digitalWrite(LED_PIN, LOW);
  delay(interval);
}
