/*
 * ============================================================
 * University Course Registration System
 * CL-2006 – Operating Systems Lab | Final Project
 * ============================================================
 * Description:
 *   Simulates a concurrent university course registration system
 *   using POSIX threads (pthreads). Students are modelled as
 *   concurrent threads that compete for limited course seats.
 *   Synchronization is achieved via per-course mutexes.
 *   High-priority (final-year) students receive preference
 *   through a small artificial delay imposed on low-priority
 *   threads before they enter the critical section.
 *
 * Compile:  gcc -o registration registration.c -lpthread
 * Run:      ./registration
 * ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>
#include <stdarg.h>

/* ─────────────────────────────────────────────
 * Compile-time Configuration
 * ───────────────────────────────────────────── */
#define MAX_COURSES        10
#define MAX_STUDENTS       200
#define NUM_COURSES        8          /* actual courses used            */
#define NUM_STUDENTS       60         /* actual student threads created */
#define HIGH_PRIORITY_RATIO 3        /* 1 in every N students is high  */
#define LOG_FILE           "registration_log.txt"

/* Priority levels */
#define PRIORITY_HIGH  1   /* final-year / graduating student */
#define PRIORITY_LOW   0   /* regular student                 */

/* Delay (microseconds) imposed on low-priority threads to give
 * high-priority threads a head start in contention scenarios. */
#define LOW_PRIORITY_DELAY_US  5000   /* 5 ms */

/* ─────────────────────────────────────────────
 * Module 1 – Data Structures
 * ───────────────────────────────────────────── */

/*
 * Course – represents a single course offered by the university.
 * Fields:
 *   id            : unique numeric identifier
 *   name          : human-readable course name
 *   total_seats   : initial seat capacity
 *   available     : remaining seats (shared mutable state)
 *   lock          : per-course mutex protecting 'available'
 *   enrolled_count: number of students successfully enrolled
 */
typedef struct {
    int         id;
    char        name[32];
    int         total_seats;
    int         available;
    pthread_mutex_t lock;
    int         enrolled_count;
} Course;

/*
 * Student – represents a single student thread.
 * Fields:
 *   id          : unique student identifier (S1001 … S1200)
 *   priority    : PRIORITY_HIGH or PRIORITY_LOW
 *   course_ids  : array of course IDs the student wants to register for
 *   num_requests: how many courses the student will attempt
 */
typedef struct {
    int id;
    int priority;
    int course_ids[3];   /* attempt up to 3 courses */
    int num_requests;
} Student;

/*
 * RegistrationResult – passed to each thread as its argument;
 * also serves as the per-thread output record.
 */
typedef struct {
    Student *student;
    int      success_count;
    int      fail_count;
} ThreadArg;

/* ─────────────────────────────────────────────
 * Global Shared State
 * ───────────────────────────────────────────── */
Course  courses[MAX_COURSES];
Student students[MAX_STUDENTS];

/* Global statistics – protected by stats_lock */
int total_success = 0;
int total_fail    = 0;
pthread_mutex_t stats_lock = PTHREAD_MUTEX_INITIALIZER;

/* Log file handle – protected by log_lock */
FILE           *log_fp = NULL;
pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;

/* Barrier: makes all threads start at approximately the same time */
pthread_barrier_t start_barrier;

/* ─────────────────────────────────────────────
 * Module 6 (partial) – Logging helpers
 * ───────────────────────────────────────────── */

/*
 * get_timestamp – fills buf with current wall-clock time formatted as
 * HH:MM:SS.mmm (hours, minutes, seconds, milliseconds).
 * Parameters:
 *   buf  : destination string buffer
 *   size : size of buf in bytes
 */
static void get_timestamp(char *buf, size_t size)
{
    struct timespec ts;
    struct tm       tm_info;
    clock_gettime(CLOCK_REALTIME, &ts);
    localtime_r(&ts.tv_sec, &tm_info);
    int ms = (int)(ts.tv_nsec / 1000000);
    snprintf(buf, size, "%02d:%02d:%02d.%03d",
             tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec, ms);
}

/*
 * log_event – thread-safe logging to both stdout and the log file.
 * Parameters:
 *   fmt : printf-style format string
 *   ... : variadic arguments
 */
static void log_event(const char *fmt, ...)
{
    char    ts[32];
    char    msg[512];
    va_list args;

    get_timestamp(ts, sizeof(ts));
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    pthread_mutex_lock(&log_lock);
    printf("[%s] %s\n", ts, msg);
    if (log_fp)
        fprintf(log_fp, "[%s] %s\n", ts, msg);
    pthread_mutex_unlock(&log_lock);
}

/* ─────────────────────────────────────────────
 * Module 1 – Initialisation helpers
 * ───────────────────────────────────────────── */

/*
 * init_courses – statically initialises NUM_COURSES courses with
 * predefined names and seat capacities, and sets up their mutexes.
 */
static void init_courses(void)
{
    /* Predefined course data: {id, name, seats} */
    struct { int id; const char *name; int seats; } data[] = {
        {101, "CS101-Intro to CS",          5},
        {102, "CS102-Data Structures",      4},
        {103, "CS103-Algorithms",           6},
        {104, "CS104-Operating Systems",    3},
        {105, "CS105-Computer Networks",    5},
        {106, "CS106-Database Systems",     4},
        {107, "CS107-Software Engineering", 7},
        {108, "CS108-AI Fundamentals",      3},
    };

    for (int i = 0; i < NUM_COURSES; i++) {
        courses[i].id              = data[i].id;
        strncpy(courses[i].name, data[i].name, sizeof(courses[i].name) - 1);
        courses[i].total_seats     = data[i].seats;
        courses[i].available       = data[i].seats;
        courses[i].enrolled_count  = 0;
        pthread_mutex_init(&courses[i].lock, NULL);
    }
}

/*
 * init_students – creates NUM_STUDENTS student records.
 * Every HIGH_PRIORITY_RATIO-th student is marked high-priority.
 * Each student randomly selects 1–3 courses to register for.
 */
static void init_students(void)
{
    srand((unsigned)time(NULL));
    for (int i = 0; i < NUM_STUDENTS; i++) {
        students[i].id       = 1001 + i;
        students[i].priority = ((i % HIGH_PRIORITY_RATIO) == 0)
                                   ? PRIORITY_HIGH : PRIORITY_LOW;

        /* Pick 1–3 distinct random course indices */
        students[i].num_requests = (rand() % 3) + 1;
        int chosen[3] = {-1, -1, -1};
        for (int r = 0; r < students[i].num_requests; r++) {
            int idx;
            int duplicate;
            do {
                duplicate = 0;
                idx = rand() % NUM_COURSES;
                for (int k = 0; k < r; k++)
                    if (chosen[k] == idx) { duplicate = 1; break; }
            } while (duplicate);
            chosen[r]                = idx;
            students[i].course_ids[r] = courses[idx].id;
        }
    }
}

/* ─────────────────────────────────────────────
 * Module 3 – Synchronisation utilities
 * ───────────────────────────────────────────── */

/*
 * find_course_index – returns the array index of the course with the
 * given id, or -1 if not found.
 * Parameters:
 *   course_id : the course ID to search for
 */
static int find_course_index(int course_id)
{
    for (int i = 0; i < NUM_COURSES; i++)
        if (courses[i].id == course_id) return i;
    return -1;
}

/*
 * attempt_register – tries to register a student for one course.
 * This function contains the critical section: it acquires the
 * per-course mutex, checks seat availability, and updates the count
 * atomically before releasing the mutex.
 *
 * Deadlock prevention: each student requests one course at a time,
 * so at most one mutex is held by any thread at any moment – making
 * circular wait impossible.
 *
 * Parameters:
 *   student   : pointer to the student attempting to register
 *   course_id : the course the student wants to join
 * Returns:
 *   1 on success, 0 on failure (no seats available)
 */
static int attempt_register(Student *student __attribute__((unused)), int course_id)
{
    int cidx = find_course_index(course_id);
    if (cidx < 0) return 0;

    /* ── Critical Section Begin ── */
    pthread_mutex_lock(&courses[cidx].lock);

    int result = 0;
    if (courses[cidx].available > 0) {
        courses[cidx].available--;
        courses[cidx].enrolled_count++;
        result = 1;
    }

    pthread_mutex_unlock(&courses[cidx].lock);
    /* ── Critical Section End ── */

    return result;
}

/* ─────────────────────────────────────────────
 * Module 2 – Thread function
 * ───────────────────────────────────────────── */

/*
 * student_thread – the entry point executed by each student thread.
 * Waits at the barrier so all threads begin at approximately the same
 * time. Low-priority threads sleep briefly to give high-priority
 * threads preference when seats are scarce. Then it attempts to
 * register for each of its requested courses.
 *
 * Parameters:
 *   arg : pointer to a ThreadArg struct (heap-allocated by main)
 * Returns:
 *   NULL (pthread convention)
 */
static void *student_thread(void *arg)
{
    ThreadArg *targ    = (ThreadArg *)arg;
    Student   *student = targ->student;

    const char *priority_label = (student->priority == PRIORITY_HIGH)
                                    ? "HIGH" : "LOW ";

    /* Synchronise: wait until all threads are ready */
    pthread_barrier_wait(&start_barrier);

    /* Module 4 – Priority handling:
     * Low-priority threads yield briefly so high-priority threads
     * can acquire the mutex first during peak contention. */
    if (student->priority == PRIORITY_LOW)
        usleep(LOW_PRIORITY_DELAY_US);

    /* Attempt registration for each requested course */
    for (int r = 0; r < student->num_requests; r++) {
        int  cid    = student->course_ids[r];
        int  cidx   = find_course_index(cid);
        const char *cname = (cidx >= 0) ? courses[cidx].name : "UNKNOWN";

        int ok = attempt_register(student, cid);

        if (ok) {
            log_event("S%d | %-30s | Priority: %s | REGISTERED",
                      student->id, cname, priority_label);
            targ->success_count++;
        } else {
            log_event("S%d | %-30s | Priority: %s | FAILED – No Seats",
                      student->id, cname, priority_label);
            targ->fail_count++;
        }
    }

    return NULL;
}

/* ─────────────────────────────────────────────
 * Module 6 – Final summary output
 * ───────────────────────────────────────────── */

/*
 * print_summary – prints (and logs) the final seat allocation for
 * every course and the overall registration statistics.
 */
static void print_summary(void)
{
    char separator[70];
    memset(separator, '=', sizeof(separator) - 1);
    separator[sizeof(separator) - 1] = '\0';

    log_event("%s", separator);
    log_event("FINAL COURSE SEAT ALLOCATION");
    log_event("%s", separator);
    log_event("%-6s %-32s %8s %8s %8s",
              "ID", "Course", "Total", "Enrolled", "Remaining");
    log_event("%-6s %-32s %8s %8s %8s",
              "------", "--------------------------------",
              "--------","--------","--------");

    for (int i = 0; i < NUM_COURSES; i++) {
        log_event("CS%-4d %-32s %8d %8d %8d",
                  courses[i].id,
                  courses[i].name,
                  courses[i].total_seats,
                  courses[i].enrolled_count,
                  courses[i].available);
    }

    log_event("%s", separator);
    log_event("SUMMARY STATISTICS");
    log_event("%s", separator);
    log_event("Total Students       : %d", NUM_STUDENTS);
    log_event("Total Requests       : (varies, 1-3 per student)");
    log_event("Successful Registers : %d", total_success);
    log_event("Failed Registers     : %d", total_fail);
    log_event("%s", separator);
}

/* ─────────────────────────────────────────────
 * Module 5 – Deadlock Prevention (summary)
 * ─────────────────────────────────────────────
 * Strategy: Each student thread acquires at most ONE course mutex at
 * a time (one course registration per attempt_register call). Because
 * no thread ever holds two locks simultaneously, the circular-wait
 * condition required for deadlock can never occur.
 * ───────────────────────────────────────────── */

/* ─────────────────────────────────────────────
 * main – entry point
 * ───────────────────────────────────────────── */

/*
 * main – initialises courses and students, spawns NUM_STUDENTS threads,
 * waits for all threads to finish, aggregates statistics, and prints
 * the final summary.
 */
int main(void)
{
    pthread_t  threads[MAX_STUDENTS];
    ThreadArg  args[MAX_STUDENTS];

    /* Open log file */
    log_fp = fopen(LOG_FILE, "w");
    if (!log_fp)
        fprintf(stderr, "Warning: could not open log file %s\n", LOG_FILE);

    log_event("University Course Registration System – Starting");
    log_event("Students: %d | Courses: %d", NUM_STUDENTS, NUM_COURSES);

    /* Initialise data */
    init_courses();
    init_students();

    /* Barrier: NUM_STUDENTS worker threads + 1 main thread */
    pthread_barrier_init(&start_barrier, NULL, NUM_STUDENTS + 1);

    log_event("Spawning %d student threads…", NUM_STUDENTS);

    /* Module 2 – Create threads */
    for (int i = 0; i < NUM_STUDENTS; i++) {
        args[i].student       = &students[i];
        args[i].success_count = 0;
        args[i].fail_count    = 0;

        if (pthread_create(&threads[i], NULL, student_thread, &args[i]) != 0) {
            fprintf(stderr, "Error: failed to create thread for student %d\n",
                    students[i].id);
            exit(EXIT_FAILURE);
        }
    }

    /* Release all threads simultaneously */
    pthread_barrier_wait(&start_barrier);
    log_event("All threads released – registration in progress…");

    /* Module 2 – Join threads (ensure proper termination) */
    for (int i = 0; i < NUM_STUDENTS; i++) {
        pthread_join(threads[i], NULL);

        /* Accumulate global statistics */
        pthread_mutex_lock(&stats_lock);
        total_success += args[i].success_count;
        total_fail    += args[i].fail_count;
        pthread_mutex_unlock(&stats_lock);
    }

    log_event("All threads terminated cleanly.");

    /* Print final summary */
    print_summary();

    /* Cleanup */
    pthread_barrier_destroy(&start_barrier);
    pthread_mutex_destroy(&stats_lock);
    pthread_mutex_destroy(&log_lock);
    for (int i = 0; i < NUM_COURSES; i++)
        pthread_mutex_destroy(&courses[i].lock);

    if (log_fp) fclose(log_fp);

    printf("\nLog saved to: %s\n", LOG_FILE);
    return 0;
}
