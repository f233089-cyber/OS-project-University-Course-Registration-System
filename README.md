# University Course Registration System
## CL-2006 – Operating Systems Lab | Final Project

**Student:** Muhammad Ahmad | **Roll No:** 23F-3089

---

## How to Compile & Run (Linux / WSL)

This project uses **POSIX threads** (`pthreads`) and must be compiled on Linux.

### Prerequisites
```bash
sudo apt update && sudo apt install -y gcc make
```

### Build
```bash
make            # builds both: registration  and  test_scenario
```

### Run – Full Simulation (60 students, 8 courses)
```bash
make run
# OR
./registration
```

### Run – Custom Student Count
```bash
./registration --students 100
./registration --students 200
```

### Run – Stress Test (200 students)
```bash
make stress
# OR
./registration --stress
```

### Run – Mandatory Demo Scenario (Section 13)
```bash
make demo
# OR
./test_scenario
```

### Clean Build Artifacts
```bash
make clean
```

---

## Project Structure

```
OS Semester Project/
├── registration.c       ← Main simulation (Modules 1–6)
├── test_scenario.c      ← Mandatory Section 13 demo
├── Makefile             ← Build system
├── README.md            ← This file
└── registration_log.txt ← Generated at runtime
```

---

## OS Concepts Implemented

| Concept | Where Used |
|---|---|
| POSIX Threads | `pthread_create` / `pthread_join` in `main()` |
| Mutex Locks | Per-course lock in `attempt_register()` |
| POSIX Semaphores | `registration_semaphore` (concurrency throttle) |
| Condition Variables | Priority gate in `priority_gate_enter/exit()` |
| Barriers | `pthread_barrier_wait()` for synchronised start |
| Critical Section | Seat check + update in `attempt_register()` |
| Deadlock Prevention | Single lock per operation, fixed lock ordering |
| Starvation Avoidance | Aging counter in `priority_gate_enter()` |

---

## Expected Output (Mandatory Demo)

```
[HH:MM:SS.mmm] S2001 | CS101 | Priority: HIGH | SUCCESS  | Success
[HH:MM:SS.mmm] S2002 | CS101 | Priority: HIGH | SUCCESS  | Success
[HH:MM:SS.mmm] S2003 | CS102 | Priority: HIGH | SUCCESS  | Success
[HH:MM:SS.mmm] S2004 | CS101 | Priority: LOW  | FAILED   | No Seats Available
...
```
