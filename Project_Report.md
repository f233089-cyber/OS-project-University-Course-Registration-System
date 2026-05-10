# Project Report: University Course Registration System
**Course:** CL-2006 – Operating Systems Lab | Final Project
**Student Name:** Muhammed Ahmad
**Roll Number:** 23F-3089

---

## 1. System Architecture and Design
The system is designed as a concurrent simulation of a university course registration process. It models students as individual POSIX threads (`pthreads`) competing for a shared resource—course seats.
The architecture consists of the following key components:
- **Global Data Structures:** An array of `Course` structs maintains the state of each course (ID, name, total seats, available seats, enrolled students) and a per-course mutex.
- **Thread Management:** The main process initializes the data, creates a thread for each student, and waits for them to complete using `pthread_join`. A `pthread_barrier` is utilized to ensure all threads start execution simultaneously, simulating high contention.
- **Concurrency Throttle:** A POSIX semaphore (`sem_t`) is used to limit the maximum number of concurrent registration operations, preventing the system from being overwhelmed.

## 2. Synchronization Strategy and Critical Sections
Proper synchronization is achieved using mutexes and semaphores:
- **Critical Section:** The critical section is the code block where a student checks for seat availability, verifies they are not already enrolled, and decrements the available seat count. 
- **Per-Course Mutex:** Each course has its own `pthread_mutex_t`. When a student attempts to register for a course, they must acquire that specific course's mutex. This allows concurrent registrations for *different* courses while ensuring mutual exclusion for the *same* course.
- **Semaphore:** A semaphore limits the number of threads actively inside the registration logic at any given time, simulating system load balancing.

## 3. Priority Handling Approach
To give preference to final-year students (high priority), a Priority Gate is implemented using a mutex and a condition variable (`pthread_cond_t`):
- **Gate Logic:** High-priority students enter the gate immediately and increment an `active` counter. Low-priority students must wait on the condition variable as long as there are active high-priority students.
- **Starvation Avoidance (Aging):** To prevent low-priority students from waiting indefinitely (starvation), an aging mechanism is implemented. Each time a low-priority student wakes up and is forced to wait again, their `age` counter increments. Once it reaches a threshold (`AGING_THRESHOLD`), they are allowed to bypass the priority gate.

## 4. Deadlock Prevention Explanation
The system is designed to be deadlock-free by eliminating the **circular wait** condition:
- Each student requests exactly **one course at a time** within a single transaction.
- Because a thread never attempts to acquire a second course mutex while already holding one, a circular wait cycle cannot form.
- Additionally, locks are always acquired in a strict global order (Priority Mutex -> Semaphore -> Course Mutex -> Log Mutex), ensuring safe execution.

## 5. Testing Strategy and Observations
Testing was conducted in two main phases:
1. **Stress Testing (`registration.c`):** We simulated 60 to 200 concurrent students randomly selecting up to 3 courses each. The system correctly handled the contention without any seat count dropping below zero.
2. **Mandatory Test Scenario (`test_scenario.c`):** As per Section 13, 10 students (3 high priority, 7 low priority) competed for 3 courses (CS101: 2 seats, CS102: 1 seat, CS103: 3 seats). 
   - **Observations:** High-priority students consistently secured seats over low-priority students. Failed registrations accurately logged "No Seats Available". All threads terminated cleanly, and final seat counts matched exactly.

## 6. Challenges Faced and Lessons Learned
- **Challenge:** Ensuring all threads start at the exact same time to create realistic race conditions. 
  - **Solution:** Implemented `pthread_barrier_t` to hold all spawned threads until the main thread releases them.
- **Challenge:** Preventing low-priority thread starvation.
  - **Solution:** Added a condition variable with an aging threshold so that low-priority threads eventually get a chance to register.
- **Lesson Learned:** Fine-grained locking (per-course mutexes) performs significantly better than a single global lock, as it allows true parallelism for independent resources.

---

## 7. Execution Evidence and Screenshots
*(Please take screenshots of the terminal output when you run the commands and paste them here before submitting)*

### Task 1: Building the Project
**Sequence to run:** `make`
*(Paste Screenshot Here)*

### Task 2: Running the Mandatory Demo Scenario (Section 13)
**Sequence to run:** `./test_scenario`
*(Paste Screenshot Here)*

### Task 3: Running the Full Simulation
**Sequence to run:** `./registration`
*(Paste Screenshot Here)*

### Task 4: Running the Stress Test
**Sequence to run:** `./registration --stress`
*(Paste Screenshot Here)*

---

## 8. Source Code

### `registration.c`
```c
// (Paste the contents of registration.c here in your Word Document)
```

### `test_scenario.c`
```c
// (Paste the contents of test_scenario.c here in your Word Document)
```

### `Makefile`
```makefile
// (Paste the contents of Makefile here in your Word Document)
```
