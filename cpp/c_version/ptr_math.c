/*
    ptr_math.c
    ==========

    Pointer arithmetic workbook.

    Mood:
    -> starts hand-holdy
    -> gets spicy gradually
    -> screams when C is being C
    -> no mystical "pointer magic"; just addresses, element sizes, and rules
    -> occasional 3rd/4th gen girl group jokes because C memory management is
       basically a survival show with fewer fancams and more undefined behavior

    Compile/run:

        cmake -S c_version -B c_version/build
        cmake --build c_version/build
        ./c_version/build/ptr_math

    Why pointer arithmetic exists
    -----------------------------

    C was designed close to the machine. Arrays are contiguous memory. Pointer
    arithmetic is how C lets you walk through that contiguous memory without
    needing a separate indexing abstraction.

    The key mental model:

        int numbers[4] = {10, 20, 30, 40};
        int *p = numbers;

        numbers[i] == *(numbers + i)
        p[i]       == *(p + i)

    The scary part:

    `p + 1` does NOT mean "advance one byte."

    It means:

        advance by one element of the pointed-to type

    So:

        int    *p; p + 1 advances sizeof(int) bytes
        double *p; p + 1 advances sizeof(double) bytes
        char   *p; p + 1 advances sizeof(char), which is 1 byte

    When to use pointer arithmetic
    ------------------------------

    Good uses:
    -> walking a buffer with begin/end pointers
    -> parsing bytes with unsigned char *
    -> implementing low-level containers
    -> using pointer difference to count elements between two pointers
    -> interoperating with C APIs that hand you pointer + length

    Usually prefer indexing when:
    -> it is clearer
    -> you need bounds checks
    -> you are writing application logic rather than buffer-walking code

    Hard safety rules
    -----------------

    -> Pointer arithmetic is only defined within the same array object.
    -> One-past-the-end pointer is legal to form, but not legal to dereference.
    -> Pointer subtraction is only defined for pointers into the same array.
    -> If you cast to char * or unsigned char *, arithmetic becomes byte-based.
    -> If you lose the length/end pointer, you are walking through fog with a chainsaw.
*/

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SECTION(title) printf("\n=== %s ===\n", title)
#define CHECK(expr)                                                            \
    do {                                                                       \
        if (!(expr)) {                                                         \
            fprintf(stderr, "CHECK FAILED: %s at %s:%d\n", #expr, __FILE__,    \
                    __LINE__);                                                 \
            exit(EXIT_FAILURE);                                                \
        }                                                                      \
    } while (0)

/*
    Demo 1: array indexing is pointer arithmetic
    --------------------------------------------

    a[i] is defined as *(a + i).

    Yes, really. That also means i[a] works for arrays, because:

        i[a] == *(i + a)

    Please do not write i[a] in real code unless you are trying to make your
    coworkers put your chair on the roof. That syntax is giving "NMIXX change-up"
    but for people who enjoy code review violence.
*/
static void demo_indexing_is_pointer_arithmetic(void) {
    SECTION("array indexing is pointer arithmetic");

    int numbers[4] = {10, 20, 30, 40};
    int *p = numbers;

    printf("numbers[2]      = %d\n", numbers[2]);
    printf("*(numbers + 2)  = %d\n", *(numbers + 2));
    printf("p[2]            = %d\n", p[2]);
    printf("*(p + 2)        = %d\n", *(p + 2));

    CHECK(numbers[2] == *(numbers + 2));
    CHECK(p[2] == *(p + 2));
}

/*
    Demo 2: pointer arithmetic scales by pointed-to type
    ----------------------------------------------------

    This is the "ohhhh" moment.

    int * plus 1 moves by sizeof(int) bytes.
    char * plus 1 moves by 1 byte.

    We print addresses as void * because printf("%p") expects void *.
*/
static void demo_scaling_by_type(void) {
    SECTION("pointer arithmetic scales by type");

    int numbers[4] = {10, 20, 30, 40};
    int *intPtr = numbers;
    unsigned char *bytePtr = (unsigned char *)numbers;

    printf("intPtr        = %p\n", (void *)intPtr);
    printf("intPtr + 1    = %p  (moves by sizeof(int) = %zu bytes)\n",
           (void *)(intPtr + 1),
           sizeof(int));
    printf("bytePtr       = %p\n", (void *)bytePtr);
    printf("bytePtr + 1   = %p  (moves by 1 byte)\n", (void *)(bytePtr + 1));

    CHECK((unsigned char *)(intPtr + 1) == bytePtr + sizeof(int));
}

/*
    Demo 3: begin/end pointer loops
    -------------------------------

    Common C style:

        for (p = begin; p != end; ++p) { ... }

    end points one past the last element.

    Legal:
    -> comparing p to end
    -> stopping when p == end

    Illegal:
    -> *end
*/
static int sum_with_pointers(const int *begin, const int *end) {
    int total = 0;
    for (const int *p = begin; p != end; ++p) {
        total += *p;
    }
    return total;
}

static void demo_begin_end_loop(void) {
    SECTION("begin/end pointer loop");

    int numbers[5] = {1, 2, 3, 4, 5};
    const int *begin = numbers;
    const int *end = numbers + 5;

    printf("sum = %d\n", sum_with_pointers(begin, end));
    CHECK(sum_with_pointers(begin, end) == 15);
}

/*
    Demo 4: pointer subtraction gives element distance
    --------------------------------------------------

    If two pointers point into the same array, subtracting them gives a ptrdiff_t.

    ptrdiff_t:
    -> signed integer type for pointer differences
    -> use %td in printf

    Important:
    -> p2 - p1 counts elements, not bytes
*/
static void demo_pointer_difference(void) {
    SECTION("pointer difference counts elements");

    int numbers[8] = {0};
    int *first = &numbers[1];
    int *last = &numbers[6];
    ptrdiff_t distance = last - first;

    printf("&numbers[6] - &numbers[1] = %td elements\n", distance);
    CHECK(distance == 5);
}

/*
    Demo 5: walking bytes
    ---------------------

    char * and unsigned char * are the byte-walking pointer types.

    Prefer unsigned char * for raw bytes:
    -> values are 0..UCHAR_MAX style, not negative signed-char surprises

    Think of unsigned char * as the ITZY shoulder move of C memory:
    -> precise
    -> byte-level
    -> dangerous if you imitate it without stretching first
*/
static unsigned int byte_sum(const unsigned char *bytes, size_t len) {
    unsigned int total = 0;
    for (const unsigned char *p = bytes; p != bytes + len; ++p) {
        total += *p;
    }
    return total;
}

static void demo_byte_walking(void) {
    SECTION("byte walking with unsigned char *");

    unsigned char bytes[4] = {1, 2, 3, 4};
    printf("byte sum = %u\n", byte_sum(bytes, 4));
    CHECK(byte_sum(bytes, 4) == 10);
}

/*
    Demo 6: struct arrays
    ---------------------

    Pointer arithmetic works on structs too.

    Player *p;
    p + 1 advances by sizeof(Player).
*/
typedef struct {
    const char *name;
    int score;
} TinyPlayer;

static int total_scores(const TinyPlayer *begin, const TinyPlayer *end) {
    int total = 0;
    for (const TinyPlayer *player = begin; player != end; ++player) {
        total += player->score;
    }
    return total;
}

static void demo_struct_array_walk(void) {
    SECTION("struct array pointer walk");

    TinyPlayer players[3] = {
        {"Miyeon", 10},
        {"Minnie", 20},
        {"Soyeon", 30},
    };

    CHECK(total_scores(players, players + 3) == 60);
    printf("total score = %d\n", total_scores(players, players + 3));
}

/*
    Challenges
    ==========

    Same contract as c_artillery:
    -> broken functions compile
    -> main calls wrappers
    -> output says SUCCESS or FAIL
    -> edit the challenge_* function bodies to make them pass
*/

/*
    Challenge 1: get element by pointer offset
    ------------------------------------------

    Goal:
    Return values[index] using pointer arithmetic, not [].

    Correct idea:
    -> *(values + index)

    Broken version always returns the first element.
*/
static int challenge_get_by_offset(const int *values, size_t index) {
    (void)index;
    return *values; /* TODO: fix me */
}

static int challenge_get_by_offset_works(void) {
    int values[4] = {11, 22, 33, 44};
    return challenge_get_by_offset(values, 2) == 33;
}

/*
    Challenge 2: sum begin/end range
    --------------------------------

    Goal:
    Sum [begin, end), meaning begin included and end excluded.

    Broken version stops one element too early.
*/
static int challenge_sum_range(const int *begin, const int *end) {
    int total = 0;
    for (const int *p = begin; p + 1 != end; ++p) { /* TODO: fix me */
        total += *p;
    }
    return total;
}

static int challenge_sum_range_works(void) {
    int values[4] = {1, 2, 3, 4};
    return challenge_sum_range(values, values + 4) == 10;
}

/*
    Challenge 3: pointer difference
    -------------------------------

    Goal:
    Return how many elements are between first and last.

    Correct:
    -> last - first

    Broken version tries byte math through unsigned char *.
*/
static ptrdiff_t challenge_element_distance(const int *first, const int *last) {
    const unsigned char *a = (const unsigned char *)first;
    const unsigned char *b = (const unsigned char *)last;
    return b - a; /* TODO: fix me */
}

static int challenge_element_distance_works(void) {
    int values[10] = {0};
    return challenge_element_distance(&values[2], &values[7]) == 5;
}

/*
    Challenge 4: one-past pointer
    -----------------------------

    Goal:
    Return pointer one past the last element.

    Correct:
    -> values + count

    Broken version returns pointer to the last element.
*/
static const int *challenge_end_pointer(const int *values, size_t count) {
    return values + count - 1; /* TODO: fix me */
}

static int challenge_end_pointer_works(void) {
    int values[3] = {7, 8, 9};
    return challenge_end_pointer(values, 3) == values + 3;
}

/*
    Challenge 5: byte offset
    ------------------------

    Goal:
    Return a pointer byteOffset bytes after base.

    Correct:
    -> cast to const unsigned char * before adding

    Broken version treats byteOffset as int elements.
*/
static const void *challenge_byte_offset(const int *base, size_t byteOffset) {
    return base + byteOffset; /* TODO: fix me */
}

static int challenge_byte_offset_works(void) {
    int values[4] = {0};
    const unsigned char *bytes = (const unsigned char *)values;
    return challenge_byte_offset(values, 3) == bytes + 3;
}

/*
    Challenge 6: find first matching byte
    -------------------------------------

    Goal:
    Return pointer to first byte equal to target, or NULL if not found.

    Correct:
    -> walk p from bytes to bytes + len

    Broken version checks only bytes[0].
    That is like watching only the first member's fancam and declaring you
    understood the whole choreography. Absolutely not. Walk the full buffer.
*/
static const unsigned char *challenge_find_byte(const unsigned char *bytes,
                                                size_t len,
                                                unsigned char target) {
    if (len > 0 && *bytes == target) {
        return bytes;
    }
    return NULL; /* TODO: fix me */
}

static int challenge_find_byte_works(void) {
    unsigned char bytes[5] = {9, 8, 7, 6, 5};
    return challenge_find_byte(bytes, 5, 6) == bytes + 3 &&
           challenge_find_byte(bytes, 5, 4) == NULL;
}

/*
    Challenge 7: struct pointer walk
    --------------------------------

    Goal:
    Return pointer to first player with score >= minScore, or NULL.

    Broken version returns the first player unconditionally.
*/
static const TinyPlayer *challenge_find_player_with_score(const TinyPlayer *begin,
                                                          const TinyPlayer *end,
                                                          int minScore) {
    (void)end;
    (void)minScore;
    return begin; /* TODO: fix me */
}

static int challenge_find_player_with_score_works(void) {
    TinyPlayer players[3] = {
        {"Yuqi", 5},
        {"Shuhua", 15},
        {"Minnie", 25},
    };

    return challenge_find_player_with_score(players, players + 3, 20) == players + 2 &&
           challenge_find_player_with_score(players, players + 3, 99) == NULL;
}

/*
    Challenge 8: reverse walk
    -------------------------

    Goal:
    Sum an array by walking backward.

    Safe pattern:
    -> start at end
    -> decrement before dereference
    -> stop when p == begin

    Broken version dereferences end immediately. That is one-past and invalid,
    so we avoid actually doing that in the scaffold; instead it just returns 0.
    One-past is backstage. You may stand there with a clipboard. You may not
    perform the chorus from there.
*/
static int challenge_reverse_sum(const int *begin, const int *end) {
    (void)begin;
    (void)end;
    return 0; /* TODO: fix me */
}

static int challenge_reverse_sum_works(void) {
    int values[4] = {3, 4, 5, 6};
    return challenge_reverse_sum(values, values + 4) == 18;
}

static void print_challenge_result(const char *name, int success) {
    printf("%s: %s\n", name, success ? "SUCCESS" : "FAIL");
}

static void run_challenges(void) {
    SECTION("challenges: pointer arithmetic reps");

    print_challenge_result("challenge 1 - get by pointer offset",
                           challenge_get_by_offset_works());
    print_challenge_result("challenge 2 - sum begin/end range",
                           challenge_sum_range_works());
    print_challenge_result("challenge 3 - pointer element distance",
                           challenge_element_distance_works());
    print_challenge_result("challenge 4 - one-past end pointer",
                           challenge_end_pointer_works());
    print_challenge_result("challenge 5 - byte offset with unsigned char *",
                           challenge_byte_offset_works());
    print_challenge_result("challenge 6 - find matching byte",
                           challenge_find_byte_works());
    print_challenge_result("challenge 7 - walk struct array",
                           challenge_find_player_with_score_works());
    print_challenge_result("challenge 8 - reverse pointer walk",
                           challenge_reverse_sum_works());
}

int main(void) {
    SECTION("pointer math workbook");
    printf("Demos should pass. Challenge FAIL output means spicy homework.\n");
    printf("Today's concept: pointer arithmetic, aka BLACKPINK in your area but the area is RAM.\n");

    demo_indexing_is_pointer_arithmetic();
    demo_scaling_by_type();
    demo_begin_end_loop();
    demo_pointer_difference();
    demo_byte_walking();
    demo_struct_array_walk();
    run_challenges();

    SECTION("done");
    printf("Pointer arithmetic: less magic, more address-flavored math.\n");
    printf("Hydrate, stream your emotional support girl group, then fix the FAILs.\n");
    return 0;
}
