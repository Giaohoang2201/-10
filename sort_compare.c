/*
 * 과제 10: 정렬 알고리즘의 성능 및 안정성 비교
 *  - 선택 정렬, 버블 정렬, 버블 정렬(조기 종료), 삽입 정렬(>, >=)
 *  - 숫자 비교 횟수와 데이터 이동 횟수 측정
 *  - 입력 상태별(무작위/정렬/역순/부분 정렬) 성능 비교
 *  - 안정성(Stability) 검증
 *
 * 컴파일: gcc -O2 -o sort_compare sort_compare.c
 * 실행  : ./sort_compare
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define N         10000      /* 데이터 개수 */
#define TRIALS    100        /* 무작위 데이터 반복 횟수 */
#define KEY_MAX   1000000    /* 키의 최댓값 */
#define SEED      20261025ULL

/* 하나의 데이터: 두 개의 키 + 원래 입력 순서(안정성 검증용, 정렬 기준 아님) */
typedef struct {
    int key1;
    int key2;
    int order;
} Data;

/* 전역 카운터: 정렬 함수 내부에서만 증가시킨다 */
static long long g_cmp;   /* 숫자 비교 횟수 (key1 비교) */
static long long g_mov;   /* 데이터 이동 횟수 (데이터 전체 복사/대입) */

typedef struct {
    long long cmp;
    long long mov;
} Stat;

/* 정렬 알고리즘 종류 */
typedef enum {
    ALG_SELECTION = 0,
    ALG_BUBBLE,
    ALG_BUBBLE_EARLY,
    ALG_INSERTION_GT,    /* Case A: data[j].key1 >  current.key1 */
    ALG_INSERTION_GE,    /* Case B: data[j].key1 >= current.key1 */
    ALG_COUNT
} Alg;

static const char *ALG_NAME[ALG_COUNT] = {
    "선택 정렬",
    "버블 정렬",
    "버블 정렬 (조기 종료)",
    "삽입 정렬 (>)",
    "삽입 정렬 (>=)"
};

/* ---------------------------------------------------------------
 * 난수 생성기 (xorshift64*): 플랫폼과 무관하게 0~1,000,000 범위 보장
 * --------------------------------------------------------------- */
static uint64_t rng_state = SEED;

static uint64_t rng_next(void) {
    uint64_t x = rng_state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    rng_state = x;
    return x * 2685821657736338717ULL;
}

static int rand_range(int lo, int hi) {            /* lo ~ hi (양끝 포함) */
    return lo + (int)(rng_next() % (uint64_t)(hi - lo + 1));
}

/* ---------------------------------------------------------------
 * 데이터 이동 횟수를 세는 교환 함수 (임시 변수 사용: 이동 3회)
 * --------------------------------------------------------------- */
static void swap_data(Data *a, Data *b) {
    Data temp = *a;  g_mov++;     /* temp = a  */
    *a = *b;         g_mov++;     /* a = b     */
    *b = temp;       g_mov++;     /* b = temp  */
}

/* ---------------------------------------------------------------
 * [1] 선택 정렬
 *  - 정렬되지 않은 부분에서 key1이 가장 작은 데이터를 찾아
 *    현재 위치와 교환한다. (최솟값이 이미 제자리면 교환 생략)
 * --------------------------------------------------------------- */
static void selection_sort(Data *a, int n) {
    for (int i = 0; i < n - 1; i++) {
        int min = i;
        for (int j = i + 1; j < n; j++) {
            g_cmp++;                                   /* key1 비교 1회 */
            if (a[j].key1 < a[min].key1) min = j;
        }
        if (min != i) swap_data(&a[i], &a[min]);
    }
}

/* ---------------------------------------------------------------
 * [2] 버블 정렬 (일반): 정해진 모든 순회를 수행
 * --------------------------------------------------------------- */
static void bubble_sort(Data *a, int n) {
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - 1 - i; j++) {
            g_cmp++;
            if (a[j].key1 > a[j + 1].key1) swap_data(&a[j], &a[j + 1]);
        }
    }
}

/* ---------------------------------------------------------------
 * [2'] 버블 정렬 (조기 종료): 한 순회에서 교환이 없으면 종료
 * --------------------------------------------------------------- */
static void bubble_sort_early(Data *a, int n) {
    for (int i = 0; i < n - 1; i++) {
        int swapped = 0;
        for (int j = 0; j < n - 1 - i; j++) {
            g_cmp++;
            if (a[j].key1 > a[j + 1].key1) {
                swap_data(&a[j], &a[j + 1]);
                swapped = 1;
            }
        }
        if (!swapped) break;           /* 교환이 한 번도 없으면 정렬 완료 */
    }
}

/* ---------------------------------------------------------------
 * [3] 삽입 정렬
 *  - strict == 1 : Case A (data[j].key1 >  current.key1)
 *  - strict == 0 : Case B (data[j].key1 >= current.key1)
 *  - 임시 변수 저장 1회, 한 칸 이동 1회씩, 최종 위치 대입 1회를 모두 센다.
 * --------------------------------------------------------------- */
static void insertion_sort(Data *a, int n, int strict) {
    for (int i = 1; i < n; i++) {
        Data current = a[i];  g_mov++;                 /* 임시 변수에 저장 */
        int j = i - 1;
        while (j >= 0) {
            g_cmp++;                                   /* key1 비교 1회 */
            int move = strict ? (a[j].key1 >  current.key1)
                              : (a[j].key1 >= current.key1);
            if (!move) break;
            a[j + 1] = a[j];  g_mov++;                 /* 한 칸 뒤로 이동 */
            j--;
        }
        a[j + 1] = current;  g_mov++;                  /* 최종 위치에 대입 */
    }
}

/* 알고리즘 실행 후 비교/이동 횟수를 반환 */
static Stat run_sort(Alg alg, Data *a, int n) {
    g_cmp = 0;
    g_mov = 0;
    switch (alg) {
        case ALG_SELECTION:    selection_sort(a, n);      break;
        case ALG_BUBBLE:       bubble_sort(a, n);         break;
        case ALG_BUBBLE_EARLY: bubble_sort_early(a, n);   break;
        case ALG_INSERTION_GT: insertion_sort(a, n, 1);   break;
        case ALG_INSERTION_GE: insertion_sort(a, n, 0);   break;
        default: break;
    }
    Stat s = { g_cmp, g_mov };
    return s;
}

/* ---------------------------------------------------------------
 * 검증 함수들
 * --------------------------------------------------------------- */
/* key1 기준 오름차순인지 확인 */
static int is_sorted(const Data *a, int n) {
    for (int i = 1; i < n; i++)
        if (a[i - 1].key1 > a[i].key1) return 0;
    return 1;
}

/* key1이 같은 인접 데이터의 원래 순서가 뒤바뀐 횟수 (0이면 안정) */
static long long stability_violations(const Data *a, int n) {
    long long v = 0;
    for (int i = 1; i < n; i++)
        if (a[i - 1].key1 == a[i].key1 && a[i - 1].order > a[i].order) v++;
    return v;
}

/* 역전 쌍(inversion) 개수: 정렬 정도를 나타내는 척도 (O(n^2), 준비 과정) */
static long long count_inversions(const Data *a, int n) {
    long long inv = 0;
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            if (a[i].key1 > a[j].key1) inv++;
    return inv;
}

/* ---------------------------------------------------------------
 * 시험 데이터 생성 (비교/이동 횟수에 포함되지 않는 준비 과정)
 * --------------------------------------------------------------- */
static void make_random(Data *a, int n) {
    for (int i = 0; i < n; i++) {
        a[i].key1  = rand_range(0, KEY_MAX);
        a[i].key2  = rand_range(0, KEY_MAX);
        a[i].order = i;
    }
}

static int cmp_for_qsort(const void *x, const void *y) {
    const Data *p = (const Data *)x, *q = (const Data *)y;
    if (p->key1 != q->key1) return (p->key1 > q->key1) - (p->key1 < q->key1);
    return (p->order > q->order) - (p->order < q->order);
}

static void make_sorted(Data *dst, const Data *src, int n) {
    memcpy(dst, src, sizeof(Data) * n);
    qsort(dst, n, sizeof(Data), cmp_for_qsort);   /* 준비용 정렬 (측정 대상 아님) */
}

static void make_reverse(Data *dst, const Data *sorted, int n) {
    for (int i = 0; i < n; i++) dst[i] = sorted[n - 1 - i];
}

/* 부분 정렬: 정렬된 데이터에서 서로 다른 두 위치를 swaps번 무작위 교환 */
static void make_partial(Data *dst, const Data *sorted, int n, int swaps) {
    memcpy(dst, sorted, sizeof(Data) * n);
    for (int k = 0; k < swaps; k++) {
        int i = rand_range(0, n - 1);
        int j = rand_range(0, n - 1);
        if (i == j) { k--; continue; }
        Data t = dst[i]; dst[i] = dst[j]; dst[j] = t;   /* 준비 과정 */
    }
}

/* ---------------------------------------------------------------
 * 출력 도우미
 * --------------------------------------------------------------- */
static void print_line(void) {
    printf("\n------------------------------------\n\n");
}

/* 무작위 데이터 100회 반복 실험 */
static void experiment_random(void) {
    static Data original[N], work[N];
    long long total_cmp[ALG_COUNT] = {0};
    long long total_mov[ALG_COUNT] = {0};
    int all_ok = 1;

    for (int t = 0; t < TRIALS; t++) {
        make_random(original, N);                  /* 반복마다 새 난수 데이터 */
        for (int alg = 0; alg < ALG_COUNT; alg++) {
            memcpy(work, original, sizeof(original));   /* 동일한 복사본 제공 */
            Stat s = run_sort((Alg)alg, work, N);
            total_cmp[alg] += s.cmp;
            total_mov[alg] += s.mov;
            if (!is_sorted(work, N)) {
                all_ok = 0;
                printf("[오류] %d회차 %s 정렬 결과가 올바르지 않음\n", t + 1, ALG_NAME[alg]);
            }
        }
    }

    printf("무작위 데이터 (Random Data)\n");
    printf("시행 횟수 (Number of Trials) : %d\n", TRIALS);
    printf("정렬 결과 검증 (key1 오름차순) : %s\n", all_ok ? "모든 시행 통과 (OK)" : "실패");

    for (int alg = 0; alg < ALG_COUNT; alg++) {
        printf("\n%s\n", ALG_NAME[alg]);
        printf("총 비교 횟수 (Total Comparisons)    : %lld\n", total_cmp[alg]);
        printf("평균 비교 횟수 (Average Comparisons) : %.1f\n", (double)total_cmp[alg] / TRIALS);
        printf("총 이동 횟수 (Total Movements)      : %lld\n", total_mov[alg]);
        printf("평균 이동 횟수 (Average Movements)   : %.1f\n", (double)total_mov[alg] / TRIALS);
    }

    double red = 100.0 * (1.0 - (double)total_cmp[ALG_BUBBLE_EARLY] / (double)total_cmp[ALG_BUBBLE]);
    printf("\n[참고] 조기 종료로 줄어든 버블 정렬 평균 비교 횟수 : %.1f회 (%.4f%% 감소)\n",
           ((double)total_cmp[ALG_BUBBLE] - (double)total_cmp[ALG_BUBBLE_EARLY]) / TRIALS, red);
}

/* 특정 입력 상태에 대한 1회 실험 */
static void experiment_state(const char *title, const Data *input, int n) {
    static Data work[N];
    printf("%s\n", title);
    printf("역전 쌍 개수 (Inversions) : %lld\n", count_inversions(input, n));

    for (int alg = 0; alg < ALG_COUNT; alg++) {
        if (alg == ALG_INSERTION_GE) continue;    /* 성능 비교는 삽입 정렬(>)만 사용 */
        memcpy(work, input, sizeof(Data) * n);
        Stat s = run_sort((Alg)alg, work, n);
        printf("\n%s\n", ALG_NAME[alg]);
        printf("비교 횟수 (Comparisons) : %lld\n", s.cmp);
        printf("이동 횟수 (Movements)   : %lld\n", s.mov);
        printf("정렬 결과 검증          : %s\n", is_sorted(work, n) ? "OK" : "실패");
    }
}

/* 작은 예시 데이터로 정렬 전후 순서 확인 (과제 예시와 동일) */
static void stability_small_example(void) {
    Data base[4] = {
        {527, 100, 0},
        {527, 200, 1},
        {125, 300, 2},
        {892, 400, 3}
    };
    printf("[검증 1] 과제 예시 데이터 (4개)\n");
    printf("원본 : ");
    for (int i = 0; i < 4; i++) printf("(%d,%d) ", base[i].key1, base[i].key2);
    printf("\n");

    for (int alg = 0; alg < ALG_COUNT; alg++) {
        if (alg == ALG_BUBBLE_EARLY) continue;     /* 일반 버블과 비교 방식이 동일 */
        Data w[4];
        memcpy(w, base, sizeof(base));
        run_sort((Alg)alg, w, 4);
        printf("%-18s : ", ALG_NAME[alg]);
        for (int i = 0; i < 4; i++) printf("(%d,%d) ", w[i].key1, w[i].key2);
        printf(" -> %s\n", stability_violations(w, 4) == 0 ? "안정" : "불안정");
    }
}

/* key1 중복이 매우 많은 대량 데이터로 안정성 검증 */
static void stability_large(void) {
    static Data base[N], work[N];
    const int KEY_RANGE = 99;          /* key1을 0~99로 제한하여 중복을 많이 발생시킴 */
    for (int i = 0; i < N; i++) {
        base[i].key1  = rand_range(0, KEY_RANGE);
        base[i].key2  = rand_range(0, KEY_MAX);
        base[i].order = i;
    }

    /* 중복된 key1을 가진 데이터가 몇 개인지 집계 */
    int cnt[KEY_RANGE + 1];
    memset(cnt, 0, sizeof(cnt));
    for (int i = 0; i < N; i++) cnt[base[i].key1]++;
    int dup_groups = 0, dup_items = 0;
    for (int k = 0; k <= KEY_RANGE; k++)
        if (cnt[k] > 1) { dup_groups++; dup_items += cnt[k]; }

    printf("\n[검증 2] 중복 키 대량 데이터 (N=%d, key1 범위 0~%d)\n", N, KEY_RANGE);
    printf("key1이 중복되는 그룹 수 : %d, 그 그룹에 속한 데이터 수 : %d\n", dup_groups, dup_items);

    for (int alg = 0; alg < ALG_COUNT; alg++) {
        if (alg == ALG_BUBBLE_EARLY) continue;
        memcpy(work, base, sizeof(base));
        run_sort((Alg)alg, work, N);
        long long v = stability_violations(work, N);
        printf("%-18s : 정렬 %s, 순서 역전 횟수 = %lld -> %s\n",
               ALG_NAME[alg],
               is_sorted(work, N) ? "OK" : "실패",
               v,
               v == 0 ? "안정 (Stable)" : "불안정 (Unstable)");
    }
}

/* 조기 종료 버블 정렬도 안정성을 해치지 않는지 확인 */
static void stability_bubble_early(void) {
    static Data base[N], work[N];
    for (int i = 0; i < N; i++) {
        base[i].key1  = rand_range(0, 99);
        base[i].key2  = rand_range(0, KEY_MAX);
        base[i].order = i;
    }
    memcpy(work, base, sizeof(base));
    run_sort(ALG_BUBBLE_EARLY, work, N);
    long long v = stability_violations(work, N);
    printf("%-18s : 정렬 %s, 순서 역전 횟수 = %lld -> %s\n",
           ALG_NAME[ALG_BUBBLE_EARLY],
           is_sorted(work, N) ? "OK" : "실패", v,
           v == 0 ? "안정 (Stable)" : "불안정 (Unstable)");
}

/* 최종 요약 출력용: 안정성 결과 (대량 데이터 기준) */
static void stability_summary(void) {
    static Data base[N], work[N];
    for (int i = 0; i < N; i++) {
        base[i].key1  = rand_range(0, 99);
        base[i].key2  = rand_range(0, KEY_MAX);
        base[i].order = i;
    }
    const char *label[ALG_COUNT] = {
        "선택 정렬         ", "버블 정렬         ", "버블 정렬 (조기종료)",
        "삽입 정렬 (>)     ", "삽입 정렬 (>=)    "
    };
    for (int alg = 0; alg < ALG_COUNT; alg++) {
        memcpy(work, base, sizeof(base));
        run_sort((Alg)alg, work, N);
        long long v = stability_violations(work, N);
        printf("%s : %s (역전 %lld건)\n", label[alg],
               v == 0 ? "안정 (Stable)" : "불안정 (Unstable)", v);
    }
}

int main(void) {
    printf("데이터 개수 (Data Count) : %d\n\n", N);

    /* 1) 무작위 데이터 100회 */
    experiment_random();
    print_line();

    /* 2) 입력 상태별 실험: 동일한 원본으로부터 정렬/역순/부분 정렬 생성 */
    static Data base[N], sorted[N], reversed[N], partial[N];
    make_random(base, N);
    make_sorted(sorted, base, N);
    make_reverse(reversed, sorted, N);
    const int PARTIAL_SWAPS = N / 20;       /* 5%: 500번의 무작위 위치 교환 */
    make_partial(partial, sorted, N, PARTIAL_SWAPS);

    experiment_state("이미 정렬된 데이터 (Sorted Data)", sorted, N);
    print_line();
    experiment_state("역순 정렬 데이터 (Reverse-Sorted Data)", reversed, N);
    print_line();
    printf("[부분 정렬 생성 방법] 정렬된 데이터에서 무작위 두 위치를 %d번(전체의 5%%) 교환\n",
           PARTIAL_SWAPS);
    experiment_state("부분 정렬 데이터 (Partially Sorted Data)", partial, N);
    print_line();

    /* 3) 안정성 검증 */
    printf("안정성 검증 (Stability Verification)\n\n");
    stability_small_example();
    stability_large();
    stability_bubble_early();
    printf("\n[최종 요약]\n");
    stability_summary();

    return 0;
}
