const activeAlerts = document.getElementById("activeAlerts");
const historyBody = document.getElementById("historyBody");
const deviceGrid = document.getElementById("deviceGrid");

const activeCount = document.getElementById("activeCount");
const ackCount = document.getElementById("ackCount");
const resolvedCount = document.getElementById("resolvedCount");
const cancelledCount = document.getElementById("cancelledCount");
const totalCount = document.getElementById("totalCount");

const lastRefresh = document.getElementById("lastRefresh");
const serverBadge = document.getElementById("serverBadge");
const testButton = document.getElementById("testButton");
const soundToggle = document.getElementById("soundToggle");

let soundEnabled = true;
let audioContext = null;
let previousEventIds = new Set();
let dashboardInitialized = false;

function ensureAudioContext() {
    if (!audioContext) {
        const AudioContextClass =
            window.AudioContext || window.webkitAudioContext;

        if (!AudioContextClass) {
            return null;
        }

        audioContext = new AudioContextClass();
    }

    if (audioContext.state === "suspended") {
        audioContext.resume().catch(() => {});
    }

    return audioContext;
}

function beepOnce(startDelay = 0, frequency = 880, duration = 0.16) {
    if (!soundEnabled) return;

    const ctx = ensureAudioContext();
    if (!ctx) return;

    const start = ctx.currentTime + startDelay;
    const oscillator = ctx.createOscillator();
    const gain = ctx.createGain();

    oscillator.type = "sine";
    oscillator.frequency.setValueAtTime(frequency, start);

    gain.gain.setValueAtTime(0.0001, start);
    gain.gain.exponentialRampToValueAtTime(0.28, start + 0.015);
    gain.gain.exponentialRampToValueAtTime(0.0001, start + duration);

    oscillator.connect(gain);
    gain.connect(ctx.destination);

    oscillator.start(start);
    oscillator.stop(start + duration + 0.02);
}

function playSosAlert() {
    if (!soundEnabled) return;

    // Three short emergency beeps.
    beepOnce(0.00, 900, 0.17);
    beepOnce(0.28, 900, 0.17);
    beepOnce(0.56, 1100, 0.24);
}

function unlockAlertSound() {
    if (!soundEnabled) return;
    ensureAudioContext();
}

function updateSoundButton() {
    soundToggle.textContent =
        soundEnabled
            ? "🔊 Alert Sound: ON"
            : "🔇 Alert Sound: OFF";

    soundToggle.classList.toggle("sound-off", !soundEnabled);
}

function detectNewSos(events) {
    const currentIds = new Set(events.map(event => event.id));

    // Do not beep for emergencies that were already present on initial load.
    if (!dashboardInitialized) {
        previousEventIds = currentIds;
        dashboardInitialized = true;
        return;
    }

    const newEmergency = events.some(event =>
        !previousEventIds.has(event.id) &&
        event.status === "ACTIVE"
    );

    previousEventIds = currentIds;

    if (newEmergency) {
        playSosAlert();
    }
}


function escapeHtml(value) {
    return String(value ?? "")
        .replaceAll("&", "&amp;")
        .replaceAll("<", "&lt;")
        .replaceAll(">", "&gt;")
        .replaceAll('"', "&quot;")
        .replaceAll("'", "&#039;");
}

function readableTime(isoTime) {
    if (!isoTime) return "—";

    const date = new Date(isoTime);

    return new Intl.DateTimeFormat("en-IN", {
        dateStyle: "medium",
        timeStyle: "medium",
    }).format(date);
}

function statusHtml(status) {
    const safe = escapeHtml(status);
    return `<span class="status ${safe.toLowerCase()}">${safe}</span>`;
}

function batteryClass(percent) {
    if (percent == null) return "";
    if (percent <= 15) return "critical";
    if (percent <= 30) return "low";
    return "";
}

function batteryText(percent) {
    return percent == null ? "Unknown" : `${percent}%`;
}

function voltageText(mv) {
    if (mv == null || mv < 0) return "—";
    return `${(mv / 1000).toFixed(2)} V`;
}

function chargingText(charging) {
    if (charging === true) return "Charging";
    if (charging === false) return "Battery";
    return "Unknown";
}


function gpsText(item) {
    if (
        item.latitude == null ||
        item.longitude == null
    ) {
        return "No coordinates";
    }

    return `${Number(item.latitude).toFixed(6)}, ${Number(item.longitude).toFixed(6)}`;
}

function gpsSourceText(item) {
    return item.gps_valid
        ? "Real GPS fix"
        : "Fallback/Demo · Palghar";
}

function gpsMapUrl(item) {
    if (
        item.latitude == null ||
        item.longitude == null
    ) {
        return null;
    }

    const lat = encodeURIComponent(item.latitude);
    const lon = encodeURIComponent(item.longitude);

    return `https://www.google.com/maps?q=${lat},${lon}`;
}

function gpsHtml(item) {
    const url = gpsMapUrl(item);

    if (!url) {
        return `<span class="gps-waiting">No coordinates</span>`;
    }

    const satellites =
        item.satellites == null ? "—" : item.satellites;

    const cssClass =
        item.gps_valid ? "gps-ok" : "gps-waiting";

    return `
        <span class="${cssClass}">${gpsText(item)}</span>
        <span class="${cssClass}">${gpsSourceText(item)}</span>
        <a
            class="map-link"
            href="${url}"
            target="_blank"
            rel="noopener noreferrer">
            View location · ${satellites} sats
        </a>
    `;
}

async function changeStatus(id, action) {
    const response = await fetch(`/api/events/${id}/${action}`, {
        method: "POST",
    });

    if (!response.ok) {
        const data = await response.json().catch(() => ({}));
        alert(data.error || "Action failed");
    }

    await loadDashboard();
}

function renderDevices(devices) {
    if (!devices.length) {
        deviceGrid.innerHTML =
            `<div class="empty-state">Waiting for Nesso telemetry...</div>`;
        return;
    }

    deviceGrid.innerHTML = devices.map(device => {
        const pct = device.battery_pct;
        const width = pct == null ? 0 : Math.max(0, Math.min(100, pct));

        return `
            <article class="device-card">
                <h3>${escapeHtml(device.device_id)}</h3>

                <div class="battery-row">
                    <div class="battery-shell" aria-label="Battery ${batteryText(pct)}">
                        <div
                            class="battery-fill ${batteryClass(pct)}"
                            style="width:${width}%">
                        </div>
                    </div>
                    <div class="battery-number">${batteryText(pct)}</div>
                </div>

                <div class="telemetry-grid">
                    <span>Battery voltage</span>
                    <b>${voltageText(device.battery_mv)}</b>

                    <span>Power state</span>
                    <b>${chargingText(device.charging)}</b>

                    <span>Wi-Fi RSSI</span>
                    <b>${device.wifi_rssi == null ? "—" : `${device.wifi_rssi} dBm`}</b>

                    <span>GPS</span>
                    <b>${gpsText(device)} · ${gpsSourceText(device)}</b>

                    <span>Satellites</span>
                    <b>${device.satellites == null ? "—" : device.satellites}</b>

                    <span>Last seen</span>
                    <b>${readableTime(device.last_seen)}</b>
                </div>
            </article>
        `;
    }).join("");
}

function renderActive(events) {
    const live = events.filter(event => !["RESOLVED", "CANCELLED"].includes(event.status));

    if (live.length === 0) {
        activeAlerts.innerHTML =
            `<div class="empty-state">No active emergency at the moment.</div>`;
        return;
    }

    activeAlerts.innerHTML = live.map(event => {
        const isAck = event.status === "ACKNOWLEDGED";

        return `
            <article class="alert-card ${isAck ? "acknowledged" : ""}">
                <div class="alert-title">
                    <div>
                        ${statusHtml(event.status)}
                        <strong style="display:block;margin-top:10px">
                            ${escapeHtml(event.device_id)}
                        </strong>
                    </div>
                    <span class="pulse"></span>
                </div>

                <div class="details">
                    <span>Emergency</span>
                    <b>${escapeHtml(event.emergency_type)}</b>

                    <span>Received</span>
                    <b>${readableTime(event.received_at)}</b>

                    <span>Battery</span>
                    <b>${batteryText(event.battery_pct)}</b>

                    <span>Voltage</span>
                    <b>${voltageText(event.battery_mv)}</b>

                    <span>Power</span>
                    <b>${chargingText(event.charging)}</b>

                    <span>Wi-Fi signal</span>
                    <b>${event.wifi_rssi == null ? "—" : `${event.wifi_rssi} dBm`}</b>

                    <span>GPS location</span>
                    <b>${gpsHtml(event)}</b>

                    <span>Event ID</span>
                    <b>#${event.id}</b>
                </div>

                <div class="actions">
                    ${
                        event.status === "ACTIVE"
                            ? `<button class="ack-button"
                                onclick="changeStatus(${event.id}, 'acknowledge')">
                                Acknowledge
                               </button>`
                            : ""
                    }

                    ${
                        event.status === "ACKNOWLEDGED"
                            ? `<button class="unack-button"
                                onclick="changeStatus(${event.id}, 'unacknowledge')">
                                Cancel Acknowledge
                               </button>`
                            : ""
                    }

                    <button class="resolve-button"
                        onclick="changeStatus(${event.id}, 'resolve')">
                        Resolve
                    </button>
                </div>
            </article>
        `;
    }).join("");
}

function renderHistory(events) {
    historyBody.innerHTML = events.map(event => `
        <tr>
            <td>#${event.id}</td>
            <td><strong>${escapeHtml(event.device_id)}</strong></td>
            <td>${readableTime(event.received_at)}</td>
            <td>${batteryText(event.battery_pct)}</td>
            <td>${voltageText(event.battery_mv)}</td>
            <td>${event.wifi_rssi == null ? "—" : `${event.wifi_rssi} dBm`}</td>
            <td>${gpsText(event)} · ${gpsSourceText(event)}</td>
            <td>${statusHtml(event.status)}</td>
        </tr>
    `).join("");
}

async function loadDashboard() {
    try {
        const [eventsResponse, devicesResponse] = await Promise.all([
            fetch("/api/events?limit=100"),
            fetch("/api/devices"),
        ]);

        if (!eventsResponse.ok || !devicesResponse.ok) {
            throw new Error("Could not load dashboard data");
        }

        const events = await eventsResponse.json();
        const devices = await devicesResponse.json();

        detectNewSos(events);

        const active = events.filter(event => event.status === "ACTIVE").length;
        const acknowledged =
            events.filter(event => event.status === "ACKNOWLEDGED").length;
        const resolved =
            events.filter(event => event.status === "RESOLVED").length;
        const cancelled =
            events.filter(event => event.status === "CANCELLED").length;

        activeCount.textContent = active;
        ackCount.textContent = acknowledged;
        resolvedCount.textContent = resolved;
        cancelledCount.textContent = cancelled;
        totalCount.textContent = events.length;

        renderDevices(devices);
        renderActive(events);
        renderHistory(events);

        serverBadge.classList.remove("offline");
        serverBadge.classList.add("online");
        serverBadge.lastChild.textContent = " SERVER ONLINE";

        lastRefresh.textContent =
            `Updated ${new Date().toLocaleTimeString("en-IN")}`;
    } catch (error) {
        serverBadge.classList.remove("online");
        serverBadge.classList.add("offline");
        serverBadge.lastChild.textContent = " SERVER OFFLINE";
        console.error(error);
    }
}

testButton.addEventListener("click", async () => {
    testButton.disabled = true;

    try {
        await fetch("/api/test-emergency", {
            method: "POST",
        });

        await loadDashboard();
    } finally {
        testButton.disabled = false;
    }
});

soundToggle.addEventListener("click", () => {
    soundEnabled = !soundEnabled;

    if (soundEnabled) {
        ensureAudioContext();
        beepOnce(0, 760, 0.09);
    }

    updateSoundButton();
});

// Modern browsers require one user interaction before Web Audio may play.
// Any first click/tap or key press unlocks alert sound.
document.addEventListener(
    "pointerdown",
    unlockAlertSound,
    { once: true }
);

document.addEventListener(
    "keydown",
    unlockAlertSound,
    { once: true }
);

updateSoundButton();

window.changeStatus = changeStatus;

loadDashboard();
setInterval(loadDashboard, 2000);
