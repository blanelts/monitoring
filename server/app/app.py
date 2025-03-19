from flask import Flask, request, jsonify, render_template
import psycopg2
from psycopg2.extras import RealDictCursor
import os
import json

app = Flask(__name__)

DB_HOST = os.getenv("DB_HOST", "db")
DB_NAME = os.getenv("DB_NAME", "monitoring")
DB_USER = os.getenv("DB_USER", "user")
DB_PASSWORD = os.getenv("DB_PASSWORD", "password")

def get_db_connection():
    return psycopg2.connect(
        host=DB_HOST,
        dbname=DB_NAME,
        user=DB_USER,
        password=DB_PASSWORD,
        cursor_factory=RealDictCursor
    )

@app.route("/")
def index():
    return render_template("index.html")

@app.route("/report", methods=["POST"])
def receive_report():
    data = request.get_json()
    if not data:
        return jsonify({"error": "Invalid JSON"}), 400

    containers = data.get("containers", [])

    try:
        conn = get_db_connection()
        cur = conn.cursor()
        cur.execute("""
        INSERT INTO server_stats 
        (hostname, ip_address, cpu_usage, memory_used, memory_total, disk_free, disk_total, os_info, containers, last_update)
        VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s, CURRENT_TIMESTAMP)
        """,
        (data["hostname"], data["ip_address"], data["cpu_usage"],
         data["memory_used"], data["memory_total"],
         data["disk_free"], data["disk_total"], data["os_info"],
         json.dumps(containers)))  # Не забудьте импортировать модуль json
        conn.commit()
        cur.close()
        conn.close()
        return jsonify({"message": "Data saved"}), 201
    except Exception as e:
        return jsonify({"error": str(e)}), 500

@app.route("/stats", methods=["GET"])
def get_stats():
    try:
        conn = get_db_connection()
        cur = conn.cursor()
        cur.execute("""
            WITH ranked_stats AS (
                SELECT *, ROW_NUMBER() OVER (PARTITION BY ip_address ORDER BY id DESC) AS row_num
                FROM server_stats
            )
            SELECT hostname, ip_address, cpu_usage, memory_used, memory_total, 
                   disk_free, disk_total, os_info,
                   last_update, EXTRACT(EPOCH FROM (NOW() - last_update)) AS last_seen
            FROM ranked_stats
            WHERE row_num = 1;
        """)
        rows = cur.fetchall()
        cur.close()
        conn.close()

        for row in rows:
            row["status"] = "online" if row["last_seen"] < 60 else "offline"

        return jsonify(rows)
    except Exception as e:
        return jsonify({"error": str(e)}), 500


@app.route("/stats/<ip_address>", methods=["GET"])
def get_agent_stats(ip_address):
    try:
        conn = get_db_connection()
        cur = conn.cursor()
        cur.execute("""
            SELECT cpu_usage, memory_used, 
                   ROUND(EXTRACT(EPOCH FROM last_update) * 1000) AS last_update,
                   containers
            FROM server_stats
            WHERE ip_address = %s
            ORDER BY id DESC
            LIMIT 100;
        """, (ip_address,))
        rows = cur.fetchall()
        cur.close()
        conn.close()
        return jsonify(rows)
    except Exception as e:
        return jsonify({"error": str(e)}), 500


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=True)
