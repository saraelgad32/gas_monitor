
const API_URL = "/api";
const MAX_HISTORY = 20;

const historyIds = new Set();

// =====================================
// UPDATE SENSOR DISPLAY
// =====================================

function updateSensorDisplay(data) {

    document.getElementById("gas-value").textContent =
        data.gas_raw ?? "--";

    document.getElementById("temperature-value").textContent =
        data.temperature != null ? data.temperature + " °C" : "--";

    document.getElementById("humidity-value").textContent =
        data.humidity != null ? data.humidity + " %" : "--";

    document.getElementById("buzzer-value").textContent =
        data.buzzer_muted ? "MUTED" : data.buzzer;

    document.getElementById("gas-detected").textContent =
        data.gas_detected ? "YES" : "NO";

    document.getElementById("alarm-latched").textContent =
        data.alarm_latched ? "YES" : "NO";

    document.getElementById("buzzer-muted").textContent =
        data.buzzer_muted ? "YES" : "NO";

    // Update gas status appearance
    const statusElement = document.getElementById("gas-status");
    const status = (data.gas_status || "UNKNOWN").toUpperCase();

    statusElement.textContent = status;
    statusElement.className = "status";

    if (status === "NORMAL") {
        statusElement.classList.add("normal");
    } else if (status === "WARNING") {
        statusElement.classList.add("warning");
    } else if (status === "DETECTED") {
        statusElement.classList.add("danger");
    }

    // Update remote mute button
    const muteButton = document.getElementById("mute-button");

    if (muteButton) {

        muteButton.disabled =
            !data.gas_detected || data.buzzer_muted;

        if (data.buzzer_muted) {
            muteButton.textContent = "Alarm Muted";
        } else {
            muteButton.textContent = "Mute Alarm";
        }
    }

    // Update optional feedback message
    const muteMessage = document.getElementById("mute-message");

    if (muteMessage && data.buzzer_muted) {
        muteMessage.textContent =
            "Alarm temporarily muted for 10 seconds.";
    } else if (muteMessage && !data.gas_detected) {
        muteMessage.textContent = "";
    }
}


// =====================================
// REMOTE BUZZER MUTE
// =====================================

async function muteBuzzer() {

    const muteButton = document.getElementById("mute-button");
    const muteMessage = document.getElementById("mute-message");

    if (!muteButton || muteButton.disabled) {
        return;
    }

    try {

        muteButton.disabled = true;
        muteButton.textContent = "Sending command...";

        if (muteMessage) {
            muteMessage.textContent =
                "Sending mute command to ESP32...";
        }

        const response = await fetch(
            API_URL + "/buzzer/mute",
            {
                method: "POST",
                headers: {
                    "Content-Type": "application/json"
                }
            }
        );

        const result = await response.json();

        if (!response.ok || !result.success) {
            throw new Error(
                result.error || "Unable to send mute command."
            );
        }

        console.log("Remote mute command published:", result);

        if (muteMessage) {
            muteMessage.textContent =
                "Command sent. Waiting for ESP32 confirmation...";
        }

        // Do not mark the buzzer as muted here.
        // Wait for the ESP32 telemetry received through SSE.

    } catch (error) {

        console.error("Remote mute error:", error);

        if (muteMessage) {
            muteMessage.textContent =
                "Error: Unable to send mute command.";
        }

    } finally {

        // Re-enable only if the alarm is still active.
        // The next sensor update will refresh the button state.
        if (muteButton.textContent === "Sending command...") {
            muteButton.textContent = "Mute Alarm";
        }
    }
}


// =====================================
// CREATE HISTORY TABLE
// =====================================

function createHistoryTable() {

    const container = document.getElementById("history-container");

    container.replaceChildren();

    const table = document.createElement("table");

    const headers = [
        "Timestamp",
        "Gas RAW",
        "Temperature",
        "Humidity",
        "Status",
        "Buzzer"
    ];

    const thead = document.createElement("thead");
    const headerRow = document.createElement("tr");

    headers.forEach(header => {

        const th = document.createElement("th");
        th.textContent = header;

        headerRow.appendChild(th);
    });

    thead.appendChild(headerRow);
    table.appendChild(thead);

    const tbody = document.createElement("tbody");

    table.appendChild(tbody);
    container.appendChild(table);
}


// =====================================
// ADD MEASUREMENT TO HISTORY
// =====================================

function addHistoryRow(measurement) {

    if (historyIds.has(measurement.id)) {
        return;
    }

    const container = document.getElementById("history-container");

    let tbody = container.querySelector("tbody");

    if (!tbody) {
        createHistoryTable();
        tbody = container.querySelector("tbody");
    }

    const row = document.createElement("tr");

    row.dataset.measurementId = measurement.id;

    const values = [
        measurement.timestamp,
        measurement.gas_raw,
        measurement.temperature != null
            ? measurement.temperature + " °C" : "--",
        measurement.humidity != null
            ? measurement.humidity + " %" : "--",
        measurement.gas_status,
        measurement.buzzer_muted
            ? "MUTED" : measurement.buzzer
    ];

    values.forEach(value => {

        const cell = document.createElement("td");
        cell.textContent = value ?? "--";

        row.appendChild(cell);
    });

    tbody.appendChild(row);
    historyIds.add(measurement.id);

    while (tbody.rows.length > MAX_HISTORY) {

        const oldestRow = tbody.rows[0];

        historyIds.delete(
            Number(oldestRow.dataset.measurementId)
        );

        tbody.removeChild(oldestRow);
    }
}


// =====================================
// LOAD INITIAL HISTORY
// =====================================

async function updateHistory() {

    try {

        const response = await fetch(
            API_URL + "/history?limit=" + MAX_HISTORY,
            { cache: "no-store" }
        );

        if (!response.ok) {
            throw new Error("Unable to retrieve history");
        }

        const result = await response.json();

        createHistoryTable();

        if (result.data.length === 0) {

            document.getElementById("history-container").textContent =
                "No measurements available.";

            return;
        }

        result.data.forEach(measurement => {
            addHistoryRow(measurement);
        });

    } catch (error) {

        console.error("History error:", error);
    }
}


// =====================================
// SERVER-SENT EVENTS
// =====================================

function connectToEventStream() {

    const eventSource = new EventSource(API_URL + "/stream");

    eventSource.onopen = function () {

        console.log("SSE connection established successfully!");
    };

    eventSource.onmessage = function (event) {

        try {

            const data = JSON.parse(event.data);

            console.log("New measurement received:", data);

            updateSensorDisplay(data);
            addHistoryRow(data);

        } catch (error) {

            console.error("SSE data error:", error);
        }
    };

    eventSource.onerror = function (error) {

        console.error("SSE connection error:", error);
    };
}


// =====================================
// INITIALIZE DASHBOARD
// =====================================

async function initializeDashboard() {

    // Connect the HTML mute button to the remote mute function
    const muteButton = document.getElementById("mute-button");

    if (muteButton) {

        muteButton.addEventListener("click", muteBuzzer);

        // Initially disabled until gas detection is confirmed
        muteButton.disabled = true;

    } else {

        console.warn("Mute button not found in HTML.");
    }

    await updateHistory();

    connectToEventStream();
}


// Start dashboard
initializeDashboard();
