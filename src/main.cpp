#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
#include <timers.h>
#include <Wire.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>

#define S_VERMELHO 5
#define S_AMARELO 6
#define S_VERDE 7

static TaskHandle_t task_handle = NULL;
static TimerHandle_t timer_handle = NULL;

static volatile int tempo = 10;
static volatile char estado = S_VERMELHO;
static volatile bool pedidoPedestreAtivo = false;

hd44780_I2Cexp lcd;

void atualizarSaidas(char estadoAtual) {
  digitalWrite(S_VERMELHO, LOW);
  digitalWrite(S_AMARELO, LOW);
  digitalWrite(S_VERDE, LOW);
  digitalWrite(estadoAtual, HIGH);
}

void atualizarLCD(char estadoAtual, int tempoAtual) {
  lcd.setCursor(0, 0);
  lcd.print("Estado:         ");
  lcd.setCursor(8, 0);

  if (estadoAtual == S_VERMELHO) {
    lcd.print("Verm");
  } else if (estadoAtual == S_AMARELO) {
    lcd.print("Amar");
  } else {
    lcd.print("Verd");
  }

  lcd.setCursor(0, 1);
  lcd.print("Tempo:          ");
  lcd.setCursor(7, 1);
  lcd.print(tempoAtual);
  lcd.print("s");
}

void proximoEstado() {
  if (estado == S_VERMELHO) {
    estado = S_VERDE;
    tempo = 10;
    pedidoPedestreAtivo = false;
  } else if (estado == S_VERDE) {
    estado = S_AMARELO;
    tempo = 2;
  } else {
    estado = S_VERMELHO;
    tempo = 10;
  }
}

void timerCallback(TimerHandle_t xTimer) {
  (void)xTimer;

  taskENTER_CRITICAL();
  if (tempo > 0) {
    tempo--;
  }
  taskEXIT_CRITICAL();

  if (task_handle != NULL) {
    vTaskResume(task_handle);
  }
}

void vTaskSemaforo(void *pvParameters) {
  (void)pvParameters;

  pinMode(S_VERMELHO, OUTPUT);
  pinMode(S_AMARELO, OUTPUT);
  pinMode(S_VERDE, OUTPUT);

  digitalWrite(S_VERMELHO, LOW);
  digitalWrite(S_AMARELO, LOW);
  digitalWrite(S_VERDE, LOW);

  taskENTER_CRITICAL();
  estado = S_VERMELHO;
  tempo = 10;
  pedidoPedestreAtivo = false;
  taskEXIT_CRITICAL();

  atualizarSaidas(estado);

  while (true) {
    int tempoAtual;
    char estadoAtual;
    bool mudouEstado = false;

    taskENTER_CRITICAL();
    if (tempo == 0) {
      proximoEstado();
      mudouEstado = true;
    }
    tempoAtual = tempo;
    estadoAtual = estado;
    taskEXIT_CRITICAL();

    if (mudouEstado) {
      atualizarSaidas(estadoAtual);
    }

    Serial.print("Tempo: ");
    Serial.print(tempoAtual);
    Serial.print("s | Estado: ");
    if (estadoAtual == S_VERMELHO) {
      Serial.println("VERMELHO");
    } else if (estadoAtual == S_AMARELO) {
      Serial.println("AMARELO");
    } else {
      Serial.println("VERDE");
    }

    atualizarLCD(estadoAtual, tempoAtual);

    vTaskSuspend(NULL);
  }
}

void vTaskPedestre(void *pvParameters) {
  (void)pvParameters;

  const uint8_t PIN_BOTAO = 2;
  bool ultimoEstado = HIGH;

  pinMode(PIN_BOTAO, INPUT_PULLUP);

  while (true) {
    bool estadoAtualBotao = (digitalRead(PIN_BOTAO) == LOW);

    if (estadoAtualBotao && !ultimoEstado) {
      taskENTER_CRITICAL();
      if (estado == S_VERDE && tempo > 3 && !pedidoPedestreAtivo) {
        tempo = 3;
        pedidoPedestreAtivo = true;
      }
      taskEXIT_CRITICAL();
    }

    ultimoEstado = estadoAtualBotao;
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void setup() {
  Serial.begin(9600);

  lcd.begin(16, 2);
  lcd.clear();

  xTaskCreate(vTaskSemaforo, "TaskSemaforo", 192, NULL, 2, &task_handle);
  xTaskCreate(vTaskPedestre, "TaskPedestre", 128, NULL, 1, NULL);

  if (task_handle != NULL) {
    vTaskSuspend(task_handle);
  }

  timer_handle = xTimerCreate("Timer1s", pdMS_TO_TICKS(1000), pdTRUE, NULL, timerCallback);
  if (timer_handle != NULL) {
    xTimerStart(timer_handle, 0);
  }
}

void loop() {}