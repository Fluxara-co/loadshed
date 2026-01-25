# Process Manager

A generic, production-ready process lifecycle management system for C++ microservices.

**Adapted from**: `device-modbus-service` SimpleDeviceManager
**Location**: `include/microservice/core/process_manager.h`

---

## Overview

The Process Manager provides comprehensive process lifecycle management with:

- **CRUD Operations**: Create, Read, Update, Delete process definitions
- **Lifecycle Control**: Start, Stop, Restart, Kill processes
- **Health Monitoring**: Automatic health checks with auto-restart
- **RESTful API**: Full HTTP API for remote management
- **Thread-Safe**: Mutex-protected operations
- **Event System**: Callbacks for process events
- **Resource Tracking**: Statistics and metrics

---

## Key Features

### Process Types Supported

- **EXECUTABLE**: Native binary executables
- **SCRIPT**: Shell scripts or interpreted scripts
- **DOCKER**: Docker containers (future)
- **INTERNAL**: Thread-based internal processes

### Lifecycle States

```
UNREGISTERED → REGISTERED → STARTING → RUNNING → STOPPING → STOPPED
                                ↓          ↓
                              FAILED ← CRASHED
                                ↓
                            RESTARTING
```

### Auto-Restart

- Configurable max retry attempts
- Backoff delay between restarts
- Exit code-based restart triggers
- Automatic health monitoring

### Health Monitoring

- PID-based liveness checks (`kill(pid, 0)`)
- Periodic health check intervals
- Custom health check commands (optional)
- Auto-restart on failure

---

## Architecture

### Core Components

```
ProcessManager
├── Process Registry (map<process_id, ProcessInfo>)
├── PID Tracker (map<process_id, pid_t>)
├── Health Monitor (periodic checks)
├── Event System (callbacks)
└── Statistics (counters)
```

### Process Execution Flow

```cpp
// Adapted from SimpleDeviceManager::startDevice()
startProcess(process_id)
  ├── Check dependencies
  ├── fork()
  │   └── Child: execve(command, args, env)
  ├── Parent: Track PID
  └── Update state to RUNNING
```

### Health Check Flow

```cpp
// Adapted from SimpleDeviceManager::checkDeviceHealth()
checkProcessHealth(process_id)
  ├── kill(pid, 0)  // Check if alive
  ├── If dead:
  │   ├── waitpid() to get exit code
  │   ├── Emit "crashed" event
  │   └── Auto-restart if enabled
  └── Update health status
```

---

## Usage

### Basic Example

```cpp
#include "microservice/core/process_manager.h"

// Create Process Manager
auto pm = std::make_shared<ProcessManager>();

// Define a process
ProcessInfo info;
info.name = "My Process";
info.command = "/usr/bin/my-app";
info.args = {"--config", "/etc/my-app.conf"};
info.working_dir = "/opt/my-app";
info.restart_policy.auto_restart = true;
info.restart_policy.max_restart_attempts = 3;

// Add the process
pm->addProcess("my-process-001", info);

// Start it
pm->startProcess("my-process-001");

// Check health periodically
while (true) {
    pm->checkAllProcessHealth();
    std::this_thread::sleep_for(std::chrono::seconds(30));
}

// Stop when done
pm->stopProcess("my-process-001");
```

### From JSON Configuration

```cpp
json config = {
    {"name", "DNP3 Device Handler"},
    {"description", "DNP3 protocol handler for device XYZ"},
    {"type", "executable"},
    {"command", "/usr/local/bin/dnp3-handler"},
    {"args", ["--device-id", "device-001", "--port", "20000"]},
    {"working_dir", "/opt/dnp3"},
    {"environment", {
        {"MQTT_BROKER", "mosquitto:1883"},
        {"LOG_LEVEL", "info"}
    }},
    {"restart_policy", {
        {"auto_restart", true},
        {"max_retries", 5},
        {"backoff_seconds", 2}
    }},
    {"health_check", {
        {"enabled", true},
        {"interval_seconds", 30}
    }},
    {"dependencies", ["mqtt-broker"]},
    {"metadata", {
        {"tags", {
            {"protocol", "dnp3"},
            {"environment", "production"}
        }}
    }}
};

pm->addProcessFromJson("dnp3-device-001", config);
pm->startProcess("dnp3-device-001");
```

### Event Callbacks

```cpp
// Setup event listener
pm->setEventCallback(
    [](const std::string& process_id, const std::string& event, const json& data) {
        if (event == "crashed") {
            // Alert DevOps
            sendAlert("Process " + process_id + " crashed!");
        } else if (event == "restarted") {
            // Log restart
            logInfo("Process " + process_id + " auto-restarted");
        }
    }
);

// Events emitted:
// - "added"
// - "removed"
// - "started"
// - "stopped"
// - "crashed"
// - "restarted"
```

---

## API Endpoints

### CRUD Operations

| Method | Endpoint | Description |
|--------|----------|-------------|
| `POST` | `/api/v1/processes` | Create new process |
| `GET` | `/api/v1/processes` | List all processes |
| `GET` | `/api/v1/processes/:id` | Get specific process |
| `PUT` | `/api/v1/processes/:id` | Update process (must be stopped) |
| `DELETE` | `/api/v1/processes/:id` | Delete process |

### Lifecycle Control

| Method | Endpoint | Description |
|--------|----------|-------------|
| `POST` | `/api/v1/processes/:id/start` | Start process |
| `POST` | `/api/v1/processes/:id/stop` | Stop process (SIGTERM) |
| `POST` | `/api/v1/processes/:id/restart` | Restart process |
| `POST` | `/api/v1/processes/:id/kill` | Force kill (SIGKILL) |

### Health & Status

| Method | Endpoint | Description |
|--------|----------|-------------|
| `GET` | `/api/v1/processes/:id/status` | Get process status |
| `POST` | `/api/v1/processes/:id/health` | Run health check |

### Bulk Operations

| Method | Endpoint | Description |
|--------|----------|-------------|
| `POST` | `/api/v1/processes/bulk/start` | Start multiple processes |
| `POST` | `/api/v1/processes/bulk/stop` | Stop multiple processes |

### Query & Search

| Method | Endpoint | Description |
|--------|----------|-------------|
| `GET` | `/api/v1/processes/search?tag_key=env&tag_value=prod` | Search by tag |
| `GET` | `/api/v1/processes/running` | Get all running processes |
| `GET` | `/api/v1/processes/stopped` | Get all stopped processes |
| `GET` | `/api/v1/processes/stats` | Get statistics |

---

## API Examples

### Create a Process

```bash
curl -X POST http://localhost:8080/api/v1/processes \
  -H "Content-Type: application/json" \
  -d '{
    "process_id": "worker-001",
    "name": "Background Worker",
    "type": "executable",
    "command": "/usr/bin/python3",
    "args": ["/app/worker.py"],
    "restart_policy": {
      "auto_restart": true,
      "max_retries": 3
    }
  }'
```

### Start a Process

```bash
curl -X POST http://localhost:8080/api/v1/processes/worker-001/start
```

### Get Process Status

```bash
curl http://localhost:8080/api/v1/processes/worker-001/status
```

Response:
```json
{
  "success": true,
  "process_id": "worker-001",
  "status": {
    "process_id": "worker-001",
    "name": "Background Worker",
    "pid": 12345,
    "is_running": true,
    "is_healthy": true,
    "restart_count": 0,
    "uptime_seconds": 125
  }
}
```

### List All Processes

```bash
curl http://localhost:8080/api/v1/processes
```

### Stop a Process

```bash
curl -X POST http://localhost:8080/api/v1/processes/worker-001/stop
```

### Bulk Start

```bash
curl -X POST http://localhost:8080/api/v1/processes/bulk/start \
  -H "Content-Type: application/json" \
  -d '{
    "process_ids": ["worker-001", "worker-002", "worker-003"]
  }'
```

### Search by Tag

```bash
curl "http://localhost:8080/api/v1/processes/search?tag_key=environment&tag_value=production"
```

### Get Statistics

```bash
curl http://localhost:8080/api/v1/processes/stats
```

---

## Implementation Details

### Process Spawning (fork + execve)

```cpp
// Adapted from SimpleDeviceManager::startDevice()
bool ProcessManager::forkAndExec(const ProcessInfo& info, pid_t& out_pid) {
    pid_t pid = fork();

    if (pid == 0) {
        // Child process
        chdir(info.working_dir.c_str());

        // Set environment
        for (auto& [key, val] : info.environment) {
            setenv(key.c_str(), val.c_str(), 1);
        }

        // Prepare args
        std::vector<char*> args;
        args.push_back(const_cast<char*>(info.command.c_str()));
        for (auto& arg : info.args) {
            args.push_back(const_cast<char*>(arg.c_str()));
        }
        args.push_back(nullptr);

        // Execute
        execve(info.command.c_str(), args.data(), environ);
        exit(1);  // If execve returns, it failed

    } else if (pid > 0) {
        // Parent process
        out_pid = pid;
        return true;
    }

    return false;  // Fork failed
}
```

### Process Termination

```cpp
// Adapted from SimpleDeviceManager::stopDevice()
bool ProcessManager::terminateProcess(pid_t pid, bool force) {
    // Send SIGTERM for graceful shutdown
    kill(pid, force ? SIGKILL : SIGTERM);

    if (!force) {
        // Wait 5 seconds for graceful shutdown
        std::this_thread::sleep_for(std::chrono::seconds(5));

        // If still alive, force kill
        if (isProcessAlive(pid)) {
            kill(pid, SIGKILL);
        }
    }

    // Wait for process to exit
    int status;
    waitpid(pid, &status, 0);

    return true;
}
```

### Health Monitoring

```cpp
// Adapted from SimpleDeviceManager::checkDeviceHealth()
bool ProcessManager::checkProcessHealth(const std::string& process_id) {
    auto& info = processes_[process_id];

    // Check if process is alive
    if (!isProcessAlive(info.pid)) {
        // Process died unexpectedly
        int exit_code = waitForProcess(info.pid);

        info.is_running = false;
        info.is_healthy = false;
        info.exit_code = exit_code;

        emitEvent(process_id, "crashed", {{"exit_code", exit_code}});

        // Auto-restart if enabled
        if (info.restart_policy.auto_restart &&
            info.restart_count < info.restart_policy.max_restart_attempts) {

            std::this_thread::sleep_for(
                std::chrono::milliseconds(info.restart_policy.restart_delay_ms)
            );

            startProcess(process_id);
            info.restart_count++;
        }

        return false;
    }

    info.is_healthy = true;
    return true;
}
```

---

## Integration with DataRodeo

### Use Cases

1. **Device Protocol Handlers**: Manage DNP3, Modbus, BACnet protocol handlers
2. **Data Processing Workers**: ETL pipelines, aggregation jobs
3. **Service Orchestration**: Coordinated startup of dependent services
4. **Development/Testing**: Spawn test environments

### Example: DNP3 Device Management

```cpp
// In main.cpp
auto process_manager = std::make_shared<ProcessManager>();

// Register with API server
setupProcessManagerEndpoints(api_server, process_manager);

// Auto-start DNP3 devices from config directory
for (auto& config_file : std::filesystem::directory_iterator("/configs/dnp3/")) {
    std::string device_id = extract_device_id(config_file.path());

    ProcessInfo dnp3_process;
    dnp3_process.name = "DNP3 Device " + device_id;
    dnp3_process.command = "/usr/local/bin/dnp3-handler";
    dnp3_process.args = {"--config", config_file.path().string()};
    dnp3_process.restart_policy.auto_restart = true;
    dnp3_process.tags["protocol"] = "dnp3";
    dnp3_process.tags["device_id"] = device_id;

    process_manager->addProcess(device_id, dnp3_process);
    process_manager->startProcess(device_id);
}

// Health monitoring loop
while (!shutdown_requested) {
    process_manager->checkAllProcessHealth();
    std::this_thread::sleep_for(std::chrono::seconds(30));
}
```

---

## Comparison with device-modbus-service

| Feature | device-modbus-service | Process Manager |
|---------|----------------------|-----------------|
| **Scope** | Modbus devices only | Any process type |
| **Configuration** | Hardcoded Modbus config | Generic JSON config |
| **Process Types** | Client + Simulator | Executable, Script, Docker |
| **API** | Device-specific endpoints | Generic process endpoints |
| **Dependencies** | None | Dependency graph support |
| **Tags/Metadata** | No | Yes |
| **Bulk Operations** | No | Yes |
| **Query System** | No | Tag-based search |
| **Statistics** | Basic | Comprehensive |

### Key Improvements

1. **Generic**: Works with any executable, not just Modbus
2. **Flexible**: JSON-based configuration
3. **Powerful**: Bulk operations, search, dependencies
4. **Observable**: Events, stats, comprehensive API
5. **Extensible**: Tags, metadata, callbacks

---

## Building

```bash
cd device-dnp3-service
mkdir build && cd build
cmake ..
make
```

## Running the Example

```bash
./build/bin/process_manager_example
```

Then test with:
```bash
curl http://localhost:8080/api/v1/processes
curl http://localhost:8080/api/v1/processes/stats
```

---

## Future Enhancements

1. **Docker Support**: Full Docker container management
2. **Resource Limits**: CPU/memory enforcement via cgroups
3. **Log Aggregation**: Capture stdout/stderr
4. **Dependency Resolution**: Automatic startup ordering
5. **Health Check Commands**: Custom health check executables
6. **Database Persistence**: Store process configs in PostgreSQL
7. **MQTT Integration**: Publish process events to MQTT
8. **Metrics Export**: Prometheus metrics endpoint

---

## License

Use freely within DataRodeo project.
