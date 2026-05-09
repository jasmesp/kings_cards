/*
    King's Cards - malloc / manual memory management implementation
    ===============================================================

    This file is the heap-allocation sibling of `main.c`.

    `main.c` uses fixed arrays:
    -> Card hand[52]
    -> char name[64]
    -> int handCount

    This file uses heap-owned dynamic containers:
    -> DynamicCardArray hand
    -> char *name
    -> size_t count/capacity inside DynamicCardArray

    The point is not "malloc is better" or "malloc is worse." The point is that
    malloc changes the shape of the program.

    What malloc buys us here:

    1. Flexible capacities.
       A DynamicCardArray can start small and grow. We are no longer forced to reserve 52
       card slots for every hand and pile up front.

    2. Reusable container behavior.
       Push, remove, copy, free, and grow logic lives in DynamicCardArray helpers instead
       of being open-coded for every pile.

    3. Heap-owned player names.
       A player's name can be exactly as long as the input line we keep, rather
       than always occupying MAX_NAME_LEN bytes. This version still reads input
       through a bounded buffer, but once read, it duplicates the name onto the
       heap.

    4. A realistic C ownership pattern.
       Bigger C programs often use "init/destroy" pairs. This file demonstrates
       that style:

       -> dynamic_card_array_init  pairs with dynamic_card_array_free
       -> player_init   pairs with player_free
       -> game_init     pairs with game_free

    What malloc costs us:

    1. Every allocation can fail.
       `malloc` and `realloc` can return NULL. This file checks that and exits
       with a clear message instead of wandering into undefined behavior.

    2. Every successful allocation needs a matching free.
       Forgetting free creates leaks. Freeing twice creates corruption. Freeing
       while something still uses the memory creates use-after-free.

    3. Pointers can dangle.
       After `free(ptr)`, the pointer value is not automatically safe.

       !!! SHORTHAND NOTE !!!
       -> `ptr` is the classic C abbreviation for "pointer."
       -> It is not a special type. It is just a variable name.
       -> This file uses fuller names for long-lived pointers, but comments may
          say `ptr` when talking about pointer behavior in general.

       Helpers in this file set freed pointers to NULL so accidental reuse is
       easier to notice.

    What "right way" means in real C projects
    -----------------------------------------

    There is no single universal C house style. The real rule is:

    -> Match the failure policy of the kind of software you are writing.
    -> Match the naming/formatting style of the project you are inside.
    -> Avoid cleverness unless the shorthand is a known local convention.

    This file is an application-style teaching program, so it uses the same
    broad allocation policy you will see in application code such as Neovim or
    GNU utilities:

    -> `xmalloc` means "checked malloc; never returns NULL."
    -> If allocation cannot be satisfied, the program reports the problem and
       exits through one central fatal path.

    curl and OpenSSL are different because they are libraries:

    -> library code should not normally terminate the host process.
    -> allocation failure is reported with NULL or a project error code.
    -> callers unwind, clean up, and decide what to do.

    Linux kernel code is different again:

    -> allocation helpers return NULL on failure.
    -> array allocation helpers like kmalloc_array/kcalloc are preferred because
       they check multiplication overflow.
    -> panic/BUG-style termination is strongly discouraged outside rare cases.

    So for this file:

    -> `xmalloc`, `xrealloc`, and `xstrdup` teach the Neovim/GNU application
       idiom.
    -> `xmalloc_array` and `xrealloc_array` teach the OpenSSL/Linux
       overflow-guard idiom for array allocation.
    -> `fatal_oom` is named semantically instead of `die`, because modern
       readable code should say why termination is happening.

    !!! HEAP OWNERSHIP RULE USED IN THIS FILE !!!
    -> The object that allocates memory is responsible for freeing it.
    -> If ownership is transferred, comments say so.
    -> Borrowed pointers must not be freed by the borrower.

    !!! MEMORY SAFETY CHECKLIST !!!
    -> Check malloc/realloc result before writing through the pointer.
    -> Do not overwrite the only pointer to allocated memory before realloc
       succeeds. Use a temporary pointer.
    -> Free every heap field in the reverse-ish order of construction.
    -> Set freed pointers to NULL and counts/capacities to 0.
    -> Never keep pointers into a DynamicCardArray across operations that may realloc it.

    Industry naming idioms used on purpose
    --------------------------------------

    C code has a lot of old-school shorthand. Some of it is cryptic cargo-cult
    nonsense. Some of it is genuinely standard enough that learning it will help
    you read real codebases.

    This file uses the useful ones, but explains them loudly:

    - `xmalloc`, `xrealloc`, `xmalloc_array`, `xrealloc_array`, `xstrdup`

      The `x` prefix commonly means "extended", "checked", or "fatal-on-failure"
      version of a standard function.

      `malloc(size)`:
      -> returns NULL if allocation fails.
      -> caller must remember to check.

      `xmalloc(size)`:
      -> wraps malloc.
      -> checks for NULL.
      -> prints an error and exits if allocation fails.
      -> returns only valid memory if it returns at all.

      `xmalloc_array(count, element_size)`:
      -> checks that count * element_size will not overflow size_t.
      -> allocates the resulting byte count.
      -> mirrors modern APIs like OpenSSL's OPENSSL_malloc_array.

      !!! IMPORTANT TEACHING POINT !!!
      -> `_array` in a function name is only a naming convention.
      -> The underscore does not make anything safer.
      -> The name tells humans what the helper promises to do.
      -> The overflow check inside the function is what actually protects you.
      -> Avoid wildcard-looking prose like star-underscore-array in C teaching
         material, because a leading star already means pointer/dereference
         syntax in real C code.

      This is a real convention used in many C projects. It is not universal, so
      you still check the local codebase, but when you see `xfoo`, think:

      "probably project-specific safer/stricter version of foo."

      !!! APPLICATION VS LIBRARY DESIGN !!!
      -> curl and OpenSSL are libraries, so they generally return NULL/error
         codes instead of killing the caller's process.
      -> This game is an application, so a fatal allocation helper is acceptable
         for teaching. If this were a reusable library, we would return errors.

    - `fatal_oom`

      `fatal` says this helper handles an unrecoverable error. `oom` means
      "out of memory." So `fatal_oom` means:

      "We cannot recover from allocation failure here; report it and exit."

      Older Unix-ish code may use names like `die`, but `fatal_oom` is more
      semantic and easier to grep for when you are learning or maintaining code.

    - `ptr`

      Short for pointer. Good in tiny scopes or generic pointer discussions.
      Bad when there are multiple pointers with different ownership meanings.

    - `tmp`

      Short for temporary. Very common in swap code:

          tmp = a;
          a = b;
          b = tmp;

      Good when the value exists only to hold something for a couple lines.
      Bad when the value has domain meaning.

    - `len`, `idx`, `src`, `dst`

      Common abbreviations for length, index, source, destination. Good when
      local and obvious. Spell things out when the variable carries game meaning.

    Two equivalent ways to say "maximum size_t"
    -------------------------------------------

    You will see both of these in real C:

        SIZE_MAX
        (size_t)-1

    They mean the same maximum value for size_t, but they teach different things.

    `SIZE_MAX`:
    -> Named macro from the standard integer-limit headers.
    -> Very readable once you know it.
    -> Common in modern code that wants the name to say exactly what it means.
    -> Downside for learners: it hides the unsigned-wraparound mechanic.

    `(size_t)-1`:
    -> Explicit C arithmetic idiom.
    -> Convert -1 into the unsigned type size_t.
    -> Unsigned conversion wraps modulo max+1, so the result is the largest
       representable size_t value.
    -> Common in older or lower-level C code.
    -> Downside for learners: it looks cursed until someone explains it.

    This file uses `(size_t)-1` inside the actual overflow checks because the
    check stays self-contained in the control flow where it matters. The comments
    also show the `SIZE_MAX` form so you can recognize both. Do not define your
    own SIZE_MAX macro; if you want the named form, include the right standard
    header for your C version/platform.
*/

#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define PLAYER_COUNT 5
#define STARTING_CARD_CAPACITY 8
#define INPUT_BUFFER_SIZE 256

static const char *DEFAULT_PLAYER_NAMES[PLAYER_COUNT] = {
    "Miyeon",
    "Minnie",
    "Soyeon",
    "Yuqi",
    "Shuhua"
};

typedef enum {
    CLUBS,
    DIAMONDS,
    HEARTS,
    SPADES
} Suit;

typedef struct {
    Suit suit;
    int rank;
} Card;

/*
    DynamicCardArray
    -------

    A tiny dynamic array for Card values.

    `items` points to heap storage owned by this DynamicCardArray.
    `count` is how many Card slots are currently meaningful.
    `capacity` is how many Card slots are allocated.

    !!! COUNT VS CAPACITY !!!
    -> count <= capacity must always be true.
    -> Valid cards are items[0] through items[count - 1].
    -> Writing items[count] is legal only after ensuring count < capacity.

    !!! REAL MALLOC ADVANTAGE !!!
    -> In the fixed-array version, every hand reserves 52 card slots forever.
    -> Here a normal 5-card hand can start with 8 slots and grow only if needed.
*/
typedef struct {
    Card *items;
    size_t count;
    size_t capacity;
} DynamicCardArray;

typedef struct {
    char *name;
    DynamicCardArray hand;
    int totalScore;
    int *reignScores;
    size_t reignScoreCount;
    size_t reignScoreCapacity;
} Player;

typedef struct {
    int cycles;
    int reignTurnsPerSubject;
} GameConfig;

typedef struct {
    GameConfig config;
    Player *players;
    size_t playerCount;
    DynamicCardArray drawDeck;
    DynamicCardArray discardPile;
    DynamicCardArray favorPile;
} Game;

/*
    fatal_oom
    -------

    `fatal` = unrecoverable error; this helper terminates the program.
    `oom` = out of memory.

    !!! INDUSTRY VOCAB !!!
    -> You may see `fatal`, `panic`, `abort`, or older Unix-style `die` for
       unrecoverable errors.
    -> Prefer project vocabulary. In new teaching code, `fatal_oom` says more
       than `die_oom`.
    -> You will see `oom` in systems code constantly. It means out-of-memory.
    -> This helper does not recover because the rest of this teaching program is
       not written to gracefully continue after allocation failure.

    !!! STANDARD C CONNECTION !!!
    -> `fatal_oom` is our project-local helper, not a C standard function.
    -> The standard C termination call here is `exit(EXIT_FAILURE)`.
    -> `exit` flushes normal stdio buffers and runs registered exit handlers.
*/
static void fatal_oom(const char *what) {
    fprintf(stderr, "Out of memory while allocating %s.\n", what);
    exit(EXIT_FAILURE);
}

/*
    xmalloc
    -------

    Thin wrapper around malloc that centralizes failure handling.

    !!! WHAT THE `x` PREFIX MEANS !!!
    -> `xmalloc` is not from the C standard library.
    -> It is a common project-local convention.
    -> Here it means "malloc, but checked; exit instead of returning NULL."
    -> If this function returns, the returned pointer is usable.

    !!! MALLOC FOOTGUN !!!
    -> malloc returns NULL on failure.
    -> Writing through NULL crashes or worse.
    -> This helper exits immediately, so callers do not need repetitive checks.

    !!! SIZE ZERO NOTE !!!
    -> malloc(0) has implementation-defined-ish practical behavior.
    -> This helper forces at least 1 byte so it always asks for real storage.
*/
static void *xmalloc(size_t size, const char *what) {
    /*
        `ptr` = pointer returned by malloc.

        !!! WHY THIS SHORT NAME IS OK HERE !!!
        -> The variable lives for only three lines.
        -> Its entire job is "hold the raw malloc pointer long enough to check it."
        -> The function name already says what the operation is.
    */
    void *ptr = malloc(size == 0 ? 1 : size);
    if (ptr == NULL) {
        fatal_oom(what);
    }
    return ptr;
}

/*
    xmalloc_array
    -------------

    Allocates an array while guarding the multiplication.

    !!! INTEGER OVERFLOW FOOTGUN !!!
    -> malloc(count * sizeof(Element)) looks normal.
    -> But count * sizeof(Element) can overflow size_t.
    -> If it wraps smaller, malloc gives you too little memory.
    -> Then later writes run past the end of the allocation. Ouch.

    !!! THE ACTUAL GUARD !!!
    -> `(size_t)-1` is the largest value a size_t can hold.
    -> Why? size_t is unsigned. Converting -1 to an unsigned type wraps to the
       maximum representable value for that type.
    -> `SIZE_MAX` is the named standard macro for the same idea, usually from
       <stdint.h> or <stdint-compatible project headers>.
    -> Both spellings exist in real C code:
       - explicit arithmetic form: `(size_t)-1 / elementSize`
       - named-limit form: `SIZE_MAX / elementSize`
    -> This file uses the explicit arithmetic form in the condition so the guard
       sits directly inside the control flow instead of depending on a name you
       might forget to look up.
    -> If count > (size_t)-1 / elementSize, then count * elementSize would not fit.
    -> In that case we stop before doing the multiplication.
    -> The function name advertises the convention; this check enforces it.

    Modern C libraries often provide array allocation helpers for this reason.
    OpenSSL has OPENSSL_malloc_array / OPENSSL_realloc_array. The Linux kernel
    has kmalloc_array / kcalloc. We use the same idea here.
*/
static void *xmalloc_array(size_t count, size_t elementSize, const char *what) {
    if (elementSize != 0 && count > (size_t)-1 / elementSize) {
        fatal_oom(what);
    }
    return xmalloc(count * elementSize, what);
}

/*
    xrealloc
    --------

    Safe-ish realloc wrapper.

    !!! WHAT THE `x` PREFIX MEANS HERE TOO !!!
    -> `xrealloc` is "realloc, but checked; exit instead of returning NULL."
    -> The caller gets either usable memory or no return at all.

    !!! REALLOC FOOTGUN !!!
    -> `ptr = realloc(ptr, newSize);` is dangerous.
    -> If realloc fails, it returns NULL and the original pointer is still valid.
    -> But if you overwrote `ptr`, you lost the original allocation and leaked it.
    -> Always assign realloc to a temporary first.
*/
static void *xrealloc(void *ptr, size_t size, const char *what) {
    /*
        `newPtr` = the potential new allocation returned by realloc.

        !!! REALLOC RED ALERT !!!
        -> DO NOT write `ptr = realloc(ptr, size)` directly.
        -> If realloc fails, it returns NULL and leaves the old allocation alive.
        -> If you overwrote ptr with NULL, you lost the only handle to old memory.
        -> That is a memory leak wearing a fake mustache.
    */
    void *newPtr = realloc(ptr, size == 0 ? 1 : size);
    if (newPtr == NULL) {
        fatal_oom(what);
    }
    return newPtr;
}

/*
    xrealloc_array
    --------------

    Same idea as xmalloc_array, but for resizing existing heap storage.

    !!! REALLOC + OVERFLOW DOUBLE FOOTGUN !!!
    -> First check multiplication overflow.
    -> Then call xrealloc.
    -> Still assign through a helper path so the original pointer is not lost
       if allocation fails.

    Same exact maximum-size rule:
    -> If count > (size_t)-1 / elementSize, multiplication would overflow.
    -> Equivalent named form in codebases that prefer it:
       count > SIZE_MAX / elementSize
    -> Do not call realloc with a wrapped byte count.
*/
static void *xrealloc_array(void *ptr, size_t count, size_t elementSize, const char *what) {
    if (elementSize != 0 && count > (size_t)-1 / elementSize) {
        fatal_oom(what);
    }
    return xrealloc(ptr, count * elementSize, what);
}

/*
    xstrdup
    -------

    Standard C does not guarantee `strdup`, so we write our own.

    !!! WHAT THE `x` PREFIX MEANS HERE TOO !!!
    -> `strdup` duplicates a string onto the heap on platforms that provide it.
    -> `xstrdup` means "duplicate string, checked; exit on allocation failure."

    Ownership:
    -> returns a heap string owned by the caller.
    -> caller must eventually call free on it.

    !!! BUFFER SAFETY !!!
    -> strlen excludes the terminating '\0'.
    -> Allocate len + 1 so strcpy/memcpy has room for the terminator.
*/
static char *xstrdup(const char *text) {
    size_t len = strlen(text);
    char *copy = xmalloc(len + 1, "string copy");
    memcpy(copy, text, len + 1);
    return copy;
}

static void dynamic_card_array_init(DynamicCardArray *cards, size_t initialCapacity) {
    cards->count = 0;
    cards->capacity = initialCapacity;
    cards->items = initialCapacity == 0
        ? NULL
        : xmalloc_array(initialCapacity, sizeof(Card), "DynamicCardArray");
}

static void dynamic_card_array_free(DynamicCardArray *cards) {
    free(cards->items);
    cards->items = NULL;
    cards->count = 0;
    cards->capacity = 0;
}

static void dynamic_card_array_reserve(DynamicCardArray *cards, size_t needed) {
    if (needed <= cards->capacity) {
        return;
    }

    size_t newCapacity = cards->capacity == 0 ? STARTING_CARD_CAPACITY : cards->capacity;
    while (newCapacity < needed) {
        /*
            Capacity growth doubles the current capacity.

            !!! OVERFLOW CHECK IN THE CONTROL FLOW !!!
            -> Before `newCapacity *= 2`, prove doubling fits in size_t.
            -> Explicit form used here: `newCapacity > (size_t)-1 / 2`
            -> Equivalent named-limit form: `newCapacity > SIZE_MAX / 2`
            -> If the check trips, doubling would wrap around to a smaller value.
        */
        if (newCapacity > (size_t)-1 / 2) {
            fatal_oom("DynamicCardArray capacity overflow");
        }
        newCapacity *= 2;
    }

    cards->items = xrealloc_array(cards->items,
                                  newCapacity,
                                  sizeof(Card),
                                  "DynamicCardArray growth");
    cards->capacity = newCapacity;
}

static void dynamic_card_array_push(DynamicCardArray *cards, Card card) {
    dynamic_card_array_reserve(cards, cards->count + 1);
    cards->items[cards->count] = card;
    cards->count++;
}

static Card dynamic_card_array_pop(DynamicCardArray *cards) {
    if (cards->count == 0) {
        fprintf(stderr, "BUG: attempted to pop an empty DynamicCardArray.\n");
        exit(EXIT_FAILURE);
    }
    cards->count--;
    return cards->items[cards->count];
}

static Card dynamic_card_array_take(DynamicCardArray *cards, size_t index) {
    if (index >= cards->count) {
        fprintf(stderr, "BUG: attempted to take DynamicCardArray index out of range.\n");
        exit(EXIT_FAILURE);
    }

    Card card = cards->items[index];
    for (size_t i = index; i + 1 < cards->count; ++i) {
        cards->items[i] = cards->items[i + 1];
    }
    cards->count--;
    return card;
}

static void dynamic_card_array_move_all(DynamicCardArray *to, DynamicCardArray *from) {
    dynamic_card_array_reserve(to, to->count + from->count);
    for (size_t i = 0; i < from->count; ++i) {
        to->items[to->count++] = from->items[i];
    }
    from->count = 0;
}

static void player_init(Player *player, const char *name) {
    player->name = xstrdup(name);
    dynamic_card_array_init(&player->hand, STARTING_CARD_CAPACITY);
    player->totalScore = 0;
    player->reignScoreCount = 0;
    player->reignScoreCapacity = 4;
    player->reignScores = xmalloc_array(player->reignScoreCapacity, sizeof(int), "reign scores");
}

static void player_free(Player *player) {
    free(player->name);
    player->name = NULL;
    dynamic_card_array_free(&player->hand);
    free(player->reignScores);
    player->reignScores = NULL;
    player->reignScoreCount = 0;
    player->reignScoreCapacity = 0;
}

static void player_add_reign_score(Player *player, int score) {
    if (player->reignScoreCount == player->reignScoreCapacity) {
        size_t newCapacity = player->reignScoreCapacity * 2;
        player->reignScores = xrealloc_array(player->reignScores,
                                             newCapacity,
                                             sizeof(int),
                                             "reign score growth");
        player->reignScoreCapacity = newCapacity;
    }
    player->reignScores[player->reignScoreCount++] = score;
}

static void read_line(char *buffer, size_t size) {
    if (fgets(buffer, (int)size, stdin) == NULL) {
        buffer[0] = '\0';
        return;
    }
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
        buffer[len - 1] = '\0';
    }
}

static int prompt_int(const char *prompt, int minValue, int maxValue, int defaultValue) {
    char line[INPUT_BUFFER_SIZE];
    for (;;) {
        printf("%s", prompt);
        read_line(line, sizeof(line));
        if (line[0] == '\0') {
            return defaultValue;
        }

        char *end = NULL;
        long value = strtol(line, &end, 10);
        while (*end != '\0' && isspace((unsigned char)*end)) {
            end++;
        }
        if (*end == '\0' && value >= minValue && value <= maxValue) {
            return (int)value;
        }
        printf("Please enter a whole number from %d to %d, or press Enter for the default.\n",
               minValue, maxValue);
    }
}

static bool prompt_yes_no(const char *prompt, bool defaultValue) {
    char line[INPUT_BUFFER_SIZE];
    for (;;) {
        printf("%s", prompt);
        read_line(line, sizeof(line));
        if (line[0] == '\0') {
            return defaultValue;
        }
        char c = (char)tolower((unsigned char)line[0]);
        if (c == 'y') return true;
        if (c == 'n') return false;
        printf("Please answer y or n, or press Enter for the default.\n");
    }
}

static const char *suit_to_string(Suit suit) {
    switch (suit) {
        case CLUBS: return "C";
        case DIAMONDS: return "D";
        case HEARTS: return "H";
        case SPADES: return "S";
    }
    return "?";
}

static const char *rank_to_string(int rank, char *buffer, size_t size) {
    switch (rank) {
        case 11: return "Jack";
        case 12: return "Queen";
        case 13: return "King";
        case 14: return "Ace";
        default:
            snprintf(buffer, size, "%d", rank);
            return buffer;
    }
}

static int card_value(Card card) {
    return card.rank;
}

static void card_to_string(Card card, char *buffer, size_t size) {
    char rankBuffer[16];
    snprintf(buffer, size, "%s%s",
             rank_to_string(card.rank, rankBuffer, sizeof(rankBuffer)),
             suit_to_string(card.suit));
}

static void shuffle_cards(DynamicCardArray *cards) {
    for (size_t i = cards->count; i > 1; --i) {
        size_t j = (size_t)(rand() % (int)i);

        /*
            `tmp` = temporary.

            !!! INDUSTRY SHORTHAND !!!
            -> `tmp` is extremely common for a short-lived swap variable.
            -> It is acceptable here because its only job is to hold one card
               for the next two lines while two array slots trade places.
            -> If the value had game meaning, we would give it a game name.
        */
        Card tmp = cards->items[i - 1];
        cards->items[i - 1] = cards->items[j];
        cards->items[j] = tmp;
    }
}

static void build_deck(Game *game) {
    for (int suit = CLUBS; suit <= SPADES; ++suit) {
        for (int rank = 2; rank <= 14; ++rank) {
            Card card = {(Suit)suit, rank};
            dynamic_card_array_push(&game->drawDeck, card);
        }
    }
}

static bool draw_one(Game *game, Card *outCard) {
    if (game->drawDeck.count == 0) {
        if (game->discardPile.count == 0) {
            return false;
        }
        dynamic_card_array_move_all(&game->drawDeck, &game->discardPile);
        shuffle_cards(&game->drawDeck);
        printf("The discard pile is shuffled back into a fresh draw deck.\n");
    }
    *outCard = dynamic_card_array_pop(&game->drawDeck);
    return true;
}

static void draw_cards(Game *game, Player *player, int count) {
    for (int i = 0; i < count; ++i) {
        Card drawn;
        if (!draw_one(game, &drawn)) {
            printf("No card could be drawn. The deck and discard pile are both empty.\n");
            return;
        }
        dynamic_card_array_push(&player->hand, drawn);
    }
}

static int total_card_value(const DynamicCardArray *cards) {
    int total = 0;
    for (size_t i = 0; i < cards->count; ++i) {
        total += card_value(cards->items[i]);
    }
    return total;
}

static int highest_single_reign_score(const Player *player) {
    int best = 0;
    for (size_t i = 0; i < player->reignScoreCount; ++i) {
        if (player->reignScores[i] > best) {
            best = player->reignScores[i];
        }
    }
    return best;
}

static void print_hand_summary(const Player *player) {
    if (player->hand.count == 0) {
        printf("(empty)");
        return;
    }
    for (size_t i = 0; i < player->hand.count; ++i) {
        char cardText[32];
        card_to_string(player->hand.items[i], cardText, sizeof(cardText));
        if (i != 0) printf(", ");
        printf("[%zu] %s (%d)", i + 1, cardText, card_value(player->hand.items[i]));
    }
}

static void show_private_hand(const Player *player) {
    printf("\n%s, this is your private hand:\n", player->name);
    print_hand_summary(player);
    printf("\n");
}

static size_t choose_card_index_from_hand(const Player *player, const char *prompt) {
    char line[INPUT_BUFFER_SIZE];
    for (;;) {
        printf("%s", prompt);
        read_line(line, sizeof(line));
        char *end = NULL;
        long choice = strtol(line, &end, 10);
        while (*end != '\0' && isspace((unsigned char)*end)) end++;
        if (*end == '\0' && choice >= 1 && (size_t)choice <= player->hand.count) {
            return (size_t)(choice - 1);
        }
        printf("Please choose a valid card number.\n");
    }
}

static int choose_player_index(const Game *game, const int *candidates, size_t candidateCount, const char *prompt) {
    char line[INPUT_BUFFER_SIZE];
    for (;;) {
        printf("%s\n", prompt);
        for (size_t i = 0; i < candidateCount; ++i) {
            printf("  [%zu] %s\n", i + 1, game->players[candidates[i]].name);
        }
        read_line(line, sizeof(line));
        char *end = NULL;
        long choice = strtol(line, &end, 10);
        while (*end != '\0' && isspace((unsigned char)*end)) end++;
        if (*end == '\0' && choice >= 1 && (size_t)choice <= candidateCount) {
            return candidates[choice - 1];
        }
        printf("Please choose one of the listed numbers.\n");
    }
}

static void discard_card(Game *game, Card card) {
    dynamic_card_array_push(&game->discardPile, card);
}

static void show_public_table_state(const Game *game, int kingIndex) {
    printf("\n=== Public Table State ===\n");
    printf("King: %s\n", game->players[kingIndex].name);
    printf("Draw deck: %zu cards\n", game->drawDeck.count);
    printf("Discard pile: %zu cards\n", game->discardPile.count);
    printf("King's favor pile: %zu cards\n", game->favorPile.count);
    printf("Scores:\n");
    for (size_t i = 0; i < game->playerCount; ++i) {
        const Player *player = &game->players[i];
        printf("  %s: %d total", player->name, player->totalScore);
        if (player->reignScoreCount > 0) {
            printf(" | best reign %d", highest_single_reign_score(player));
        }
        printf("\n");
    }
}

static void resolve_parlay(Game *game, int kingIndex, int donorIndex, int interceptorIndex,
                           Card donationCard, bool rebellionActive, int rebelLeader) {
    Player *interceptor = &game->players[interceptorIndex];
    Player *defender = &game->players[donorIndex];
    Player *king = &game->players[kingIndex];
    printf("\nParlay declared by %s against %s's donation to %s.\n",
           interceptor->name, defender->name, king->name);

    int interceptorRoll = (rand() % 6) + 1;
    int defenderRoll = (rand() % 6) + 1;
    printf("%s rolls %d.\n", interceptor->name, interceptorRoll);
    printf("%s rolls %d.\n", defender->name, defenderRoll);

    char prompt[INPUT_BUFFER_SIZE];
    if (defenderRoll > interceptorRoll) {
        printf("%s has the higher roll and may cancel the parlay by discarding one card.\n",
               defender->name);
        snprintf(prompt, sizeof(prompt), "%s, discard one card to cancel? [y/N] ", defender->name);
        if (defender->hand.count > 0 && prompt_yes_no(prompt, false)) {
            snprintf(prompt, sizeof(prompt), "%s, choose a card to discard: ", defender->name);
            Card discarded = dynamic_card_array_take(&defender->hand, choose_card_index_from_hand(defender, prompt));
            discard_card(game, discarded);
            dynamic_card_array_push(&defender->hand, donationCard);
            char cardText[32];
            card_to_string(discarded, cardText, sizeof(cardText));
            printf("%s cancels the parlay by discarding %s.\n", defender->name, cardText);
            return;
        }
    }

    int bustPoint = interceptorRoll + defenderRoll;
    printf("Bust point is %d.\n", bustPoint);
    if (interceptor->hand.count == 0 || defender->hand.count == 0) {
        printf("A player lacks a required parlay card. Donation returns.\n");
        dynamic_card_array_push(&defender->hand, donationCard);
        return;
    }

    snprintf(prompt, sizeof(prompt), "%s, choose a card for the parlay: ", interceptor->name);
    Card interceptorCard = dynamic_card_array_take(&interceptor->hand, choose_card_index_from_hand(interceptor, prompt));
    snprintf(prompt, sizeof(prompt), "%s, play it face up? [y/N] ", interceptor->name);
    bool interceptorFaceUp = prompt_yes_no(prompt, false);
    snprintf(prompt, sizeof(prompt), "%s, choose a face-down parlay card: ", defender->name);
    Card defenderCard = dynamic_card_array_take(&defender->hand, choose_card_index_from_hand(defender, prompt));

    char interceptorCardText[32];
    char defenderCardText[32];
    card_to_string(interceptorCard, interceptorCardText, sizeof(interceptorCardText));
    card_to_string(defenderCard, defenderCardText, sizeof(defenderCardText));
    printf("%s plays %s %s.\n",
           interceptor->name,
           interceptorCardText,
           interceptorFaceUp ? "face up" : "face down");
    printf("Reveal: %s -> %s, %s -> %s.\n",
           interceptor->name,
           interceptorCardText,
           defender->name,
           defenderCardText);

    /*
        These are domain values, so they get full names.

        !!! NAMING JUDGMENT CALL !!!
        -> `i`, `j`, `tmp`, `ptr`, and `len` are common C idioms.
        -> `iv`, `dv`, `io`, and `dvo` are not useful industry vocabulary here.
        -> Spelling them out makes the parlay logic readable without decoding.
    */
    int interceptorValue = card_value(interceptorCard);
    int defenderValue = card_value(defenderCard);
    bool interceptorOverBustPoint = interceptorValue > bustPoint;
    bool defenderOverBustPoint = defenderValue > bustPoint;

    if (interceptorValue == defenderValue) {
        printf("Tie. Parlay cards discarded; donation returns.\n");
        discard_card(game, interceptorCard);
        discard_card(game, defenderCard);
        dynamic_card_array_push(&defender->hand, donationCard);
    } else if (interceptorOverBustPoint && defenderOverBustPoint) {
        printf("Both busted. Donation and parlay cards discarded.\n");
        discard_card(game, interceptorCard);
        discard_card(game, defenderCard);
        discard_card(game, donationCard);
    } else if (!interceptorOverBustPoint && !defenderOverBustPoint) {
        printf("Both under bust point. Cards return; each draws one.\n");
        dynamic_card_array_push(&interceptor->hand, interceptorCard);
        dynamic_card_array_push(&defender->hand, defenderCard);
        Card draw;
        if (draw_one(game, &draw)) dynamic_card_array_push(&interceptor->hand, draw);
        if (draw_one(game, &draw)) dynamic_card_array_push(&defender->hand, draw);
        dynamic_card_array_push(&defender->hand, donationCard);
    } else if (interceptorOverBustPoint && !defenderOverBustPoint) {
        printf("%s wins the parlay.\n", interceptor->name);
        discard_card(game, interceptorCard);
        discard_card(game, defenderCard);
        if (rebellionActive && rebelLeader >= 0 &&
            prompt_yes_no("  Give intercepted donation to Rebel Leader? [y/N] ", false)) {
            dynamic_card_array_push(&game->players[rebelLeader].hand, donationCard);
        } else {
            dynamic_card_array_push(&interceptor->hand, donationCard);
        }
    } else {
        printf("%s wins the parlay. Donation goes to favor pile.\n", defender->name);
        discard_card(game, interceptorCard);
        discard_card(game, defenderCard);
        dynamic_card_array_push(&game->favorPile, donationCard);
    }
}

static void resolve_donation(Game *game, int kingIndex, int donorIndex, bool rebellionActive, int rebelLeader) {
    Player *donor = &game->players[donorIndex];
    Player *king = &game->players[kingIndex];
    if (donor->hand.count == 0) {
        printf("%s has no cards to donate.\n", donor->name);
        return;
    }
    show_private_hand(donor);
    char prompt[INPUT_BUFFER_SIZE];
    snprintf(prompt, sizeof(prompt), "%s, choose a card to donate: ", donor->name);
    Card donation = dynamic_card_array_take(&donor->hand, choose_card_index_from_hand(donor, prompt));
    printf("%s places a donation face down before %s.\n", donor->name, king->name);

    for (int step = 1; step < PLAYER_COUNT; ++step) {
        int challengerIndex = (donorIndex + step) % PLAYER_COUNT;
        if (challengerIndex == kingIndex || game->players[challengerIndex].hand.count == 0) continue;
        snprintf(prompt, sizeof(prompt), "%s, do you want to Parlay? [y/N] ",
                 game->players[challengerIndex].name);
        if (prompt_yes_no(prompt, false)) {
            resolve_parlay(game, kingIndex, donorIndex, challengerIndex, donation, rebellionActive, rebelLeader);
            return;
        }
    }
    dynamic_card_array_push(&game->favorPile, donation);
    printf("No one parlayed. The donation goes to %s's favor pile.\n", king->name);
}

static void resolve_petition(Game *game, int kingIndex, int subjectIndex, bool *accusedTyranny) {
    Player *subject = &game->players[subjectIndex];
    Player *king = &game->players[kingIndex];
    printf("\n%s petitions %s.\n", subject->name, king->name);
    printf("1. Accuse tyranny\n2. Ask for a tax\n3. General petition / speech\n");
    int choice = prompt_int("Choose a petition: ", 1, 3, 3);
    if (choice == 1) {
        accusedTyranny[subjectIndex] = true;
        printf("%s publicly accuses %s of tyranny.\n", subject->name, king->name);
    } else if (choice == 2) {
        printf("%s asks for a tax, but the King is not forced to act.\n", subject->name);
    } else {
        printf("%s makes a general petition.\n", subject->name);
    }
}

static void resolve_tax(Game *game, int kingIndex) {
    int candidates[PLAYER_COUNT];
    size_t count = 0;
    for (int i = 0; i < PLAYER_COUNT; ++i) {
        if (i != kingIndex && game->players[i].hand.count > 0) candidates[count++] = i;
    }
    if (count == 0) {
        printf("No subject has a card available to tax.\n");
        return;
    }
    char prompt[INPUT_BUFFER_SIZE];
    snprintf(prompt, sizeof(prompt), "%s, choose a subject to tax:", game->players[kingIndex].name);
    int targetIndex = choose_player_index(game, candidates, count, prompt);
    Player *target = &game->players[targetIndex];
    snprintf(prompt, sizeof(prompt), "%s, choose a card to pay the tax: ", target->name);
    Card discarded = dynamic_card_array_take(&target->hand, choose_card_index_from_hand(target, prompt));
    discard_card(game, discarded);
    char cardText[32];
    card_to_string(discarded, cardText, sizeof(cardText));
    printf("%s taxes %s for %s.\n", game->players[kingIndex].name, target->name, cardText);
}

static int resolve_throne_entry(Game *game, int kingIndex) {
    Player *king = &game->players[kingIndex];
    if (king->hand.count == 0) {
        printf("%s arrives at the throne with no hand to manage.\n", king->name);
        return 0;
    }
    printf("\n%s is about to begin a reign and currently holds a hand.\n", king->name);
    show_private_hand(king);
    printf("1. Bank the Hand\n2. Open the Hand\n");
    char prompt[INPUT_BUFFER_SIZE];
    snprintf(prompt, sizeof(prompt), "%s, choose how to handle your throne hand: ", king->name);
    if (prompt_int(prompt, 1, 2, 1) == 1) {
        int bonus = total_card_value(&king->hand) / 2;
        printf("%s banks the hand for a Royal Coffers bonus of %d.\n", king->name, bonus);
        dynamic_card_array_move_all(&game->discardPile, &king->hand);
        return bonus;
    }

    int nextIndex = (kingIndex + 1) % PLAYER_COUNT;
    Player *next = &game->players[nextIndex];
    printf("%s opens the hand publicly.\n", king->name);
    printf("Current hand: "); print_hand_summary(king);
    printf("\n%s's hand: ", next->name); print_hand_summary(next); printf("\n");
    size_t maxTake = king->hand.count < next->hand.count ? king->hand.count : next->hand.count;
    snprintf(prompt, sizeof(prompt), "%s, how many cards do you want to take? ", next->name);
    int takeCount = prompt_int(prompt, 0, (int)maxTake, 0);
    for (int i = 0; i < takeCount; ++i) {
        printf("\n%s's revealed hand: ", king->name); print_hand_summary(king); printf("\n");
        snprintf(prompt, sizeof(prompt), "%s, choose a card to take: ", next->name);
        Card taken = dynamic_card_array_take(&king->hand, choose_card_index_from_hand(king, prompt));
        dynamic_card_array_push(&next->hand, taken);
        snprintf(prompt, sizeof(prompt), "%s, choose a card to discard in payment: ", next->name);
        Card paid = dynamic_card_array_take(&next->hand, choose_card_index_from_hand(next, prompt));
        discard_card(game, paid);
    }
    dynamic_card_array_move_all(&game->discardPile, &king->hand);
    printf("Any unclaimed throne cards are discarded.\n");
    return 0;
}

static int count_true(const bool *flags) {
    int count = 0;
    for (int i = 0; i < PLAYER_COUNT; ++i) if (flags[i]) count++;
    return count;
}

static void play_reign(Game *game, int kingIndex, int reignNumber, int turnsPerSubject) {
    Player *king = &game->players[kingIndex];
    printf("\n============================================================\n");
    printf("Reign %d of the cycle: %s is King.\n", reignNumber, king->name);
    printf("============================================================\n");
    int reignScore = resolve_throne_entry(game, kingIndex);
    bool accused[PLAYER_COUNT] = {false};
    bool rebels[PLAYER_COUNT] = {false};
    bool rebellionActive = false;
    int rebelLeader = -1;

    for (int round = 1; round <= turnsPerSubject; ++round) {
        printf("\n--- Subject round %d ---\n", round);
        for (int step = 1; step < PLAYER_COUNT; ++step) {
            int subjectIndex = (kingIndex + step) % PLAYER_COUNT;
            Player *subject = &game->players[subjectIndex];
            printf("\nIt is %s's turn.\nHand size: %zu\n", subject->name, subject->hand.count);
            if (rebels[subjectIndex]) printf("%s is part of the rebellion this reign.\n", subject->name);
            printf("1. Donate\n2. Draw\n3. Petition\n4. Declare Rebellion\n5. Pass\n");
            bool done = false;
            while (!done) {
                char prompt[INPUT_BUFFER_SIZE];
                snprintf(prompt, sizeof(prompt), "%s, choose an action: ", subject->name);
                int choice = prompt_int(prompt, 1, 5, 5);
                if (choice == 1) {
                    if (rebels[subjectIndex]) {
                        printf("Rebels may no longer donate to the current King.\n");
                    } else {
                        resolve_donation(game, kingIndex, subjectIndex, rebellionActive, rebelLeader);
                        done = true;
                    }
                } else if (choice == 2) {
                    Card drawn;
                    if (draw_one(game, &drawn)) dynamic_card_array_push(&subject->hand, drawn);
                    else printf("No cards can be drawn right now.\n");
                    done = true;
                } else if (choice == 3) {
                    resolve_petition(game, kingIndex, subjectIndex, accused);
                    done = true;
                } else if (choice == 4) {
                    if (count_true(accused) < 3) {
                        printf("Rebellion is not yet legal. At least three subjects must accuse tyranny.\n");
                    } else {
                        rebellionActive = true;
                        rebels[subjectIndex] = true;
                        printf("%s declares rebellion against %s!\n", subject->name, king->name);
                        if (rebelLeader < 0 && prompt_yes_no("Name a Rebel Leader now? [y/N] ", false)) {
                            int candidates[PLAYER_COUNT];
                            size_t candidateCount = 0;
                            for (int i = 0; i < PLAYER_COUNT; ++i) {
                                if (i != kingIndex) {
                                    candidates[candidateCount++] = i;
                                }
                            }
                            rebelLeader = choose_player_index(game, candidates, candidateCount,
                                                              "Choose the Rebel Leader from the subjects:");
                        }
                        done = true;
                    }
                } else {
                    printf("%s passes.\n", subject->name);
                    done = true;
                }
            }
        }

        char taxPrompt[INPUT_BUFFER_SIZE];
        snprintf(taxPrompt, sizeof(taxPrompt), "%s, do you want to levy a tax this round? [y/N] ", king->name);
        if (prompt_yes_no(taxPrompt, false)) resolve_tax(game, kingIndex);
        show_public_table_state(game, kingIndex);
    }

    reignScore += total_card_value(&game->favorPile);
    king->totalScore += reignScore;
    player_add_reign_score(king, reignScore);
    printf("\n%s scores this reign: %d. Total score now: %d\n", king->name, reignScore, king->totalScore);
    dynamic_card_array_move_all(&game->discardPile, &game->favorPile);
    if (king->hand.count == 0) {
        draw_cards(game, king, 5);
        printf("%s draws a new 5-card hand for the next time they are a subject.\n", king->name);
    }
}

static void game_init(Game *game) {
    memset(game, 0, sizeof(*game));
    game->playerCount = PLAYER_COUNT;
    game->players = xmalloc_array(game->playerCount, sizeof(Player), "players");
    dynamic_card_array_init(&game->drawDeck, 52);
    dynamic_card_array_init(&game->discardPile, STARTING_CARD_CAPACITY);
    dynamic_card_array_init(&game->favorPile, STARTING_CARD_CAPACITY);
    game->config.cycles = 1;
    game->config.reignTurnsPerSubject = 3;

    printf("Enter the five player names. Press Enter to use the G-IDLE defaults.\n");
    for (size_t i = 0; i < game->playerCount; ++i) {
        char input[INPUT_BUFFER_SIZE];
        printf("Player %zu name [%s]: ", i + 1, DEFAULT_PLAYER_NAMES[i]);
        read_line(input, sizeof(input));
        if (input[0] == '\0') {
            player_init(&game->players[i], DEFAULT_PLAYER_NAMES[i]);
        } else {
            player_init(&game->players[i], input);
        }
    }
    game->config.cycles = prompt_int("How many cycles do you want to play? [1-3, default 1] ", 1, 3, 1);
    game->config.reignTurnsPerSubject =
        prompt_int("How many turns should each subject get per reign? [1-6, default 3] ", 1, 6, 3);
    build_deck(game);
    shuffle_cards(&game->drawDeck);
    for (size_t i = 0; i < game->playerCount; ++i) draw_cards(game, &game->players[i], 5);
}

static void game_free(Game *game) {
    if (game->players != NULL) {
        for (size_t i = 0; i < game->playerCount; ++i) {
            player_free(&game->players[i]);
        }
    }
    free(game->players);
    game->players = NULL;
    game->playerCount = 0;
    dynamic_card_array_free(&game->drawDeck);
    dynamic_card_array_free(&game->discardPile);
    dynamic_card_array_free(&game->favorPile);
}

static int find_top_scorers(const Game *game, int *out) {
    int top = game->players[0].totalScore;
    for (size_t i = 1; i < game->playerCount; ++i) {
        if (game->players[i].totalScore > top) top = game->players[i].totalScore;
    }
    int count = 0;
    for (size_t i = 0; i < game->playerCount; ++i) {
        if (game->players[i].totalScore == top) out[count++] = (int)i;
    }
    return count;
}

static int resolve_tie_by_best_reign(const Game *game, const int *tied, int tiedCount, int *out) {
    int best = -1;
    int count = 0;
    for (int i = 0; i < tiedCount; ++i) {
        int score = highest_single_reign_score(&game->players[tied[i]]);
        if (score > best) {
            best = score;
            count = 0;
            out[count++] = tied[i];
        } else if (score == best) {
            out[count++] = tied[i];
        }
    }
    return count;
}

int main(void) {
    srand((unsigned int)time(NULL));
    Game game;
    game_init(&game);

    int kingIndex = rand() % PLAYER_COUNT;
    printf("\nA random first King has been chosen: %s.\n", game.players[kingIndex].name);
    printf("King order proceeds clockwise from there.\n");

    int reignNumber = 0;
    for (int cycle = 1; cycle <= game.config.cycles; ++cycle) {
        printf("\n==================== Cycle %d ====================\n", cycle);
        for (int reignInCycle = 1; reignInCycle <= PLAYER_COUNT; ++reignInCycle) {
            play_reign(&game, kingIndex, ++reignNumber, game.config.reignTurnsPerSubject);
            kingIndex = (kingIndex + 1) % PLAYER_COUNT;
        }
    }

    printf("\n==================== Final Scores ====================\n");
    for (size_t i = 0; i < game.playerCount; ++i) {
        printf("%s: total %d, best reign %d\n",
               game.players[i].name,
               game.players[i].totalScore,
               highest_single_reign_score(&game.players[i]));
    }

    int top[PLAYER_COUNT];
    int topCount = find_top_scorers(&game, top);
    int best[PLAYER_COUNT];
    int bestCount = resolve_tie_by_best_reign(&game, top, topCount, best);
    if (topCount == 1) {
        printf("\nWinner: %s\n", game.players[top[0]].name);
    } else if (bestCount == 1) {
        printf("\nWinner by best single reign: %s\n", game.players[best[0]].name);
    } else {
        printf("\nShared victory between:\n");
        for (int i = 0; i < bestCount; ++i) {
            printf("  %s\n", game.players[best[i]].name);
        }
    }

    game_free(&game);
    return 0;
}
