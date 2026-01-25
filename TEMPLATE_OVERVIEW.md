# C++ Microservice Template - Overview

A clean, minimal, production-ready C++ microservice shell.

## ✅ What's Included

### **Core Components**

1. **API Server** (`api_server.h/cpp`) - 450 lines
   - HTTP REST API with route registration
   - Built-in service lifecycle endpoints
   - CORS support
   - API key authentication
   - Request statistics
   - Custom route registration

2. **MQTT Publisher** (`mqtt_publisher.h/cpp`) - 300 lines
   - Generic MQTT publishing
   - JSON publishing
   - Batch publishing
   - Connection management
   - Statistics tracking

3. **MQTT Subscriber** (`mqtt_subscriber.h/cpp`) - 350 lines
   - Generic MQTT subscription
   - Topic wildcard support
   - Per-topic callbacks
   - Connection management
   - Statistics tracking

4. **Service Manager** (`service_manager.h/cpp`) - 450 lines
   - Service lifecycle management
   - Component orchestration
   - Health checking
   - Metrics aggregation
   - Custom logic callbacks

5. **Health Monitor** (`health_monitor.h`) - Header only
   - Component health tracking
   - Metrics collection
   - Error tracking
   - Health history

6. **Utilities**
   - `logger.h` - Logging interface
   - `uuid_utils.h/cpp` - UUID generation

---

## 📋 Default API Endpoints

All endpoints are prefixed with `/api/v1`:

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/health` | Simple health check |
| GET | `/service/status` | Full service status |
| GET | `/service/metrics` | Performance metrics |
| POST | `/service/start` | Start service |
| POST | `/service/stop` | Stop service |
| POST | `/service/restart` | Restart service |
| POST | `/service/reset` | Reset service state |
| GET | `/api/info` | API version info |
| GET | `/endpoints` | List all registered endpoints |

---

## 🔧 How to Use

### **1. Add Custom Business Logic**

Create your modules in `src/core/` and `include/microservice/core/`:

```cpp
// include/microservice/core/my_processor.h
#pragma once
namespace microservice {
    class MyProcessor {
    public:
        void processData(const std::string& data);
    };
}

// src/core/my_processor.cpp
#include "microservice/core/my_processor.h"
void MyProcessor::processData(const std::string& data) {
    // Your logic here
}
```

### **2. Register Custom API Endpoints**

In `main.cpp` → `setupApiEndpoints()`:

```cpp
// Add custom endpoint
api_server->registerGetRoute("/custom/data",
    [](const Request& req, Response& res) {
        json response = {
            {"data", "your custom data"},
            {"status", "ok"}
        };
        res.set_content(response.dump(), "application/json");
    });
```

### **3. Handle MQTT Messages**

In `main.cpp` → `setupMqttHandlers()`:

```cpp
mqtt_subscriber->subscribe("device/+/data",
    [](const std::string& topic, const std::string& payload) {
        // Process incoming message
        auto data = json::parse(payload);
        // Your logic here
    });
```

### **4. Implement Service Callbacks**

In `main.cpp` → `setupServiceCallbacks()`:

```cpp
service_manager->setCustomStartLogic([]() -> bool {
    // Your initialization logic
    return true;
});

service_manager->setCustomStopLogic([]() -> bool {
    // Your cleanup logic
    return true;
});
```

---

## 🏗️ Architecture

```
┌─────────────────────────────────────────────────────┐
│                   main.cpp                          │
│  • Creates components                               │
│  • Registers API routes                             │
│  • Sets up MQTT handlers                            │
│  • Implements custom callbacks                      │
└──────────────┬──────────────────────────────────────┘
               │
    ┌──────────┴──────────┐
    │  ServiceManager     │ ◄── Orchestrates everything
    │  • Lifecycle        │
    │  • Health           │
    │  • Metrics          │
    └──────────┬──────────┘
               │
    ┌──────────┴───────────────────────────────┐
    │                                          │
┌───▼──────┐  ┌──────▼──────┐  ┌──────▼──────┐
│ApiServer │  │ MQTT Pub/Sub│  │HealthMonitor│
│ • Routes │  │ • Publish   │  │ • Checks    │
│ • Stats  │  │ • Subscribe │  │ • Metrics   │
└──────────┘  └─────────────┘  └─────────────┘
```

---

## 📦 Dependencies

- **C++17** or later
- **CMake** 3.16+
- **httplib** - HTTP server (header-only)
- **Paho MQTT C++** - MQTT client
- **spdlog** - Logging (header-only)
- **nlohmann/json** - JSON (header-only)

---

## 🚀 Build & Run

```bash
# Build
mkdir build && cd build
cmake ..
make

# Run
./bin/microservice_template

# Test
curl http://localhost:8080/api/v1/health
```

---

## 📁 Directory Structure

```
cpp-microservice-template/
├── src/
│   ├── main.cpp              ← Customize here
│   ├── core/                 ← Your business logic
│   ├── services/             ← Generic services (don't modify)
│   └── utils/                ← Utilities
│
├── include/microservice/
│   ├── services/             ← Service headers
│   ├── core/                 ← Your custom headers
│   └── utils/                ← Utility headers
│
├── configs/
│   └── service_config.json   ← Configuration
│
└── CMakeLists.txt
```

---

## 🎯 Use Cases

Use this template to build:
- Device health monitors
- Data processors
- Protocol converters  
- Event handlers
- Aggregation services
- Communication bridges
- **ANY C++ microservice!**

---

## 💡 Key Design Principles

✅ **Minimal** - Only essential components  
✅ **Generic** - No domain-specific logic  
✅ **Extensible** - Easy to add functionality  
✅ **Production-ready** - Proper error handling, logging, metrics  
✅ **Documented** - Clear examples and comments  
✅ **Clean** - Modern C++17, RAII, PIMPL pattern  

---

## 🔍 Next Steps

1. Copy this template for your new service
2. Rename `microservice_template` to your service name
3. Implement your logic in `src/core/`
4. Register custom endpoints in `main.cpp`
5. Build and deploy!

