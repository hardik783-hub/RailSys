
# 🚆 RailSys — Railway Reservation System

A full-stack railway reservation system built with **C++17, Crow, SQLite, HTML, CSS, and JavaScript**. RailSys provides train search, seat availability, ticket booking, PNR lookup, cancellation, and waitlist management through a responsive web interface and REST API.

> **Project status:** Working local prototype using demo train data. It does not connect to live Indian Railways systems.

## ✨ Features

- 🔎 **Train Search** — Search for trains using journey details.
- 💺 **Seat Availability** — Check available seats for a journey.
- 🎫 **Ticket Booking** — Create reservations and generate PNRs.
- 🔍 **PNR Lookup** — Retrieve booking details using a PNR.
- ❌ **Ticket Cancellation** — Cancel an existing reservation.
- ⏳ **Waitlist Management** — Automatically promote eligible waitlisted bookings when seats become available.
- 🌐 **REST API** — HTTP endpoints for train search, availability, bookings, and cancellations.
- 🗃️ **SQLite Database** — Persistent local storage for application data.
- 📱 **Responsive Frontend** — HTML, CSS, and JavaScript interface.
- 🧩 **Modular Architecture** — Separate models, repositories, services, routes, and database components.
- 🧪 **Automated Tests** — Unit, API, and frontend smoke tests.

## 🛠️ Tech Stack

| Component | Technology |
|---|---|
| Backend | C++17 |
| HTTP Framework | Crow |
| Database | SQLite |
| Frontend | HTML5, CSS3, JavaScript |
| Build System | CMake |
| Testing | CTest, C++ tests, Python API tests, JavaScript smoke tests |
| Version Control | Git and GitHub |

## 🏗️ Architecture

```text
                   ┌─────────────────────┐
                   │   Web Frontend      │
                   │ HTML / CSS / JS     │
                   └──────────┬──────────┘
                              │ HTTP
                   ┌──────────▼──────────┐
                   │    Crow HTTP Server │
                   │     REST API Routes │
                   └──────────┬──────────┘
                              │
                   ┌──────────▼──────────┐
                   │   Service Layer     │
                   │ Booking / Availability
                   └──────────┬──────────┘
                              │
                   ┌──────────▼──────────┐
                   │ Repository Layer    │
                   │ Data Access         │
                   └──────────┬──────────┘
                              │
                   ┌──────────▼──────────┐
                   │   SQLite Database   │
                   └─────────────────────┘
```

### Project Structure

```text
RailSys/
├── include/
│   ├── app/             # Errors and validation
│   ├── database/        # Database, statements, transactions
│   ├── models/          # Train, booking, journey and availability
│   ├── repositories/    # Database access layer
│   ├── routes/          # API and static routes
│   └── services/        # Booking and availability logic
├── src/                 # C++ implementations and application entry point
├── public/
│   ├── index.html        # Web interface
│   ├── style.css         # Styling
│   └── app.js            # Frontend behavior
├── sql/
│   └── schema.sql        # Database schema
├── tests/                # Automated tests
├── third_party/
│   └── Crow/             # Crow Git submodule
├── CMakeLists.txt
├── .gitmodules
└── README.md
```

## 🚀 Getting Started

### Prerequisites

- Linux, Ubuntu, or WSL
- A C++17-compatible compiler
- CMake
- Git
- SQLite development libraries
- Required dependencies for the Crow submodule

### 1. Clone the Repository

```bash
git clone --recurse-submodules https://github.com/hardik783-hub/RailSys.git
cd RailSys
```

If you cloned the repository without its submodules, initialize Crow with:

```bash
git submodule update --init --recursive
```

### 2. Build the Project

```bash
cmake -S . -B build
cmake --build build -j$(nproc)
```

### 3. Run RailSys

```bash
./build/railsys
```

Open the application in your browser:

**http://127.0.0.1:18080/**

Keep the terminal running while using the application.

> If the build reports missing dependencies, install the packages required by your `CMakeLists.txt` and Crow configuration.

## 🔌 REST API

The backend exposes HTTP endpoints for the core reservation workflows.

| Method | Endpoint | Purpose |
|---|---|---|
| `GET` | `/api/health` | Check server health |
| `GET` | `/api/trains` | Retrieve train data |
| `GET` | `/api/availability` | Check seat availability |
| `GET` | `/api/search` | Search trains |
| `GET` | `/api/meta` | Retrieve application metadata |
| `POST` | `/api/bookings` | Create a booking |
| `GET` | `/api/bookings/{PNR}` | Retrieve booking details |
| `POST` | `/api/bookings/{PNR}/cancel` | Cancel a booking |

### Example: Health Check

```bash
curl http://127.0.0.1:18080/api/health
```

### Example: Retrieve Trains

```bash
curl http://127.0.0.1:18080/api/trains
```

### Example: Check Availability

```bash
curl "http://127.0.0.1:18080/api/availability"
```

> **Note:** Query parameters, request bodies, and response schemas depend on the implemented route definitions. Consult `src/routes/ApiRoutes.cpp` for the exact contract before integrating another client.

## 🧪 Testing

Run the C++ test suite through CTest:

```bash
ctest --test-dir build --output-on-failure
```

Run the C++ test executable directly:

```bash
./build/railsys_tests
```

Run the frontend smoke tests:

```bash
node tests/frontend_smoke_test.js
```

Run the end-to-end API tests, if the Python test dependencies are installed:

```bash
python3 tests/e2e_api_test.py
```

The end-to-end tests may start their own test server and use an isolated database. Follow any prerequisites documented in the test file.

## 🔐 Data and Limitations

- RailSys is a local demonstration project and uses demo train data.
- It does not provide live train schedules, live seat availability, real ticket issuance, or integration with official Indian Railways services.
- SQLite stores application data locally.
- Use test passenger information only.
- Do not commit database files, credentials, or personal information.

## 🗺️ Future Improvements

- Authentication and role-based access control
- Improved journey planning and train filtering
- Booking history and passenger profiles
- Deployment using Docker
- CI/CD pipeline with automated testing
- Cloud deployment and monitoring
- Integration with a legitimate live railway data provider, where access is available

## 👨‍💻 Author

**Hardik Garg**

- GitHub: [@hardik783-hub](https://github.com/hardik783-hub)

## 📄 License

Add a `LICENSE` file if you intend to distribute this project publicly. Ensure the selected license is compatible with all dependencies, including the Crow submodule.

---

<p align="center">
  Built with C++ and a focus on clean architecture, reliable booking workflows, and practical backend engineering.
</p>
