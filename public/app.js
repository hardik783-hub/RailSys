/* ==========================================================================
   RailSys frontend
   All data comes from the live backend API (/api/*). There is no fake or
   hardcoded booking data anywhere in this file.
   ========================================================================== */

"use strict";

const $ = (id) => document.getElementById(id);

/* --------------------------------------------------------------- helpers */

async function api(url, options = {}) {
    const response = await fetch(url, options);
    const data = await response.json().catch(() => ({}));

    if (!response.ok) {
        throw new Error(data.error || `Request failed (${response.status})`);
    }

    return data;
}

function el(tag, className, text) {
    const node = document.createElement(tag);
    if (className) node.className = className;
    if (text !== undefined) node.textContent = text;
    return node;
}

function money(value) {
    return `₹${Number(value).toLocaleString("en-IN", {
        minimumFractionDigits: 0,
        maximumFractionDigits: 2
    })}`;
}

// "2026-11-01" -> "01 Nov 2026" (no timezone shifting).
function formatDate(isoDate) {
    const match = /^(\d{4})-(\d{2})-(\d{2})$/.exec(isoDate || "");
    if (!match) return isoDate || "";

    const months = ["Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"];

    return `${match[3]} ${months[Number(match[2]) - 1]} ${match[1]}`;
}

function isoToday(offsetDays = 0) {
    const date = new Date();
    date.setDate(date.getDate() + offsetDays);
    const pad = (n) => String(n).padStart(2, "0");
    return `${date.getFullYear()}-${pad(date.getMonth() + 1)}-${pad(date.getDate())}`;
}

// Inline SVG icons reused in dynamic content.
const icons = {
    arrow: `<svg class="route-arrow" viewBox="0 0 24 24" width="18" height="18" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M5 12h14m0 0-5-5m5 5-5 5"/></svg>`,
    search: `<svg viewBox="0 0 24 24" width="24" height="24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><circle cx="11" cy="11" r="7"/><path d="m20 20-3.5-3.5"/></svg>`,
    warn: `<svg viewBox="0 0 24 24" width="24" height="24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M12 9v4m0 4h.01M10.3 3.9 2.4 17.5A2 2 0 0 0 4.1 20.5h15.8a2 2 0 0 0 1.7-3L13.7 3.9a2 2 0 0 0-3.4 0Z"/></svg>`
};

/* --------------------------------------------------------------- toasts */

function toast(message, type = "info") {
    const region = $("toast-region");
    const node = el("div", `toast ${type}`, message);
    region.append(node);

    setTimeout(() => {
        node.classList.add("leaving");
        node.addEventListener("animationend", () => node.remove(),
            { once: true });
        // Safety net if animations are disabled.
        setTimeout(() => node.remove(), 500);
    }, 4500);
}

/* ----------------------------------------------------------- state + boot */

const state = {
    meta: null,          // { stations, travelClasses, trainCount }
    trains: [],
    results: [],         // last search results
    selectedJourney: null, // availability object chosen for booking
    activeSearch: null,  // last search params, for silent refreshes
    pendingCancelPnr: null
};

const LAST_PNR_KEY = "railsys.lastPnr";

function boot() {
    setupNavigation();
    setupSearchForm();
    setupBookingForm();
    setupLookup();
    setupConfirmationActions();
    setupTrainsSection();

    // Sensible date bounds: the demo schedule covers the next 10 days.
    const dateInput = $("search-date");
    dateInput.min = isoToday();
    dateInput.value = isoToday(1);
    dateInput.max = isoToday(30);

    loadMeta();
    loadTrains();
    pollHealth();
    setInterval(pollHealth, 30000);

    // Restore the most recent PNR so a page refresh keeps management easy.
    const savedPnr = localStorage.getItem(LAST_PNR_KEY);
    if (savedPnr) {
        $("lookup-pnr").value = savedPnr;
        lookupBooking(savedPnr);
    }
}

/* ------------------------------------------------------------- health pill */

async function pollHealth() {
    const pill = $("system-status");

    try {
        await api("/api/health");
        pill.classList.add("online");
        pill.classList.remove("offline");
        pill.querySelector(".status-text").textContent = "System online";
    } catch (_) {
        pill.classList.add("offline");
        pill.classList.remove("online");
        pill.querySelector(".status-text").textContent = "System offline";
    }
}

/* ------------------------------------------------------------------- meta */

async function loadMeta() {
    try {
        const meta = await api("/api/meta");
        state.meta = meta;

        fillSelect($("search-from"), meta.stations, "Any origin");
        fillSelect($("search-to"), meta.stations, "Any destination");
        fillSelect($("search-class"), meta.travelClasses, "Any class");

        $("stat-trains").textContent = meta.trainCount;
        $("stat-stations").textContent = meta.stations.length;
        $("stat-classes").textContent = meta.travelClasses.length;
    } catch (error) {
        // Non-fatal: the selects simply stay empty and the user can
        // still use the train directory below.
        console.warn("Could not load /api/meta:", error);
    }
}

function fillSelect(select, values, placeholder) {
    select.replaceChildren();

    const placeholderOption = document.createElement("option");
    placeholderOption.value = "";
    placeholderOption.textContent = placeholder;
    select.append(placeholderOption);

    for (const value of values) {
        const option = document.createElement("option");
        option.value = value;
        option.textContent = value;
        select.append(option);
    }
}

/* -------------------------------------------------------------- navigation */

function setupNavigation() {
    const toggle = $("nav-toggle");
    const nav = $("main-nav");

    toggle.addEventListener("click", () => {
        const open = nav.classList.toggle("open");
        toggle.setAttribute("aria-expanded", String(open));
    });

    nav.addEventListener("click", (event) => {
        if (event.target.closest("a")) {
            nav.classList.remove("open");
            toggle.setAttribute("aria-expanded", "false");
        }
    });
}

/* ----------------------------------------------------------------- search */

function setupSearchForm() {
    $("swap-stations").addEventListener("click", () => {
        const from = $("search-from");
        const to = $("search-to");
        [from.value, to.value] = [to.value, from.value];
    });

    $("search-form").addEventListener("submit", (event) => {
        event.preventDefault();
        runSearch({ scroll: true });
    });
}

async function runSearch({ scroll = false } = {}) {
    const date = $("search-date").value;

    if (!date) {
        toast("Please choose a journey date.", "error");
        $("search-date").focus();
        return;
    }

    const params = new URLSearchParams({ journeyDate: date });
    const from = $("search-from").value;
    const to = $("search-to").value;
    const travelClass = $("search-class").value;

    if (from) params.set("source", from);
    if (to) params.set("destination", to);
    if (travelClass) params.set("travelClass", travelClass);

    state.activeSearch = { from, to, date, travelClass };

    const section = $("results");
    const list = $("results-list");

    section.hidden = false;
    list.setAttribute("aria-busy", "true");
    $("results-summary").textContent = "Searching trains…";
    list.replaceChildren(...skeletonCards(3));

    if (scroll) {
        section.scrollIntoView({ behavior: "smooth", block: "start" });
    }

    try {
        const data = await api(`/api/search?${params}`);
        state.results = data.results || [];
        renderResults(state.results);
    } catch (error) {
        $("results-summary").textContent = "Search failed";
        list.replaceChildren(errorState(
            "Could not search trains",
            error.message,
            () => runSearch({ scroll: false })
        ));
    } finally {
        list.setAttribute("aria-busy", "false");
    }
}

async function refreshSearchQuietly() {
    if (state.activeSearch && !$("results").hidden) {
        try {
            const params = new URLSearchParams({
                journeyDate: state.activeSearch.date
            });
            if (state.activeSearch.from) {
                params.set("source", state.activeSearch.from);
            }
            if (state.activeSearch.to) {
                params.set("destination", state.activeSearch.to);
            }
            if (state.activeSearch.travelClass) {
                params.set("travelClass", state.activeSearch.travelClass);
            }

            const data = await api(`/api/search?${params}`);
            state.results = data.results || [];
            renderResults(state.results);

            // Keep the booking summary in sync with fresh numbers.
            if (state.selectedJourney) {
                const updated = state.results.find(
                    (item) => item.trainNumber === state.selectedJourney.trainNumber
                );
                if (updated) {
                    state.selectedJourney = updated;
                    renderJourneySummary(updated);
                }
            }
        } catch (_) {
            /* silent refresh must never break the UI */
        }
    }
}

function renderResults(results) {
    const list = $("results-list");
    const summary = $("results-summary");
    list.replaceChildren();

    if (results.length === 0) {
        summary.textContent = "No trains found";
        list.append(emptyState(
            "No trains on this route",
            "Try a different date, class, or station pair. " +
            "The demo schedule covers trains between the seeded " +
            "stations over the next several days."
        ));
        return;
    }

    const seats = results.reduce((sum, item) => sum + item.availableSeats, 0);
    const waiting = results.reduce((sum, item) => sum + item.waitlistedCount, 0);

    summary.textContent =
        `${results.length} train${results.length === 1 ? "" : "s"} · ` +
        `${seats} seat${seats === 1 ? "" : "s"} available` +
        (waiting > 0 ? ` · ${waiting} waitlisted` : "");

    for (const item of results) {
        list.append(resultCard(item));
    }
}

function resultCard(item) {
    const card = el("article", "result-card");

    // Left side -------------------------------------------------------
    const left = el("div");

    const top = el("div", "result-top");
    top.append(
        el("span", "badge info", item.trainNumber),
        el("span", "chip", item.travelClass)
    );

    const name = el("h3", "result-name", item.trainName);

    const route = el("div", "result-route");
    route.append(
        el("span", undefined, item.source),
        iconSpan(icons.arrow),
        el("span", undefined, item.destination)
    );

    const meta = el("div", "result-meta");
    meta.append(
        el("span", "chip", `📅 ${formatDate(item.journeyDate)}`),
        el("span", "chip", `${item.capacity} seats per coach`)
    );

    left.append(top, name, route, meta);

    // Right side ------------------------------------------------------
    const side = el("div", "result-side");

    const fare = el("div", "result-fare");
    fare.append(
        document.createTextNode(money(item.baseFareRupees)),
        el("small", undefined, " base fare")
    );

    const availability = el("div", "availability");
    const meter = el("div", "meter");
    const fill = el("div", "meter-fill");

    const percentUsed = item.totalSeats > 0
        ? Math.round((item.confirmedSeats / item.totalSeats) * 100)
        : 0;

    fill.style.width = `${Math.min(percentUsed, 100)}%`;
    if (item.isFull) fill.classList.add("full");
    else if (percentUsed >= 67) fill.classList.add("warn");

    meter.append(fill);

    const availabilityText = el("div", "availability-text");
    availabilityText.append(
        el("span", undefined,
            item.isFull
                ? `Coach full · ${item.waitlistedCount} waitlisted`
                : `${item.availableSeats} of ${item.totalSeats} seats left`)
    );

    availability.append(meter, availabilityText);

    const button = el("button",
        item.isFull ? "btn btn-secondary" : "btn btn-primary",
        item.isFull ? "Join waitlist" : "Book now");
    button.type = "button";
    button.addEventListener("click", () => openBooking(item));

    side.append(fare, availability, button);

    card.append(left, side);
    return card;
}

function iconSpan(svg) {
    const span = document.createElement("span");
    span.innerHTML = svg;
    return span;
}

function skeletonCards(count) {
    return Array.from({ length: count }, () => {
        const card = el("div", "skeleton-card");
        card.append(
            el("div", "skeleton-line w25"),
            el("div", "skeleton-line w60 h24"),
            el("div", "skeleton-line w40"),
            el("div", "skeleton-line w60")
        );
        return card;
    });
}

function emptyState(title, message) {
    const box = el("div", "empty-state");
    const icon = el("div", "icon");
    icon.innerHTML = icons.search;
    box.append(icon, el("h3", undefined, title), el("p", undefined, message));
    return box;
}

function errorState(title, message, onRetry) {
    const box = el("div", "error-state");
    const icon = el("div", "icon");
    icon.innerHTML = icons.warn;
    box.append(icon, el("h3", undefined, title), el("p", undefined, message));

    if (onRetry) {
        const retry = el("button", "btn btn-secondary", "Try again");
        retry.type = "button";
        retry.addEventListener("click", onRetry);
        box.append(retry);
    }

    return box;
}

/* ---------------------------------------------------------------- booking */

function openBooking(item) {
    state.selectedJourney = item;

    const section = $("booking");
    section.hidden = false;
    $("booking-panel").hidden = false;
    $("confirmation-panel").hidden = true;

    renderJourneySummary(item);
    $("booking-fare").textContent = money(item.baseFareRupees);

    section.scrollIntoView({ behavior: "smooth", block: "start" });
    $("passenger-name").focus({ preventScroll: true });
}

function renderJourneySummary(item) {
    const summary = $("journey-summary");
    summary.replaceChildren();

    const route = el("div", "summary-route");
    route.append(
        el("span", undefined, item.source),
        iconSpan(icons.arrow),
        el("span", undefined, item.destination)
    );

    const meta = el("div", "summary-meta");
    meta.append(
        el("span", undefined, `Train ${item.trainNumber} · ${item.trainName}`),
        el("span", undefined, formatDate(item.journeyDate)),
        el("span", undefined, item.travelClass),
        el("span", undefined,
            item.isFull
                ? `Full · ${item.waitlistedCount} waitlisted`
                : `${item.availableSeats} of ${item.totalSeats} seats open`)
    );

    summary.append(route, meta);
}

function setupBookingForm() {
    const form = $("booking-form");

    // Client-side validation helpers shown next to each field.
    function setFieldError(inputId, errorId, message) {
        const input = $(inputId);
        const errorNode = $(errorId);

        if (message) {
            errorNode.textContent = message;
            errorNode.hidden = false;
            input.setAttribute("aria-invalid", "true");
        } else {
            errorNode.hidden = true;
            input.removeAttribute("aria-invalid");
        }
    }

    form.addEventListener("submit", async (event) => {
        event.preventDefault();

        if (!state.selectedJourney) {
            toast("Choose a train from the search results first.", "error");
            return;
        }

        const name = $("passenger-name").value.trim();
        const ageRaw = $("passenger-age").value.trim();
        const age = Number(ageRaw);

        let valid = true;

        if (!name) {
            setFieldError("passenger-name", "name-error",
                "Passenger name is required.");
            valid = false;
        } else if (name.length > 80) {
            setFieldError("passenger-name", "name-error",
                "Name must be 80 characters or fewer.");
            valid = false;
        } else {
            setFieldError("passenger-name", "name-error", null);
        }

        if (!ageRaw || !Number.isInteger(age) || age < 0 || age > 120) {
            setFieldError("passenger-age", "age-error",
                "Enter an age between 0 and 120.");
            valid = false;
        } else {
            setFieldError("passenger-age", "age-error", null);
        }

        if (!valid) return;

        const submit = $("booking-submit");
        submit.classList.add("loading");
        submit.disabled = true;

        try {
            const booking = await api("/api/bookings", {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify({
                    trainNumber: state.selectedJourney.trainNumber,
                    journeyDate: state.selectedJourney.journeyDate,
                    travelClass: state.selectedJourney.travelClass,
                    passengerName: name,
                    passengerAge: age
                })
            });

            showConfirmation(booking);

            localStorage.setItem(LAST_PNR_KEY, booking.pnr);
            $("lookup-pnr").value = booking.pnr;

            form.reset();
            toast(
                booking.status === "CONFIRMED"
                    ? `Booking confirmed! PNR ${booking.pnr}`
                    : `Added to waitlist. PNR ${booking.pnr}`,
                "success"
            );

            // Availability changed: refresh the visible search results.
            refreshSearchQuietly();
            loadTrains();
        } catch (error) {
            toast(error.message, "error");
        } finally {
            submit.classList.remove("loading");
            submit.disabled = false;
        }
    });
}

function showConfirmation(booking) {
    const panel = $("confirmation-panel");
    const waitlisted = booking.status === "WAITLISTED";

    panel.classList.toggle("waitlisted", waitlisted);
    panel.hidden = false;
    $("booking-panel").hidden = true;

    $("confirm-title").textContent = waitlisted
        ? "Added to the waitlist"
        : "Booking confirmed";
    $("confirm-sub").textContent = waitlisted
        ? "The coach is full right now. If a seat is freed, you will be " +
          "promoted automatically — check your PNR any time."
        : "Your seat is reserved. Keep the PNR handy to manage or " +
          "cancel this booking.";
    $("confirm-pnr").textContent = booking.pnr;

    const details = $("confirm-details");
    details.replaceChildren();

    const rows = [
        ["Train", `${booking.trainNumber} · ${booking.trainName}`],
        ["Route", `${booking.source} → ${booking.destination}`],
        ["Journey date", formatDate(booking.journeyDate)],
        ["Travel class", booking.travelClass],
        ["Passenger", `${booking.passengerName} (${booking.passengerAge})`],
        ["Seat", booking.seatNumber != null
            ? `Seat ${booking.seatNumber}` : "Not assigned (waitlist)"],
        ["Status", booking.status],
        ["Fare", money(booking.fareRupees)]
    ];

    for (const [label, value] of rows) {
        const wrapper = el("div");
        wrapper.append(
            Object.assign(el("dt"), { textContent: label }),
            Object.assign(el("dd"), { textContent: value })
        );
        details.append(wrapper);
    }

    panel.scrollIntoView({ behavior: "smooth", block: "center" });
}

function setupConfirmationActions() {
    $("copy-pnr").addEventListener("click", async () => {
        const pnr = $("confirm-pnr").textContent;

        try {
            await navigator.clipboard.writeText(pnr);
            toast(`Copied ${pnr} to clipboard`, "success");
        } catch (_) {
            // Clipboard blocked: select the text so it can be copied.
            const range = document.createRange();
            range.selectNodeContents($("confirm-pnr"));
            const selection = window.getSelection();
            selection.removeAllRanges();
            selection.addRange(range);
            toast("PNR selected — press Ctrl+C to copy.");
        }
    });

    $("manage-new-pnr").addEventListener("click", () => {
        const pnr = $("confirm-pnr").textContent;
        $("lookup-pnr").value = pnr;
        lookupBooking(pnr);
        $("manage").scrollIntoView({ behavior: "smooth", block: "start" });
    });

    $("book-another").addEventListener("click", () => {
        $("booking").hidden = true;
        state.selectedJourney = null;
        $("search").scrollIntoView({ behavior: "smooth", block: "start" });
    });
}

/* ----------------------------------------------------------------- lookup */

function setupLookup() {
    $("lookup-form").addEventListener("submit", (event) => {
        event.preventDefault();
        const pnr = $("lookup-pnr").value.trim().toUpperCase();
        lookupBooking(pnr);
    });
}

async function lookupBooking(pnr) {
    const box = $("lookup-result");
    box.hidden = false;
    box.replaceChildren(...skeletonCards(1));

    if (!pnr) {
        box.replaceChildren(errorState(
            "Enter a PNR", "Please type the PNR from your booking " +
            "confirmation, e.g. RS384B3B9B."));
        return;
    }

    try {
        const booking = await api(`/api/bookings/${encodeURIComponent(pnr)}`);
        localStorage.setItem(LAST_PNR_KEY, booking.pnr);
        box.replaceChildren(bookingCard(booking));
    } catch (error) {
        box.replaceChildren(errorState(
            "Booking not found", error.message, () => lookupBooking(pnr)));
    }
}

function bookingCard(booking) {
    const card = el("article", "booking-card");

    const head = el("div", "booking-card-head");
    const pnrWrap = el("div");
    pnrWrap.append(
        el("div", "pnr-inline", booking.pnr),
        el("div", "results-summary",
            `${booking.source} → ${booking.destination} · ` +
            formatDate(booking.journeyDate))
    );

    const statusClass = {
        CONFIRMED: "success",
        WAITLISTED: "warn",
        CANCELLED: "danger"
    }[booking.status] || "neutral";

    head.append(pnrWrap, el("span", `badge ${statusClass}`, booking.status));

    card.append(head);

    if (booking.status === "WAITLISTED") {
        const note = el("div", "promo-note");
        note.innerHTML = icons.warn;
        note.append(el("span", undefined,
            "This journey is full — you are on the waitlist. " +
            "If a confirmed booking is cancelled, the earliest " +
            "waitlisted passenger is promoted automatically."));
        card.append(note);
    }

    const details = el("dl", "detail-grid");
    const rows = [
        ["Train", `${booking.trainNumber} · ${booking.trainName}`],
        ["Journey date", formatDate(booking.journeyDate)],
        ["Travel class", booking.travelClass],
        ["Passenger", `${booking.passengerName} (${booking.passengerAge})`],
        ["Seat", booking.seatNumber != null
            ? `Seat ${booking.seatNumber}` : "Not assigned"],
        ["Fare", money(booking.fareRupees)]
    ];

    for (const [label, value] of rows) {
        const wrapper = el("div");
        wrapper.append(
            Object.assign(el("dt"), { textContent: label }),
            Object.assign(el("dd"), { textContent: value })
        );
        details.append(wrapper);
    }
    card.append(details);

    const actions = el("div", "booking-actions");

    const refresh = el("button", "btn btn-ghost btn-small", "Refresh");
    refresh.type = "button";
    refresh.addEventListener("click", () => lookupBooking(booking.pnr));
    actions.append(refresh);

    if (booking.status !== "CANCELLED") {
        const cancel = el("button", "btn btn-danger btn-small",
            "Cancel booking");
        cancel.type = "button";
        cancel.addEventListener("click", () => askCancel(booking));
        actions.append(cancel);
    }

    card.append(actions);
    return card;
}

/* -------------------------------------------------------------- cancellation */

const cancelDialog = () => $("cancel-dialog");

function askCancel(booking) {
    state.pendingCancelPnr = booking.pnr;
    $("cancel-dialog-text").textContent =
        `This will cancel booking ${booking.pnr} permanently. If any ` +
        `passenger is waiting for this journey, they will be promoted ` +
        `automatically.`;

    const dialog = cancelDialog();
    if (typeof dialog.showModal === "function") {
        dialog.showModal();
    } else if (window.confirm(`Cancel booking ${booking.pnr}?`)) {
        performCancel(booking.pnr);
    }
}

function setupDialog() {
    const dialog = cancelDialog();

    dialog.addEventListener("close", () => {
        if (dialog.returnValue === "cancel" && state.pendingCancelPnr) {
            performCancel(state.pendingCancelPnr);
        }
        state.pendingCancelPnr = null;
    });
}

async function performCancel(pnr) {
    try {
        const result = await api(
            `/api/bookings/${encodeURIComponent(pnr)}/cancel`,
            { method: "POST" });

        let message = `Booking ${result.pnr} cancelled.`;

        if (result.waitlistPromoted) {
            message +=
                ` Waitlisted booking ${result.promotedPNR} was promoted ` +
                `to seat ${result.promotedSeatNumber}.`;
        }

        toast(message, "success");
        await lookupBooking(result.pnr);
        refreshSearchQuietly();
        loadTrains();
    } catch (error) {
        toast(error.message, "error");
        await lookupBooking(pnr);
    }
}

/* -------------------------------------------------------- train directory */

function setupTrainsSection() {
    $("refresh-trains").addEventListener("click", () => loadTrains());
}

async function loadTrains() {
    const list = $("train-list");
    list.setAttribute("aria-busy", "true");
    list.replaceChildren(...skeletonCards(3));

    try {
        const data = await api("/api/trains");
        state.trains = data.trains || [];

        list.replaceChildren();

        if (state.trains.length === 0) {
            list.append(emptyState(
                "No trains listed",
                "The demo database has no trains yet."));
            return;
        }

        for (const train of state.trains) {
            list.append(trainCard(train));
        }
    } catch (error) {
        list.replaceChildren(errorState(
            "Could not load trains",
            error.message,
            () => loadTrains()));
    } finally {
        list.setAttribute("aria-busy", "false");
    }
}

function trainCard(train) {
    const card = el("article", "train-card");

    const top = el("div", "result-top");
    top.append(
        el("span", "badge info", train.trainNumber),
        el("span", "chip", "Demo service")
    );

    const route = el("div", "train-route");
    route.append(
        el("span", undefined, train.source),
        iconSpan(icons.arrow),
        el("span", undefined, train.destination)
    );

    const button = el("button", "btn btn-secondary btn-small",
        "Search this route");
    button.type = "button";
    button.addEventListener("click", () => {
        $("search-from").value = train.source;
        $("search-to").value = train.destination;
        runSearch({ scroll: true });
    });

    card.append(top, el("h3", undefined, train.name), route, button);
    return card;
}

/* ------------------------------------------------------------------- init */

document.addEventListener("DOMContentLoaded", () => {
    setupDialog();
    boot();
});
