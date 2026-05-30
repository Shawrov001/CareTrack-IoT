
# CareTrack - Module Embarqué IoT d'Acquisition des Constantes

Ce sous-système IoT fait partie du projet global **CareTrack**. Il automatise la prise et la centralisation des constantes vitales d'un patient (pouls, saturation en oxygène $\text{SpO}_2$ et température corporelle), éliminant ainsi les risques d'erreurs humaines liés à la saisie manuelle.

Le système repose sur un microcontrôleur **ESP32** qui communique via des requêtes HTTP POST (JSON) avec une passerelle **Flask (Python)** chargée d'alimenter la base de données PostgreSQL du projet.

---

## 📁 Structure du Dépôt

* `max30102_esp32.ino` : Code source embarqué (C++) pour l'ESP32 gérant la capture des signaux, le filtrage et l'envoi HTTP.


* `server_update.py` : Serveur Flask intermédiaire gérant l'authentification de session interactive et la persistance en base de données.

---

## 🔌 Schéma de Câblage Matériel

Les composants partagent une alimentation commune de 3.3V fournie par l'ESP32. Deux bus $\text{I}^2\text{C}$ distincts sont configurés pour éviter tout conflit d'adressage.

### 1. Alimentation commune

* **VCC / VIN (MAX30105 & GY-906)** ➡️ **3.3V** de l'ESP32 


* **GND (MAX30105 & GY-906)** ➡️ **GND** de l'ESP32 



### 2. Bus $\text{I}^2\text{C}$ principal : Capteur Pouls/SpO2 (MAX30105)

* **SDA** ➡️ **GPIO 21** 


* **SCL** ➡️ **GPIO 22** 



### 3. Bus $\text{I}^2\text{C}$ secondaire : Capteur Température (GY-906 / MLX90614)

* **SDA** ➡️ **GPIO 32** 


* **SCL** ➡️ **GPIO 33** 



---

## 🛠️ Configuration et Prérequis

### Côté ESP32 (Arduino IDE)

1. Installez les bibliothèques suivantes dans votre IDE :
* `MAX30105 Tyco` (ou équivalent gérant le traitement d'onde rouge/IR) 


* `Adafruit MLX90614` 




2. Configurez vos identifiants réseau locaux (Partage de connexion en 2.4 GHz obligatoire) et l'adresse IP de votre serveur Flask dans les variables `ssid`, `password` et `serverName` du fichier `.ino`.



### Côté Serveur (Python)

1. Installez les dépendances nécessaires via votre terminal :
```bash
pip install flask psycopg2

```


2. Ajustez vos identifiants de base de données PostgreSQL dans le bloc de configuration de `server_update.py` (`DB_HOST`, `DB_USER`, `DB_PASS`, etc.).
3. **Important :** Pensez à désactiver les pare-feux de la machine hôte ou à autoriser le port 5000 pour que l'ESP32 puisse joindre le serveur.

---

## 🚀 Utilisation du Système

### 1. Lancement du Serveur

Exécutez le script Python sur votre poste de développement :

```bash
python server_update.py

```

Le serveur démarre et lance simultanément un fil d'exécution en arrière-plan dédié à l'écoute de votre console.

### 2. Déroulement d'une Mesure Clinique

1. **Sélection du patient :** Dans le terminal du serveur Python, le système affiche une invite : `[MEDECIN] Entrez l'ID du patient : `. Tapez l'identifiant du patient concerné.
2. **Lancement du scan :** Ouvrez le moniteur série de l'ESP32 (vitesse : `115200 baud`) et appuyez sur la touche **'s'** pour déclencher le cycle d'acquisition.


3. **Prise de mesures :** Le patient pose son doigt sur les capteurs pendant 5 secondes.


* *Sécurité active :* L'ESP32 applique un filtre de présence ($irMoy \ge 50000$) et un filtre physiologique éliminant les valeurs aberrantes ($40 \le BPM \le 120$).




4. **Envoi et réinitialisation :** Les valeurs moyennes sont calculées , envoyées au serveur Flask via un payload JSON, puis enregistrées dans la table `constantes_vitales`. La session se réinitialise ensuite automatiquement, prête pour le patient suivant.
