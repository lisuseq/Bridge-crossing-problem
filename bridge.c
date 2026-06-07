#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>

static int N;
static int use_cond;

typedef enum { NONE = 0, A_TO_B = 1, B_TO_A = 2 } Direction;

// zmienne wyświetlania wspólne dla obu trybów

static int arrived_a = 0, arrived_b = 0;
static int queue_a   = 0, queue_b   = 0;
static int bridge_car = -1;
static Direction bridge_car_dir;
static pthread_mutex_t disp_mtx = PTHREAD_MUTEX_INITIALIZER;

// Wyświetlanie

static void print_state(void)
{
    printf("A-%d %d>>> ", arrived_a, queue_a);
    if (bridge_car >= 0) {
        if (bridge_car_dir == A_TO_B) printf("[>> %d >>] ", bridge_car);
        else                          printf("[<< %d <<] ", bridge_car);
    } else {
        printf("[-------] ");
    }
    printf("<<<%d %d-B\n", queue_b, arrived_b);
    fflush(stdout);
}

static void join_queue_a(void)
{
    pthread_mutex_lock(&disp_mtx);
    queue_a++;
    print_state();
    pthread_mutex_unlock(&disp_mtx);
}
static void join_queue_b(void)
{
    pthread_mutex_lock(&disp_mtx);
    queue_b++;
    print_state();
    pthread_mutex_unlock(&disp_mtx);
}
static void leave_queue_a(void)
{
    pthread_mutex_lock(&disp_mtx);
    queue_a--;
    pthread_mutex_unlock(&disp_mtx);
}
static void leave_queue_b(void)
{
    pthread_mutex_lock(&disp_mtx);
    queue_b--;
    pthread_mutex_unlock(&disp_mtx);
}
static void enter_bridge(int id, Direction dir)
{
    pthread_mutex_lock(&disp_mtx);
    bridge_car = id;
    bridge_car_dir = dir;
    print_state();
    pthread_mutex_unlock(&disp_mtx);
}
static void leave_bridge(Direction dir)
{
    pthread_mutex_lock(&disp_mtx);
    bridge_car = -1;
    if (dir == A_TO_B) arrived_b++; else arrived_a++;
    print_state();
    pthread_mutex_unlock(&disp_mtx);
}
static void print_round_end(void)
{
    pthread_mutex_lock(&disp_mtx);
    printf("--- koniec rundy: %d dotarlo do A, %d dotarlo do B ---\n", arrived_a, arrived_b);
    arrived_a = 0; arrived_b = 0;
    print_state();
    pthread_mutex_unlock(&disp_mtx);
}


// Bariera dla trybu A – tylko semafory i mutexy. Wymagana aby zapobiec wyścigowi między rundami.


static pthread_mutex_t bar_a_mtx = PTHREAD_MUTEX_INITIALIZER;
static sem_t           bar_a_sem1, bar_a_sem2;  // dwie fazy – zapobiega wyścigowi
static int             bar_a_count = 0;

static void barrier_a(int after_round)
{
    pthread_mutex_lock(&bar_a_mtx);
    bar_a_count++;
    if (bar_a_count == N){
        bar_a_count = 0;
        if (after_round){
            print_round_end();
        }
        for (int i = 0; i < N; i++){
            sem_post(&bar_a_sem1);
        }
    }
    pthread_mutex_unlock(&bar_a_mtx);
    sem_wait(&bar_a_sem1);

    /* faza 2: czekaj aż wszyscy wyszli z sem_wait – zapobiega ponownemu użyciu */
    pthread_mutex_lock(&bar_a_mtx);
    bar_a_count++;
    if (bar_a_count == N) {
        bar_a_count = 0;
        for (int i = 0; i < N; i++) sem_post(&bar_a_sem2);
    }
    pthread_mutex_unlock(&bar_a_mtx);
    sem_wait(&bar_a_sem2);
}


// Bariera dla trybu B – zmienne warunkowe. Wymagana aby zapobiec wyścigowi między rundami.


static pthread_mutex_t bar_b_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  bar_b_cond = PTHREAD_COND_INITIALIZER;
static int             bar_b_count = 0;
static int             bar_b_gen   = 0;

static void barrier_b(int after_round)
{
    pthread_mutex_lock(&bar_b_mtx);
    int gen = bar_b_gen;
    bar_b_count++;
    if (bar_b_count == N) {
        bar_b_count = 0;
        bar_b_gen++;
        if (after_round) print_round_end();
        pthread_cond_broadcast(&bar_b_cond);
    } else {
        while (gen == bar_b_gen)
            pthread_cond_wait(&bar_b_cond, &bar_b_mtx);
    }
    pthread_mutex_unlock(&bar_b_mtx);
}

static void do_barrier(int after_round)
{
    if (use_cond) barrier_b(after_round);
    else          barrier_a(after_round);
}

// Tryb A – mutexy + semafory

static sem_t           bridge_sem;
static pthread_mutex_t bridge_mtx_a = PTHREAD_MUTEX_INITIALIZER;
static Direction       dir_a = NONE;
static int             on_a  = 0;

static void a_lock(int id, Direction dir)
{
    while (1) {
        sem_wait(&bridge_sem);
        pthread_mutex_lock(&bridge_mtx_a);
        if (dir_a == NONE || dir_a == dir) {
            dir_a = dir; on_a++;
            pthread_mutex_unlock(&bridge_mtx_a);
            break;
        }
        pthread_mutex_unlock(&bridge_mtx_a);
        sem_post(&bridge_sem);
        usleep(1000 + rand() % 5000);
    }
    if (dir == A_TO_B) leave_queue_a(); else leave_queue_b();
    enter_bridge(id, dir);
}

static void a_unlock(Direction dir)
{
    pthread_mutex_lock(&bridge_mtx_a);
    if (--on_a == 0) dir_a = NONE;
    pthread_mutex_unlock(&bridge_mtx_a);
    leave_bridge(dir);
    sem_post(&bridge_sem);
}

// Tryb B – zmienne warunkowe

static pthread_mutex_t bridge_mtx_b = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  cond_ab = PTHREAD_COND_INITIALIZER;
static pthread_cond_t  cond_ba = PTHREAD_COND_INITIALIZER;
static Direction       dir_b   = NONE;
static int             on_b    = 0;

static void b_lock(int id, Direction dir)
{
    pthread_mutex_lock(&bridge_mtx_b);
    while (on_b > 0 || (dir_b != NONE && dir_b != dir))
        pthread_cond_wait(dir == A_TO_B ? &cond_ab : &cond_ba, &bridge_mtx_b);
    dir_b = dir; on_b++;
    if (dir == A_TO_B) leave_queue_a(); else leave_queue_b();
    /* enter_bridge pod mutexa – żadne inne auto nie wejdzie między unlock a enter */
    pthread_mutex_lock(&disp_mtx);
    bridge_car = id; bridge_car_dir = dir;
    print_state();
    pthread_mutex_unlock(&disp_mtx);
    pthread_mutex_unlock(&bridge_mtx_b);
}

static void b_unlock(Direction dir)
{
    pthread_mutex_lock(&bridge_mtx_b);
    if (--on_b == 0) {
        dir_b = NONE;
        pthread_cond_broadcast(&cond_ab);
        pthread_cond_broadcast(&cond_ba);
    }
    pthread_mutex_unlock(&bridge_mtx_b);
    leave_bridge(dir);
}

// Wątki samochodów – wspólne dla obu trybów

static void *car_thread(void *arg)
{
    int id    = (int)(long)arg;
    Direction dir = (id % 2 == 0) ? A_TO_B : B_TO_A;

    while (1) {
        usleep(50000 + rand() % 150000);
        if (dir == A_TO_B) join_queue_a(); else join_queue_b();

        /* czekaj aż wszyscy będą w kolejkach */
        do_barrier(0);

        if (use_cond) b_lock(id, dir); else a_lock(id, dir);
        usleep(200000 + rand() % 200000);
        if (use_cond) b_unlock(dir);   else a_unlock(dir);

        /* czekaj aż wszyscy przejadą – ostatni resetuje liczniki */
        do_barrier(1);

        dir = (dir == A_TO_B) ? B_TO_A : A_TO_B;
    }
    return NULL;
}


int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Użycie: %s <N> <a|b>\n", argv[0]);
        return 1;
    }
    N = atoi(argv[1]);
    if (N < 1) {
        fprintf(stderr, "N >= 1\n");
        return 1; 
    }
    use_cond = (argv[2][0] == 'b');

    srand((unsigned)time(NULL));

    printf("=== Wąski most | N=%d | tryb %c ===\n\n", N, use_cond ? 'B' : 'A');

    sem_init(&bridge_sem, 0, 1);
    sem_init(&bar_a_sem1, 0, 0);
    sem_init(&bar_a_sem2, 0, 0);

    pthread_t *t = malloc(N * sizeof(pthread_t));
    for (int i = 0; i < N; i++)
        pthread_create(&t[i], NULL, car_thread, (void*)(long)(i+1));
    for (int i = 0; i < N; i++)
        pthread_join(t[i], NULL);

    free(t);
    sem_destroy(&bridge_sem);
    sem_destroy(&bar_a_sem1);
    sem_destroy(&bar_a_sem2);
    return 0;
}
