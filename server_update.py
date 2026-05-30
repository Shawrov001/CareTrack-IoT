import psycopg2
from flask import Flask, request, jsonify
import threading

app = Flask(__name__)

current_patient_id = None

DB_HOST = "172.17.129.172"
DB_PORT = "5432"
DB_NAME = "hospital_db"
DB_USER = "hospital_owner"
DB_PASS = "namosash"


def ecouter_clavier():
    global current_patient_id
    while True:
        if current_patient_id is None:
            id_saisi = input("[MEDECIN] Entrez l'ID du patient : ")
            if id_saisi.strip():
                current_patient_id = id_saisi.strip()
                print(f"👤 Patient ID = {current_patient_id} activé.\n")


@app.route('/set-patient', methods=['POST'])
def set_patient():
    global current_patient_id
    data = request.get_json()
    patient_id = data.get('patient_id')
    
    if not patient_id:
        return jsonify({"status": "error", "message": "ID patient manquant"}), 400
        
    current_patient_id = patient_id
    print(f"👤 Patient actif configuré sur le PC : ID = {current_patient_id}")
    return jsonify({"status": "success", "message": f"Patient {patient_id} actif"}), 200


@app.route('/data', methods=['POST'])
def receive_data():
    global current_patient_id
    data = request.get_json()
    
    bpm = data.get('bpm', 0)
    spo2 = data.get('spo2', 0)
    temperature = data.get('temperature', 0.0)
    
    print(f"🔥 Données reçues de l'ESP32 : BPM={bpm}, SpO2={spo2}%, Temp={temperature}°C")

    if current_patient_id is None:
        print("⚠️ Données rejetées : Aucun patient sélectionné.")
        return jsonify({"status": "error", "message": "Aucun patient actif sur le PC"}), 400

    try:
        conn = psycopg2.connect(
            host=DB_HOST, port=DB_PORT, database=DB_NAME, user=DB_USER, password=DB_PASS
        )
        cursor = conn.cursor()

        insert_query = """
            INSERT INTO constantes_vitales 
            (id_patient, temperature, frequence_cardiaque, spo2, source)
            VALUES (%s, %s, %s, %s, %s)
        """
        source = "ESP32_IoT"

        cursor.execute(insert_query, (current_patient_id, temperature, bpm, spo2, source))
        conn.commit() 
        
        cursor.close()
        conn.close()
        
        print(f"✅ Données sauvegardées pour le Patient ID = {current_patient_id}")
        current_patient_id = None 

    except Exception as e:
        print(repr(e)) 
        return jsonify({"status": "error", "message": "Erreur BDD"}), 500

    return jsonify({"status": "success"}), 200


if __name__ == '__main__':
    thread_clavier = threading.Thread(target=ecouter_clavier, daemon=True)
    thread_clavier.start()
    app.run(host='192.168.137.1', port=5000)