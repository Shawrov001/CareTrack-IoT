#include <Wire.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"
#include <Adafruit_MLX90614.h>
#include <WiFi.h>
#include <HTTPClient.h>


// Voici le récapitulatif propre de tes branchements finaux, prêt à être glissé direct dans ton Obsidian pour ne rien oublier au prochain montage :

// ### 🔌 Alimentation (Commune aux deux capteurs)

// * **VCC / VIN (MAX30105)** ➡️ **3.3V** de l'ESP32
// * **VIN (GY-906)** ➡️ **3.3V** de l'ESP32
// * *Astuce : Si ton ESP32 n'a qu'une seule broche 3.3V, torsade les deux fils ensemble pour les insérer dans le même trou.*


// * **GND (MAX30105)** ➡️ **GND** de l'ESP32
// * **GND (GY-906)** ➡️ **GND** de l'ESP32

// ### 🫀 Capteur 1 : MAX30105 (Pouls & SpO2)

// Il utilise le bus I2C principal de l'ESP32 (`Wire`).

// * **SDA** ➡️ **GPIO 21**
// * **SCL** ➡️ **GPIO 22**
// * **INT** ➡️ *Non connecté (inutile avec notre code)*

// ### 🌡️ Capteur 2 : GY-906 / MLX90614 (Température)

// Il utilise le deuxième bus I2C de l'ESP32 qu'on a configuré sur mesure (`Wire1`).

// * **SDA** ➡️ **GPIO 32**
// * **SCL** ➡️ **GPIO 33**


// partage de conexion en 2.4ghz
// désactiver tous les parefeu du pc sinon ça marche pas


// --- RÉGLAGES RÉSEAU ---
const char* ssid = "student-laptop";
const char* password = "MDPPPPPP";
const char* serverName = "http://192.168.137.1:5000/data"; 

// --- INITIALISATION DES CAPTEURS ---
MAX30105 particleSensor;
Adafruit_MLX90614 mlx = Adafruit_MLX90614();

#define I2C2_SDA 32
#define I2C2_SCL 33

#define BUFFER_SIZE 100
uint32_t irBuffer[BUFFER_SIZE];
uint32_t redBuffer[BUFFER_SIZE];
int32_t  spo2, heartRate;
int8_t   validSPO2, validHeartRate;

// --- VARIABLES POUR LE SCAN DE 5 SECONDES ---
bool enCoursDeScan = false;
unsigned long debutScan = 0;
const unsigned long DUREE_SCAN = 5000; // 5000 millisecondes = 5 secondes

long sommeBpm = 0;
long sommeSpo2 = 0;
double sommeTemp = 0.0;
int compteBpm = 0;
int compteSpo2 = 0;
int compteTemp = 0;

// Filtre BPM
bool bpmValide(int bpm) {
    return bpm >= 40 && bpm <= 120;
}

void setup() {
    Serial.begin(115200);
    
    // --- CONNEXION WI-FI ---
    Serial.print("\nConnexion au Wi-Fi ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWi-Fi connecté !");
    
    // --- DÉMARRAGE I2C ---
    Wire.begin(21, 22);                  
    Wire1.begin(I2C2_SDA, I2C2_SCL);     

    if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
        Serial.println("Capteur MAX30105 non trouvé !");
        while (true);
    }
    if (!mlx.begin(0x5A, &Wire1)) {
        Serial.println("Capteur GY-906 non trouvé !");
        while (true);
    }

    // Paramètres MAX30105
    particleSensor.setup(0x7F, 4, 2, 400, 411, 16384);

    // Remplissage du buffer initial
    for (byte i = 0; i < BUFFER_SIZE; i++) {
        while (!particleSensor.available()) particleSensor.check();
        redBuffer[i] = particleSensor.getRed();
        irBuffer[i]  = particleSensor.getIR();
        particleSensor.nextSample();
    }
    maxim_heart_rate_and_oxygen_saturation(irBuffer, BUFFER_SIZE, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);

    Serial.println("\n=========================================");
    Serial.println("🤖 SYSTÈME PRÊT !");
    Serial.println("👉 Tapez 's' dans le moniteur série pour lancer un scan de 5 secondes.");
    Serial.println("=========================================\n");
}

void loop() {
    // 1. --- GESTION DU DÉCLENCHEUR ---
    // On lit le moniteur série. Si on reçoit 's', on démarre le scan.
    if (Serial.available() > 0) {
        char c = Serial.read();
        if ((c == 's' || c == 'S') && !enCoursDeScan) {
            Serial.println("\n🚀 DÉMARRAGE DU SCAN (5 secondes)... Posez le doigt !");
            enCoursDeScan = true;
            debutScan = millis();
            
            // Remise à zéro des compteurs pour faire une moyenne propre
            sommeBpm = 0;
            sommeSpo2 = 0;
            sommeTemp = 0.0;
            compteBpm = 0;
            compteSpo2 = 0;
            compteTemp = 0;
        }
    }

    // 2. --- LECTURE CONTINUE DES CAPTEURS ---
    // (Même si on ne scanne pas, l'algorithme a besoin de tourner pour rester précis)
    for (byte i = 25; i < BUFFER_SIZE; i++) {
        redBuffer[i - 25] = redBuffer[i];
        irBuffer[i - 25]  = irBuffer[i];
    }
    for (byte i = 75; i < BUFFER_SIZE; i++) {
        while (!particleSensor.available()) particleSensor.check();
        redBuffer[i] = particleSensor.getRed();
        irBuffer[i]  = particleSensor.getIR();
        particleSensor.nextSample();
    }

    maxim_heart_rate_and_oxygen_saturation(irBuffer, BUFFER_SIZE, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);

    long irMoy = 0;
    for (int i = 0; i < BUFFER_SIZE; i++) irMoy += irBuffer[i];
    irMoy /= BUFFER_SIZE;

    double tempSurface = mlx.readObjectTempC();

    // 3. --- ACCUMULATION DES DONNÉES (Uniquement pendant les 5s) ---
    if (enCoursDeScan) {
        // On accumule la température en continu
        sommeTemp += tempSurface;
        compteTemp++;

        // On accumule le pouls et SpO2 que si un doigt est bien détecté
        if (irMoy >= 50000) {
            if (validHeartRate && bpmValide(heartRate)) {
                sommeBpm += heartRate;
                compteBpm++;
            }
            if (spo2 > 0) {
                if (spo2 > 100) spo2 = 100;
                sommeSpo2 += spo2;
                compteSpo2++;
            }
        }

        // 4. --- FIN DU CHRONO (5 secondes écoulées) ---
        if (millis() - debutScan >= DUREE_SCAN) {
            enCoursDeScan = false; // On arrête le scan
            Serial.println("\n✅ SCAN TERMINÉ ! Envoi au serveur...");

            // Calcul des moyennes finales
            int bpmFinal = (compteBpm > 0) ? (sommeBpm / compteBpm) : 0;
            int spo2Final = (compteSpo2 > 0) ? (sommeSpo2 / compteSpo2) : 0;
            double tempFinal = (compteTemp > 0) ? (sommeTemp / compteTemp) : 0.0;

            // Affichage dans le moniteur pour vérification
            Serial.print("Résultats -> BPM: "); Serial.print(bpmFinal);
            Serial.print(" | SpO2: "); Serial.print(spo2Final); Serial.print("%");
            Serial.print(" | Temp: "); Serial.print(tempFinal); Serial.println("°C");

            // 5. --- ENVOI HTTP AU SERVEUR PYTHON ---
            if(WiFi.status() == WL_CONNECTED) { 
                WiFiClient client; // IMPORTANT : L'objet client pour la stabilité Wi-Fi
                HTTPClient http;
                
                http.begin(client, serverName); 
                http.addHeader("Content-Type", "application/json");

                String httpRequestData = "{";
                httpRequestData += "\"bpm\":" + String(bpmFinal) + ",";
                httpRequestData += "\"spo2\":" + String(spo2Final) + ",";
                httpRequestData += "\"temperature\":" + String(tempFinal);
                httpRequestData += "}";

                int httpResponseCode = http.POST(httpRequestData);

                if (httpResponseCode > 0) {
                    Serial.println("🌐 Données reçues par le PC ! (Code 200)");
                } else {
                    Serial.print("❌ Erreur de réseau. Code : ");
                    Serial.println(httpResponseCode);
                }
                http.end(); 
            } else {
                Serial.println("❌ Wi-Fi déconnecté !");
            }
            
            Serial.println("\n👉 Tapez 's' pour lancer un nouveau scan.");
        }
    }
}