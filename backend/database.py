
import sqlite3
from datetime import datetime

# Database file
DATABASE_NAME = "gas_monitor.db"


def connect_db():
    """Create a connection to the SQLite database."""
    connection = sqlite3.connect(DATABASE_NAME)
    return connection


def create_table():
    """Create the measurements table if it doesn't exist."""

    connection = connect_db()
    cursor = connection.cursor()

    cursor.execute("""
        CREATE TABLE IF NOT EXISTS measurements (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp TEXT NOT NULL,
            gas_raw INTEGER,
            temperature REAL,
            humidity REAL,
            gas_status TEXT,
            gas_detected INTEGER,
            alarm_latched INTEGER,
            buzzer TEXT,
            buzzer_muted INTEGER
        )
    """)

    connection.commit()
    connection.close()

    print("Database table is ready!")


def save_measurement(data):
    """Save one sensor measurement into the database."""

    connection = connect_db()
    cursor = connection.cursor()

    timestamp = datetime.now().isoformat(timespec="seconds")

    cursor.execute("""
        INSERT INTO measurements (
            timestamp,
            gas_raw,
            temperature,
            humidity,
            gas_status,
            gas_detected,
            alarm_latched,
            buzzer,
            buzzer_muted
        )
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
    """, (
        timestamp,
        data["gas_raw"],
        data["temperature"],
        data["humidity"],
        data["gas_status"],
        int(data["gas_detected"]),
        int(data["alarm_latched"]),
        data["buzzer"],
        int(data["buzzer_muted"])
    ))

    connection.commit()
    connection.close()

    print("Measurement saved successfully!")
