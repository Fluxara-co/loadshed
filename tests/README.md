# Process Manager API Tests

Comprehensive Python test suite for the Process Manager REST API.

## Overview

This test suite validates all Process Manager endpoints:
- ✅ **CRUD Operations**: Create, Read, Update, Delete processes
- ✅ **Lifecycle Control**: Start, Stop, Restart processes
- ✅ **Health Monitoring**: Status checks and health validation
- ✅ **Bulk Operations**: Start/stop multiple processes
- ✅ **Query & Search**: Tag-based search, running/stopped filters
- ✅ **Statistics**: Process manager metrics

## Quick Start

### Prerequisites

```bash
# Install Python dependencies
cd tests
pip install -r requirements.txt
```

### Run Tests

```bash
# For DataRodeo stack (port 8092)
python test_process_manager_api.py

# For standalone service (port 8086)
python test_process_manager_api.py --standalone

# Custom URL
python test_process_manager_api.py --url http://localhost:8080
```

## Test Coverage

### 1. CRUD Operations (4 tests)
- ✅ Create Process
- ✅ List All Processes
- ✅ Get Specific Process
- ✅ Delete Process

### 2. Lifecycle Operations (3 tests)
- ✅ Start Process
- ✅ Stop Process
- ✅ Restart Process

### 3. Health & Status (2 tests)
- ✅ Get Process Status
- ✅ Process Health Check

### 4. Bulk Operations (2 tests)
- ✅ Bulk Start Processes
- ✅ Bulk Stop Processes

### 5. Query & Search (3 tests)
- ✅ Search by Tag
- ✅ Get Running Processes
- ✅ Get Stopped Processes

### 6. Statistics (1 test)
- ✅ Get Process Manager Statistics

**Total**: 15 test cases

## Example Output

```
======================================================================
Process Manager API Test Suite
======================================================================
Base URL: http://localhost:8092

Checking service health...
✓ Service is healthy and responding

=== CRUD Operations ===

Testing: Create Process
✓ Process created: test-process-create-001
ℹ Response: {
  "success": true,
  "message": "Process created",
  "process_id": "test-process-create-001"
}

Testing: List All Processes
✓ Retrieved 5 processes

Processes:
  - test-process-create-001: Test Process test-process-create-001 [STOPPED]
  - test-process-get-001: Test Process test-process-get-001 [STOPPED]
  ...

=== Lifecycle Operations ===

Testing: Start Process
✓ Process started: test-process-start-001 (PID: 12345)
ℹ Status: {
  "process_id": "test-process-start-001",
  "pid": 12345,
  "is_running": true,
  "is_healthy": true
}

Testing: Stop Process
✓ Process stopped: test-process-stop-001

=== Health & Status ===

Testing: Get Process Status

Status Details:
  PID: 12346
  Running: True
  Healthy: True
  Uptime: 5s

=== Statistics ===

Testing: Get Statistics
✓ Retrieved statistics

Process Manager Statistics:
  Total Processes: 15
  Running: 8
  Stopped: 7
  Failed: 0
  Total Starts: 20
  Total Stops: 12
  Total Restarts: 3

======================================================================
Test Summary
======================================================================
Passed: 15
Failed: 0
Total: 15
Pass Rate: 100.0%

✓ All tests passed!
```

## Service Setup

### Option 1: DataRodeo Stack

```bash
# From dataRodeo root
cd /path/to/dataRodeo

# Start the service
docker-compose up -d device-dnp3-service

# Check logs
docker-compose logs -f device-dnp3-service

# Run tests
cd device-dnp3-service/tests
python test_process_manager_api.py
```

### Option 2: Standalone

```bash
# From device-dnp3-service directory
cd /path/to/device-dnp3-service

# Start with docker-compose
docker-compose up -d

# OR build and run locally
mkdir build && cd build
cmake ..
make
./bin/loadshed

# Run tests (from device-dnp3-service directory)
cd tests
python test_process_manager_api.py --standalone
```

## Test Configuration

### Environment Variables

You can customize the test behavior:

```bash
# Change base URL
export API_BASE_URL="http://custom-host:8080"
python test_process_manager_api.py

# Or use command line argument
python test_process_manager_api.py --url http://custom-host:8080
```

### Ports

| Mode | Port | URL |
|------|------|-----|
| **DataRodeo Stack** | 8092 | http://localhost:8092 |
| **Standalone** | 8086 | http://localhost:8086 |
| **Local Build** | 8080 | http://localhost:8080 |

## Test Details

### CRUD Tests

```python
# Create Process
POST /api/v1/processes
{
  "process_id": "test-001",
  "name": "Test Process",
  "command": "/bin/sleep",
  "args": ["30"]
}

# List Processes
GET /api/v1/processes

# Get Process
GET /api/v1/processes/test-001

# Delete Process
DELETE /api/v1/processes/test-001
```

### Lifecycle Tests

```python
# Start Process
POST /api/v1/processes/test-001/start

# Stop Process
POST /api/v1/processes/test-001/stop

# Restart Process
POST /api/v1/processes/test-001/restart
```

### Health Tests

```python
# Get Status
GET /api/v1/processes/test-001/status

# Health Check
POST /api/v1/processes/test-001/health
```

### Bulk Operations

```python
# Bulk Start
POST /api/v1/processes/bulk/start
{
  "process_ids": ["test-001", "test-002", "test-003"]
}

# Bulk Stop
POST /api/v1/processes/bulk/stop
{
  "process_ids": ["test-001", "test-002"],
  "force": false
}
```

### Query Tests

```python
# Search by Tag
GET /api/v1/processes/search?tag_key=environment&tag_value=production

# Get Running
GET /api/v1/processes/running

# Get Stopped
GET /api/v1/processes/stopped

# Get Stats
GET /api/v1/processes/stats
```

## Troubleshooting

### Service Not Responding

```bash
# Check if service is running
docker ps | grep device-dnp3-service

# Check service logs
docker logs device-dnp3-service

# Check health directly
curl http://localhost:8092/api/v1/health
```

### Connection Refused

```bash
# Make sure you're using the correct port
# DataRodeo stack: 8092
# Standalone: 8086

# Check if port is open
nc -zv localhost 8092
```

### Tests Failing

```bash
# Clean up test processes
curl -X DELETE http://localhost:8092/api/v1/processes/test-process-*

# Restart the service
docker-compose restart device-dnp3-service

# Re-run tests
python test_process_manager_api.py
```

## Development

### Adding New Tests

```python
def test_my_feature(self):
    """Test my new feature"""
    self._print_test("My Feature")

    # Your test code
    response = self.session.get(self._url("/my/endpoint"))

    if response.status_code == 200:
        self._print_success("Test passed")
    else:
        self._print_error(f"Test failed: {response.status_code}")
```

### Running Individual Tests

Modify `run_all_tests()` to run specific tests:

```python
def run_all_tests(self):
    # Comment out tests you don't want to run
    self.test_create_process()
    # self.test_list_processes()  # Skip this
    self.test_get_process()
    # ...
```

## CI/CD Integration

### GitHub Actions

```yaml
name: API Tests
on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Start Service
        run: docker-compose up -d device-dnp3-service
      - name: Wait for Service
        run: sleep 10
      - name: Run Tests
        run: |
          cd tests
          pip install -r requirements.txt
          python test_process_manager_api.py
```

## Manual Testing

You can also test endpoints manually with curl:

```bash
# Create a process
curl -X POST http://localhost:8092/api/v1/processes \
  -H "Content-Type: application/json" \
  -d '{
    "process_id": "manual-test-001",
    "name": "Manual Test Process",
    "command": "/bin/sleep",
    "args": ["60"]
  }'

# Start it
curl -X POST http://localhost:8092/api/v1/processes/manual-test-001/start

# Check status
curl http://localhost:8092/api/v1/processes/manual-test-001/status | jq

# Stop it
curl -X POST http://localhost:8092/api/v1/processes/manual-test-001/stop

# Delete it
curl -X DELETE http://localhost:8092/api/v1/processes/manual-test-001
```

## License

Use freely within DataRodeo project.
