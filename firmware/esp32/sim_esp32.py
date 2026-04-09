import random
import time
from datetime import datetime
import requests

API_URL = 'http://127.0.0.1:5000/leituras'
intervalo_tempo_segundos = 5

def gerar_leitura() -> dict:
    return{
        "ph": round(random.uniform(6.5, 8.5), 2),
        "turbidez": round(random.uniform(1.0, 50.0), 2),
        "temperatura": round(random.uniform(20.00, 30.00), 2),
        "orp": round(random.uniform(200.00, 300.00), 2),
        "data_hora": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
    }

def enviar_leitura(dados: dict) -> None:
    try:
        resposta = requests.post(API_URL, json=dados, timeout = 5)
        print(f"[{resposta.status_code}] Enviado: {dados}")
        print(f"Resposta: ", resposta.json())
    except requests.RequestException as exc:
        print("Erro ao enviar dados para API: ", exc)

def main() -> None:
    print("Indicando simulador de leituras...")
    print(f"Enviando dados para {API_URL}")
    print(f"Intervalo: {intervalo_tempo_segundos} segundos\n")

    while True:
        leitura = gerar_leitura()
        enviar_leitura(leitura)
        time.sleep(intervalo_tempo_segundos)

if __name__ == "__main__":
    main()