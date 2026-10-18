#!/usr/bin/env node
/* Frontend smoke test for public/app.js.
 *
 * Runs the REAL app.js inside a Node vm with a minimal DOM shim and a
 * mocked fetch() that mimics the live backend contracts. It simulates:
 *   boot -> search -> select train -> book -> confirmation -> PNR lookup
 *   -> cancellation dialog -> promotion notice
 * and asserts that the rendered DOM state is correct.
 *
 * Run:  node tests/frontend_smoke_test.js
 */

"use strict";

const fs = require("fs");
const path = require("path");
const vm = require("vm");

let checksRun = 0;
let checksFailed = 0;

function check(condition, label) {
    checksRun += 1;
    if (condition) {
        console.log(`  ok    ${label}`);
    } else {
        checksFailed += 1;
        console.log(`  FAIL  ${label}`);
    }
}

/* ------------------------------------------------------------ DOM shim */

const allElements = [];

function makeClassList(node) {
    const set = new Set();
    return {
        add: (...names) => names.forEach((n) => set.add(n)),
        remove: (...names) => names.forEach((n) => set.delete(n)),
        toggle: (name, force) => {
            const wanted = force !== undefined ? force : !set.has(name);
            if (wanted) set.add(name); else set.delete(name);
            return wanted;
        },
        contains: (name) => set.has(name),
        _set: set
    };
}

function makeElement(tag) {
    const node = {
        tagName: String(tag).toUpperCase(),
        children: [],
        listeners: {},
        attributes: {},
        style: {},
        textContent: "",
        innerHTML: "",
        value: "",
        hidden: false,
        disabled: false,
        className: "",
        _queryCache: {},

        classList: null,
        classOfChildren: [],

        append(...nodes) {
            for (const child of nodes) this.children.push(child);
        },
        replaceChildren(...nodes) {
            this.children = [];
            this.append(...nodes);
        },
        addEventListener(type, handler) {
            (this.listeners[type] = this.listeners[type] || []).push(handler);
        },
        dispatch(type, event = {}) {
            for (const handler of this.listeners[type] || []) {
                handler({ preventDefault() {}, target: this, ...event });
            }
        },
        setAttribute(name, value) { this.attributes[name] = String(value); },
        removeAttribute(name) { delete this.attributes[name]; },
        getAttribute(name) { return this.attributes[name] ?? null; },
        focus() {},
        scrollIntoView() {},
        closest() { return null; },
        remove() {},
        reset() { this.value = ""; },
        querySelector(selector) {
            if (!this._queryCache[selector]) {
                this._queryCache[selector] = makeElement("span");
            }
            return this._queryCache[selector];
        },
        showModal() { this._modalOpen = true; },
        get classNameSet() { return this.classList._set; }
    };

    node.classList = makeClassList(node);
    allElements.push(node);
    return node;
}

const elementsById = {};
let domReadyHandler = null;

const documentShim = {
    getElementById(id) {
        if (!elementsById[id]) elementsById[id] = makeElement("div");
        return elementsById[id];
    },
    createElement(tag) { return makeElement(tag); },
    createTextNode(text) {
        const node = makeElement("#text");
        node.textContent = String(text);
        return node;
    },
    addEventListener(type, handler) {
        if (type === "DOMContentLoaded") domReadyHandler = handler;
    },
    createRange() { return { selectNodeContents() {} }; }
};

/* ---------------------------------------------------------- fetch mock */

const PNR = "RSABC12345";

const journeyItem = {
    trainNumber: "12001",
    trainName: "Demo Express",
    source: "Wardha",
    destination: "Nagpur",
    journeyDate: "2026-10-25",
    travelClass: "Sleeper",
    capacity: 6,
    totalSeats: 6,
    confirmedSeats: 2,
    availableSeats: 4,
    waitlistedCount: 0,
    baseFarePaise: 45000,
    baseFareRupees: 450,
    isFull: false
};

const createdBooking = {
    pnr: PNR,
    status: "CONFIRMED",
    trainNumber: "12001",
    trainName: "Demo Express",
    source: "Wardha",
    destination: "Nagpur",
    journeyDate: "2026-10-25",
    travelClass: "Sleeper",
    passengerName: "Test User",
    passengerAge: 30,
    passengerStatus: "CONFIRMED",
    farePaise: 45000,
    fareRupees: 450,
    seatNumber: 3
};

// The lookup mock is stateful: after the cancellation, the API would
// return the booking with CANCELLED status and no seat, exactly like
// the real backend does.
let cancelHappened = false;

function currentBookingPayload() {
    return cancelHappened
        ? { ...createdBooking, status: "CANCELLED",
            passengerStatus: "CANCELLED", seatNumber: null }
        : { ...createdBooking };
}

const cancelPayload = {
    message: "Booking cancelled successfully",
    pnr: PNR,
    status: "CANCELLED",
    waitlistPromoted: true,
    promotedPNR: "RSDEF67890",
    promotedSeatNumber: 3
};

const fetchLog = [];

function jsonResponse(data, ok = true, status = 200) {
    return {
        ok,
        status,
        headers: { get: () => "application/json" },
        json: async () => data
    };
}

async function fetchMock(url, options = {}) {
    const method = (options.method || "GET").toUpperCase();
    fetchLog.push(`${method} ${url}`);

    if (url.startsWith("/api/health")) {
        return jsonResponse({ status: "ok", project: "RailSys" });
    }
    if (url.startsWith("/api/meta")) {
        return jsonResponse({
            stations: ["Wardha", "Nagpur"],
            travelClasses: ["Sleeper", "AC 3 Tier"],
            trainCount: 6
        });
    }
    if (url.startsWith("/api/trains")) {
        return jsonResponse({
            trains: [{
                id: 1, trainNumber: "12001", name: "Demo Express",
                source: "Wardha", destination: "Nagpur"
            }]
        });
    }
    if (url.startsWith("/api/search")) {
        return jsonResponse({
            journeyDate: "2026-10-25",
            count: 1,
            results: [journeyItem]
        });
    }
    if (method === "POST" && url === "/api/bookings") {
        return jsonResponse(createdBooking, true, 201);
    }
    if (method === "POST" && url === `/api/bookings/${PNR}/cancel`) {
        cancelHappened = true;
        return jsonResponse(cancelPayload);
    }
    if (method === "GET" && url === `/api/bookings/${PNR}`) {
        return jsonResponse(currentBookingPayload());
    }

    return jsonResponse({ error: `unmocked ${method} ${url}` }, false, 404);
}

/* ------------------------------------------------------------- sandbox */

const sandbox = {
    document: documentShim,
    window: {
        getSelection() {
            return { removeAllRanges() {}, addRange() {} };
        },
        confirm: () => true
    },
    navigator: {},
    localStorage: {
        _data: {},
        getItem(key) { return this._data[key] ?? null; },
        setItem(key, value) { this._data[key] = String(value); },
        removeItem(key) { delete this._data[key]; }
    },
    fetch: fetchMock,
    console,
    URLSearchParams,
    Date,
    Number,
    Array,
    JSON,
    Math,
    String,
    encodeURIComponent,
    Promise,
    setTimeout: () => 0,   // keep the process from waiting on timers
    clearTimeout: () => {},
    setInterval: () => 0,
    clearInterval: () => {}
};

vm.createContext(sandbox);

const appPath = path.join(__dirname, "..", "public", "app.js");
vm.runInContext(fs.readFileSync(appPath, "utf8"), sandbox, {
    filename: "public/app.js"
});

/* --------------------------------------------------------------- tests */

async function main() {
    console.log("== boot ==");
    check(domReadyHandler !== null, "app.js registered DOMContentLoaded");

    domReadyHandler();

    await new Promise((resolve) => setImmediate(resolve));
    await new Promise((resolve) => setImmediate(resolve));

    const from = documentShim.getElementById("search-from");
    const to = documentShim.getElementById("search-to");
    const travelClass = documentShim.getElementById("search-class");

    check(from.children.length === 3,
        "origin select populated from /api/meta (placeholder + 2)");
    check(travelClass.children.length === 3,
        "class select populated from /api/meta");
    check(documentShim.getElementById("stat-trains").textContent === 6,
        "hero stat shows train count from /api/meta");
    check(documentShim.getElementById("train-list").children.length === 1,
        "train directory rendered 1 train card from /api/trains");
    check(documentShim.getElementById("search-date").value !== "",
        "journey date input defaulted to a real date");
    check(fetchLog.some((entry) => entry === "GET /api/health"),
        "health poll hit /api/health");
    check(documentShim.getElementById("system-status")
        .classList.contains("online"),
        "status pill shows online after successful health check");

    console.log("\n== search ==");
    // Swap button really swaps the two station selects.
    from.value = "Wardha";
    to.value = "Nagpur";
    documentShim.getElementById("swap-stations").dispatch("click");
    check(from.value === "Nagpur" && to.value === "Wardha",
        "swap button exchanges origin and destination");
    from.value = "Wardha";
    to.value = "Nagpur";

    documentShim.getElementById("search-form").dispatch("submit");

    await new Promise((resolve) => setImmediate(resolve));
    await new Promise((resolve) => setImmediate(resolve));

    const resultsSection = documentShim.getElementById("results");
    const resultsList = documentShim.getElementById("results-list");
    check(resultsSection.hidden === false, "results section shown");
    check(resultsList.children.length === 1,
        "one result card rendered for one search hit");
    check(documentShim.getElementById("results-summary")
        .textContent.includes("1 train"),
        "results summary reports the train count");
    check(fetchLog.some((entry) =>
        entry.startsWith("GET /api/search?") &&
        entry.includes("journeyDate=") &&
        entry.includes("source=Wardha") &&
        entry.includes("destination=Nagpur")),
        "search request carried date, source and destination params");

    console.log("\n== booking ==");
    const resultCard = resultsList.children[0];
    const bookButton = resultCard.children[1].children.find(
        (child) => child.tagName === "BUTTON");
    check(bookButton && bookButton.textContent === "Book now",
        "result card offers a Book now button");
    bookButton.dispatch("click");

    check(documentShim.getElementById("booking").hidden === false,
        "booking section opened");
    check(documentShim.getElementById("booking-fare").textContent
        .includes("450"), "fare shown in the booking form");
    check(documentShim.getElementById("journey-summary").children
        .length === 2, "journey summary rendered route + meta");

    // Client-side validation blocks an empty name.
    documentShim.getElementById("passenger-name").value = "";
    documentShim.getElementById("passenger-age").value = "30";
    const fetchCountBeforeInvalid = fetchLog.length;
    documentShim.getElementById("booking-form").dispatch("submit");
    await new Promise((resolve) => setImmediate(resolve));
    check(fetchLog.length === fetchCountBeforeInvalid,
        "invalid form (empty name) does NOT hit the API");
    check(documentShim.getElementById("name-error").hidden === false,
        "inline error shown for empty passenger name");

    // Valid booking goes through.
    documentShim.getElementById("passenger-name").value = "Test User";
    documentShim.getElementById("passenger-age").value = "30";
    documentShim.getElementById("booking-form").dispatch("submit");
    await new Promise((resolve) => setImmediate(resolve));
    await new Promise((resolve) => setImmediate(resolve));

    check(fetchLog.includes("POST /api/bookings"),
        "booking form POSTs /api/bookings");
    check(documentShim.getElementById("confirmation-panel").hidden === false,
        "confirmation panel shown after booking");
    check(documentShim.getElementById("booking-panel").hidden === true,
        "booking form hidden after confirmation");
    check(documentShim.getElementById("confirm-pnr").textContent === PNR,
        "confirmation shows the PNR returned by the API");
    check(documentShim.getElementById("confirm-title").textContent
        .includes("confirmed"), "confirmation title matches CONFIRMED status");
    check(documentShim.getElementById("confirm-details").children
        .length === 8, "confirmation shows all 8 detail rows");
    check(sandbox.localStorage.getItem("railsys.lastPnr") === PNR,
        "PNR persisted to localStorage for refresh recovery");
    check(documentShim.getElementById("lookup-pnr").value === PNR,
        "PNR copied into the manage-booking input");

    console.log("\n== PNR lookup ==");
    documentShim.getElementById("lookup-form").dispatch("submit");
    await new Promise((resolve) => setImmediate(resolve));
    await new Promise((resolve) => setImmediate(resolve));

    check(fetchLog.includes(`GET /api/bookings/${PNR}`),
        "lookup requests the booking by PNR");
    const lookupBox = documentShim.getElementById("lookup-result");
    check(lookupBox.hidden === false && lookupBox.children.length === 1,
        "booking card rendered");
    const bookingCard = lookupBox.children[0];
    const cancelButton = bookingCard.children.find(
        (child) => child.tagName === "DIV" && child.className === "booking-actions"
    )?.children.find((child) => child.textContent === "Cancel booking");
    check(Boolean(cancelButton), "active booking shows a Cancel button");

    console.log("\n== cancellation ==");
    cancelButton.dispatch("click");
    const dialog = documentShim.getElementById("cancel-dialog");
    check(dialog._modalOpen === true,
        "cancel button opens the confirmation dialog");
    check(documentShim.getElementById("cancel-dialog-text").textContent
        .includes(PNR), "dialog text names the PNR being cancelled");

    // User confirms -> dialog close event fires with returnValue cancel.
    dialog.returnValue = "cancel";
    dialog.dispatch("close");
    await new Promise((resolve) => setImmediate(resolve));
    await new Promise((resolve) => setImmediate(resolve));

    check(fetchLog.includes(`POST /api/bookings/${PNR}/cancel`),
        "confirmed dialog triggers the cancel API call");

    const refreshedCard = lookupBox.children[0];
    const statusBadge = refreshedCard.children[0].children.find(
        (child) => child.tagName === "SPAN" &&
            child.className.includes("badge"));
    check(statusBadge && statusBadge.textContent === "CANCELLED",
        "refreshed card shows CANCELLED status badge");
    check(!refreshedCard.children.some(
        (child) => child.className === "booking-actions" &&
        child.children.some((b) => b.textContent === "Cancel booking")),
        "cancelled booking offers no second Cancel button");

    const toastChildren = documentShim
        .getElementById("toast-region").children;
    check(toastChildren.some((t) =>
        t.textContent.includes("promoted")),
        "promotion announced in a toast (promotedPNR + seat)");
    check(documentShim.getElementById("train-list").children.length === 1,
        "train directory refreshed after cancellation");

    console.log(`\n${checksRun - checksFailed} / ${checksRun} ` +
        "frontend smoke checks passed");

    if (checksFailed > 0) {
        process.exit(1);
    }
    console.log("Frontend smoke test passed");
}

main().catch((error) => {
    console.error("Smoke test crashed:", error);
    process.exit(1);
});
