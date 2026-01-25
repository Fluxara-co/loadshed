#!/usr/bin/env python3
"""
Process Manager API Test Suite

Tests all Process Manager endpoints including:
- CRUD operations
- Lifecycle control
- Health monitoring
- Bulk operations
- Query/search
- Statistics
"""

import requests
import time
import json
import sys
from typing import Dict, Any, Optional

# Configuration
API_BASE_URL = "http://localhost:8092"  # Update for DataRodeo stack
API_PREFIX = "/api/v1"

# Colors for output
class Colors:
    GREEN = '\033[92m'
    RED = '\033[91m'
    YELLOW = '\033[93m'
    BLUE = '\033[94m'
    CYAN = '\033[96m'
    RESET = '\033[0m'
    BOLD = '\033[1m'

class ProcessManagerAPITester:
    def __init__(self, base_url: str = API_BASE_URL):
        self.base_url = base_url.rstrip('/')
        self.session = requests.Session()
        self.test_processes = []  # Track created processes for cleanup
        self.passed = 0
        self.failed = 0

    def _url(self, path: str) -> str:
        """Construct full URL"""
        return f"{self.base_url}{API_PREFIX}{path}"

    def _print_test(self, name: str):
        """Print test name"""
        print(f"\n{Colors.CYAN}Testing: {Colors.BOLD}{name}{Colors.RESET}")

    def _print_success(self, message: str):
        """Print success message"""
        print(f"{Colors.GREEN}✓ {message}{Colors.RESET}")
        self.passed += 1

    def _print_error(self, message: str):
        """Print error message"""
        print(f"{Colors.RED}✗ {message}{Colors.RESET}")
        self.failed += 1

    def _print_info(self, message: str):
        """Print info message"""
        print(f"{Colors.BLUE}ℹ {message}{Colors.RESET}")

    def _print_warning(self, message: str):
        """Print warning message"""
        print(f"{Colors.YELLOW}⚠ {message}{Colors.RESET}")

    # ========================================================================
    # Helper Methods
    # ========================================================================

    def check_service_health(self) -> bool:
        """Check if service is running and healthy"""
        try:
            response = self.session.get(f"{self.base_url}/api/v1/health", timeout=5)
            return response.status_code == 200
        except Exception as e:
            return False

    def create_test_process(self, process_id: str, **kwargs) -> Dict[str, Any]:
        """Create a test process"""
        process_config = {
            "process_id": process_id,
            "name": kwargs.get("name", f"Test Process {process_id}"),
            "description": kwargs.get("description", "Test process"),
            "type": kwargs.get("type", "executable"),
            "command": kwargs.get("command", "/bin/sleep"),
            "args": kwargs.get("args", ["30"]),
            "restart_policy": kwargs.get("restart_policy", {
                "auto_restart": True,
                "max_retries": 3,
                "backoff_seconds": 1
            }),
            "metadata": kwargs.get("metadata", {
                "tags": {
                    "environment": "test",
                    "type": "demo"
                }
            })
        }

        response = self.session.post(
            self._url("/processes"),
            json=process_config
        )

        if response.status_code in [200, 201]:
            self.test_processes.append(process_id)

        return response.json() if response.status_code in [200, 201] else {}

    def cleanup_test_processes(self):
        """Delete all test processes"""
        self._print_info("Cleaning up test processes...")
        for process_id in self.test_processes:
            try:
                # Stop first
                self.session.post(self._url(f"/processes/{process_id}/stop"))
                time.sleep(0.5)
                # Delete
                self.session.delete(self._url(f"/processes/{process_id}"))
            except:
                pass
        self.test_processes = []

    # ========================================================================
    # Test Cases - CRUD Operations
    # ========================================================================

    def test_create_process(self):
        """Test creating a new process"""
        self._print_test("Create Process")

        process_id = "test-process-create-001"
        result = self.create_test_process(process_id)

        if result.get("success"):
            self._print_success(f"Process created: {process_id}")
            self._print_info(f"Response: {json.dumps(result, indent=2)}")
        else:
            self._print_error(f"Failed to create process: {result}")

    def test_list_processes(self):
        """Test listing all processes"""
        self._print_test("List All Processes")

        response = self.session.get(self._url("/processes"))

        if response.status_code == 200:
            data = response.json()
            self._print_success(f"Retrieved {data.get('count', 0)} processes")

            if data.get('processes'):
                print(f"\n{Colors.CYAN}Processes:{Colors.RESET}")
                for proc in data['processes'][:5]:  # Show first 5
                    status = "RUNNING" if proc.get('is_running') else "STOPPED"
                    print(f"  - {proc['process_id']}: {proc['name']} [{status}]")
        else:
            self._print_error(f"Failed to list processes: {response.status_code}")

    def test_get_process(self):
        """Test getting a specific process"""
        self._print_test("Get Specific Process")

        # Create a process first
        process_id = "test-process-get-001"
        self.create_test_process(process_id)

        response = self.session.get(self._url(f"/processes/{process_id}"))

        if response.status_code == 200:
            data = response.json()
            if data.get('success') and data.get('process'):
                self._print_success(f"Retrieved process: {process_id}")
                self._print_info(f"Process details: {json.dumps(data['process'], indent=2)}")
            else:
                self._print_error(f"Process data missing")
        else:
            self._print_error(f"Failed to get process: {response.status_code}")

    def test_delete_process(self):
        """Test deleting a process"""
        self._print_test("Delete Process")

        # Create a process first
        process_id = "test-process-delete-001"
        self.create_test_process(process_id)

        response = self.session.delete(self._url(f"/processes/{process_id}"))

        if response.status_code == 200:
            data = response.json()
            if data.get('success'):
                self._print_success(f"Process deleted: {process_id}")
                # Remove from tracking
                if process_id in self.test_processes:
                    self.test_processes.remove(process_id)
            else:
                self._print_error(f"Delete failed: {data}")
        else:
            self._print_error(f"Failed to delete process: {response.status_code}")

    # ========================================================================
    # Test Cases - Lifecycle Operations
    # ========================================================================

    def test_start_process(self):
        """Test starting a process"""
        self._print_test("Start Process")

        # Create a process
        process_id = "test-process-start-001"
        self.create_test_process(process_id)

        response = self.session.post(self._url(f"/processes/{process_id}/start"))

        if response.status_code == 200:
            data = response.json()
            if data.get('success'):
                pid = data.get('pid', 'unknown')
                self._print_success(f"Process started: {process_id} (PID: {pid})")
                self._print_info(f"Status: {json.dumps(data.get('status', {}), indent=2)}")
            else:
                self._print_error(f"Start failed: {data}")
        else:
            self._print_error(f"Failed to start process: {response.status_code}")

    def test_stop_process(self):
        """Test stopping a process"""
        self._print_test("Stop Process")

        # Create and start a process
        process_id = "test-process-stop-001"
        self.create_test_process(process_id)
        self.session.post(self._url(f"/processes/{process_id}/start"))
        time.sleep(1)  # Let it start

        response = self.session.post(self._url(f"/processes/{process_id}/stop"))

        if response.status_code == 200:
            data = response.json()
            if data.get('success'):
                self._print_success(f"Process stopped: {process_id}")
            else:
                self._print_error(f"Stop failed: {data}")
        else:
            self._print_error(f"Failed to stop process: {response.status_code}")

    def test_restart_process(self):
        """Test restarting a process"""
        self._print_test("Restart Process")

        # Create and start a process
        process_id = "test-process-restart-001"
        self.create_test_process(process_id)
        self.session.post(self._url(f"/processes/{process_id}/start"))
        time.sleep(1)

        response = self.session.post(self._url(f"/processes/{process_id}/restart"))

        if response.status_code == 200:
            data = response.json()
            if data.get('success'):
                restart_count = data.get('status', {}).get('restart_count', 0)
                self._print_success(f"Process restarted: {process_id} (restarts: {restart_count})")
            else:
                self._print_error(f"Restart failed: {data}")
        else:
            self._print_error(f"Failed to restart process: {response.status_code}")

    # ========================================================================
    # Test Cases - Health & Status
    # ========================================================================

    def test_get_process_status(self):
        """Test getting process status"""
        self._print_test("Get Process Status")

        # Create and start a process
        process_id = "test-process-status-001"
        self.create_test_process(process_id)
        self.session.post(self._url(f"/processes/{process_id}/start"))
        time.sleep(1)

        response = self.session.get(self._url(f"/processes/{process_id}/status"))

        if response.status_code == 200:
            data = response.json()
            if data.get('success') and data.get('status'):
                status = data['status']
                self._print_success(f"Got status for: {process_id}")
                print(f"\n{Colors.CYAN}Status Details:{Colors.RESET}")
                print(f"  PID: {status.get('pid')}")
                print(f"  Running: {status.get('is_running')}")
                print(f"  Healthy: {status.get('is_healthy')}")
                print(f"  Uptime: {status.get('uptime_seconds')}s")
            else:
                self._print_error(f"Status data missing")
        else:
            self._print_error(f"Failed to get status: {response.status_code}")

    def test_health_check(self):
        """Test process health check"""
        self._print_test("Process Health Check")

        # Create and start a process
        process_id = "test-process-health-001"
        self.create_test_process(process_id)
        self.session.post(self._url(f"/processes/{process_id}/start"))
        time.sleep(1)

        response = self.session.post(self._url(f"/processes/{process_id}/health"))

        if response.status_code == 200:
            data = response.json()
            if data.get('success'):
                healthy = data.get('healthy', False)
                status_icon = "✓" if healthy else "✗"
                self._print_success(f"Health check complete: {status_icon} {'Healthy' if healthy else 'Unhealthy'}")
            else:
                self._print_error(f"Health check failed: {data}")
        else:
            self._print_error(f"Failed health check: {response.status_code}")

    # ========================================================================
    # Test Cases - Bulk Operations
    # ========================================================================

    def test_bulk_start(self):
        """Test starting multiple processes"""
        self._print_test("Bulk Start Processes")

        # Create multiple processes
        process_ids = [
            "test-bulk-start-001",
            "test-bulk-start-002",
            "test-bulk-start-003"
        ]

        for pid in process_ids:
            self.create_test_process(pid)

        response = self.session.post(
            self._url("/processes/bulk/start"),
            json={"process_ids": process_ids}
        )

        if response.status_code == 200:
            data = response.json()
            if data.get('success'):
                results = data.get('results', {})
                success_count = sum(1 for v in results.values() if v)
                self._print_success(f"Bulk start: {success_count}/{len(process_ids)} succeeded")

                for pid, success in results.items():
                    icon = "✓" if success else "✗"
                    print(f"  {icon} {pid}")
            else:
                self._print_error(f"Bulk start failed: {data}")
        else:
            self._print_error(f"Failed bulk start: {response.status_code}")

    def test_bulk_stop(self):
        """Test stopping multiple processes"""
        self._print_test("Bulk Stop Processes")

        # Create and start multiple processes
        process_ids = [
            "test-bulk-stop-001",
            "test-bulk-stop-002",
            "test-bulk-stop-003"
        ]

        for pid in process_ids:
            self.create_test_process(pid)
            self.session.post(self._url(f"/processes/{pid}/start"))

        time.sleep(1)

        response = self.session.post(
            self._url("/processes/bulk/stop"),
            json={
                "process_ids": process_ids,
                "force": False
            }
        )

        if response.status_code == 200:
            data = response.json()
            if data.get('success'):
                results = data.get('results', {})
                success_count = sum(1 for v in results.values() if v)
                self._print_success(f"Bulk stop: {success_count}/{len(process_ids)} succeeded")
            else:
                self._print_error(f"Bulk stop failed: {data}")
        else:
            self._print_error(f"Failed bulk stop: {response.status_code}")

    # ========================================================================
    # Test Cases - Query & Search
    # ========================================================================

    def test_search_by_tag(self):
        """Test searching processes by tag"""
        self._print_test("Search Processes by Tag")

        # Create processes with specific tags
        self.create_test_process(
            "test-search-001",
            metadata={"tags": {"environment": "production", "type": "worker"}}
        )
        self.create_test_process(
            "test-search-002",
            metadata={"tags": {"environment": "production", "type": "api"}}
        )

        response = self.session.get(
            self._url("/processes/search"),
            params={"tag_key": "environment", "tag_value": "production"}
        )

        if response.status_code == 200:
            data = response.json()
            if data.get('success'):
                count = data.get('count', 0)
                process_ids = data.get('process_ids', [])
                self._print_success(f"Found {count} processes with tag 'environment=production'")

                for pid in process_ids:
                    print(f"  - {pid}")
            else:
                self._print_error(f"Search failed: {data}")
        else:
            self._print_error(f"Failed search: {response.status_code}")

    def test_get_running_processes(self):
        """Test getting all running processes"""
        self._print_test("Get Running Processes")

        # Create and start some processes
        for i in range(3):
            pid = f"test-running-00{i+1}"
            self.create_test_process(pid)
            self.session.post(self._url(f"/processes/{pid}/start"))

        time.sleep(1)

        response = self.session.get(self._url("/processes/running"))

        if response.status_code == 200:
            data = response.json()
            if data.get('success'):
                count = data.get('count', 0)
                self._print_success(f"Found {count} running processes")
            else:
                self._print_error(f"Failed to get running processes")
        else:
            self._print_error(f"Failed: {response.status_code}")

    def test_get_stopped_processes(self):
        """Test getting all stopped processes"""
        self._print_test("Get Stopped Processes")

        response = self.session.get(self._url("/processes/stopped"))

        if response.status_code == 200:
            data = response.json()
            if data.get('success'):
                count = data.get('count', 0)
                self._print_success(f"Found {count} stopped processes")
            else:
                self._print_error(f"Failed to get stopped processes")
        else:
            self._print_error(f"Failed: {response.status_code}")

    # ========================================================================
    # Test Cases - Statistics
    # ========================================================================

    def test_get_statistics(self):
        """Test getting process manager statistics"""
        self._print_test("Get Statistics")

        response = self.session.get(self._url("/processes/stats"))

        if response.status_code == 200:
            data = response.json()
            if data.get('success') and data.get('stats'):
                stats = data['stats']
                self._print_success("Retrieved statistics")

                print(f"\n{Colors.CYAN}Process Manager Statistics:{Colors.RESET}")
                print(f"  Total Processes: {stats.get('total_processes')}")
                print(f"  Running: {stats.get('running_processes')}")
                print(f"  Stopped: {stats.get('stopped_processes')}")
                print(f"  Failed: {stats.get('failed_processes')}")
                print(f"  Total Starts: {stats.get('total_starts')}")
                print(f"  Total Stops: {stats.get('total_stops')}")
                print(f"  Total Restarts: {stats.get('total_restarts')}")
            else:
                self._print_error(f"Stats data missing")
        else:
            self._print_error(f"Failed to get stats: {response.status_code}")

    # ========================================================================
    # Main Test Runner
    # ========================================================================

    def run_all_tests(self):
        """Run all test cases"""
        print(f"\n{Colors.BOLD}{Colors.CYAN}{'='*70}{Colors.RESET}")
        print(f"{Colors.BOLD}{Colors.CYAN}Process Manager API Test Suite{Colors.RESET}")
        print(f"{Colors.BOLD}{Colors.CYAN}{'='*70}{Colors.RESET}")
        print(f"Base URL: {self.base_url}")

        # Check service health
        print(f"\n{Colors.YELLOW}Checking service health...{Colors.RESET}")
        if not self.check_service_health():
            print(f"{Colors.RED}✗ Service is not responding at {self.base_url}{Colors.RESET}")
            print(f"{Colors.YELLOW}Make sure the service is running:{Colors.RESET}")
            print(f"  docker-compose up -d device-dnp3-service")
            print(f"  OR")
            print(f"  cd build && ./bin/microservice_template")
            return

        self._print_success("Service is healthy and responding")

        try:
            # CRUD Tests
            print(f"\n{Colors.BOLD}{Colors.YELLOW}=== CRUD Operations ==={Colors.RESET}")
            self.test_create_process()
            self.test_list_processes()
            self.test_get_process()
            self.test_delete_process()

            # Lifecycle Tests
            print(f"\n{Colors.BOLD}{Colors.YELLOW}=== Lifecycle Operations ==={Colors.RESET}")
            self.test_start_process()
            self.test_stop_process()
            self.test_restart_process()

            # Health Tests
            print(f"\n{Colors.BOLD}{Colors.YELLOW}=== Health & Status ==={Colors.RESET}")
            self.test_get_process_status()
            self.test_health_check()

            # Bulk Operations
            print(f"\n{Colors.BOLD}{Colors.YELLOW}=== Bulk Operations ==={Colors.RESET}")
            self.test_bulk_start()
            self.test_bulk_stop()

            # Query Tests
            print(f"\n{Colors.BOLD}{Colors.YELLOW}=== Query & Search ==={Colors.RESET}")
            self.test_search_by_tag()
            self.test_get_running_processes()
            self.test_get_stopped_processes()

            # Statistics
            print(f"\n{Colors.BOLD}{Colors.YELLOW}=== Statistics ==={Colors.RESET}")
            self.test_get_statistics()

        finally:
            # Cleanup
            print(f"\n{Colors.YELLOW}Cleaning up test processes...{Colors.RESET}")
            self.cleanup_test_processes()

        # Summary
        print(f"\n{Colors.BOLD}{Colors.CYAN}{'='*70}{Colors.RESET}")
        print(f"{Colors.BOLD}{Colors.CYAN}Test Summary{Colors.RESET}")
        print(f"{Colors.BOLD}{Colors.CYAN}{'='*70}{Colors.RESET}")

        total = self.passed + self.failed
        pass_rate = (self.passed / total * 100) if total > 0 else 0

        print(f"{Colors.GREEN}Passed: {self.passed}{Colors.RESET}")
        print(f"{Colors.RED}Failed: {self.failed}{Colors.RESET}")
        print(f"Total: {total}")
        print(f"Pass Rate: {pass_rate:.1f}%")

        if self.failed == 0:
            print(f"\n{Colors.GREEN}{Colors.BOLD}✓ All tests passed!{Colors.RESET}")
            return 0
        else:
            print(f"\n{Colors.RED}{Colors.BOLD}✗ Some tests failed{Colors.RESET}")
            return 1


def main():
    """Main entry point"""
    import argparse

    parser = argparse.ArgumentParser(description='Test Process Manager API')
    parser.add_argument(
        '--url',
        default=API_BASE_URL,
        help=f'Base URL of the API (default: {API_BASE_URL})'
    )
    parser.add_argument(
        '--standalone',
        action='store_true',
        help='Use standalone port (8086 instead of 8092)'
    )

    args = parser.parse_args()

    # Adjust URL for standalone mode
    if args.standalone:
        base_url = "http://localhost:8086"
    else:
        base_url = args.url

    tester = ProcessManagerAPITester(base_url=base_url)
    exit_code = tester.run_all_tests()
    sys.exit(exit_code)


if __name__ == "__main__":
    main()
