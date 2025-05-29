#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS 15
#define PH_PIN 34
#define TURBIDITY_PIN 13

const int in1 = 33, in2 = 25, in3 = 26, in4 = 27;
int enA = 32, enB = 14;

const int in5 = 16, in6 = 17, in7 = 18, in8 = 19;
int enC = 13, enD = 23;

const int in9 = 4, in10 = 5, in11 = 21, in12 = 2;

unsigned long waktuMulaiIsiAir = 0;
const unsigned long durasiIsiAir = 10000; // 10 detik
bool sedangMengisiAir = false;
bool airSebelumnyaKeruh = false;

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// PWM pompa
void hidupkanPompa(int pin1, int pin2, int enPin, int kecepatan)
{
  digitalWrite(pin1, HIGH);
  digitalWrite(pin2, LOW);
  analogWrite(enPin, kecepatan);
}

void hidupkanPompa1(int pinsatu, int pindua)
{
  digitalWrite(pinsatu, HIGH);
  digitalWrite(pindua, LOW);
}

void matikanPompa(int pin1, int pin2, int enPin)
{
  digitalWrite(pin1, LOW);
  digitalWrite(pin2, LOW);
  analogWrite(enPin, 0);
}

void matikanPompa1(int pin1, int pin2)
{
  digitalWrite(pin1, LOW);
  digitalWrite(pin2, LOW);
}

// Fungsi keanggotaan fuzzy
float fuzzyLow(float x, float a, float b)
{
  if (x <= a)
    return 1;
  else if (x >= b)
    return 0;
  else
    return (b - x) / (b - a);
}

float fuzzyMid(float x, float a, float b, float c)
{
  if (x <= a || x >= c)
    return 0;
  else if (x == b)
    return 1;
  else if (x > a && x < b)
    return (x - a) / (b - a);
  else
    return (c - x) / (c - b);
}

float fuzzyHigh(float x, float a, float b)
{
  if (x <= a)
    return 0;
  else if (x >= b)
    return 1;
  else
    return (x - a) / (b - a);
}

void setup()
{
  Serial.begin(115200);
  sensors.begin();

  pinMode(TURBIDITY_PIN, INPUT);
  pinMode(enA, OUTPUT);
  pinMode(in1, OUTPUT);
  pinMode(in2, OUTPUT);
  pinMode(enB, OUTPUT);
  pinMode(in3, OUTPUT);
  pinMode(in4, OUTPUT);
  pinMode(enC, OUTPUT);
  pinMode(in5, OUTPUT);
  pinMode(in6, OUTPUT);
  pinMode(enD, OUTPUT);
  pinMode(in7, OUTPUT);
  pinMode(in8, OUTPUT);
  pinMode(in9, OUTPUT);
  pinMode(in10, OUTPUT);
  pinMode(in11, OUTPUT);
  pinMode(in12, OUTPUT);
  Serial.println("Inisialisasi selesai. Pompa siap digunakan!");
}

void loop()
{
  sensors.requestTemperatures();
  float suhuC = sensors.getTempCByIndex(0);

  int nilaiADC = analogRead(PH_PIN);
  float tegangan = (float)nilaiADC * (3.3 / 4095.0);
  float pH = 7 + ((2.5 - tegangan) / 0.18);

  int statusAir = digitalRead(TURBIDITY_PIN);
  Serial.println(statusAir);
  String kondisiAir = (statusAir == HIGH) ? "Jernih" : "Keruh 🌫️";

  Serial.printf("Suhu: %.2f °C\tpH: %.2f\tAir: %s\n", suhuC, pH, kondisiAir.c_str());

  // FUZZY INPUT
  float suhu_dingin = fuzzyLow(suhuC, 16, 26);
  float suhu_normal = fuzzyMid(suhuC, 26, 28, 30);
  float suhu_panas = fuzzyHigh(suhuC, 30, 34);

  float ph_asam = fuzzyLow(pH, 4.5, 6.5);      // 4.5, 6.5
  float ph_netral = fuzzyMid(pH, 6.5, 7, 7.5); // 6, 7, 8
  float ph_basa = fuzzyHigh(pH, 7.5, 8.5);     // 7.5, 9.5

  // Tampilkan nilai keanggotaan fuzzy
  Serial.println("--- Keanggotaan Fuzzy ---");
  Serial.printf("Suhu Dingin: %.2f\tNormal: %.2f\tPanas: %.2f\n", suhu_dingin, suhu_normal, suhu_panas);
  Serial.printf("pH Asam: %.2f\tNetral: %.2f\tBasa: %.2f\n", ph_asam, ph_netral, ph_basa);

  // FUZZY OUTPUT (PWM)
  int M1 = suhu_panas * 255;  // Air dingin
  int M2 = suhu_dingin * 255; // Air hangat
  int M3 = ph_basa * 255;     // Asam (netralisasi basa)
  int M4 = ph_asam * 255;  

// === KONTROL MOTOR BERDASARKAN KEKERUHAN DAN millis ===
if (statusAir == HIGH) {
  // Air jernih
  kondisiAir = "Jernih";
  matikanPompa1(in9, in10); 

  if (airSebelumnyaKeruh && !sedangMengisiAir) {
    sedangMengisiAir = true;
    waktuMulaiIsiAir = millis();
    hidupkanPompa1(in11, in12);
    Serial.println("Air jernih terdeteksi, mulai isi air...");
  }

  // Jika sedang mengisi dan sudah lewat waktunya, matikan relay1
  if (sedangMengisiAir && (millis() - waktuMulaiIsiAir >= durasiIsiAir)) {
    matikanPompa1(in9, in10); 
    sedangMengisiAir = false;
    Serial.println("Pengisian air selesai.");
  }

  airSebelumnyaKeruh = false; // reset flag
} else {
  kondisiAir = "Keruh 🌫️";
  hidupkanPompa1(in11, in12);
  matikanPompa1(in9, in10); 
  sedangMengisiAir = false;    // Reset proses isi air
  airSebelumnyaKeruh = true;   // Set flag bahwa sebelumnya keruh
}

  Serial.printf("PWM M1 (Air Dingin): %d\tM2 (Air Hangat): %d\tM3 (Asam): %d\tM4 (Basa): %d\n", M1, M2, M3, M4);

  // EKSEKUSI POMPA
  if (M1 > 20)
    hidupkanPompa(in1, in2, enA, M1);
  else
    matikanPompa(in1, in2, enA);
  if (M2 > 20)
    hidupkanPompa(in3, in4, enB, M2);
  else
    matikanPompa(in3, in4, enB);
  if (M3 > 20)
    hidupkanPompa(in5, in6, enC, M3);
  else
    matikanPompa(in5, in6, enC);
  if (M4 > 20)
    hidupkanPompa(in7, in8, enD, M4);
  else
    matikanPompa(in7, in8, enD);

  Serial.println("--------------------------------------------------\n");
  delay(2000);
}