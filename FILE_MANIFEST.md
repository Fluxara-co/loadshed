# File Manifest - C++ Microservice Template

## 📂 Complete File List

### **Headers (`include/microservice/`)**

```
services/
├── api_server.h               154 lines  ✅ Generic API server
├── mqtt_publisher.h           105 lines  ✅ Generic MQTT publisher
├── mqtt_subscriber.h          101 lines  ✅ Generic MQTT subscriber
├── service_manager.h          183 lines  ✅ Service orchestration
└── health_monitor.h           117 lines  ✅ Health monitoring

utils/
├── logger.h                           ✅ Logging interface
└── uuid_utils.h                       ✅ UUID utilities
```

### **Implementation (`src/`)**

```
main.cpp                       200 lines  ✅ Entry point (customize here)

services/
├── api_server.cpp             450 lines  ✅ Generic API implementation
├── mqtt_publisher.cpp         300 lines  ✅ Generic MQTT pub
├── mqtt_subscriber.cpp        350 lines  ✅ Generic MQTT sub
└── service_manager.cpp        450 lines  ✅ Service manager

utils/
└── uuid_utils.cpp                     ✅ UUID implementation

core/
└── (empty - add your logic here)
```

### **Configuration & Build**

```
CMakeLists.txt                         ✅ Root build config
src/CMakeLists.txt                     ✅ Source build config
Dockerfile                             ✅ Container definition
docker-compose.yml                     ✅ Docker compose
.gitignore                             ✅ Git ignore rules
```

### **Configuration**

```
configs/
└── service_config.json                ✅ Service configuration
```

### **Documentation**

```
README.md                              ✅ Main documentation
TEMPLATE_OVERVIEW.md                   ✅ Architecture overview
FILE_MANIFEST.md                       ✅ This file
```

---

## 📊 Statistics

- **Total Headers:** 7 files (~660 lines)
- **Total Implementation:** 6 files (~1750 lines)
- **Build Files:** 5 files
- **Documentation:** 3 files

**Total Code:** ~2410 lines of clean, generic C++ code

---

## 🔑 Key Files to Customize

When creating a new service, you'll mainly work with:

### **1. `src/main.cpp`** (Your Main Entry Point)
- `setupApiEndpoints()` - Register custom API routes
- `setupMqttHandlers()` - Define MQTT message handling
- `setupServiceCallbacks()` - Implement start/stop/reset logic
- Main loop - Your periodic tasks

### **2. `src/core/` & `include/microservice/core/`** (Your Business Logic)
- Create your custom classes here
- Implement domain-specific logic
- Keep it separate from infrastructure

### **3. `configs/service_config.json`** (Configuration)
- API server settings
- MQTT broker settings
- Your custom config

---

## 🚫 Files You Shouldn't Modify

These are infrastructure - leave them as-is:

- `src/services/api_server.cpp`
- `src/services/mqtt_publisher.cpp`
- `src/services/mqtt_subscriber.cpp`
- `src/services/service_manager.cpp`
- All files in `include/microservice/services/`
- All files in `src/utils/` and `include/microservice/utils/`

If you need to modify these, you're probably doing something wrong or need a feature request.

---

## 🎯 Usage Pattern

```cpp
// 1. Create components
auto service_manager = std::make_shared<ServiceManager>();
auto api_server = std::make_shared<ApiServer>();

// 2. Configure
api_server->initialize(8080, "0.0.0.0");

// 3. Register with manager
service_manager->setApiServer(api_server);

// 4. Setup integrations
setupApiEndpoints(api_server, service_manager);
setupServiceCallbacks(service_manager);

// 5. Initialize and start
service_manager->initialize("configs/service_config.json");
service_manager->start();

// 6. Run your main loop
while (!service_manager->isShutdownRequested()) {
    // Your logic here
}

// 7. Shutdown
service_manager->shutdown();
```

---

## 📝 What's NOT Included (By Design)

- Database connectivity (add libpqxx if needed)
- Complex MQTT routing
- Message queuing
- Load balancing
- Service discovery
- Authentication beyond simple API key

These can be added based on your needs!

