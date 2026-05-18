import os
from dotenv import load_dotenv
from contextlib import closing
from datetime import datetime
from typing import Any

from flask import Flask, jsonify, request
import mysql.connector

app = Flask(__name__)
load_dotenv()


def get_connection():
    return mysql.connector.connect(
        host = os.getenv("DB_HOST"),
        user = os.getenv("DB_USER"),
        password = os.getenv("DB_PASSWORD"),
        database = os.getenv("DB_NAME"),
        port = int(os.getenv("DB_PORT", 3306))
    )


def init_db() -> None:
    with closing(get_connection()) as conn:
        cursor = conn.cursor()

        cursor.execute(
            """
            CREATE TABLE IF NOT EXISTS leituras (
                id INT AUTO_INCREMENT PRIMARY KEY,
                ph FLOAT NOT NULL,
                turbidez FLOAT NOT NULL,
                temperatura FLOAT NOT NULL,
                orp FLOAT,
                data_hora DATETIME NOT NULL
            )
            """
        )

        conn.commit()
        cursor.close()


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

@app.route("/status", methods=["GET"])
def status():
    try:
        with closing(get_connection()) as conn:
            cursor = conn.cursor()
            cursor.execute("SELECT 1")
            cursor.fetchone()
            cursor.close()

        return jsonify(
            {
                "status": "ok",
                "api": "online",
                "database": "conectado"
            }
        ), 200

    except Exception as erro:
        return jsonify(
            {
                "status": "erro",
                "api": "online",
                "database": "desconectado",
                "detalhe": str(erro)
            }
        ), 500

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
                "message": mensagem
            }
        ), 400

    ph = float(dados["ph"])
    turbidez = float(dados["turbidez"])
    temperatura = float(dados["temperatura"])

    orp = (
        float(dados["orp"])
        if "orp" in dados and dados["orp"] is not None
        else None
    )

    data_hora = dados.get(
        "data_hora",
        datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    )

    with closing(get_connection()) as conn:
        cursor = conn.cursor()

        cursor.execute(
            """
            INSERT INTO leituras
            (ph, turbidez, temperatura, orp, data_hora)
            VALUES (%s, %s, %s, %s, %s)
            """,
            (ph, turbidez, temperatura, orp, data_hora)
        )

        conn.commit()

        leitura_id = cursor.lastrowid

        cursor.close()

    return jsonify(
        {
            "status": "ok",
            "mensagem": "Leitura salva com sucesso",
            "id": leitura_id
        }
    ), 201


@app.route("/leituras", methods=["GET"])
def listar_leituras():
    limite = request.args.get("limite", default=10, type=int)

    with closing(get_connection()) as conn:
        cursor = conn.cursor(dictionary=True)

        cursor.execute(
            """
            SELECT id, ph, turbidez, temperatura, orp, data_hora
            FROM leituras
            ORDER BY id DESC
            LIMIT %s
            """,
            (limite,)
        )

        resultados = cursor.fetchall()

        cursor.close()

    return jsonify(
        {
            "status": "ok",
            "leituras": resultados
        }
    ), 200


if __name__ == "__main__":
    #init_db()
    app.run(debug=True)