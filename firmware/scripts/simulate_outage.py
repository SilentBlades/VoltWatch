#!/usr/bin/env python3
"""
VoltWatch Outage & Email Alert Simulator
=========================================
Simulates power outage & restoration events and dispatches live email alerts
using the exact SMTP credentials and HTML/Plain-text formatting configured
in include/secrets.h.

Usage:
    python3 scripts/simulate_outage.py                   # Interactive menu
    python3 scripts/simulate_outage.py --type outage     # Immediate outage alert
    python3 scripts/simulate_outage.py --type restore    # Immediate restoration alert
    python3 scripts/simulate_outage.py --type full       # Outage -> wait 20s -> Restoration
"""

import sys
import os
import re
import time
import ssl
import smtplib
from datetime import datetime, timezone, timedelta
from email.mime.multipart import MIMEMultipart
from email.mime.text import MIMEText
import argparse

# Battery bank parameters matching include/config.h (12V 150Ah)
BATTERY_VOLTAGE = 12.0       # Volts
BATTERY_CAPACITY_AH = 150.0  # Ah
INVERTER_EFFICIENCY = 0.85   # 85%
MAX_DOD = 0.80               # 80% safe Depth of Discharge
TARE_LOSS_WATTS = 20.0       # Inverter idle parasitic draw
USABLE_WH = BATTERY_VOLTAGE * BATTERY_CAPACITY_AH * MAX_DOD * INVERTER_EFFICIENCY  # 1224 Wh

# IST Timezone (UTC + 5:30)
IST = timezone(timedelta(hours=5, minutes=30))

def load_secrets(secrets_path=None):
    """Parse firmware/include/secrets.h for credentials."""
    if secrets_path is None:
        script_dir = os.path.dirname(os.path.abspath(__file__))
        candidates = [
            os.path.join(script_dir, "..", "include", "secrets.h"),
            "firmware/include/secrets.h",
            "include/secrets.h",
        ]
        for cand in candidates:
            if os.path.exists(cand):
                secrets_path = cand
                break
        if secrets_path is None:
            secrets_path = os.path.join(script_dir, "..", "include", "secrets.h")

    if not os.path.exists(secrets_path):
        raise FileNotFoundError(
            f"Cannot find secrets file at {secrets_path}. "
            "Please copy firmware/include/secrets.example.h to firmware/include/secrets.h."
        )

    with open(secrets_path, "r", encoding="utf-8") as f:
        content = f.read()

    def get_macro(name):
        match = re.search(r'#define\s+' + name + r'\s+"([^"]*)"', content)
        return match.group(1) if match else None

    secrets = {
        "sender_name": get_macro("SMTP_SENDER_NAME") or "VoltWatch Sentinel",
        "sender_email": get_macro("SMTP_SENDER_EMAIL"),
        "app_password": get_macro("SMTP_APP_PASSWORD"),
        "recipients": []
    }

    for key in ["SMTP_RECIPIENT_1", "SMTP_RECIPIENT_2", "SMTP_RECIPIENT_3"]:
        val = get_macro(key)
        if val and len(val.strip()) > 0:
            secrets["recipients"].append(val.strip())

    if not secrets["sender_email"] or "@" not in secrets["sender_email"]:
        raise ValueError("Invalid or missing SMTP_SENDER_EMAIL in include/secrets.h")
    if not secrets["app_password"] or len(secrets["app_password"]) < 8:
        raise ValueError("Invalid or missing SMTP_APP_PASSWORD in include/secrets.h")
    if not secrets["recipients"]:
        raise ValueError("No recipients defined in include/secrets.h")

    return secrets

def calculate_runtime_minutes(load_watts):
    """Calculates remaining battery runtime based on 12V 150Ah bank."""
    effective_load = max(load_watts + TARE_LOSS_WATTS, 25.0)
    minutes = (USABLE_WH * 60.0) / effective_load
    return min(int(round(minutes)), 2880)

def format_ist_timestamp(dt=None):
    if dt is None:
        dt = datetime.now(IST)
    return dt.strftime("%Y-%m-%d %H:%M:%S IST")

def format_duration(seconds):
    hours = seconds // 3600
    mins = (seconds % 3600) // 60
    secs = seconds % 60
    return f"{hours}h {mins}m {secs}s"

def build_outage_message(secrets, load_watts=320.0, pre_trip_v=234.2, current_a=1.45, pf=0.96):
    """Constructs the exact Outage email alert matching VoltWatch firmware."""
    timestamp_str = format_ist_timestamp()
    runtime_min = calculate_runtime_minutes(load_watts)
    rt_hours = runtime_min // 60
    rt_mins = runtime_min % 60
    runtime_str = f"{runtime_min} min ({rt_hours}h {rt_mins}m)"

    subject = "🚨 [ALERT] Power Outage Detected - Grid Down"

    text_body = f"""====================================================
🚨 VOLTWATCH ALERT: POWER OUTAGE DETECTED
====================================================

Status:       GRID DOWN (Running on Battery / Inverter)
Timestamp:    {timestamp_str}
Pre-Trip V:   {pre_trip_v:.1f} V

--- Live Inverter Load Snapshot ---
Active Power: {load_watts:.1f} W
Load Current: {current_a:.2f} A
Power Factor: {pf:.2f}
Est. Autonomy:{runtime_str}

VoltWatch Sentinel | Standalone Zero-Server Metrology
"""

    html_body = f"""<!DOCTYPE html><html><head><meta charset='utf-8'></head>
<body style='font-family:-apple-system,BlinkMacSystemFont,Segoe UI,Roboto,Helvetica,Arial,sans-serif;
background-color:#121212;color:#E0E0E0;margin:0;padding:20px;'>
<div style='max-width:600px;margin:0 auto;background:#1E1E1E;border-radius:12px;overflow:hidden;
box-shadow:0 8px 24px rgba(0,0,0,0.5);border:1px solid #333;'>
<div style='background:#D32F2F;padding:24px;text-align:center;'>
<h1 style='color:#FFFFFF;margin:0;font-size:22px;letter-spacing:1px;'>🚨 Power Outage Detected</h1>
<p style='color:rgba(255,255,255,0.85);margin:6px 0 0;font-size:14px;'>Grid Mains Failure - Running on Inverter</p>
</div>
<div style='padding:24px;'>
<table style='width:100%;border-collapse:collapse;font-size:14px;'>
<tr style='border-bottom:1px solid #2C2C2C;'><td style='padding:10px 0;color:#9E9E9E;'>Trip Timestamp</td>
<td style='padding:10px 0;text-align:right;font-weight:bold;color:#FFFFFF;'>{timestamp_str}</td></tr>
<tr style='border-bottom:1px solid #2C2C2C;'><td style='padding:10px 0;color:#9E9E9E;'>Grid Voltage Pre-Trip</td>
<td style='padding:10px 0;text-align:right;font-weight:bold;color:#FF5252;'>{pre_trip_v:.1f} V</td></tr>
<tr style='border-bottom:1px solid #2C2C2C;'><td style='padding:10px 0;color:#9E9E9E;'>Inverter Active Load</td>
<td style='padding:10px 0;text-align:right;font-weight:bold;color:#FFA726;'>{load_watts:.1f} W</td></tr>
<tr style='border-bottom:1px solid #2C2C2C;'><td style='padding:10px 0;color:#9E9E9E;'>Load Current</td>
<td style='padding:10px 0;text-align:right;font-weight:bold;color:#FFFFFF;'>{current_a:.2f} A</td></tr>
<tr style='border-bottom:1px solid #2C2C2C;'><td style='padding:10px 0;color:#9E9E9E;'>Power Factor</td>
<td style='padding:10px 0;text-align:right;font-weight:bold;color:#FFFFFF;'>{pf:.2f}</td></tr>
<tr><td style='padding:12px 0 0;color:#9E9E9E;'>Est. Battery Autonomy</td>
<td style='padding:12px 0 0;text-align:right;font-weight:bold;color:#66BB6A;font-size:16px;'>{runtime_str}</td></tr>
</table></div>
<div style='background:#181818;padding:14px;text-align:center;font-size:12px;color:#757575;border-top:1px solid #282828;'>
VoltWatch Standalone Edge Sentinel • Zero Server Dependency</div>
</div></body></html>"""

    return subject, text_body, html_body

def build_restoration_message(secrets, duration_seconds=1845, energy_kwh=0.164, restored_v=231.8, freq=50.0):
    """Constructs the exact Restoration email alert matching VoltWatch firmware."""
    timestamp_str = format_ist_timestamp()
    duration_str = format_duration(duration_seconds)

    subject = "✅ [RESTORED] Grid Power Resumed"

    text_body = f"""====================================================
✅ VOLTWATCH ALERT: GRID POWER RESUMED
====================================================

Status:       GRID RESTORED (Normal Operation)
Timestamp:    {timestamp_str}
Duration:     {duration_str} ({duration_seconds} s)
Battery Drain:{energy_kwh:.3f} kWh

--- Live Grid Mains Telemetry ---
Grid Voltage: {restored_v:.1f} V
Frequency:    {freq:.1f} Hz

VoltWatch Sentinel | Historical record preserved in NVS
"""

    html_body = f"""<!DOCTYPE html><html><head><meta charset='utf-8'></head>
<body style='font-family:-apple-system,BlinkMacSystemFont,Segoe UI,Roboto,Helvetica,Arial,sans-serif;
background-color:#121212;color:#E0E0E0;margin:0;padding:20px;'>
<div style='max-width:600px;margin:0 auto;background:#1E1E1E;border-radius:12px;overflow:hidden;
box-shadow:0 8px 24px rgba(0,0,0,0.5);border:1px solid #333;'>
<div style='background:#2E7D32;padding:24px;text-align:center;'>
<h1 style='color:#FFFFFF;margin:0;font-size:22px;letter-spacing:1px;'>✅ Grid Power Resumed</h1>
<p style='color:rgba(255,255,255,0.85);margin:6px 0 0;font-size:14px;'>Utility Power Restored - Running on Grid</p>
</div>
<div style='padding:24px;'>
<table style='width:100%;border-collapse:collapse;font-size:14px;'>
<tr style='border-bottom:1px solid #2C2C2C;'><td style='padding:10px 0;color:#9E9E9E;'>Restoration Time</td>
<td style='padding:10px 0;text-align:right;font-weight:bold;color:#FFFFFF;'>{timestamp_str}</td></tr>
<tr style='border-bottom:1px solid #2C2C2C;'><td style='padding:10px 0;color:#9E9E9E;'>Total Outage Duration</td>
<td style='padding:10px 0;text-align:right;font-weight:bold;color:#66BB6A;font-size:15px;'>{duration_str}</td></tr>
<tr style='border-bottom:1px solid #2C2C2C;'><td style='padding:10px 0;color:#9E9E9E;'>Battery Energy Consumed</td>
<td style='padding:10px 0;text-align:right;font-weight:bold;color:#FFA726;'>{energy_kwh:.3f} kWh</td></tr>
<tr style='border-bottom:1px solid #2C2C2C;'><td style='padding:10px 0;color:#9E9E9E;'>Restored Line Voltage</td>
<td style='padding:10px 0;text-align:right;font-weight:bold;color:#4CAF50;'>{restored_v:.1f} V</td></tr>
<tr><td style='padding:10px 0;color:#9E9E9E;'>Restored Frequency</td>
<td style='padding:10px 0;text-align:right;font-weight:bold;color:#FFFFFF;'>{freq:.1f} Hz</td></tr>
</table></div>
<div style='background:#181818;padding:14px;text-align:center;font-size:12px;color:#757575;border-top:1px solid #282828;'>
VoltWatch Standalone Edge Sentinel • Event Logged to NVS Flash</div>
</div></body></html>"""

    return subject, text_body, html_body

def send_alert(secrets, subject, text_body, html_body):
    """Sends the multipart email using Google SMTPS over TLS (Port 465)."""
    sender = secrets["sender_email"]
    sender_name = secrets["sender_name"]
    password = secrets["app_password"]
    recipients = secrets["recipients"]

    msg = MIMEMultipart("alternative")
    msg["Subject"] = subject
    msg["From"] = f"{sender_name} <{sender}>"
    msg["To"] = ", ".join(recipients)

    part_text = MIMEText(text_body, "plain", "utf-8")
    part_html = MIMEText(html_body, "html", "utf-8")

    msg.attach(part_text)
    msg.attach(part_html)

    print(f"Connecting to Google SMTP (smtp.gmail.com:465 SMTPS)...")
    context = ssl.create_default_context()

    try:
        with smtplib.SMTP_SSL("smtp.gmail.com", 465, context=context, timeout=15) as server:
            print(f"Authenticating as {sender}...")
            server.login(sender, password)
            print(f"Dispatching email to {len(recipients)} recipient(s):")
            for r in recipients:
                print(f"  • {r}")
            server.sendmail(sender, recipients, msg.as_string())
            print("✅ Email delivered to Google SMTP successfully!\n")
            return True
    except smtplib.SMTPAuthenticationError as e:
        print(f"❌ SMTP Authentication Failed (Code {e.smtp_code}): {e.smtp_error.decode('utf-8', errors='ignore')}")
        print("   Tip: Check that the 16-character Google App Password has no typos.")
        return False
    except Exception as e:
        print(f"❌ Connection or delivery error: {e}")
        return False

def main():
    parser = argparse.ArgumentParser(description="VoltWatch Outage & Email Alert Simulator")
    parser.add_argument("--type", choices=["outage", "restore", "full"], help="Alert simulation type")
    args = parser.parse_args()

    print("====================================================")
    print("      VoltWatch Power Outage & Alert Simulator      ")
    print("====================================================")

    try:
        secrets = load_secrets()
    except Exception as e:
        print(f"Error reading secrets: {e}")
        sys.exit(1)

    print(f"Sender:     {secrets['sender_email']}")
    print(f"Recipients: {', '.join(secrets['recipients'])}")
    print(f"Battery:    {BATTERY_VOLTAGE}V {BATTERY_CAPACITY_AH}Ah (Usable: {USABLE_WH:.0f} Wh)")
    print("----------------------------------------------------\n")

    sim_type = args.type
    if not sim_type:
        print("Select simulation action:")
        print("  1. Simulate Power Outage Alert     (🚨 Grid Trips -> Inverter Mode)")
        print("  2. Simulate Power Restoration Alert (✅ Grid Restored -> Normal Mode)")
        print("  3. Run Full Outage Lifecycle       (Outage -> 20s delay -> Restoration)")
        print("  4. Exit")
        try:
            choice = input("\nEnter choice [1-4]: ").strip()
        except KeyboardInterrupt:
            print("\nExiting.")
            sys.exit(0)

        if choice == "1":
            sim_type = "outage"
        elif choice == "2":
            sim_type = "restore"
        elif choice == "3":
            sim_type = "full"
        else:
            print("Cancelled.")
            sys.exit(0)

    if sim_type == "outage":
        print("\n⚡ Triggering simulated GRID OUTAGE...")
        subj, txt, html = build_outage_message(secrets, load_watts=310.0, pre_trip_v=232.5)
        send_alert(secrets, subj, txt, html)

    elif sim_type == "restore":
        print("\n⚡ Triggering simulated GRID RESTORATION...")
        subj, txt, html = build_restoration_message(secrets, duration_seconds=1240, energy_kwh=0.108, restored_v=231.2)
        send_alert(secrets, subj, txt, html)

    elif sim_type == "full":
        print("\n⚡ [1/2] Triggering simulated GRID OUTAGE...")
        start_t = time.time()
        subj, txt, html = build_outage_message(secrets, load_watts=340.0, pre_trip_v=233.1)
        ok = send_alert(secrets, subj, txt, html)
        if not ok:
            print("Aborting full simulation due to failure.")
            sys.exit(1)

        wait_sec = 20
        print(f"⏳ Outage active... Simulating {wait_sec} seconds on inverter power...")
        for remaining in range(wait_sec, 0, -1):
            sys.stdout.write(f"\r   Time remaining until restoration: {remaining:2d}s ")
            sys.stdout.flush()
            time.sleep(1)
        print("\n")

        print("⚡ [2/2] Grid mains restored! Triggering RESTORATION alert...")
        duration = int(time.time() - start_t)
        # Approximate energy: 340W * (duration/3600) kWh
        energy = (340.0 * duration) / 3600000.0
        subj, txt, html = build_restoration_message(secrets, duration_seconds=duration, energy_kwh=energy, restored_v=230.8)
        send_alert(secrets, subj, txt, html)

    print("Check your inboxes on both devices!")

if __name__ == "__main__":
    main()
