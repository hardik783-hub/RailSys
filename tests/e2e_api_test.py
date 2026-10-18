#!/usr/bin/env python3
"""End-to-end API tests for the RailSys HTTP server.

Starts a *fresh* railsys server in a temporary working directory, so it
creates its own demo database and NEVER touches the real data/railsys.db.

Run:  python3 tests/e2e_api_test.py
"""

import json
import os
import shutil
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.request
from datetime import datetime, timedelta, timezone

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BINARY = os.path.join(ROOT, "build", "railsys")
BASE_URL = "http://127.0.0.1:18080"

CHECKS_RUN = 0
CHECKS_FAILED = 0


def check(condition, label):
    global CHECKS_RUN, CHECKS_FAILED
    CHECKS_RUN += 1
    if condition:
        print(f"  ok    {label}")
    else:
        CHECKS_FAILED += 1
        print(f"  FAIL  {label}")


def request(method, path, body=None):
    """Returns (status, parsed_json_or_text)."""
    url = BASE_URL + path
    data = json.dumps(body).encode() if body is not None else None
    req = urllib.request.Request(url, data=data, method=method)
    if data:
        req.add_header("Content-Type", "application/json")

    try:
        with urllib.request.urlopen(req, timeout=10) as resp:
            raw = resp.read().decode()
            status = resp.status
            ctype = resp.headers.get("Content-Type", "")
    except urllib.error.HTTPError as error:
        raw = error.read().decode()
        status = error.code
        ctype = error.headers.get("Content-Type", "")

    if "application/json" in ctype:
        try:
            return status, json.loads(raw)
        except json.JSONDecodeError:
            return status, raw
    return status, raw


def wait_for_server(timeout=15):
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            status, _ = request("GET", "/api/health")
            if status == 200:
                return True
        except OSError:
            time.sleep(0.3)
    return False


def main():
    if not os.path.exists(BINARY):
        print(f"Binary not found: {BINARY}. Build the project first "
              f"(cmake --build build).")
        return 1

    # Fresh, isolated working directory -> fresh SQLite demo database.
    workdir = tempfile.mkdtemp(prefix="railsys_e2e_")
    server = subprocess.Popen(
        [BINARY],
        cwd=workdir,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )

    try:
        if not wait_for_server():
            print("Server did not become healthy in time.")
            return 1

        # The server seeds journeys with SQLite date('now'), which is UTC.
        tomorrow = (datetime.now(timezone.utc) + timedelta(days=1)).strftime("%Y-%m-%d")

        # ---------------------------------------------------------- health
        print("\n== health & metadata ==")
        status, body = request("GET", "/api/health")
        check(status == 200 and body["status"] == "ok",
              "GET /api/health -> 200 ok")

        status, meta = request("GET", "/api/meta")
        check(status == 200 and len(meta["stations"]) >= 2,
              "GET /api/meta -> stations")
        check(len(meta["travelClasses"]) >= 2 and meta["trainCount"] >= 4,
              "GET /api/meta -> classes and train count")

        # ---------------------------------------------------------- static
        print("\n== static frontend ==")
        status, html = request("GET", "/")
        check(status == 200 and "<title>RailSys" in html,
              "GET / serves index.html")
        status, css = request("GET", "/style.css")
        check(status == 200 and "--navy-900" in css,
              "GET /style.css serves stylesheet")
        status, js = request("GET", "/app.js")
        check(status == 200 and "runSearch" in js,
              "GET /app.js serves script")

        # ---------------------------------------------------------- trains
        print("\n== trains & search ==")
        status, trains = request("GET", "/api/trains")
        check(status == 200 and len(trains["trains"]) >= 6,
              "GET /api/trains -> seeded demo trains")

        source = meta["stations"][0]
        destination = meta["stations"][1]
        query = (f"source={source}&destination={destination}"
                 f"&journeyDate={tomorrow}")
        status, search = request("GET", f"/api/search?{query}")
        check(status == 200 and search["count"] >= 1,
              "GET /api/search -> route results")
        check(all(r["source"] == source and r["destination"] == destination
                  for r in search["results"]),
              "search results match the requested route")

        # Use an isolated journey nobody else depends on: train 12003
        # tomorrow, AC 3 Tier (capacity 4 in the seed).
        train = search["results"][0]["trainNumber"]
        status, avail = request(
            "GET",
            f"/api/availability?trainNumber={train}&journeyDate={tomorrow}"
            f"&travelClass=AC%203%20Tier")
        check(status == 200 and avail["capacity"] == 4,
              "GET /api/availability -> capacity 4 coach")
        check(avail["availableSeats"] == 4 and avail["confirmedSeats"] == 0,
              "fresh journey has all seats available")

        status, body = request(
            "GET", "/api/availability?trainNumber=9999&journeyDate="
            f"{tomorrow}&travelClass=Sleeper")
        check(status == 404 and "error" in body,
              "unknown journey -> 404 with error message")

        status, body = request("GET", "/api/availability")
        check(status == 400 and "error" in body,
              "missing availability params -> 400")

        # ------------------------------------------------- booking workflow
        print("\n== booking workflow (isolated coach, capacity 4) ==")
        def book(name, age):
            return request("POST", "/api/bookings", {
                "trainNumber": train,
                "journeyDate": tomorrow,
                "travelClass": "AC 3 Tier",
                "passengerName": name,
                "passengerAge": age,
            })

        confirmed = []
        for index in range(4):
            status, booking = book(f"E2E Passenger {index + 1}", 25 + index)
            check(status == 201 and booking["status"] == "CONFIRMED",
                  f"booking {index + 1}/4 confirmed")
            confirmed.append(booking)

        seats = sorted(b["seatNumber"] for b in confirmed)
        check(seats == [1, 2, 3, 4],
              f"seat allotment is sequential and unique ({seats})")
        check(len({b["pnr"] for b in confirmed}) == 4,
              "every booking has a unique PNR")

        status, wait1 = book("E2E Waitlist One", 30)
        status, wait2 = book("E2E Waitlist Two", 31)
        check(wait1["status"] == "WAITLISTED" and wait1["seatNumber"] is None,
              "5th booking on a full coach is waitlisted")
        check(wait2["status"] == "WAITLISTED",
              "6th booking is waitlisted too")

        status, avail = request(
            "GET",
            f"/api/availability?trainNumber={train}&journeyDate={tomorrow}"
            f"&travelClass=AC%203%20Tier")
        check(avail["availableSeats"] == 0 and avail["waitlistedCount"] == 2,
              "availability reflects full coach + 2 waitlisted")

        # -------------------------------------------------- PNR behaviour
        print("\n== PNR lookup & validation ==")
        status, found = request(
            "GET", f"/api/bookings/{confirmed[0]['pnr']}")
        check(status == 200 and found["passengerName"] == "E2E Passenger 1",
              "PNR lookup returns the correct booking")

        status, body = request("GET", "/api/bookings/RS00000000")
        check(status == 404, "unknown PNR -> 404")
        status, body = request("GET", "/api/bookings/GARBAGE")
        check(status == 400, "malformed PNR -> 400")

        status, body = request("POST", "/api/bookings", {
            "trainNumber": train, "journeyDate": tomorrow,
            "travelClass": "AC 3 Tier", "passengerName": "", "passengerAge": 5})
        check(status == 400, "empty passenger name -> 400")
        status, body = request("POST", "/api/bookings", {
            "trainNumber": train, "journeyDate": "not-a-date",
            "travelClass": "AC 3 Tier", "passengerName": "X", "passengerAge": 5})
        check(status == 400, "invalid date -> 400")
        status, body = request("POST", "/api/bookings", {
            "trainNumber": train, "journeyDate": tomorrow,
            "travelClass": "AC 3 Tier", "passengerName": "X",
            "passengerAge": 200})
        check(status == 400, "invalid age -> 400")

        # ------------------------------------------------- cancel + promote
        print("\n== cancellation & FIFO waitlist promotion ==")
        status, cancel = request(
            "POST", f"/api/bookings/{confirmed[1]['pnr']}/cancel")
        check(status == 200 and cancel["waitlistPromoted"] is True,
              "cancelling a confirmed booking promotes a waitlisted one")
        check(cancel["promotedPNR"] == wait1["pnr"]
              and cancel["promotedSeatNumber"] == 2,
              "the EARLIEST waitlisted booking got the freed seat")

        status, promoted = request("GET", f"/api/bookings/{wait1['pnr']}")
        check(promoted["status"] == "CONFIRMED"
              and promoted["seatNumber"] == 2,
              "promoted booking is now confirmed with a seat")

        status, still_waiting = request(
            "GET", f"/api/bookings/{wait2['pnr']}")
        check(still_waiting["status"] == "WAITLISTED",
              "later waitlisted booking is untouched")

        status, body = request(
            "POST", f"/api/bookings/{confirmed[1]['pnr']}/cancel")
        check(status == 409 and "already cancelled" in body["error"],
              "duplicate cancellation -> 409")

        status, body = request(
            "POST", "/api/bookings/RS00000000/cancel")
        check(status == 404, "cancelling unknown PNR -> 404")

        # Cancelling the waitlisted booking should NOT promote anyone.
        status, body = request(
            "POST", f"/api/bookings/{wait2['pnr']}/cancel")
        check(status == 200 and body["waitlistPromoted"] is False,
              "cancelling a waitlisted booking promotes nobody")

        status, avail = request(
            "GET",
            f"/api/availability?trainNumber={train}&journeyDate={tomorrow}"
            f"&travelClass=AC%203%20Tier")
        check(avail["confirmedSeats"] == 4 and avail["availableSeats"] == 0
              and avail["waitlistedCount"] == 0,
              "availability updated correctly after all mutations")

        # Cancelled booking still readable with CANCELLED status.
        status, cancelled = request(
            "GET", f"/api/bookings/{confirmed[1]['pnr']}")
        check(cancelled["status"] == "CANCELLED",
              "cancelled booking still retrievable with CANCELLED status")

        print(f"\n{CHECKS_RUN - CHECKS_FAILED} / {CHECKS_RUN} "
              f"end-to-end checks passed")
        return 1 if CHECKS_FAILED else 0

    finally:
        server.terminate()
        try:
            server.wait(timeout=5)
        except subprocess.TimeoutExpired:
            server.kill()
        shutil.rmtree(workdir, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
