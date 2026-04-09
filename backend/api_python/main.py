import sqlite3
from contextlib import closing
from datetime import datetime
from pathlib import Path
from typing import Any
from flask import Flask, jsonify, request

app = Flask(__name__)
BASE_DIR = Path(__file__).resolve().parent
DB_PATH = BASE_DIR / "monitoramento_agua.db"

def get_connection() -> sqlite3.Connection:
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    return conn

def init_db() -> None:
    with closing(get_connection()) as conn:
        with conn:
            conn.execute(
                """
                CREATE TABLE IF NOT EXISTS leituras (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    ph REAL NOT NULL,
                    turbidez REAL NOT NULL,
                    temperatura REAL NOT NULL,
                    orp REAL,
                    data_hora TEXT NOT NULL
                )
                """
            )  

def validar_dados(dados: dict[str, Any]) -> tuple[bool, str]:
    campos_obrigatorios = ["ph", "turbidez", "temperatura"]

    for campo in campos_obrigatorios:
        if campo not in dados:
            return False, f"Campo obrigatório ausente: {campo}"
        
    try:
        float(dados["ph"])
        float(dados["turbidez"])
        float(dados["temperatura"])

        if "orp" in dados and dados["orp"] is not None:
            float(dados["orp"])
    except (TypeError, ValueError):
        return False, "Os campos numéricos devem conter valores válidos"
    
    return True, ""

@app.route("/", methods=["GET"])
def home():
    return jsonify(
        {
        "status": "ok",
        "message": "API de monitoramento da água em execução"
        }
    ), 200

@app.route("/leituras", methods=["POST"])
def receber_leitura():
    dados = request.get_json(silent=True)

    if not dados:
        return jsonify(
            {
                "status": "erro",
                "message": "JSON inválido ou ausente"
            }
        ), 400
    
    valido, mensagem = validar_dados(dados)
    if not valido:
        return jsonify(
            {
                "status": "erro",
                "messagem": mensagem
            }
        ), 400
    
    ph = float(dados["ph"])
    turbidez = float(dados["turbidez"])
    temperatura = float(dados["temperatura"])
    orp = float(dados["orp"])
    data_hora = dados.get("data_hora", datetime.now().strftime("%Y-%m-%d %H:%M:%S"))

    with closing(get_connection()) as conn:
        with conn:
            cursor = conn.execute(
                 """
                INSERT INTO leituras (ph, turbidez, temperatura, orp, data_hora)
                VALUES (?, ?, ?, ?, ?)
                """,
                (ph, turbidez, temperatura, orp, data_hora)
            )
            leitura_id = cursor.lastrowid

    return jsonify(
        {
        "status": "ok",
        "mensagem": "Leitura salva com sucesso",
        "id": leitura_id
        }
    ), 201

@app.route("/leituras", methods = ["GET"])
def listar_leituras():
    limite = request.args.get("limite", default=10, type=int)

    with closing(get_connection()) as conn:
        cursor = conn.execute(
            """
            SELECT id, ph, turbidez, temperatura, orp, data_hora
            FROM leituras
            ORDER BY id DESC
            LIMIT ?
            """,
            (limite),
        )
        retultados = [dict(linha) for linha in cursor.fetchall()]
    return jsonify(
        {
            "status": "ok",
            "leituras": resultados
        }
    ), 200

if __name__ == "__main__":
    init_db()
    app.run(host="0.0.0.0", port=5000, debug=True)