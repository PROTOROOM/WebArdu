/* @WEBARDU_META
{
  "id": "pwm_fade",
  "name": "PWM Fade",
  "description": "PWM 밝기를 점진적으로 올리고 내립니다.",
  "params": [
    {
      "key": "PWM_PIN",
      "label": "PWM Pin",
      "type": "int",
      "min": 3,
      "max": 13,
      "default": 9,
      "description": "UNO 기준 PWM 지원 핀 번호"
    },
    {
      "key": "STEP_SIZE",
      "label": "Brightness Step",
      "type": "int",
      "min": 1,
      "max": 32,
      "default": 5,
      "description": "밝기 변경 폭"
    },
    {
      "key": "STEP_DELAY",
      "label": "Step Delay (ms)",
      "type": "int",
      "min": 1,
      "max": 100,
      "default": 30,
      "description": "밝기 단계 사이의 지연 시간"
    }
  ]
}
*/

int pwmPin = {{PWM_PIN}};
int stepSize = {{STEP_SIZE}};
int stepDelay = {{STEP_DELAY}};

void setup() {
  pinMode(pwmPin, OUTPUT);
}

void loop() {
  for (int value = 0; value <= 255; value += stepSize) {
    analogWrite(pwmPin, value);
    delay(stepDelay);
  }

  for (int value = 255; value >= 0; value -= stepSize) {
    analogWrite(pwmPin, value);
    delay(stepDelay);
  }
}
