# LoadShed Service

A production-ready C++ microservice for load shedding management with HTTP API, MQTT connectivity, and service lifecycle management.

## Features

✅ **HTTP REST API Server** - Built-in API server with route registration  
✅ **MQTT Publisher/Subscriber** - Full MQTT client support  
✅ **Service Management** - Graceful startup and shutdown  
✅ **Logging** - Structured logging system  
✅ **Thread-safe** - Safe multi-threading primitives  
✅ **Docker Ready** - Dockerfile included  
✅ **Extensible** - Easy to add custom business logic  

## Directory Structure

```
loadshed/
├── src/
│   ├── main.cpp              # Application entry point (customize here)
│   ├── services/             # Copied infrastructure services
│   ├── utils/                # Utility functions
│   └── core/                 # Your custom business logic goes here
│
├── include/microservice/
│   ├── services/             # Service headers
│   ├── utils/                # Utility headers
│   └── core/                 # Your custom headers go here
│
├── configs/                  # Configuration files
├── docs/                     # Documentation
├── tests/                    # Unit tests
├── CMakeLists.txt            # Build configuration
└── Dockerfile                # Container definition
```

## Quick Start

### 1. Build

```bash
mkdir build && cd build
cmake ..
make
```

### 2. Run

```bash
./loadshed
```

### 3. Test API

```bash
curl http://localhost:8080/health
```

### 4. Test MQTT (if configured)

```bash
mosquitto_pub -h localhost -t "test/topic" -m "Hello from MQTT"
```

## Customize for Your Use Case

### Add Your Business Logic

1. **Create your custom modules** in `src/core/` and `include/microservice/core/`
2. **Edit `main.cpp`** - look for `TODO` comments
3. **Register API routes** in the `register_api_routes()` function
4. **Subscribe to MQTT topics** in the `subscribe_to_topics()` function
5. **Implement main loop logic** in the `run()` function

### Example: Adding a Health Check Endpoint

```cpp
void register_api_routes() {
    api_server_->register_route("GET", "/health", [](const Request& req) {
        json response = {
            {"status", "healthy"},
            {"uptime", get_uptime()},
            {"timestamp", get_timestamp()}
        };
        return Response{200, response.dump(), "application/json"};
    });
}
```

### Example: Processing MQTT Messages

```cpp
void subscribe_to_topics() {
    mqtt_subscriber_->subscribe("device/+/data", [this](const std::string& topic, const std::string& payload) {
        // Parse payload
        auto data = json::parse(payload);
        
        // Process message
        process_device_data(topic, data);
        
        // Publish response
        mqtt_publisher_->publish("system/response", response.dump());
    });
}
```

## Configuration

Create a `configs/service_config.json` file:

```json
{
  "api_server": {
    "host": "0.0.0.0",
    "port": 8080
  },
  "mqtt": {
    "broker_host": "localhost",
    "broker_port": 1883,
    "client_id": "my_microservice"
  },
  "logging": {
    "level": "info",
    "file": "/var/log/microservice.log"
  }
}
```

## Dependencies

- **C++17** or later
- **CMake** 3.10+
- **libmosquitto** (MQTT)
- **httplib** (HTTP server)
- **nlohmann/json** (JSON parsing)
- **PostgreSQL** (optional, for database)

## Docker

Build the Docker image:

```bash
docker build -t my-microservice .
```

Run the container:

```bash
docker run -p 8080:8080 -p 1883:1883 my-microservice
```

## Use Cases

This template can be used to build:

- **Device Health Monitors**
- **Data Processing Services**
- **Protocol Converters**
- **Event Processors**
- **Aggregation Services**
- **Notification Services**
- **Any C++ microservice!**

## Architecture

```
┌─────────────────────────────────────┐
│     Your Business Logic             │
│   (Implement in src/core/)          │
└──────────────┬──────────────────────┘
               │
┌──────────────┴──────────────────────┐
│   Microservice Template Shell       │
│                                     │
│  ┌─────────────┐  ┌──────────────┐ │
│  │  API Server │  │     MQTT     │ │
│  │   (HTTP)    │  │  Pub/Sub     │ │
│  └─────────────┘  └──────────────┘ │
│                                     │
│  ┌─────────────┐  ┌──────────────┐ │
│  │   Logging   │  │   Service    │ │
│  │             │  │  Management  │ │
│  └─────────────┘  └──────────────┘ │
└─────────────────────────────────────┘
```

## Next Steps

1. Implement your business logic in `src/core/`
2. Update configuration
3. Build and run!

## License

Use freely for any project.

