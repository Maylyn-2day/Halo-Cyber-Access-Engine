import csv
import random
import time
import argparse
from datetime import datetime, timedelta

# --- Config ---
USERS = [f"U{i:05d}" for i in range(1, 20000)]
DEVICES = [f"D{i:05d}" for i in range(1, 5000)]
APPS = [f"APP{i:04d}" for i in range(1, 500)]
RESOURCES = [f"R{i:05d}" for i in range(1, 10000)]

EVENT_TYPES = ["LOGIN", "LOGOUT", "TOKEN_REFRESH", "ACCESS", "FAILED_LOGIN", "OPEN_APP", "DOWNLOAD", "ADMIN_ACTION"]
LOCATIONS = ["US", "VN", "JP", "KR", "SG", "CN", "DE", "FR", "UK", "AU", "CA", "IN", "BR", "RU", "TH"]

def generate_log_entry(timestamp, user=None, device=None, app=None, resource=None, event=None, loc=None):
    return [
        user or random.choice(USERS),
        device or random.choice(DEVICES),
        app or random.choice(APPS),
        resource or random.choice(RESOURCES),
        event or random.choices(EVENT_TYPES, weights=[10, 5, 20, 40, 5, 10, 8, 2], k=1)[0],
        loc or random.choices(LOCATIONS, weights=[30, 20, 10, 10, 5, 5, 5, 5, 2, 2, 2, 1, 1, 1, 1], k=1)[0],
        int(timestamp)
    ]

def main():
    parser = argparse.ArgumentParser(description="Halo Engine - High-Volume Dataset Generator")
    parser.add_argument("--rows", type=int, default=10000000, help="Number of rows to generate (default: 10,000,000)")
    parser.add_argument("--out", type=str, default="data/halo_massive_dataset.csv", help="Output file path")
    args = parser.parse_args()

    print(f"[*] Generating {args.rows:,} rows of log data to {args.out}...")
    
    start_time = time.time()
    current_time = datetime(2026, 1, 1, 8, 0, 0)
    
    # Pre-compute some anomalies to inject
    anomaly_user_exfil = USERS[0]
    anomaly_user_lateral = USERS[1]
    anomaly_device_compromised = DEVICES[0]

    with open(args.out, "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(["user_id", "device_id", "app_id", "resource_id", "event_type", "location", "timestamp"])

        buffer = []
        for i in range(args.rows):
            # Advance time by 1-5 seconds randomly
            current_time += timedelta(seconds=random.randint(1, 5))
            ts = current_time.timestamp()

            # --- INJECT ANOMALIES EVERY 1 MILLION ROWS ---
            if i > 0 and i % 1000000 == 0:
                # 1. Inject Data Exfiltration (5 unique downloads out-of-hours within 10 mins)
                exfil_time = current_time.replace(hour=2) # 2 AM
                for res_idx in range(6):
                    buffer.append(generate_log_entry(exfil_time.timestamp() + res_idx*60, user=anomaly_user_exfil, event="DOWNLOAD", resource=RESOURCES[res_idx]))
                
                # 2. Inject Lateral Movement (4 unique apps in 2 mins)
                for app_idx in range(5):
                    buffer.append(generate_log_entry(ts + app_idx*10, user=anomaly_user_lateral, event="OPEN_APP", app=APPS[app_idx]))

                # 3. Inject Compromised Device (3 unique users on same device in 5 mins)
                for u_idx in range(4):
                    buffer.append(generate_log_entry(ts + u_idx*30, user=USERS[u_idx+10], device=anomaly_device_compromised, event="LOGIN"))

            # Normal noise
            buffer.append(generate_log_entry(ts))

            # Flush buffer
            if len(buffer) >= 100000:
                writer.writerows(buffer)
                buffer.clear()
                print(f"  ... {i:,} rows generated.")

        # Flush remaining
        if buffer:
            writer.writerows(buffer)

    elapsed = time.time() - start_time
    print(f"\n[+] Successfully generated {args.rows:,} rows in {elapsed:.2f} seconds!")
    print(f"[+] File saved to: {args.out}")

if __name__ == "__main__":
    main()
