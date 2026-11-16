
from database import create_table, save_measurement
import json
import paho.mqtt.client as mqtt

# MQTT configuration
BROKER = "localhost"
PORT = 1883
TOPIC = "gasmonitor/esp32/telemetry"


def on_connect(client, userdata, flags, reason_code, properties):
    if reason_code == 0:
        print("Connected to MQTT Broker successfully!")
        client.subscribe(TOPIC)
        print(f"Subscribed to topic: {TOPIC}")
    else:
        print(f"MQTT connection failed: {reason_code}")


def on_message(client, userdata, msg):
    try:
        payload = msg.payload.decode("utf-8")
        data = json.loads(payload)
        save_measurement(data)

        print("\n========== NEW SENSOR DATA ==========")
        print(f"Gas RAW       : {data['gas_raw']}")
        print(f"Temperature   : {data['temperature']} °C")
        print(f"Humidity      : {data['humidity']} %")
        print(f"Gas Status    : {data['gas_status']}")
        print(f"Gas Detected  : {data['gas_detected']}")
        print(f"Alarm Latched : {data['alarm_latched']}")
        print(f"Buzzer        : {data['buzzer']}")
        print(f"Buzzer Muted  : {data['buzzer_muted']}")
        print("=====================================")

    except (json.JSONDecodeError, KeyError, UnicodeDecodeError) as error:
        print(f"Error processing MQTT message: {error}")


# Create MQTT client (Paho MQTT 2.x)
client = mqtt.Client(
    callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
    client_id="gas_monitor_backend"
)

client.on_connect = on_connect
client.on_message = on_message

create_table()

print("Connecting to MQTT broker...")

client.connect(BROKER, PORT, 60)

client.loop_forever()
