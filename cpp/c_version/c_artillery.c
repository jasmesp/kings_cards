/*
    c_artillery.c
    =============

    A one-file C memory/naming/convention workbook.

    Vibe:
    -> Rustlings-ish: tiny sections, runnable examples, loud comments.
    -> Freshman-on-finals-week intensity: every footgun gets a neon sign.
    -> Reference quality: conventions are labeled as conventions, mechanics are
       labeled as mechanics, and names are never treated as magic safety spells.

    Compile through this folder's CMake target:

        cmake -S c_version -B c_version/build
        cmake --build c_version/build
        ./c_version/build/c_artillery

    Big idea:

    C gives you raw power and no babysitter. The language will usually let you:

    -> allocate too little
    -> write past the end
    -> free the same pointer twice
    -> use memory after freeing it
    -> lose the only pointer to heap memory
    -> compare signed and unsigned numbers in surprising ways
    -> pass negative char values into ctype functions and summon undefined vibes

    Good C style is not just "pretty names." Good C style is:

    -> boring ownership rules
    -> checked allocation
    -> overflow guards before multiplication
    -> small scopes for shorthand names
    -> explicit cleanup paths
    -> project-local conventions explained once and followed consistently

    About SIZE_MAX vs (size_t)-1
    ----------------------------

    User question: "shouldn't size max be size_t in 99% of cases?"

    Yes, in the sense that SIZE_MAX is the maximum value of the size_t type.
    If you are asking "what is the largest valid size_t value?", SIZE_MAX is the
    named modern spelling.

    But you will also see this:

        (size_t)-1

    That expression means:

    -> start with signed integer -1
    -> convert it to size_t
    -> size_t is unsigned
    -> unsigned conversion wraps modulo max+1
    -> result becomes the largest possible size_t value

    Both are viable in real C:

        SIZE_MAX        // named limit, clearer if you know the macro
        (size_t)-1      // explicit old-school unsigned wraparound idiom

    This workbook shows BOTH. The runnable code uses both and asserts they are
    equal. That is the point: learn the named form AND the machine-room form.

    About xmalloc
    -------------

    xmalloc is not standard C. It is a common project-local idiom.

    malloc:
    -> may return NULL
    -> caller must check

    xmalloc:
    -> wraps malloc
    -> checks for NULL
    -> exits or otherwise handles failure centrally
    -> if it returns, returned memory is usable

    In application code, xmalloc can be reasonable. In library code like curl or
    OpenSSL, killing the host process is usually rude; library code returns NULL
    or an error code and lets the caller decide.

    Names are promises; code is enforcement.
*/

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
    SECTION 0: tiny test harness
    ----------------------------

    This is intentionally primitive. No framework, no ceremony. We want one file
    you can read top-to-bottom.
*/
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
    SECTION 1: fatal_oom
    --------------------

    `fatal_oom` = fatal out-of-memory handler.

    fatal:
    -> unrecoverable in this program

    oom:
    -> out of memory

    This is a project-local helper. It is not standard C.

    Standard C termination function used inside:
    -> exit(EXIT_FAILURE)

    Why not "die"?
    -> Some Unix-ish code uses die().
    -> It is not standard C.
    -> fatal_oom says the cause more clearly.
*/
static void fatal_oom(const char *what) {
    fprintf(stderr, "fatal_oom: out of memory while allocating %s\n", what);
    exit(EXIT_FAILURE);
}

/*
    SECTION 2: xmalloc
    ------------------

    `x` prefix convention:
    -> project-local checked/extended version of a standard-ish operation.

    xmalloc:
    -> calls malloc
    -> checks NULL
    -> exits on failure
    -> returns usable memory if it returns at all

    `ptr` naming:
    -> classic shorthand for "pointer"
    -> okay in tiny generic scopes like this
    -> bad if many pointers have different ownership meanings
*/
static void *xmalloc(size_t size, const char *what) {
    void *ptr = malloc(size == 0 ? 1 : size);
    if (ptr == NULL) {
        fatal_oom(what);
    }
    return ptr;
}

/*
    SECTION 3: xrealloc
    -------------------

    realloc footgun:

        ptr = realloc(ptr, new_size);

    Why dangerous?
    -> if realloc fails, it returns NULL
    -> the original allocation is still alive
    -> but you overwrote ptr with NULL
    -> now you leaked the original allocation

    Safer pattern:

        new_ptr = realloc(old_ptr, new_size);
        if (new_ptr == NULL) handle_error();
        old_ptr = new_ptr;

    xrealloc wraps that policy for application-style fatal OOM behavior.
*/
static void *xrealloc(void *oldPtr, size_t size, const char *what) {
    void *newPtr = realloc(oldPtr, size == 0 ? 1 : size);
    if (newPtr == NULL) {
        fatal_oom(what);
    }
    return newPtr;
}

/*
    SECTION 4: xmalloc_array
    ------------------------

    The classic bug:

        malloc(count * sizeof(Item))

    If count * sizeof(Item) overflows size_t, the multiplication wraps to a
    smaller number. malloc succeeds. Then your later writes smash memory.

    Guard form A, named max:

        count > SIZE_MAX / element_size

    Guard form B, explicit unsigned max:

        count > (size_t)-1 / element_size

    Both are checking the same thing.

    This function uses form B in code so the maximum is visibly tied to size_t.
    Comments show form A because modern code often prefers the named macro.
*/
static bool size_multiply_would_overflow(size_t count, size_t elementSize) {
    /*
        If elementSize is 0, count * elementSize is 0. No overflow.

        If elementSize is nonzero, division is safe and gives the largest count
        that can be multiplied by elementSize without exceeding size_t max.

        Equivalent named-limit expression:

            count > SIZE_MAX / elementSize
    */
    return elementSize != 0 && count > (size_t)-1 / elementSize;
}

static void *xmalloc_array(size_t count, size_t elementSize, const char *what) {
    if (size_multiply_would_overflow(count, elementSize)) {
        fatal_oom(what);
    }
    return xmalloc(count * elementSize, what);
}

static void *xrealloc_array(void *oldPtr,
                            size_t count,
                            size_t elementSize,
                            const char *what) {
    if (size_multiply_would_overflow(count, elementSize)) {
        fatal_oom(what);
    }
    return xrealloc(oldPtr, count * elementSize, what);
}

/*
    SECTION 5: xstrdup
    ------------------

    strdup is common POSIX, but not ISO C. So portable C often writes its own.

    Ownership contract:
    -> returned pointer is heap memory
    -> caller owns it
    -> caller must free it exactly once

    len:
    -> common shorthand for length
    -> good here because the scope is tiny and the meaning is standard
*/
static char *xstrdup(const char *text) {
    size_t len = strlen(text);
    char *copy = xmalloc(len + 1, "string copy");
    memcpy(copy, text, len + 1);
    return copy;
}

/*
    SECTION 6: a tiny dynamic array
    -------------------------------

    This is the minimum shape of a heap-backed vector:

        data pointer
        count
        capacity

    count:
    -> number of meaningful elements

    capacity:
    -> number of allocated slots

    invariant:
    -> count <= capacity

    If count == capacity and you want to push, grow first.
*/
typedef struct {
    int *items;
    size_t count;
    size_t capacity;
} IntVec;

static void intvec_init(IntVec *vec) {
    vec->items = NULL;
    vec->count = 0;
    vec->capacity = 0;
}

static void intvec_free(IntVec *vec) {
    /*
        free(NULL) is legal. That makes cleanup functions simpler.

        Setting fields to zero/NULL after free is not magic, but it helps prevent
        accidental reuse through this struct.
    */
    free(vec->items);
    vec->items = NULL;
    vec->count = 0;
    vec->capacity = 0;
}

static void intvec_reserve(IntVec *vec, size_t needed) {
    if (needed <= vec->capacity) {
        return;
    }

    size_t newCapacity = vec->capacity == 0 ? 4 : vec->capacity;
    while (newCapacity < needed) {
        /*
            Doubling overflow guard.

            Two equivalent spellings:

                newCapacity > SIZE_MAX / 2
                newCapacity > (size_t)-1 / 2

            We use explicit form here. You should recognize both.
        */
        if (newCapacity > (size_t)-1 / 2) {
            fatal_oom("IntVec capacity growth");
        }
        newCapacity *= 2;
    }

    vec->items = xrealloc_array(vec->items, newCapacity, sizeof(int), "IntVec items");
    vec->capacity = newCapacity;
}

static void intvec_push(IntVec *vec, int value) {
    intvec_reserve(vec, vec->count + 1);
    vec->items[vec->count] = value;
    vec->count++;
}

static int intvec_at(const IntVec *vec, size_t idx) {
    /*
        idx:
        -> common shorthand for index
        -> acceptable because this function's whole job is indexing

        Bounds check:
        -> idx must be less than count
        -> capacity does NOT mean valid element count
    */
    if (idx >= vec->count) {
        fprintf(stderr, "intvec_at: index %zu out of range, count is %zu\n", idx,
                vec->count);
        exit(EXIT_FAILURE);
    }
    return vec->items[idx];
}

/*
    SECTION 7: ownership vocabulary
    -------------------------------

    owned pointer:
    -> this code must free it

    borrowed pointer:
    -> this code may inspect/use it temporarily
    -> this code must NOT free it

    transferred pointer:
    -> ownership moves from one place to another
    -> old owner must stop freeing/using it as owner
*/
typedef struct {
    char *ownedName;
} Person;

static void person_init(Person *person, const char *borrowedName) {
    /*
        borrowedName:
        -> caller owns it
        -> we copy it
        -> after xstrdup, person->ownedName is ours to free
    */
    person->ownedName = xstrdup(borrowedName);
}

static void person_free(Person *person) {
    free(person->ownedName);
    person->ownedName = NULL;
}

/*
    SECTION 8: ctype footgun
    ------------------------

    Functions like tolower/isalpha take int, but the value must be EOF or
    representable as unsigned char.

    Wrong:

        tolower(c)

    if c is a negative signed char, behavior is undefined.

    Right:

        tolower((unsigned char)c)
*/
static char ascii_lower(char c) {
    return (char)tolower((unsigned char)c);
}

/*
    SECTION 9: signed/unsigned comparisons
    --------------------------------------

    size_t is unsigned.

    This can bite:

        int n = -1;
        size_t len = 5;
        if (n < len) ...

    n gets converted to size_t before comparison, becoming a huge positive value.

    Rule:
    -> use size_t for sizes/counts/indexes
    -> validate signed user input before casting to size_t
*/
static size_t checked_user_index_to_size_t(long userChoice, size_t count) {
    /*
        User-facing choices are 1-based:
        -> first item is 1

        C indexes are 0-based:
        -> first item is 0

        Validate while still signed, THEN cast.
    */
    if (userChoice < 1) {
        fprintf(stderr, "choice must be at least 1\n");
        exit(EXIT_FAILURE);
    }

    size_t index = (size_t)(userChoice - 1);
    if (index >= count) {
        fprintf(stderr, "choice out of range\n");
        exit(EXIT_FAILURE);
    }
    return index;
}

/*
    SECTION 10: exercises / demos
    -----------------------------

    These functions run small checks. Treat them like executable notes.
*/
static void demo_size_limits(void) {
    SECTION("size_t max: SIZE_MAX and (size_t)-1");

    size_t maxByMacro = SIZE_MAX;
    size_t maxByCast = (size_t)-1;

    printf("SIZE_MAX    = %zu\n", maxByMacro);
    printf("(size_t)-1  = %zu\n", maxByCast);

    CHECK(maxByMacro == maxByCast);
    CHECK(size_multiply_would_overflow(maxByCast, 2));
    CHECK(!size_multiply_would_overflow(10, sizeof(int)));
}

static void demo_xstrdup_and_ownership(void) {
    SECTION("owned vs borrowed strings");

    const char *borrowed = "Minnie";
    Person person;
    person_init(&person, borrowed);

    printf("borrowed string literal: %s\n", borrowed);
    printf("owned heap copy:         %s\n", person.ownedName);

    CHECK(strcmp(person.ownedName, borrowed) == 0);
    CHECK(person.ownedName != borrowed);

    person_free(&person);
    CHECK(person.ownedName == NULL);
}

static void demo_intvec(void) {
    SECTION("dynamic array count/capacity");

    IntVec vec;
    intvec_init(&vec);

    for (int i = 0; i < 10; ++i) {
        intvec_push(&vec, i * 10);
        printf("push %2d -> count=%zu capacity=%zu\n", i * 10, vec.count,
               vec.capacity);
    }

    CHECK(vec.count == 10);
    CHECK(vec.capacity >= vec.count);
    CHECK(intvec_at(&vec, 3) == 30);

    intvec_free(&vec);
    CHECK(vec.items == NULL);
    CHECK(vec.count == 0);
    CHECK(vec.capacity == 0);
}

static void demo_ctype(void) {
    SECTION("ctype unsigned-char cast");

    CHECK(ascii_lower('A') == 'a');
    CHECK(ascii_lower('z') == 'z');
    printf("tolower((unsigned char)'A') == '%c'\n", ascii_lower('A'));
}

static void demo_user_index(void) {
    SECTION("1-based user choice to 0-based size_t index");

    size_t idx = checked_user_index_to_size_t(3, 5);
    printf("user choice 3 in a 5-item list -> C index %zu\n", idx);
    CHECK(idx == 2);
}

/*
    SECTION 11: challenges
    ----------------------

    These are intentionally malformed scaffold functions.

    Rules:
    -> They compile.
    -> They are wrong on purpose.
    -> main calls them directly.
    -> main prints SUCCESS or FAIL based on their return values.
    -> No fancy harness. No hidden magic. Just booleans.

    How to use:
    -> Run ./c_version/build/c_artillery.
    -> See which challenges print FAIL.
    -> Fix the body of the challenge function.
    -> Rebuild and rerun.

    Leave the `challenge_*_works` wrapper functions alone at first. They encode
    the expected behavior. Edit the malformed `challenge_*` function above each
    wrapper.
*/

/*
    Challenge 1: multiplication overflow
    ------------------------------------

    Goal:
    Return true if count * elementSize would overflow size_t.

    Correct pattern:
    -> if elementSize is 0, multiplication cannot overflow.
    -> otherwise, overflow happens when count > (size_t)-1 / elementSize.

    This version is malformed because it only checks for zero and otherwise says
    "nah, impossible." Classic overconfident C goblin.
*/
static bool challenge_mul_overflow(size_t count, size_t elementSize) {
    if (elementSize == 0) {
        return false;
    }

    return false; /* TODO: fix me */
}

static bool challenge_mul_overflow_works(void) {
    bool smallDoesNotOverflow = !challenge_mul_overflow(10, sizeof(int));
    bool zeroSizeDoesNotOverflow = !challenge_mul_overflow((size_t)-1, 0);
    bool giantDoesOverflow = challenge_mul_overflow((size_t)-1, 2);

    return smallDoesNotOverflow && zeroSizeDoesNotOverflow && giantDoesOverflow;
}

/*
    Challenge 2: bytes needed for a C string copy
    ---------------------------------------------

    Goal:
    Return how many bytes are needed to copy a string INCLUDING the terminating
    '\0' byte.

    Reminder:
    -> strlen("abc") is 3.
    -> But storage needed is 4: 'a', 'b', 'c', '\0'.

    This version is malformed because it forgets the terminator byte.
*/
static size_t challenge_c_string_copy_bytes(const char *text) {
    return strlen(text); /* TODO: fix me */
}

static bool challenge_c_string_copy_bytes_works(void) {
    return challenge_c_string_copy_bytes("") == 1 &&
           challenge_c_string_copy_bytes("abc") == 4 &&
           challenge_c_string_copy_bytes("Shuhua") == 7;
}

/*
    Challenge 3: signed user choice to size_t index
    -----------------------------------------------

    Goal:
    Convert a 1-based user choice into a 0-based C index.

    Return true on success and write the result to *outIndex.
    Return false on invalid input.

    Correct policy:
    -> validate while the value is still signed
    -> reject choices less than 1
    -> reject choices greater than itemCount
    -> only then cast to size_t

    This version is malformed because it casts before validation AND forgets to
    convert from the user's 1-based choice to C's 0-based index.
*/
static bool challenge_user_choice_to_index(long userChoice,
                                           size_t itemCount,
                                           size_t *outIndex) {
    size_t index = (size_t)userChoice; /* TODO: fix me */
    if (index >= itemCount) {
        return false;
    }

    *outIndex = index;
    return true;
}

static bool challenge_user_choice_to_index_works(void) {
    size_t index = 999;

    bool acceptsValidChoice = challenge_user_choice_to_index(3, 5, &index) && index == 2;
    bool rejectsZero = !challenge_user_choice_to_index(0, 5, &index);
    bool rejectsNegative = !challenge_user_choice_to_index(-4, 5, &index);
    bool rejectsTooLarge = !challenge_user_choice_to_index(6, 5, &index);

    return acceptsValidChoice && rejectsZero && rejectsNegative && rejectsTooLarge;
}

/*
    Challenge 4: heap string duplication
    ------------------------------------

    Goal:
    Return a heap-owned duplicate of text.

    Correct policy:
    -> allocate strlen(text) + 1 bytes
    -> copy the terminator too
    -> return a distinct pointer that the caller can free

    This version is malformed because it returns the borrowed pointer. The text
    contents compare equal, but ownership is wrong. The caller must not free a
    string literal or someone else's storage.
*/
static char *challenge_strdup(const char *text) {
    return (char *)text; /* TODO: fix me */
}

static bool challenge_strdup_works(void) {
    const char *source = "Miyeon";
    char *copy = challenge_strdup(source);

    bool pointerExists = copy != NULL;
    bool contentsMatch = pointerExists && strcmp(copy, source) == 0;
    bool pointerIsDistinct = copy != source;

    if (pointerExists && pointerIsDistinct) {
        free(copy);
    }

    return pointerExists && contentsMatch && pointerIsDistinct;
}

/*
    Challenge 5: dynamic capacity growth
    ------------------------------------

    Goal:
    Given current capacity and needed count, return a new capacity that is:

    -> at least needed
    -> at least 4 when starting from 0
    -> grown by doubling
    -> protected against size_t overflow before doubling

    This version is malformed because it grows only once. If needed is much
    larger than current capacity * 2, it still returns too small a capacity.
*/
static size_t challenge_grow_capacity(size_t currentCapacity, size_t needed) {
    size_t newCapacity = currentCapacity == 0 ? 4 : currentCapacity;
    if (newCapacity < needed) {
        if (newCapacity > (size_t)-1 / 2) {
            fatal_oom("challenge capacity growth");
        }
        newCapacity *= 2; /* TODO: fix me: one doubling is not always enough */
    }
    return newCapacity;
}

static bool challenge_grow_capacity_works(void) {
    bool startsAtFour = challenge_grow_capacity(0, 1) == 4;
    bool keepsEnoughCapacity = challenge_grow_capacity(8, 6) == 8;
    bool doublesWhenClose = challenge_grow_capacity(8, 9) == 16;
    bool keepsDoublingUntilEnough = challenge_grow_capacity(4, 100) >= 100;

    return startsAtFour && keepsEnoughCapacity && doublesWhenClose &&
           keepsDoublingUntilEnough;
}

/*
    Challenge 6: zero-byte allocation policy
    ----------------------------------------

    Goal:
    Return the actual byte count our xmalloc/xrealloc wrappers should request
    from malloc/realloc.

    Policy introduced above:
    -> malloc(0) is awkward and implementation-dependent in practice.
    -> this workbook's xmalloc asks for at least 1 byte.

    This version is malformed because it returns 0 unchanged.
*/
static size_t challenge_normalize_alloc_size(size_t requestedSize) {
    return requestedSize; /* TODO: fix me */
}

static bool challenge_normalize_alloc_size_works(void) {
    return challenge_normalize_alloc_size(0) == 1 &&
           challenge_normalize_alloc_size(1) == 1 &&
           challenge_normalize_alloc_size(42) == 42;
}

/*
    Challenge 7: realloc failure must preserve old pointer
    ------------------------------------------------------

    Goal:
    Given an old pointer and the result of realloc, return the pointer the owner
    should keep.

    Correct policy:
    -> if reallocResult is non-NULL, use it.
    -> if reallocResult is NULL, keep oldPtr because realloc failure leaves the
       old allocation alive.

    This is a simulation. We do not actually allocate here. The pointer values
    are just tokens so you can focus on the policy.

    This version is malformed because it blindly returns reallocResult.
*/
static void *challenge_realloc_owner_after_attempt(void *oldPtr, void *reallocResult) {
    (void)oldPtr;
    return reallocResult; /* TODO: fix me */
}

static bool challenge_realloc_owner_after_attempt_works(void) {
    int oldObject = 1;
    int newObject = 2;

    void *oldPtr = &oldObject;
    void *newPtr = &newObject;

    bool successUsesNewPointer =
        challenge_realloc_owner_after_attempt(oldPtr, newPtr) == newPtr;
    bool failureKeepsOldPointer =
        challenge_realloc_owner_after_attempt(oldPtr, NULL) == oldPtr;

    return successUsesNewPointer && failureKeepsOldPointer;
}

/*
    Challenge 8: count vs capacity
    ------------------------------

    Goal:
    Return true only when idx is a valid element index.

    Correct rule:
    -> idx < count

    Incorrect temptation:
    -> idx < capacity

    capacity is allocated storage, not meaningful elements.

    This version is malformed because it uses capacity.
*/
static bool challenge_index_is_valid(size_t idx, size_t count, size_t capacity) {
    (void)count;
    return idx < capacity; /* TODO: fix me */
}

static bool challenge_index_is_valid_works(void) {
    bool acceptsRealElement = challenge_index_is_valid(2, 3, 8);
    bool rejectsUnusedCapacity = !challenge_index_is_valid(5, 3, 8);
    bool rejectsAtCount = !challenge_index_is_valid(3, 3, 8);

    return acceptsRealElement && rejectsUnusedCapacity && rejectsAtCount;
}

/*
    Challenge 9: cleanup should remove the dangling pointer from the owner
    ---------------------------------------------------------------------

    Goal:
    Free person->ownedName and set person->ownedName to NULL.

    Correct policy:
    -> free owned heap memory exactly once
    -> clear the owner field after free

    This version is malformed because it does not clean up at all.

    Why not make the broken version call free but forget NULL?
    -> Because then the checker would have to inspect a dangling pointer field.
    -> For a workbook, we want the wrong code to be wrong without leaning on
       undefined-behavior-adjacent nonsense.
*/
static void challenge_person_free(Person *person) {
    (void)person;
    /* TODO: free person->ownedName and set person->ownedName to NULL */
}

static bool challenge_person_free_works(void) {
    Person person;
    person_init(&person, "Yuqi");
    challenge_person_free(&person);

    bool ownerWasCleared = person.ownedName == NULL;
    if (person.ownedName != NULL) {
        free(person.ownedName);
        person.ownedName = NULL;
    }

    return ownerWasCleared;
}

/*
    Challenge 10: ctype-style character classification
    --------------------------------------------------

    Goal:
    Return true if c is an ASCII uppercase letter.

    Correct ASCII condition:
    -> c >= 'A' && c <= 'Z'

    Related best practice from the workbook:
    -> for real ctype functions like isupper/tolower, cast to unsigned char.

    This version is malformed because it uses || instead of &&, so almost every
    character passes.
*/
static bool challenge_ascii_is_upper(char c) {
    return c >= 'A' || c <= 'Z'; /* TODO: fix me */
}

static bool challenge_ascii_is_upper_works(void) {
    return challenge_ascii_is_upper('A') &&
           challenge_ascii_is_upper('Z') &&
           !challenge_ascii_is_upper('a') &&
           !challenge_ascii_is_upper('0') &&
           !challenge_ascii_is_upper('@');
}

/*
    Challenge 11: array length vs pointer size
    ------------------------------------------

    Goal:
    Return the number of int elements in the local array below.

    Correct when you still have the actual array object:
    -> sizeof numbers / sizeof numbers[0]

    Wrong after array decay:
    -> doing sizeof on an int * parameter measures the pointer, not the array.

    This version is malformed because it thinks in terms of pointer bytes rather
    than array element count. A very common version of this bug is using sizeof
    on a pointer parameter; compilers often warn about that exact spelling.
*/
static size_t challenge_count_ints_wrong(int *numbers) {
    return sizeof(void *) / sizeof(numbers[0]); /* TODO: fix me */
}

static bool challenge_count_ints_wrong_works(void) {
    int numbers[5] = {1, 2, 3, 4, 5};
    return challenge_count_ints_wrong(numbers) == 5;
}

static void print_challenge_result(const char *name, bool success) {
    printf("%s: %s\n", name, success ? "SUCCESS" : "FAIL");
}

static void run_challenges(void) {
    SECTION("challenges: fix the malformed functions");

    print_challenge_result("challenge 1 - multiplication overflow",
                           challenge_mul_overflow_works());
    print_challenge_result("challenge 2 - C string copy byte count",
                           challenge_c_string_copy_bytes_works());
    print_challenge_result("challenge 3 - signed choice to size_t index",
                           challenge_user_choice_to_index_works());
    print_challenge_result("challenge 4 - heap-owned string duplicate",
                           challenge_strdup_works());
    print_challenge_result("challenge 5 - dynamic capacity growth",
                           challenge_grow_capacity_works());
    print_challenge_result("challenge 6 - zero-byte allocation policy",
                           challenge_normalize_alloc_size_works());
    print_challenge_result("challenge 7 - realloc owner after failure",
                           challenge_realloc_owner_after_attempt_works());
    print_challenge_result("challenge 8 - count vs capacity index check",
                           challenge_index_is_valid_works());
    print_challenge_result("challenge 9 - free then clear owner pointer",
                           challenge_person_free_works());
    print_challenge_result("challenge 10 - ASCII uppercase condition",
                           challenge_ascii_is_upper_works());
    print_challenge_result("challenge 11 - array length vs pointer size",
                           challenge_count_ints_wrong_works());
}

int main(void) {
    SECTION("C artillery workbook");
    printf("If this program exits normally, the workbook checks passed.\n");

    demo_size_limits();
    demo_xstrdup_and_ownership();
    demo_intvec();
    demo_ctype();
    demo_user_index();
    run_challenges();

    SECTION("done");
    printf("Workbook demos passed. Challenge FAIL output means there is homework.\n");
    return 0;
}
