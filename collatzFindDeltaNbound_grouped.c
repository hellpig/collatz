/*
 * collatzFindDeltaNbound_grouped.c
 * Written by ChatGPT in 2026
 *
 * The base-3 DeltaN recursion was developed by Bradley Knockel in
 * collatzFindDeltaNbound.c.  This rewrite keeps that recursion, makes its carry
 * invariant explicit, rejects modular wraparound, and groups joining pairs by
 * their larger residue so that only the nearest smaller partner is retained.
 *
 * Find a tight cumulative upper bound on DeltaN for the following Collatz-sieve
 * exclusion rule.
 *
 * Let
 *
 *     T(n) = (3*n + 1)/2,  if n is odd,
 *            n/2,          if n is even.
 *
 * For a fixed k, suppose two starting values nLarge > nSmall have length-k
 * parity vectors containing the same number c of odd/increase steps, and suppose
 * T^k(nLarge) == T^k(nSmall).  Then every number in the residue family belonging
 * to nLarge joins the corresponding smaller family after k steps.  DeltaN is
 * nLarge - nSmall.
 *
 * A direct search needs only the NEAREST smaller joining partner for each larger
 * parity vector.  The older loose-bound program maximized DeltaN over every
 * joining pair; this program instead computes
 *
 *     max over larger patterns P of
 *         min DeltaN(P,Q) over smaller joining patterns Q.
 *
 * The cumulative maximum through k also covers joins that occur before step k:
 * after two paths join, their remaining parity suffix is identical, and removing
 * that common suffix does not change DeltaN.
 *
 * ---------------------------------------------------------------------------
 * Why the recursion works
 * ---------------------------------------------------------------------------
 *
 * For a length-k parity vector with c increase steps, define s by
 *
 *     2^k T^k(n) = 3^c n + s.
 *
 * Starting with s=0, an increase at zero-based step r changes
 *
 *     s -> 3*s + 2^r,
 *
 * while a decrease leaves s unchanged.
 *
 * If two vectors have the same c and their s-values are s1 and s2, joining after
 * k steps requires
 *
 *     DeltaN = (s2 - s1) / 3^c
 *
 * to be an integer (with the sign deciding which vector starts larger).
 *
 * The recursion places increase positions from latest to earliest.  Its variable
 * "carry" obeys
 *
 *     newCarry = (carry + 2^position2 - 2^position1) / 3.
 *
 * At every level the parity tests ensure that the numerator is divisible by 3.
 * After all c increases have been placed, carry is exactly the signed DeltaN.
 * This is best viewed as a base-3 carry construction.
 *
 * The final increase positions of the two normalized vectors differ.  Any common
 * suffix may be deleted without changing DeltaN, so one final increase is fixed
 * at k-1; divisibility forces the other final increase to have the same parity,
 * hence its largest possible position is k-3.
 *
 * ---------------------------------------------------------------------------
 * Guarantee and the coefficient-stopping-time issue
 * ---------------------------------------------------------------------------
 *
 * FAST_COEFFICIENT_MODE == 1 (default):
 *
 *   The recursion considers only parity vectors for which 3^q/2^t remains above
 *   1 at every prefix t, where q is the number of increases in that prefix.  This
 *   guarantees that the represented starting value has not fallen below itself.
 *
 *   The converse is Terras's coefficient-stopping-time (CST) conjecture.  A recent
 *   result verifies t(n) = tau(n) for 2 <= n <= 2.8e19:
 *
 *     O. Rozier and C. Terracol, "Paradoxical behavior in Collatz sequences",
 *     Discrete Mathematics 349(10), 115167 (2026), Corollary 5.4.
 *     DOI: 10.1016/j.disc.2026.115167
 *     Open manuscript: https://arxiv.org/abs/2502.00948
 *
 *   Since 2^64 < 2.8e19 < 2^65, fast-mode results are covered by that finite
 *   verification for every k <= 64.  For k >= 65 they are conditional on CST (or
 *   on a later finite verification reaching 2^k).
 *
 * FAST_COEFFICIENT_MODE == 0:
 *
 *   The program allows every possible c, reconstructs each represented residue,
 *   and directly checks whether it stays above its start for all k steps.  This
 *   removes the CST dependency, but the recursion becomes enormously slower and
 *   is practical only for much smaller k.
 *
 * In either mode, modular wraparound pairs are rejected by reconstructing both
 * actual residues modulo 2^k and checking that their ordinary positive difference
 * is exactly DeltaN.
 *
 * ---------------------------------------------------------------------------
 * Build and run
 * ---------------------------------------------------------------------------
 *
 *     gcc -O3 -std=gnu11 collatzFindDeltaNbound_grouped.c -o delta_bound
 *     ./delta_bound 40
 *
 *     gcc -O3 -std=gnu11 -DFAST_COEFFICIENT_MODE=0 collatzFindDeltaNbound_grouped.c -o delta_bound_exact
 *     ./delta_bound_exact 65
 *
 * The first optional argument is the largest k.  By default the program computes
 * every k from 6 through that value so that it can print the cumulative bound.
 * A resumed run may give a starting k and the already-known cumulative bound:
 *
 *     ./delta_bound 51 38 88
 *
 * which computes k=38,...,51 while beginning with cumulative DeltaN=88.
 *
 * This implementation uses GCC/Clang __uint128_t and supports 6 <= k <= 80.
 * Arithmetic through k=80 fits in 128 bits for this construction.
 *
 * RAM is an issue (its usage is proportional to the printed number of candidates,
 * and the hash table size doubles in powers of two),
 * and time is also an issue (proportional to leaves)...
 *    k = 42   required  ~530 MB    and    228 seconds
 *    k = 43   required  ~530 MB    and    258 seconds
 *    k = 44   required ~2113 MB    and    1260 seconds
 */

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef DEFAULT_MAX_K
#define DEFAULT_MAX_K 40
#endif

#ifndef FAST_COEFFICIENT_MODE
#define FAST_COEFFICIENT_MODE 1
#endif
#define INITIAL_HASH_LOG2 10
#define HASH_LOAD_NUMERATOR 7
#define HASH_LOAD_DENOMINATOR 10

typedef __uint128_t u128;
typedef __int128_t i128;

typedef struct {
    u128 *keys;
    u128 *minimum_delta;
    unsigned char *state; /* 0=unused, 2=eligible */
    size_t capacity;
    size_t used;
    size_t eligible;
} CandidateTable;

typedef struct {
    int k;
    int c;
    int last_depth;
    u128 modulus_mask;
    u128 inverse_three_to_c;
    CandidateTable table;
    u128 leaf_pairs;
    u128 ordered_join_pairs;
} SearchContext;

static void print_u128(u128 value)
{
    char buffer[40];
    char *p = buffer + sizeof(buffer);
    *--p = '\0';

    do {
        *--p = (char)('0' + value % 10);
        value /= 10;
    } while (value != 0);

    fputs(p, stdout);
}


static u128 parse_u128_decimal(const char *text)
{
    if (text == NULL || *text == '\0') {
        fprintf(stderr, "Error: empty unsigned integer.\n");
        exit(EXIT_FAILURE);
    }

    u128 value = 0;
    for (const unsigned char *p = (const unsigned char *)text; *p != '\0'; p++) {
        if (*p < '0' || *p > '9') {
            fprintf(stderr, "Error: invalid unsigned integer: %s\n", text);
            exit(EXIT_FAILURE);
        }
        value = value * 10 + (u128)(*p - '0');
    }
    return value;
}

static uint64_t splitmix64(uint64_t x)
{
    x += UINT64_C(0x9e3779b97f4a7c15);
    x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
    return x ^ (x >> 31);
}

static uint64_t hash_u128(u128 key)
{
    uint64_t low = (uint64_t)key;
    uint64_t high = (uint64_t)(key >> 64);
    return splitmix64(low ^ splitmix64(high));
}

static void table_allocate(CandidateTable *table, size_t capacity)
{
    table->keys = (u128 *)calloc(capacity, sizeof(u128));
    table->minimum_delta = (u128 *)calloc(capacity, sizeof(u128));
    table->state = (unsigned char *)calloc(capacity, sizeof(unsigned char));
    if (table->keys == NULL || table->minimum_delta == NULL || table->state == NULL) {
        fprintf(stderr, "Error: unable to allocate candidate hash table.\n");
        exit(EXIT_FAILURE);
    }
    table->capacity = capacity;
    table->used = 0;
    table->eligible = 0;
}

static void table_free(CandidateTable *table)
{
    free(table->keys);
    free(table->minimum_delta);
    free(table->state);
    memset(table, 0, sizeof(*table));
}

static size_t table_find_slot(const CandidateTable *table, u128 key)
{
    const size_t mask = table->capacity - 1;
    size_t slot = (size_t)hash_u128(key) & mask;

    while (table->state[slot] != 0 && table->keys[slot] != key) {
        slot = (slot + 1) & mask;
    }
    return slot;
}

static void table_insert_existing(CandidateTable *table, u128 key,
                                  u128 minimum_delta, unsigned char state)
{
    size_t slot = table_find_slot(table, key);
    table->keys[slot] = key;
    table->minimum_delta[slot] = minimum_delta;
    table->state[slot] = state;
    table->used++;
    table->eligible++;
}

static void table_grow(CandidateTable *table)
{
    CandidateTable larger;
    table_allocate(&larger, table->capacity * 2);

    for (size_t i = 0; i < table->capacity; i++) {
        if (table->state[i] != 0) {
            table_insert_existing(&larger, table->keys[i],
                                  table->minimum_delta[i], table->state[i]);
        }
    }

    table_free(table);
    *table = larger;
}

/* Return the table slot for key, growing first if another insertion may be needed. */
static size_t table_prepare_slot(CandidateTable *table, u128 key)
{
    if ((table->used + 1) * HASH_LOAD_DENOMINATOR >
        table->capacity * HASH_LOAD_NUMERATOR) {
        table_grow(table);
    }
    return table_find_slot(table, key);
}

/* Newton iteration for the inverse of an odd integer modulo 2^128.
 * Masking afterward gives its inverse modulo 2^k. */
static u128 inverse_odd_mod_power_of_two(u128 odd, u128 modulus_mask)
{
    u128 inverse = 1;
    for (int i = 0; i < 7; i++) {
        inverse *= 2 - odd * inverse;
    }
    return inverse & modulus_mask;
}

/* The residue represented by a parity vector is the unique r modulo 2^k with
 * 3^c*r + s == 0 (mod 2^k).  Unsigned overflow preserves the low k bits. */
static u128 residue_from_s(u128 s, u128 inverse_three_to_c, u128 modulus_mask)
{
    return ((u128)0 - s) * inverse_three_to_c & modulus_mask;
}

#if FAST_COEFFICIENT_MODE
/* Fast sufficient test: the coefficient 3^q/2^t stays greater than 1 at every
 * nonempty prefix.  For k <= 64, the cited finite CST verification makes this
 * equivalent to actual non-reduction for every represented residue. */
static int coefficient_stays_above_one(u128 increase_mask, int k)
{
    u128 three_to_q = 1;
    u128 two_to_t = 1;

    for (int step = 0; step < k; step++) {
        two_to_t <<= 1;
        if ((increase_mask >> step) & 1) {
            three_to_q *= 3;
        }
        if (three_to_q < two_to_t) {
            return 0;
        }
    }
    return 1;
}
#endif

#if !FAST_COEFFICIENT_MODE
/* Unconditional test used when FAST_COEFFICIENT_MODE is zero. */
static int actually_stays_above_start(u128 start, u128 increase_mask, int k)
{
    u128 n = start;

    for (int step = 0; step < k; step++) {
        int expected_odd = (int)((increase_mask >> step) & 1);
        if ((int)(n & 1) != expected_odd) {
            fprintf(stderr, "Internal error: reconstructed residue has wrong parity vector.\n");
            exit(EXIT_FAILURE);
        }

        if (n & 1) {
            /* Equal to (3*n+1)/2 for odd n, with delayed overflow. */
            n = 3 * (n >> 1) + 2;
        } else {
            n >>= 1;
        }

        if (n <= start) {
            return 0;
        }
    }
    return 1;
}
#endif

static int candidate_is_eligible(u128 residue, u128 increase_mask, int k)
{
#if FAST_COEFFICIENT_MODE
    (void)residue;
    return coefficient_stays_above_one(increase_mask, k);
#else
    return actually_stays_above_start(residue, increase_mask, k);
#endif
}

/* Record a joining pair.  Only the nearest smaller partner is retained for each
 * larger residue. */
static void record_pair(SearchContext *context,
                        u128 mask1, u128 mask2,
                        u128 s1, u128 s2,
                        i128 signed_delta)
{
    if (signed_delta == 0) {
        return;
    }

    u128 larger_mask;
    u128 smaller_mask;
    u128 larger_s;
    u128 smaller_s;
    u128 delta;

    /* signed_delta = (s2-s1)/3^c. */
    if (signed_delta > 0) {
        larger_mask = mask1;
        smaller_mask = mask2;
        larger_s = s1;
        smaller_s = s2;
        delta = (u128)signed_delta;
    } else {
        larger_mask = mask2;
        smaller_mask = mask1;
        larger_s = s2;
        smaller_s = s1;
        delta = (u128)(-signed_delta);
    }

    u128 larger_residue = residue_from_s(larger_s,
                                         context->inverse_three_to_c,
                                         context->modulus_mask);
    u128 smaller_residue = residue_from_s(smaller_s,
                                          context->inverse_three_to_c,
                                          context->modulus_mask);

    /* Divisibility alone is a congruence condition.  Reject modular wraparound:
     * the ordinary residues must really be ordered and separated by DeltaN. */
    if (larger_residue <= smaller_residue ||
        larger_residue - smaller_residue != delta) {
        return;
    }

    context->ordered_join_pairs++;

    /* Store only candidates that can actually remain above their start.  The
     * number of ineligible parity vectors is vastly larger; caching all of them
     * makes the grouped algorithm consume unreasonable amounts of RAM. */
    if (!candidate_is_eligible(larger_residue, larger_mask, context->k)) {
        return;
    }

    CandidateTable *table = &context->table;
    size_t slot = table_prepare_slot(table, larger_residue);

    if (table->state[slot] == 0) {
        table->keys[slot] = larger_residue;
        table->minimum_delta[slot] = delta;
        table->state[slot] = 2;
        table->used++;
        table->eligible++;
    } else if (delta < table->minimum_delta[slot]) {
        table->minimum_delta[slot] = delta;
    }

    (void)smaller_mask; /* Kept above to make the orientation explicit. */
}

/* Place paired increase positions from latest to earliest. */
static void search_pairs(SearchContext *context,
                         int highest_position1,
                         int highest_position2,
                         i128 carry,
                         int depth,
                         u128 mask1,
                         u128 mask2,
                         u128 s1,
                         u128 s2,
                         u128 three_to_depth)
{
    int increases_after_this = context->last_depth - depth;

    for (int position1 = highest_position1;
         position1 >= increases_after_this;
         position1--) {

        int parity1 = position1 & 1;

        for (int position2 = highest_position2;
             position2 >= increases_after_this;
             position2--) {

            int parity2 = position2 & 1;
            int carry_mod_3 = (int)(carry % 3);
            if (carry_mod_3 < 0) {
                carry_mod_3 += 3;
            }

            if (carry_mod_3 == 0) {
                if (depth == 0 && position2 != context->k - 1) {
                    continue;
                }
                if (parity1 != parity2) {
                    continue;
                }
            } else if (carry_mod_3 == 1) {
                if (parity1 == 0 || parity2 != 0) {
                    continue;
                }
            } else {
                if (parity1 != 0 || parity2 == 0) {
                    continue;
                }
            }

            u128 bit1 = (u128)1 << position1;
            u128 bit2 = (u128)1 << position2;
            u128 next_mask1 = mask1 | bit1;
            u128 next_mask2 = mask2 | bit2;
            u128 next_s1 = s1 + three_to_depth * bit1;
            u128 next_s2 = s2 + three_to_depth * bit2;

            i128 numerator = carry + (i128)bit2 - (i128)bit1;
            if (numerator % 3 != 0) {
                fprintf(stderr, "Internal error: nonintegral base-3 carry.\n");
                exit(EXIT_FAILURE);
            }
            i128 next_carry = numerator / 3;

            if (depth == context->last_depth) {
                context->leaf_pairs++;
                record_pair(context, next_mask1, next_mask2,
                            next_s1, next_s2, next_carry);
            } else {
                search_pairs(context,
                             position1 - 1,
                             position2 - 1,
                             next_carry,
                             depth + 1,
                             next_mask1,
                             next_mask2,
                             next_s1,
                             next_s2,
                             three_to_depth * 3);
            }
        }
    }
}

static u128 maximum_minimum_delta(const CandidateTable *table)
{
    u128 maximum = 0;
    for (size_t i = 0; i < table->capacity; i++) {
        if (table->state[i] == 2 && table->minimum_delta[i] > maximum) {
            maximum = table->minimum_delta[i];
        }
    }
    return maximum;
}

static u128 bound_for_one_k(int k, size_t *eligible_candidates_out,
                            u128 *leaf_pairs_out, u128 *ordered_pairs_out)
{
    SearchContext context;
    memset(&context, 0, sizeof(context));
    context.k = k;
    context.modulus_mask = ((u128)1 << k) - 1;
    table_allocate(&context.table, (size_t)1 << INITIAL_HASH_LOG2);

#if FAST_COEFFICIENT_MODE
    /* Smallest integer c with 3^c > 2^k.  Integer arithmetic avoids rounding. */
    int first_c = 0;
    u128 three_to_c = 1;
    u128 two_to_k = (u128)1 << k;
    while (three_to_c <= two_to_k) {
        three_to_c *= 3;
        first_c++;
    }
#else
    int first_c = 1;
    u128 three_to_c = 3;
#endif

    /* c=k gives identical all-increase vectors.  c=k-1 is impossible after the
     * common suffix is removed, so the largest useful c is k-2. */
    for (int c = first_c; c <= k - 2; c++) {
        if (c != first_c) {
            three_to_c *= 3;
        }

        context.c = c;
        context.last_depth = c - 1;
        context.inverse_three_to_c =
            inverse_odd_mod_power_of_two(three_to_c, context.modulus_mask);

        search_pairs(&context,
                     k - 3,
                     k - 1,
                     0,
                     0,
                     0,
                     0,
                     0,
                     0,
                     1);
    }

    u128 result = maximum_minimum_delta(&context.table);
    *eligible_candidates_out = context.table.eligible;
    *leaf_pairs_out = context.leaf_pairs;
    *ordered_pairs_out = context.ordered_join_pairs;
    table_free(&context.table);
    return result;
}

static double elapsed_seconds(struct timespec start, struct timespec end)
{
    return (double)(end.tv_sec - start.tv_sec) +
           (double)(end.tv_nsec - start.tv_nsec) / 1.0e9;
}

int main(int argc, char **argv)
{
    int maximum_k = DEFAULT_MAX_K;
    int minimum_k = 6;
    u128 cumulative_bound = 0;

    if (argc >= 2) {
        maximum_k = atoi(argv[1]);
    }
    if (argc >= 3) {
        minimum_k = atoi(argv[2]);
    }
    if (argc >= 4) {
        cumulative_bound = parse_u128_decimal(argv[3]);
    }

    if (argc > 4 || minimum_k < 6 || maximum_k < minimum_k || maximum_k > 80) {
        fprintf(stderr,
                "Usage: %s [maximum_k [minimum_k [prior_cumulative_bound]]], "
                "with 6 <= minimum_k <= maximum_k <= 80\n",
                argv[0]);
        return EXIT_FAILURE;
    }

#if FAST_COEFFICIENT_MODE
    printf("Mode: fast coefficient-prefix mode\n");
    if (maximum_k <= 64) {
        printf("Finite CST verification cited in the source covers all residues used.\n");
    } else {
        printf("WARNING: results above k=64 are conditional on the CST conjecture\n");
        printf("or on a finite CST verification extending beyond 2^k.\n");
    }
#else
    printf("Mode: unconditional direct-survival mode (very slow)\n");
#endif

    if (minimum_k > 6 && argc < 4) {
        printf("WARNING: no prior cumulative bound was supplied; cumulative values\n");
        printf("will cover only k=%d through the current k.\n", minimum_k);
    }

    for (int k = minimum_k; k <= maximum_k; k++) {
        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);

        size_t eligible_candidates = 0;
        u128 leaf_pairs = 0;
        u128 ordered_pairs = 0;
        u128 raw_bound = bound_for_one_k(k, &eligible_candidates,
                                        &leaf_pairs, &ordered_pairs);
        if (raw_bound > cumulative_bound) {
            cumulative_bound = raw_bound;
        }

        clock_gettime(CLOCK_MONOTONIC, &end);

        printf("k=%d  raw=", k);
        print_u128(raw_bound);
        printf("  cumulative=");
        print_u128(cumulative_bound);
        printf("  candidates=%zu  leaves=", eligible_candidates);
        print_u128(leaf_pairs);
        printf("  ordered_pairs=");
        print_u128(ordered_pairs);
        printf("  time=%.6f s\n", elapsed_seconds(start, end));
        fflush(stdout);
    }

    return EXIT_SUCCESS;
}
