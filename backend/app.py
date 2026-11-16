
from flask import Flask, jsonify, request, render_template, Response, stream_with_context
import paho.mqtt.client as mqtt
import sqlite3
import os
import json
import time


app = Flask(__name__)


# ==========================================
# DATABASE CONFIGURATION
# ==========================================

DATABASE = os.path.join(
    os.path.dirname(os.path.abspath(__file__)),
    "gas_monitor.db"
)


def get_db_connection():

    conn = sqlite3.connect(DATABASE)
    conn.row_factory = sqlite3.Row

    return conn
# ==========================================
# MQTT COMMAND CONFIGURATION
# ==========================================

MQTT_BROKER = "localhost"
MQTT_PORT = 1883

MQTT_COMMAND_TOPIC = "gasmonitor/esp32/command"

# ==========================================
# FORMAT MEASUREMENTS
# ==========================================

def format_measurement(row):

    data = dict(row)

    data["gas_detected"] = bool(data["gas_detected"])
    data["alarm_latched"] = bool(data["alarm_latched"])
    data["buzzer_muted"] = bool(data["buzzer_muted"])

    return data


# ==========================================
# HOME ROUTE
# ==========================================

@app.route("/")
def home():

    return jsonify({
        "message": "Gas Monitoring System API",
        "status": "Running"
    })


# ==========================================
# DASHBOARD ROUTE
# ==========================================

@app.route("/dashboard")
def dashboard():

    return render_template("index.html")


# ==========================================
# LATEST MEASUREMENT API
# ==========================================

@app.route("/api/latest")
def latest_measurement():

    conn = get_db_connection()

    try:

        row = conn.execute("""
            SELECT *
            FROM measurements
            ORDER BY id DESC
            LIMIT 1
        """).fetchone()

    finally:
        conn.close()

    if row is None:

        return jsonify({
            "message": "No measurements available"
        }), 404

    return jsonify(format_measurement(row))


# ==========================================
# MEASUREMENT HISTORY API
# ==========================================

@app.route("/api/history")
def measurement_history():

    limit = request.args.get("limit", default=50, type=int)

    if limit is None or limit < 1:

        return jsonify({
            "error": "Invalid limit"
        }), 400

    limit = min(limit, 500)

    conn = get_db_connection()

    try:

        rows = conn.execute("""
            SELECT *
            FROM measurements
            ORDER BY id DESC
            LIMIT ?
        """, (limit,)).fetchall()

    finally:
        conn.close()

    measurements = [
        format_measurement(row)
        for row in reversed(rows)
    ]

    return jsonify({
        "count": len(measurements),
        "data": measurements
    })


# ==========================================
# SERVER-SENT EVENTS (SSE)
# ==========================================

@app.route("/api/stream")
def stream_measurements():

    @stream_with_context
    def event_stream():

        # Check browser reconnection ID
        last_event_id = request.headers.get("Last-Event-ID")

        if last_event_id:

            try:
                last_id = int(last_event_id)

            except ValueError:
                last_id = 0

        else:

            # Send the latest measurement on initial connection
            conn = get_db_connection()

            try:

                row = conn.execute("""
                    SELECT MAX(id) AS max_id
                    FROM measurements
                """).fetchone()

            finally:
                conn.close()

            max_id = row["max_id"] or 0

            last_id = max(0, max_id - 1)

        heartbeat = 0

        while True:

            conn = get_db_connection()

            try:

                rows = conn.execute("""
                    SELECT *
                    FROM measurements
                    WHERE id > ?
                    ORDER BY id ASC
                    LIMIT 100
                """, (last_id,)).fetchall()

            finally:
                conn.close()

            if rows:

                for row in rows:

                    data = format_measurement(row)

                    yield (
                        f"id: {data['id']}\n"
                        f"data: {json.dumps(data)}\n\n"
                    )

                    last_id = data["id"]

                heartbeat = 0

            else:

                heartbeat += 1

                if heartbeat >= 60:

                    yield ": keep-alive\n\n"

                    heartbeat = 0

            # Check database every 250 milliseconds
            time.sleep(0.25)

    return Response(
        event_stream(),
        mimetype="text/event-stream",
        headers={
            "Cache-Control": "no-cache",
            "X-Accel-Buffering": "no"
        }
    )
# ==========================================
# BUZZER REMOTE CONTROL API
# ==========================================

@app.route("/api/buzzer/mute", methods=["POST"])
def mute_buzzer():

    mqtt_publisher = mqtt.Client(
        callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
        client_id="gas_monitor_web_control"
    )

    try:

        print("Connecting to MQTT broker for buzzer control...")

        mqtt_publisher.connect(
            MQTT_BROKER,
            MQTT_PORT,
            60
        )

        mqtt_publisher.loop_start()

        result = mqtt_publisher.publish(
            MQTT_COMMAND_TOPIC,
            payload="MUTE",
            qos=1,
            retain=False
        )

        result.wait_for_publish(timeout=3)

        if not result.is_published():

            raise RuntimeError("MQTT command was not published")

        print("Remote MUTE command published successfully!")

        return jsonify({
            "success": True,
            "message": "Mute command sent to ESP32"
        }), 200

    except Exception as error:

        print(f"Buzzer control error: {error}")

        return jsonify({
            "success": False,
            "error": "Unable to send buzzer command"
        }), 500

    finally:

        if mqtt_publisher.is_connected():
            mqtt_publisher.disconnect()

        mqtt_publisher.loop_stop()

# ==========================================
# RUN FLASK APPLICATION
# ==========================================

if __name__ == "__main__":

    app.run(
        host="0.0.0.0",
        port=5000,
        debug=True,
        threaded=True
    )
