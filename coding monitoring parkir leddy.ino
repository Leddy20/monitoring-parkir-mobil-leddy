#include <Servo.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

// Konfigurasi Pin
const int infraredMasukPin = 2;
const int infraredKeluarPin = 4;
const int slotSensorPins[] = {6, 7, 8, 9, 10, 11};
const int jumlahSlot = 6;

// Objek kontrol
Servo servoMasuk, servoKeluar;
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Variabel state
struct PintuState {
  bool sedangTerbuka = false;
  bool kendaraanTerdeteksi = false;
  unsigned long waktuBuka = 0;
  unsigned long waktuTerakhirDeteksi = 0;
};
PintuState pintuMasuk, pintuKeluar;

bool slotTerisi[jumlahSlot] = {false};
int slotTerisiCount = 0;
const int servoTutup = 0, servoBuka = 90;
const unsigned long durasiBuka = 8000; // 8 detik
const unsigned long delayProteksi = 3000; // 3 detik proteksi setelah tidak terdeteksi

void setup() {
  // Inisialisasi hardware
  servoMasuk.attach(3);
  servoKeluar.attach(5);
  servoMasuk.write(servoTutup);
  servoKeluar.write(servoTutup);

  // Inisialisasi sensor
  pinMode(infraredMasukPin, INPUT_PULLUP);
  pinMode(infraredKeluarPin, INPUT_PULLUP);
  for (int i = 0; i < jumlahSlot; i++) {
    pinMode(slotSensorPins[i], INPUT_PULLUP);
  }

  // Inisialisasi LCD
  Wire.begin();
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.print("Sistem Parkir");
  updateDisplay();

  Serial.begin(9600);
  Serial.println("Sistem Parkir Dimulai");
}

void loop() {
  unsigned long currentMillis = millis();
  
  // Update status deteksi kendaraan
  updateSensorStatus();
  
  // Proses pintu masuk dan keluar
  cekPintuMasuk();
  cekPintuKeluar();
  
  // Auto-close pintu dengan proteksi
  tutupPintuOtomatis(&pintuMasuk, &servoMasuk, currentMillis);
  tutupPintuOtomatis(&pintuKeluar, &servoKeluar, currentMillis);
  
  // Update status slot
  cekStatusSlotParkir();
  
  // Update display setiap 500ms
  static unsigned long lastUpdate = 0;
  if (currentMillis - lastUpdate >= 500) {
    lastUpdate = currentMillis;
    updateDisplay();
  }
}

void updateSensorStatus() {
  // Update status sensor masuk
  if (digitalRead(infraredMasukPin) == LOW) {
    pintuMasuk.kendaraanTerdeteksi = true;
    pintuMasuk.waktuTerakhirDeteksi = millis();
  } else {
    // Hanya anggap tidak terdeteksi setelah melewati delay proteksi
    if (millis() - pintuMasuk.waktuTerakhirDeteksi > delayProteksi) {
      pintuMasuk.kendaraanTerdeteksi = false;
    }
  }

  // Update status sensor keluar
  if (digitalRead(infraredKeluarPin) == LOW) {
    pintuKeluar.kendaraanTerdeteksi = true;
    pintuKeluar.waktuTerakhirDeteksi = millis();
  } else {
    if (millis() - pintuKeluar.waktuTerakhirDeteksi > delayProteksi) {
      pintuKeluar.kendaraanTerdeteksi = false;
    }
  }
}

void cekPintuMasuk() {
  if (pintuMasuk.kendaraanTerdeteksi && !pintuMasuk.sedangTerbuka) {
    if (slotTerisiCount < jumlahSlot) {
      bukaPintu(&pintuMasuk, &servoMasuk);
      Serial.println("Pintu Masuk Dibuka - Kendaraan terdeteksi");
    } else {
      tampilkanPesan("PARKIR PENUH!", 2000);
      Serial.println("Parkir Penuh!");
    }
  }
}

void cekPintuKeluar() {
  if (pintuKeluar.kendaraanTerdeteksi && !pintuKeluar.sedangTerbuka) {
    bukaPintu(&pintuKeluar, &servoKeluar);
    Serial.println("Pintu Keluar Dibuka - Kendaraan terdeteksi");
  }
}

void bukaPintu(PintuState* pintu, Servo* servo) {
  servo->write(servoBuka);
  pintu->sedangTerbuka = true;
  pintu->waktuBuka = millis();
  pintu->waktuTerakhirDeteksi = millis(); // Reset waktu deteksi
}

void tutupPintuOtomatis(PintuState* pintu, Servo* servo, unsigned long currentTime) {
  // Hanya tutup jika:
  // 1. Pintu sedang terbuka
  // 2. Sudah melebihi durasi buka
  // 3. Tidak ada kendaraan yang terdeteksi
  // 4. Sudah melewati waktu proteksi setelah kendaraan tidak terdeteksi
  if (pintu->sedangTerbuka && 
      (currentTime - pintu->waktuBuka >= durasiBuka) &&
      !pintu->kendaraanTerdeteksi &&
      (currentTime - pintu->waktuTerakhirDeteksi >= delayProteksi)) {
    servo->write(servoTutup);
    pintu->sedangTerbuka = false;
    Serial.println("Pintu Ditutup - Aman");
  }
}

void cekStatusSlotParkir() {
  int count = 0;
  for (int i = 0; i < jumlahSlot; i++) {
    bool statusTerisi = (digitalRead(slotSensorPins[i]) == LOW);
    if (statusTerisi != slotTerisi[i]) {
      slotTerisi[i] = statusTerisi;
      if (statusTerisi) {
        slotTerisiCount++;
        Serial.print("Slot ");
        Serial.print(i+1);
        Serial.println(" Terisi");
      } else {
        slotTerisiCount--;
        Serial.print("Slot ");
        Serial.print(i+1);
        Serial.println(" Kosong");
      }
    }
  }
}

void updateDisplay() {
  lcd.setCursor(0, 1);
  lcd.print("Tersedia: ");
  lcd.print(jumlahSlot - slotTerisiCount);
  lcd.print("/");
  lcd.print(jumlahSlot);
  lcd.print("   ");
}

void tampilkanPesan(const char* pesan, unsigned long durasi) {
  lcd.setCursor(0, 1);
  lcd.print(pesan);
  delay(durasi);
  updateDisplay();
}
