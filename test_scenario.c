/*
 * ============================================================
 * test_scenario.c – Mandatory Demo Scenario (Section 13)
 * CL-2006 – Operating Systems Lab | Final Project
 * ============================================================
 * Description:
 *   Implements the EXACT scenario from Section 13 of the project
 *   specification:
 *     • 3 courses: CS101 (2 seats), CS102 (1 seat), CS103 (3 seats)
 *     • 10 student threads created concurrently
 *     • At least 3 high-priority (final-year) students
 *   All threads start simultaneously via a pthread_barrier.
 *   Output is timestamped and includes Student ID, Course ID,
 *   Priority level, and Result.
 *
 * Compile:  gcc -o test_scenario test_scenario.c -lpthread
 * Run:      ./test_scenario
 * ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <stdarg.h>

/* ── Configuration ──────────────────────────── */
#define DEMO_COURSES  3
#define DEMO_STUDENTS 10
#define HIGH_PRIO     1
#define LOW_PRIO      0
#define DELAY_US      3000   /* 3 ms advantage for high-priority threads */

/* ── Data Structures ────────────────────────── */

/*
 * DemoCourse – a single course for the demonstration scenario.
 */
typedef struct {
    int             id;
    char            name[16];
    int             total_seats;
    int             available;
    int             enrolled;
    pthread_mutex_t lock;
} DemoCourse;

/*
 * DemoStudent – represents one student in the demo.
 */
typedef struct {
    int id;
    int priority;
    int course_id;   /* each student tries exactly one course */
} DemoStudent;

/*
 * DemoArg – passed to each student thread.
 */
typedef struct {
    DemoStudent *student;
    DemoCourse  *courses;
    int          result;   /* 1=success, 0=fail */
} DemoArg;

/* ── Globals ────────────────────────────────── */
static int             demo_success = 0;
static int             demo_fail    = 0;
static pthread_mutex_t log_mutex    = PTHREAD_MUTEX_INITIALIZER;
static pthread_barrier_t barrier;

/* ── Helpers ────────────────────────────────── */

/*
 * ts – writes a HH:MM:SS.mmm timestamp into buf.
 */
static void ts(char *buf, size_t sz)
{
    struct timespec t;
    struct tm       m;
    clock_gettime(CLOCK_REALTIME, &t);
    localtime_r(&t.tv_sec, &m);
    snprintf(buf, sz, "%02d:%02d:%02d.%03d",
             m.tm_hour, m.tm_min, m.tm_sec, (int)(t.tv_nsec/1000000));
}

/*
 * log_line – thread-safe timestamped print to stdout.
 */
static void log_line(const char *fmt, ...)
{
    char buf[512], tbuf[32];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    ts(tbuf, sizeof(tbuf));
    pthread_mutex_lock(&log_mutex);
    printf("[%s] %s\n", tbuf, buf);
    pthread_mutex_unlock(&log_mutex);
}

/*
 * find_demo_course – returns index of course matching id, else -1.
 */
static int find_demo_course(DemoCourse *courses, int id)
{
    for (int i = 0; i < DEMO_COURSES; i++)
        if (courses[i].id == id) return i;
    return -1;
}

/* ── Thread Function ────────────────────────── */

/*
 * demo_student_thread – student thread for the mandatory scenario.
 * Uses a barrier for simultaneous start, and a small delay for
 * low-priority threads to give high-priority threads preference.
 */
static void *demo_student_thread(void *arg)
{
    DemoArg     *a   = (DemoArg *)arg;
    DemoStudent *stu = a->student;
    DemoCourse  *crs = a->courses;

    const char *plabel = (stu->priority == HIGH_PRIO) ? "HIGH" : "LOW ";

    /* Wait for all threads to be ready */
    pthread_barrier_wait(&barrier);

    /* Priority handling: low-priority threads wait briefly */
    if (stu->priority == LOW_PRIO)
        usleep(DELAY_US);

    int cidx = find_demo_course(crs, stu->course_id);
    if (cidx < 0) {
        log_line("S%d | UNKNOWN      | Priority:%-4s | FAILED – Bad Course ID",
                 stu->id, plabel);
        a->result = 0;
        return NULL;
    }

    /* ── Critical Section ── */
    pthread_mutex_lock(&crs[cidx].lock);

    int ok = 0;
    if (crs[cidx].available > 0) {
        crs[cidx].available--;
        crs[cidx].enrolled++;
        ok = 1;
    }

    pthread_mutex_unlock(&crs[cidx].lock);
    /* ── End Critical Section ── */

    a->result = ok;

    if (ok) {
        log_line("S%d | CS%d | Priority: %s | SUCCESS",
                 stu->id, stu->course_id, plabel);
    } else {
        log_line("S%d | CS%d | Priority: %s | FAILED – No Seats",
                 stu->id, stu->course_id, plabel);
    }

    return NULL;
}

/* ── Main ───────────────────────────────────── */

/*
 * main – sets up the mandatory demo scenario and runs it.
 */
int main(void)
{
    /* ── Course setup ── */
    DemoCourse courses[DEMO_COURSES] = {
        {101, "CS101", 2, 2, 0, PTHREAD_MUTEX_INITIALIZER},
        {102, "CS102", 1, 1, 0, PTHREAD_MUTEX_INITIALIZER},
        {103, "CS103", 3, 3, 0, PTHREAD_MUTEX_INITIALIZER},
    };
    for (int i = 0; i < DEMO_COURSES; i++)
        pthread_mutex_init(&courses[i].lock, NULL);

    /* ── Student setup (Section 13 spec) ──
     * Students 0,1,2 = HIGH priority (final-year)
     * Students 3–9   = LOW priority
     * Course distribution covers all three courses under contention */
    DemoStudent students[DEMO_STUDENTS] = {
        {2001, HIGH_PRIO, 101},
        {2002, HIGH_PRIO, 101},
        {2003, HIGH_PRIO, 102},
        {2004, LOW_PRIO,  101},
        {2005, LOW_PRIO,  102},
        {2006, LOW_PRIO,  103},
        {2007, LOW_PRIO,  103},
        {2008, LOW_PRIO,  103},
        {2009, LOW_PRIO,  103},
        {2010, LOW_PRIO,  101},
    };

    DemoArg    args[DEMO_STUDENTS];
    pthread_t  threads[DEMO_STUDENTS];

    pthread_barrier_init(&barrier, NULL, DEMO_STUDENTS + 1);

    printf("\n");
    printf("=============================================================\n");
    printf("  MANDATORY TEST SCENARIO – Section 13\n");
    printf("  Courses: CS101(2 seats) | CS102(1 seat) | CS103(3 seats)\n");
    printf("  Students: 10 threads | 3 high-priority\n");
    printf("=============================================================\n\n");

    /* Spawn threads */
    for (int i = 0; i < DEMO_STUDENTS; i++) {
        args[i].student = &students[i];
        args[i].courses = courses;
        args[i].result  = 0;
        pthread_create(&threads[i], NULL, demo_student_thread, &args[i]);
    }

    /* Release all threads simultaneously */
    pthread_barrier_wait(&barrier);

    /* Join all threads */
    for (int i = 0; i < DEMO_STUDENTS; i++) {
        pthread_join(threads[i], NULL);
        if (args[i].result) demo_success++;
        else                demo_fail++;
    }

    /* ── Final Summary ── */
    printf("\n=============================================================\n");
    printf("  FINAL SEAT ALLOCATION\n");
    printf("=============================================================\n");
    printf("%-8s %-14s %8s %8s %8s\n",
           "Course", "Name", "Total", "Enrolled", "Remaining");
    printf("%-8s %-14s %8s %8s %8s\n",
           "--------","----------","--------","--------","--------");
    for (int i = 0; i < DEMO_COURSES; i++) {
        printf("CS%-6d %-14s %8d %8d %8d\n",
               courses[i].id, courses[i].name,
               courses[i].total_seats,
               courses[i].enrolled,
               courses[i].available);
    }
    printf("\n");
    printf("Successful Registrations : %d\n", demo_success);
    printf("Failed  Registrations    : %d\n", demo_fail);
    printf("=============================================================\n\n");

    /* Cleanup */
    pthread_barrier_destroy(&barrier);
    for (int i = 0; i < DEMO_COURSES; i++)
        pthread_mutex_destroy(&courses[i].lock);

    return 0;
}
