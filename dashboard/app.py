from flask import Flask, jsonify, render_template, request
from pathlib import Path
from datetime import datetime
from zoneinfo import ZoneInfo
import sqlite3
from contextlib import contextmanager

BASE_DIR = Path(__file__).resolve().parent
DB_PATH = BASE_DIR / "emergency.db"

API_KEY = "NESSO_CAPSTONE_2026"
TIMEZONE = ZoneInfo("Asia/Kolkata")

app = Flask(__name__)


@contextmanager
def db_connection():
    """
    Open one SQLite connection for the current operation and ALWAYS close it.

    Important: sqlite3.Connection's own context manager commits/rolls back,
    but it does not close the connection. The old project leaked one file
    descriptor per API request, eventually causing "Too many open files".
    """
    connection = sqlite3.connect(
        DB_PATH,
        timeout=10,
    )
    connection.row_factory = sqlite3.Row

    # Wait briefly if telemetry/status requests overlap with a write.
    connection.execute("PRAGMA busy_timeout = 10000")

    try:
        yield connection
    finally:
        connection.close()


def table_columns(db, table_name):
    return {
        row["name"]
        for row in db.execute(f"PRAGMA table_info({table_name})").fetchall()
    }


def ensure_column(db, table_name, column_name, definition):
    columns = table_columns(db, table_name)
    if column_name not in columns:
        db.execute(
            f"ALTER TABLE {table_name} ADD COLUMN {column_name} {definition}"
        )


def init_db():
    with db_connection() as db:
        # Better for frequent Nesso telemetry + dashboard reads.
        db.execute("PRAGMA journal_mode = WAL")

        db.execute(
            """
            CREATE TABLE IF NOT EXISTS emergency_events (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                device_id TEXT NOT NULL,
                status TEXT NOT NULL DEFAULT 'ACTIVE',
                emergency_type TEXT NOT NULL DEFAULT 'EMERGENCY',
                received_at TEXT NOT NULL,
                wifi_rssi INTEGER,
                acknowledged_at TEXT,
                resolved_at TEXT
            )
            """
        )

        # Safe migrations from earlier project versions.
        ensure_column(db, "emergency_events", "battery_pct", "INTEGER")
        ensure_column(db, "emergency_events", "battery_mv", "INTEGER")
        ensure_column(db, "emergency_events", "charging", "INTEGER")

        ensure_column(db, "emergency_events", "gps_valid", "INTEGER")
        ensure_column(db, "emergency_events", "latitude", "REAL")
        ensure_column(db, "emergency_events", "longitude", "REAL")
        ensure_column(db, "emergency_events", "satellites", "INTEGER")

        ensure_column(db, "emergency_events", "cancelled_at", "TEXT")
        ensure_column(db, "emergency_events", "closed_by", "TEXT")

        db.execute(
            """
            CREATE TABLE IF NOT EXISTS device_telemetry (
                device_id TEXT PRIMARY KEY,
                battery_pct INTEGER,
                battery_mv INTEGER,
                charging INTEGER,
                wifi_rssi INTEGER,
                last_seen TEXT NOT NULL
            )
            """
        )

        ensure_column(db, "device_telemetry", "gps_valid", "INTEGER")
        ensure_column(db, "device_telemetry", "latitude", "REAL")
        ensure_column(db, "device_telemetry", "longitude", "REAL")
        ensure_column(db, "device_telemetry", "satellites", "INTEGER")

        db.commit()


def now_iso():
    return datetime.now(TIMEZONE).isoformat(timespec="seconds")


def clean_int(value):
    try:
        return int(value) if value is not None else None
    except (TypeError, ValueError):
        return None


def clean_float(value):
    try:
        return float(value) if value is not None else None
    except (TypeError, ValueError):
        return None


def clean_bool(value):
    if isinstance(value, bool):
        return value

    if value in (1, "1", "true", "TRUE", "True"):
        return True

    if value in (0, "0", "false", "FALSE", "False"):
        return False

    return None


def gps_fields(data):
    gps_valid = clean_bool(data.get("gps_valid"))
    latitude = clean_float(data.get("latitude"))
    longitude = clean_float(data.get("longitude"))
    satellites = clean_int(data.get("satellites"))

    # gps_valid tells us whether the coordinates came from a real satellite
    # fix. Coordinates may still be present when gps_valid is False because
    # the Nesso can send a labelled demo/fallback Palghar location.
    gps_valid = bool(
        gps_valid
        and latitude is not None
        and longitude is not None
    )

    return gps_valid, latitude, longitude, satellites


def row_to_dict(row):
    return {
        "id": row["id"],
        "device_id": row["device_id"],
        "status": row["status"],
        "emergency_type": row["emergency_type"],
        "received_at": row["received_at"],
        "wifi_rssi": row["wifi_rssi"],
        "battery_pct": row["battery_pct"],
        "battery_mv": row["battery_mv"],
        "charging": (
            None if row["charging"] is None else bool(row["charging"])
        ),
        "gps_valid": bool(row["gps_valid"]) if row["gps_valid"] is not None else False,
        "latitude": row["latitude"],
        "longitude": row["longitude"],
        "satellites": row["satellites"],
        "acknowledged_at": row["acknowledged_at"],
        "resolved_at": row["resolved_at"],
        "cancelled_at": row["cancelled_at"],
        "closed_by": row["closed_by"],
    }


def telemetry_row_to_dict(row):
    return {
        "device_id": row["device_id"],
        "battery_pct": row["battery_pct"],
        "battery_mv": row["battery_mv"],
        "charging": (
            None if row["charging"] is None else bool(row["charging"])
        ),
        "wifi_rssi": row["wifi_rssi"],
        "gps_valid": bool(row["gps_valid"]) if row["gps_valid"] is not None else False,
        "latitude": row["latitude"],
        "longitude": row["longitude"],
        "satellites": row["satellites"],
        "last_seen": row["last_seen"],
    }


def require_api_key():
    return request.headers.get("X-API-Key") == API_KEY


def save_telemetry(
    db,
    device_id,
    battery_pct=None,
    battery_mv=None,
    charging=None,
    wifi_rssi=None,
    gps_valid=False,
    latitude=None,
    longitude=None,
    satellites=None,
):
    seen_at = now_iso()

    db.execute(
        """
        INSERT INTO device_telemetry
            (
                device_id,
                battery_pct,
                battery_mv,
                charging,
                wifi_rssi,
                gps_valid,
                latitude,
                longitude,
                satellites,
                last_seen
            )
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(device_id) DO UPDATE SET
            battery_pct = excluded.battery_pct,
            battery_mv = excluded.battery_mv,
            charging = excluded.charging,
            wifi_rssi = excluded.wifi_rssi,
            gps_valid = excluded.gps_valid,
            latitude = excluded.latitude,
            longitude = excluded.longitude,
            satellites = excluded.satellites,
            last_seen = excluded.last_seen
        """,
        (
            device_id,
            battery_pct,
            battery_mv,
            None if charging is None else int(charging),
            wifi_rssi,
            int(bool(gps_valid)),
            latitude,
            longitude,
            satellites,
            seen_at,
        ),
    )

    # Keep the currently-open alert refreshed with current battery/GPS data.
    db.execute(
        """
        UPDATE emergency_events
        SET battery_pct = ?,
            battery_mv = ?,
            charging = ?,
            wifi_rssi = ?,
            gps_valid = ?,
            latitude = ?,
            longitude = ?,
            satellites = ?
        WHERE id = (
            SELECT id
            FROM emergency_events
            WHERE device_id = ?
              AND status IN ('ACTIVE', 'ACKNOWLEDGED')
            ORDER BY id DESC
            LIMIT 1
        )
        """,
        (
            battery_pct,
            battery_mv,
            None if charging is None else int(charging),
            wifi_rssi,
            int(bool(gps_valid)),
            latitude,
            longitude,
            satellites,
            device_id,
        ),
    )

    return seen_at


@app.get("/")
def dashboard():
    return render_template("index.html")


@app.get("/api/health")
def health():
    return jsonify(
        {
            "ok": True,
            "service": "Nesso Emergency Dashboard",
            "time": now_iso(),
        }
    )


@app.post("/api/telemetry")
def telemetry():
    if not require_api_key():
        return jsonify({"ok": False, "error": "Invalid API key"}), 401

    data = request.get_json(silent=True) or {}

    device_id = str(data.get("device_id", "")).strip()
    if not device_id:
        return jsonify({"ok": False, "error": "device_id is required"}), 400

    battery_pct = clean_int(data.get("battery_pct"))
    battery_mv = clean_int(data.get("battery_mv"))
    charging = clean_bool(data.get("charging"))
    wifi_rssi = clean_int(data.get("wifi_rssi"))
    gps_valid, latitude, longitude, satellites = gps_fields(data)

    if battery_pct is not None:
        battery_pct = max(0, min(100, battery_pct))

    with db_connection() as db:
        seen_at = save_telemetry(
            db,
            device_id,
            battery_pct,
            battery_mv,
            charging,
            wifi_rssi,
            gps_valid,
            latitude,
            longitude,
            satellites,
        )
        db.commit()

    return jsonify(
        {
            "ok": True,
            "device_id": device_id,
            "last_seen": seen_at,
        }
    )


@app.get("/api/devices")
def devices():
    with db_connection() as db:
        rows = db.execute(
            """
            SELECT *
            FROM device_telemetry
            ORDER BY last_seen DESC
            """
        ).fetchall()

    return jsonify([telemetry_row_to_dict(row) for row in rows])


@app.post("/api/emergency")
def create_emergency():
    if not require_api_key():
        return jsonify({"ok": False, "error": "Invalid API key"}), 401

    data = request.get_json(silent=True) or {}

    device_id = str(data.get("device_id", "")).strip()
    emergency_type = str(
        data.get("status", "EMERGENCY")
    ).strip() or "EMERGENCY"

    wifi_rssi = clean_int(data.get("wifi_rssi"))
    battery_pct = clean_int(data.get("battery_pct"))
    battery_mv = clean_int(data.get("battery_mv"))
    charging = clean_bool(data.get("charging"))

    gps_valid, latitude, longitude, satellites = gps_fields(data)

    if not device_id:
        return jsonify({"ok": False, "error": "device_id is required"}), 400

    if battery_pct is not None:
        battery_pct = max(0, min(100, battery_pct))

    received_at = now_iso()

    with db_connection() as db:
        save_telemetry(
            db,
            device_id,
            battery_pct,
            battery_mv,
            charging,
            wifi_rssi,
            gps_valid,
            latitude,
            longitude,
            satellites,
        )

        existing = db.execute(
            """
            SELECT *
            FROM emergency_events
            WHERE device_id = ?
              AND status IN ('ACTIVE', 'ACKNOWLEDGED')
            ORDER BY id DESC
            LIMIT 1
            """,
            (device_id,),
        ).fetchone()

        if existing is not None:
            db.commit()

            return jsonify(
                {
                    "ok": True,
                    "message": "Emergency already active",
                    "event_id": existing["id"],
                    "device_id": device_id,
                    "status": existing["status"],
                    "received_at": existing["received_at"],
                }
            ), 200

        cursor = db.execute(
            """
            INSERT INTO emergency_events
                (
                    device_id,
                    status,
                    emergency_type,
                    received_at,
                    wifi_rssi,
                    battery_pct,
                    battery_mv,
                    charging,
                    gps_valid,
                    latitude,
                    longitude,
                    satellites
                )
            VALUES (?, 'ACTIVE', ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            """,
            (
                device_id,
                emergency_type,
                received_at,
                wifi_rssi,
                battery_pct,
                battery_mv,
                None if charging is None else int(charging),
                int(bool(gps_valid)),
                latitude,
                longitude,
                satellites,
            ),
        )

        db.commit()
        event_id = cursor.lastrowid

    print(
        f"[SOS] device={device_id} time={received_at} "
        f"battery={battery_pct}% rssi={wifi_rssi} "
        f"gps_valid={gps_valid} lat={latitude} lon={longitude} "
        f"sats={satellites} event_id={event_id}",
        flush=True,
    )

    return jsonify(
        {
            "ok": True,
            "message": "Emergency received",
            "event_id": event_id,
            "device_id": device_id,
            "status": "ACTIVE",
            "received_at": received_at,
            "gps_valid": gps_valid,
            "latitude": latitude,
            "longitude": longitude,
        }
    ), 201


@app.get("/api/device-status/<device_id>")
def device_status(device_id):
    if not require_api_key():
        return jsonify({"ok": False, "error": "Invalid API key"}), 401

    device_id = device_id.strip()

    with db_connection() as db:
        event = db.execute(
            """
            SELECT *
            FROM emergency_events
            WHERE device_id = ?
              AND status IN ('ACTIVE', 'ACKNOWLEDGED')
            ORDER BY id DESC
            LIMIT 1
            """,
            (device_id,),
        ).fetchone()

    if event is None:
        return jsonify(
            {
                "ok": True,
                "device_id": device_id,
                "status": "SAFE",
                "event_id": None,
            }
        )

    return jsonify(
        {
            "ok": True,
            "device_id": device_id,
            "status": event["status"],
            "event_id": event["id"],
            "received_at": event["received_at"],
            "acknowledged_at": event["acknowledged_at"],
        }
    )


@app.get("/api/events")
def list_events():
    limit = request.args.get("limit", default=100, type=int)
    limit = max(1, min(limit, 500))

    with db_connection() as db:
        rows = db.execute(
            """
            SELECT *
            FROM emergency_events
            ORDER BY id DESC
            LIMIT ?
            """,
            (limit,),
        ).fetchall()

    return jsonify([row_to_dict(row) for row in rows])


@app.post("/api/events/<int:event_id>/acknowledge")
def acknowledge_event(event_id):
    when = now_iso()

    with db_connection() as db:
        row = db.execute(
            "SELECT * FROM emergency_events WHERE id = ?",
            (event_id,),
        ).fetchone()

        if row is None:
            return jsonify({"ok": False, "error": "Event not found"}), 404

        if row["status"] in ("RESOLVED", "CANCELLED"):
            return jsonify(
                {"ok": False, "error": "Closed event cannot be acknowledged"}
            ), 409

        db.execute(
            """
            UPDATE emergency_events
            SET status = 'ACKNOWLEDGED',
                acknowledged_at = COALESCE(acknowledged_at, ?)
            WHERE id = ?
            """,
            (when, event_id),
        )
        db.commit()

    return jsonify(
        {
            "ok": True,
            "status": "ACKNOWLEDGED",
            "event_id": event_id,
            "acknowledged_at": when,
        }
    )


@app.post("/api/events/<int:event_id>/unacknowledge")
def unacknowledge_event(event_id):
    with db_connection() as db:
        row = db.execute(
            "SELECT * FROM emergency_events WHERE id = ?",
            (event_id,),
        ).fetchone()

        if row is None:
            return jsonify({"ok": False, "error": "Event not found"}), 404

        if row["status"] in ("RESOLVED", "CANCELLED"):
            return jsonify(
                {"ok": False, "error": "Closed event cannot be unacknowledged"}
            ), 409

        db.execute(
            """
            UPDATE emergency_events
            SET status = 'ACTIVE',
                acknowledged_at = NULL
            WHERE id = ?
            """,
            (event_id,),
        )
        db.commit()

    return jsonify(
        {
            "ok": True,
            "status": "ACTIVE",
            "event_id": event_id,
        }
    )


@app.post("/api/events/<int:event_id>/resolve")
def resolve_event(event_id):
    """
    Dashboard operator resolved the incident.
    This MUST count as RESOLVED, never CANCELLED.
    """
    when = now_iso()

    with db_connection() as db:
        row = db.execute(
            "SELECT * FROM emergency_events WHERE id = ?",
            (event_id,),
        ).fetchone()

        if row is None:
            return jsonify({"ok": False, "error": "Event not found"}), 404

        db.execute(
            """
            UPDATE emergency_events
            SET status = 'RESOLVED',
                resolved_at = ?,
                cancelled_at = NULL,
                closed_by = 'DASHBOARD_RESOLVE'
            WHERE id = ?
            """,
            (when, event_id),
        )
        db.commit()

    print(f"[RESOLVE] event_id={event_id} time={when}", flush=True)

    return jsonify(
        {
            "ok": True,
            "status": "RESOLVED",
            "event_id": event_id,
            "resolved_at": when,
        }
    )


@app.post("/api/cancel")
def cancel_emergency():
    """
    Nesso hardware-button cancellation.
    This MUST count as CANCELLED, never RESOLVED.
    """
    if not require_api_key():
        return jsonify({"ok": False, "error": "Invalid API key"}), 401

    data = request.get_json(silent=True) or {}
    device_id = str(data.get("device_id", "")).strip()

    if not device_id:
        return jsonify({"ok": False, "error": "device_id is required"}), 400

    cancelled_at = now_iso()

    with db_connection() as db:
        event = db.execute(
            """
            SELECT *
            FROM emergency_events
            WHERE device_id = ?
              AND status IN ('ACTIVE', 'ACKNOWLEDGED')
            ORDER BY id DESC
            LIMIT 1
            """,
            (device_id,),
        ).fetchone()

        if event is None:
            return jsonify(
                {
                    "ok": True,
                    "message": "No active emergency; device is already safe",
                    "device_id": device_id,
                    "status": "SAFE",
                }
            ), 200

        db.execute(
            """
            UPDATE emergency_events
            SET status = 'CANCELLED',
                cancelled_at = ?,
                resolved_at = NULL,
                closed_by = 'NESSO_CANCEL'
            WHERE id = ?
            """,
            (cancelled_at, event["id"]),
        )
        db.commit()

    print(
        f"[CANCEL] device={device_id} event_id={event['id']} "
        f"time={cancelled_at}",
        flush=True,
    )

    return jsonify(
        {
            "ok": True,
            "message": "Emergency cancelled",
            "device_id": device_id,
            "event_id": event["id"],
            "status": "CANCELLED",
            "cancelled_at": cancelled_at,
        }
    )


@app.post("/api/test-emergency")
def test_emergency():
    received_at = now_iso()

    with db_connection() as db:
        cursor = db.execute(
            """
            INSERT INTO emergency_events
                (
                    device_id,
                    status,
                    emergency_type,
                    received_at,
                    wifi_rssi,
                    battery_pct,
                    battery_mv,
                    charging,
                    gps_valid,
                    latitude,
                    longitude,
                    satellites
                )
            VALUES (
                'TEST-NESSO',
                'ACTIVE',
                'EMERGENCY',
                ?,
                -55,
                72,
                3970,
                0,
                0,
                NULL,
                NULL,
                0
            )
            """,
            (received_at,),
        )

        save_telemetry(
            db,
            "TEST-NESSO",
            72,
            3970,
            False,
            -55,
            False,
            None,
            None,
            0,
        )

        db.commit()

    return jsonify(
        {
            "ok": True,
            "message": "Test emergency created",
            "event_id": cursor.lastrowid,
        }
    )


if __name__ == "__main__":
    init_db()
    app.run(
        host="0.0.0.0",
        port=5001,
        debug=False,
        threaded=True,
        use_reloader=False,
    )
