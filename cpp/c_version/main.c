/*
    King's Cards - C implementation
    =================================

    This file is a straight C version of the game implemented in the sibling
    C++ project. The goal is not to write the shortest possible C program; the
    goal is to make the moving parts visible.

    A few important design choices:

    1. Everything lives in one source file, just like the C++ version. That makes
       it easier to trace the whole game from setup, to reigns, to final scoring.

    2. The program uses fixed-size arrays instead of dynamic allocation. A normal
       deck only has 52 cards, and there are always 5 players, so we can keep the
       memory model simple:

       - A Card pile is `Card pile[52]` plus an integer count.
       - A player's hand is also `Card hand[52]` plus `handCount`.
       - Adding a card writes at `pile[count]`, then increments count.
       - Removing a card shifts everything after it one slot to the left.

       This is less flexible than C++ `std::vector`, but much easier to inspect
       while learning C because ownership is obvious. No malloc, no free, no
       hidden resizing.

       !!! NO MALLOC BY DESIGN !!!
       -> THIS VERSION INTENTIONALLY USES ZERO `malloc` CALLS.
       -> THAT MEANS THERE IS NO HEAP OWNERSHIP TO TRACK.
       -> THAT ALSO MEANS THERE IS NO `free` TO FORGET.
       -> THE TRADEOFF: EVERY ARRAY HAS A HARD LIMIT.
       -> ANY WRITE PAST THAT LIMIT IS MEMORY CORRUPTION, NOT A NICE EXCEPTION.

       Why not one malloc'd deck?

       We absolutely could do that. For example, we could allocate one `Card *`
       array with 52 cards and then move pointers to cards between lists. That is
       a common C design when objects need identity and long lifetimes.

       This program chooses fixed arrays instead because the game has small,
       known maximum sizes:

       - There are only 52 physical cards.
       - There are exactly 5 players.
       - A hand, draw deck, discard pile, or favor pile can never need more than
         52 card slots.
       - At most 15 reign scores are stored: 3 cycles times 5 kings.

       So the simplest safe model is:

       - Store `Card` values directly.
       - Copy cards between arrays when they move.
       - Track how many slots are currently active with an integer count.

       That avoids several heap-specific hazards:

       - No memory leaks, because there is no heap memory to lose.
       - No double-free bugs, because nothing is freed manually.
       - No use-after-free bugs, because cards are values, not borrowed heap
         pointers.
       - No ownership confusion, because every pile owns its own card slots.

       How do we know memory is freed appropriately?

       In this version, the main `Game` object is created as an ordinary local
       variable in `main`:

           Game game = create_game();

       A local variable like that has automatic storage duration. In normal words:
       it lives until the function returns. When `main` returns, the operating
       system reclaims the program's stack and static process memory. Since this
       program never calls `malloc`, there are no heap allocations that require
       matching `free` calls.

       !!! IMPORTANT DISTINCTION !!!
       -> "No malloc" does NOT mean "memory safety is automatic."
       -> It only removes heap-lifetime bugs.
       -> Fixed arrays can still overflow if counts are wrong.
       -> The safety burden moves from "did I free this?" to "did I stay inside
          the array bounds?"

    3. Most functions that change game state take `Game *game` or `Player *player`.
       In C, passing a struct by value copies the whole struct. Passing a pointer
       lets the function mutate the original object.

    4. Return values are used for success/failure in places where the C++ version
       used `std::optional`. For example, `draw_one` returns `true` if it produced
       a card and `false` if both the draw deck and discard pile are empty.

       !!! C FOOTGUN MAP !!!
       -> BUFFER OVERFLOW: fixed `char[]` buffers must use bounded writes.
       -> ARRAY OVERFLOW: `Card[52]` writes must check count < DECK_SIZE.
       -> ARRAY UNDERFLOW: popping from an empty pile gives index -1. Badness.
       -> SIGNED/UNSIGNED ISSUES: `sizeof` and `strlen` produce `size_t`, but
          most game counters here are `int`; do not mix casually.
       -> INDEX CONVERSION: user choices are 1-based; C arrays are 0-based.

    5. The comments are intentionally verbose. Some of them explain things an
       experienced C programmer would consider "obvious", because this version is
       meant to double as a map of the implementation and the control flow.
*/

#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*
    Constants
    ---------

    `#define` performs textual substitution before compilation. These constants
    are used for array sizes, so the compiler needs to know them at compile time.
*/
#define PLAYER_COUNT 5       /* The ruleset uses exactly five players. */
#define DECK_SIZE 52         /* A standard deck has 52 cards, no jokers. */
#define MAX_NAME_LEN 64      /* Fixed storage for each player's display name. */
#define MAX_REIGNS 15        /* 3 cycles * 5 players = at most 15 reign scores. */

static const char *DEFAULT_PLAYER_NAMES[PLAYER_COUNT] = {
    "Miyeon",
    "Minnie",
    "Soyeon",
    "Yuqi",
    "Shuhua"
};

/*
    Suit
    ----

    C enums are plain integer constants under the hood. We use them to make card
    suits readable in the source code instead of writing magic numbers like 0, 1,
    2, and 3 everywhere.
*/
typedef enum {
    CLUBS,
    DIAMONDS,
    HEARTS,
    SPADES
} Suit;

/*
    Card
    ----

    Rank uses the same representation as the C++ version:

    - 2 through 10 are numbered cards.
    - 11 is Jack.
    - 12 is Queen.
    - 13 is King.
    - 14 is Ace.

    The donation value of a card is just its rank, so this simple integer
    representation keeps scoring straightforward.

    !!! SAFETY NOTE !!!
    -> `rank` is assumed to be 2..14 after deck construction.
    -> The user never directly creates cards, so `build_deck` is the main trust
       boundary for valid card data.
*/
typedef struct {
    Suit suit;
    int rank;
} Card;

/*
    Player
    ------

    Notice how arrays and counts travel together:

    - `hand` stores the actual card slots.
    - `handCount` says how many of those slots are currently meaningful.

    If `handCount` is 5, then valid cards are `hand[0]` through `hand[4]`.
    Anything beyond that is just leftover memory and should be ignored.

    !!! BUFFER SAFETY !!!
    -> `name` is a fixed-size buffer, not a C++ string.
    -> It can hold 63 visible characters plus the required terminating '\0'.
    -> We fill it with `snprintf`, which truncates instead of overflowing.

    !!! ARRAY SAFETY !!!
    -> `handCount` must never be negative.
    -> `handCount` must never exceed DECK_SIZE.
    -> Every card access must satisfy: 0 <= index < handCount.
*/
typedef struct {
    char name[MAX_NAME_LEN];
    Card hand[DECK_SIZE];
    int handCount;
    int totalScore;
    int reignScores[MAX_REIGNS];
    int reignScoreCount;
} Player;

/*
    GameConfig
    ----------

    These are setup choices made by the players before the game begins. They are
    separated from the rest of `Game` because they describe the rules/options for
    this run, not the changing table state.
*/
typedef struct {
    int cycles;
    int reignTurnsPerSubject;
} GameConfig;

/*
    Game
    ----

    This is the whole mutable table state. Passing a `Game *` to a function gives
    that function access to players, decks, discard piles, scores, and config.

    In the C++ version, each pile was a `std::vector<Card>`. Here each pile is a
    fixed array plus a count:

    - `drawDeck` and `drawCount`
    - `discardPile` and `discardCount`
    - `favorPile` and `favorCount`

    The "top" of any pile is the last valid element, index `count - 1`. That makes
    push and pop cheap because they do not require shifting the array.

    !!! NO MALLOC !!!
    -> This struct owns all game storage directly.
    -> `Game game;` reserves the arrays immediately.
    -> `memset(&game, 0, sizeof(game))` later clears every field and count.
    -> Returning `Game` by value copies this whole struct. That is safe here,
       just not something you would do thoughtlessly for giant objects.
*/
typedef struct {
    GameConfig config;
    Player players[PLAYER_COUNT];
    Card drawDeck[DECK_SIZE];
    int drawCount;
    Card discardPile[DECK_SIZE];
    int discardCount;
    Card favorPile[DECK_SIZE];
    int favorCount;
} Game;

/*
    read_line
    ---------

    A small wrapper around `fgets`.

    Why use `fgets` instead of `scanf`?

    `scanf` leaves the newline character sitting in the input buffer in many
    common cases, which makes later line-based input annoying. Using `fgets`
    everywhere gives us one predictable model: every prompt reads one whole line.

    The function also removes the trailing newline, so callers receive `"Alice"`
    instead of `"Alice\n"`.

    !!! BUFFER OVERFLOW GUARD !!!
    -> `fgets(buffer, size, stdin)` reads at most `size - 1` characters.
    -> That leaves one byte for the terminating '\0'.
    -> DO NOT replace this with `gets`. `gets` is historically infamous and was
       removed from the C standard because it cannot be used safely.

    !!! TRUNCATION NOTE !!!
    -> If input is longer than the buffer, `fgets` reads only the first chunk.
    -> This toy program accepts that truncation.
    -> A production CLI would also drain the rest of the long line.
*/
static void read_line(char *buffer, size_t size) {
    if (fgets(buffer, (int)size, stdin) == NULL) {
        buffer[0] = '\0';
        return;
    }

    /*
        `strlen` returns `size_t`, an unsigned integer type.

        !!! SIGNED/UNSIGNED WARNING !!!
        -> Keep this variable as `size_t` because it describes a buffer length.
        -> Do not compare unsigned lengths with negative ints. C will convert the
           negative value to a huge unsigned value, and your condition may lie.
    */
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
        buffer[len - 1] = '\0';
    }
}

/*
    prompt_int
    ----------

    Repeatedly asks the user for a number until the input is valid.

    Parameters:

    - `prompt`: text shown to the user.
    - `minValue` / `maxValue`: accepted inclusive range.
    - `defaultValue`: returned when the user presses Enter on an empty line.

    `strtol` is used instead of `atoi` because `strtol` lets us detect junk after
    the number. For example, `"12abc"` should not be accepted as 12.

    !!! PARSING FOOTGUN !!!
    -> `atoi("abc")` returns 0, which is useless if 0 might be meaningful.
    -> `strtol` gives us an end pointer so we can reject half-parsed input.
    -> We parse into `long`, check bounds, then cast to `int`.
    -> That order matters. Cast first, validate later is how bugs sneak in.
*/
static int prompt_int(const char *prompt, int minValue, int maxValue, int defaultValue) {
    char line[128];

    for (;;) {
        printf("%s", prompt);
        read_line(line, sizeof(line));

        if (line[0] == '\0') {
            return defaultValue;
        }

        /*
            `end` points at the first character that was NOT part of the number.

            Example:
            -> input "12abc"
            -> value becomes 12
            -> end points at 'a'

            We reject that because the user did not enter a clean whole number.
        */
        char *end = NULL;
        long value = strtol(line, &end, 10);
        while (end != NULL && *end != '\0' && isspace((unsigned char)*end)) {
            ++end;
        }

        /*
            !!! CAST SAFETY !!!
            -> `value` is a long.
            -> We only convert it to int AFTER confirming it is inside the
               caller's requested int range.
        */
        if (end != NULL && *end == '\0' && value >= minValue && value <= maxValue) {
            return (int)value;
        }

        printf("Please enter a whole number from %d to %d, or press Enter for the default.\n",
               minValue, maxValue);
    }
}

/*
    prompt_yes_no
    -------------

    Similar to `prompt_int`, but accepts yes/no answers.

    Only the first character matters, so `y`, `yes`, `Y`, and `Yep` all count as
    yes. Empty input returns the caller-provided default.

    !!! CTYPE FOOTGUN !!!
    -> Functions like `tolower` are only safe for EOF or unsigned char values.
    -> Plain `char` may be signed depending on compiler/platform.
    -> So we cast to `(unsigned char)` before calling `tolower`.
*/
static bool prompt_yes_no(const char *prompt, bool defaultValue) {
    char line[128];

    for (;;) {
        printf("%s", prompt);
        read_line(line, sizeof(line));

        if (line[0] == '\0') {
            return defaultValue;
        }

        char c = (char)tolower((unsigned char)line[0]);
        if (c == 'y') {
            return true;
        }
        if (c == 'n') {
            return false;
        }

        printf("Please answer y or n, or press Enter for the default.\n");
    }
}

/*
    Card formatting helpers
    -----------------------

    These functions convert internal card data into terminal-friendly text.

    The C++ version returned `std::string`. In C, functions do not return owned
    strings conveniently unless you allocate memory, so this file mostly follows
    a common C pattern:

    - The caller provides a character buffer.
    - The function writes text into that buffer with `snprintf`.

    !!! BUFFER RULE !!!
    -> These formatting helpers DO NOT allocate strings.
    -> The caller owns the destination buffer.
    -> The caller must pass the true buffer size.
    -> Passing the wrong size defeats `snprintf`'s protection.
*/
static const char *suit_to_string(Suit suit) {
    switch (suit) {
        case CLUBS: return "C";
        case DIAMONDS: return "D";
        case HEARTS: return "H";
        case SPADES: return "S";
    }
    return "?";
}

/*
    rank_to_string returns a pointer to readable rank text.

    Face cards can return string literals like `"Queen"`. Number cards need to be
    formatted into the caller-provided buffer because there is no permanent
    string literal for every possible integer.

    !!! LIFETIME NOTE !!!
    -> Returning `"Queen"` is safe: string literals live for the whole program.
    -> Returning `buffer` is safe because the caller owns that storage.
    -> DO NOT return a pointer to a local `char local[16]`; it dies on return.
*/
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

/* Donation/scoring value currently equals rank. This wrapper names that rule. */
static int card_value(Card card) {
    return card.rank;
}

/*
    Builds a compact card label such as:

    - `2C`
    - `10H`
    - `QueenD`
    - `AceS`

    !!! BUFFER OVERFLOW GUARD !!!
    -> `snprintf` receives `size`, the destination capacity.
    -> If the destination is too small, output is truncated, not overflowed.
    -> Truncation can still be a display bug, but it is not memory corruption.
*/
static void card_to_string(Card card, char *buffer, size_t size) {
    char rankBuffer[16];
    snprintf(buffer, size, "%s%s", rank_to_string(card.rank, rankBuffer, sizeof(rankBuffer)),
             suit_to_string(card.suit));
}

/*
    push_card
    ---------

    Adds one card to the end of a pile/hand.

    This is the fixed-array replacement for `vector.push_back(card)`:

    - Write the card at the first unused slot.
    - Increase the count.

    The `count` parameter is a pointer because the function must update the
    caller's count variable, not a local copy.

    !!! ARRAY OVERFLOW GUARD !!!
    -> This is the main gatekeeper for writing into card arrays.
    -> If `*count >= DECK_SIZE`, writing `cards[*count]` would be out of bounds.
    -> Out-of-bounds writes in C can corrupt random nearby state.
    -> That kind of bug can look unrelated to the actual mistake. Very rude.
*/
static void push_card(Card *cards, int *count, Card card) {
    if (*count >= DECK_SIZE) {
        printf("Internal warning: a card pile is full, so %d%s could not be added.\n",
               card.rank, suit_to_string(card.suit));
        return;
    }
    cards[*count] = card;
    ++(*count);
}

/*
    take_card_at
    ------------

    Removes and returns the card at a chosen index.

    This is the fixed-array replacement for the C++ helper that erased from a
    vector. Arrays cannot shrink themselves, so we manually close the gap:

    Before removing index 1:
      [A][B][C][D] count = 4

    Save B, shift C and D left:
      [A][C][D][D] count = 4

    Decrement count:
      [A][C][D]    count = 3

    The duplicate leftover in the final physical slot is ignored because the
    count says the pile ends earlier.

    !!! PRECONDITION !!!
    -> Caller must guarantee: 0 <= index < *count.
    -> This function does NOT re-check the index because current callers get
       indexes from validation helpers.
    -> If you reuse it somewhere else, validate before calling.

    !!! ARRAY UNDERFLOW WARNING !!!
    -> Calling this with *count == 0 would decrement the count to -1.
    -> Negative counts poison later bounds checks.
    -> In C, the compiler will not save you from this. Tiny trapdoor, big fall.
*/
static Card take_card_at(Card *cards, int *count, int index) {
    Card card = cards[index];
    for (int i = index; i < *count - 1; ++i) {
        cards[i] = cards[i + 1];
    }
    --(*count);
    return card;
}

/*
    build_deck
    ----------

    Fills the draw deck with all 52 suit/rank combinations in a predictable
    order. The deck is shuffled later, after setup is complete.

    !!! CAPACITY CHECK !!!
    -> These loops create exactly 4 * 13 = 52 cards.
    -> `push_card` still checks capacity, so future loop edits fail loudly
       instead of silently writing past the end.
*/
static void build_deck(Game *game) {
    game->drawCount = 0;
    for (int suit = CLUBS; suit <= SPADES; ++suit) {
        for (int rank = 2; rank <= 14; ++rank) {
            Card card = {(Suit)suit, rank};
            push_card(game->drawDeck, &game->drawCount, card);
        }
    }
}

/*
    shuffle_cards
    -------------

    Fisher-Yates shuffle.

    Starting from the last card, choose a random earlier position (possibly the
    same position) and swap. After each iteration, the suffix of the array is
    randomized and locked in place.

    `rand()` is not fancy cryptographic randomness, but it is adequate for this
    terminal game prototype.

    !!! RANDOMNESS NOTE !!!
    -> `rand() % n` has modulo bias.
    -> Fine for a casual prototype.
    -> Not fine for gambling, security, or rigorous statistical fairness.

    !!! SIGNED/UNSIGNED UNDERFLOW NOTE !!!
    -> `i` is a signed int on purpose.
    -> The common `for (size_t i = count - 1; i >= 0; --i)` pattern is broken:
       unsigned integers never go below 0; they wrap to a huge number.
*/
static void shuffle_cards(Card *cards, int count) {
    for (int i = count - 1; i > 0; --i) {
        int j = rand() % (i + 1);
        Card tmp = cards[i];
        cards[i] = cards[j];
        cards[j] = tmp;
    }
}

/*
    draw_one
    --------

    Attempts to draw a single card into `*outCard`.

    Control flow:

    1. If the draw deck has cards, pop the last card and return true.
    2. If the draw deck is empty but the discard pile has cards, move the discard
       pile into the draw deck, shuffle it, then draw.
    3. If both are empty, return false.

    This function is where the C implementation imitates C++ `std::optional`.
    Instead of returning "maybe a Card", it returns a boolean success flag and
    writes the card through an output pointer.

    !!! ARRAY UNDERFLOW GUARD !!!
    -> The dangerous expression is `drawDeck[drawCount - 1]`.
    -> If drawCount is 0, that becomes `drawDeck[-1]`.
    -> So the function first either refills the deck or returns false.

    !!! OUTPUT POINTER CONTRACT !!!
    -> Caller must pass a valid `Card *outCard`.
    -> This function does not allocate a card. It writes into caller storage.
*/
static bool draw_one(Game *game, Card *outCard) {
    if (game->drawCount == 0) {
        if (game->discardCount == 0) {
            return false;
        }

        for (int i = 0; i < game->discardCount; ++i) {
            game->drawDeck[i] = game->discardPile[i];
        }
        game->drawCount = game->discardCount;
        game->discardCount = 0;
        shuffle_cards(game->drawDeck, game->drawCount);
        printf("The discard pile is shuffled back into a fresh draw deck.\n");
    }

    *outCard = game->drawDeck[game->drawCount - 1];
    --game->drawCount;
    return true;
}

/*
    draw_cards
    ----------

    Draws up to `count` cards into a player's hand. If the deck system runs dry,
    the function stops early and leaves the player's hand with however many cards
    were successfully drawn.

    !!! PARTIAL SUCCESS NOTE !!!
    -> If you ask for 5 cards and only 3 can be drawn, the player keeps those 3.
    -> The function does not roll back. That matches the simple interactive game
       style but is worth noticing.
*/
static void draw_cards(Game *game, Player *player, int count) {
    for (int i = 0; i < count; ++i) {
        Card drawn;
        if (!draw_one(game, &drawn)) {
            printf("No card could be drawn. The deck and discard pile are both empty.\n");
            return;
        }
        push_card(player->hand, &player->handCount, drawn);
    }
}

/*
    total_card_value
    ----------------

    Sums the scoring value of a pile or hand.

    !!! ARRAY BOUNDS CONTRACT !!!
    -> `count` must be the number of valid cards in `cards`.
    -> The function trusts that `cards[0]` through `cards[count - 1]` exist.
*/
static int total_card_value(const Card *cards, int count) {
    int total = 0;
    for (int i = 0; i < count; ++i) {
        total += card_value(cards[i]);
    }
    return total;
}

/*
    highest_single_reign_score
    --------------------------

    Used for the final tiebreaker and score display.

    If a player somehow has no recorded reign scores yet, returning 0 keeps the
    final scoring code simple.
*/
static int highest_single_reign_score(const Player *player) {
    int best = 0;
    for (int i = 0; i < player->reignScoreCount; ++i) {
        if (player->reignScores[i] > best) {
            best = player->reignScores[i];
        }
    }
    return best;
}

/*
    print_hand_summary
    ------------------

    Prints cards using 1-based labels because humans naturally choose "card 1",
    while the underlying array stores that card at index 0.

    !!! INDEX TRANSLATION !!!
    -> Display: [1], [2], [3]
    -> Storage:  0,   1,   2
    -> All user selections are converted after validation.
*/
static void print_hand_summary(const Player *player) {
    if (player->handCount == 0) {
        printf("(empty)");
        return;
    }

    for (int i = 0; i < player->handCount; ++i) {
        char cardText[32];
        card_to_string(player->hand[i], cardText, sizeof(cardText));
        if (i != 0) {
            printf(", ");
        }
        printf("[%d] %s (%d)", i + 1, cardText, card_value(player->hand[i]));
    }
}

/*
    show_private_hand
    -----------------

    This is separated from `print_hand_summary` so callers get a consistent
    heading and spacing whenever a player needs to choose from their hand.
*/
static void show_private_hand(const Player *player) {
    printf("\n%s, this is your private hand:\n", player->name);
    print_hand_summary(player);
    printf("\n");
}

/*
    show_public_table_state
    -----------------------

    Prints only public information: pile sizes and scores. It does not expose
    card contents from private hands or face-down donations.
*/
static void show_public_table_state(const Game *game, int kingIndex) {
    printf("\n=== Public Table State ===\n");
    printf("King: %s\n", game->players[kingIndex].name);
    printf("Draw deck: %d cards\n", game->drawCount);
    printf("Discard pile: %d cards\n", game->discardCount);
    printf("King's favor pile: %d cards\n", game->favorCount);
    printf("Scores:\n");

    for (int i = 0; i < PLAYER_COUNT; ++i) {
        const Player *player = &game->players[i];
        printf("  %s: %d total", player->name, player->totalScore);
        if (player->reignScoreCount > 0) {
            printf(" | best reign %d", highest_single_reign_score(player));
        }
        printf("\n");
    }
}

/*
    choose_card_index_from_hand
    ---------------------------

    Prompts until the user chooses a valid card number from a player's hand.

    Return value:
    -> zero-based array index, safe to pass to `take_card_at`.

    !!! OFF-BY-ONE WARNING !!!
    -> User enters 1 for the first card.
    -> C needs index 0 for the first card.
    -> We validate `choice >= 1` first, then return `choice - 1`.
*/
static int choose_card_index_from_hand(const Player *player, const char *prompt) {
    char line[128];

    for (;;) {
        printf("%s", prompt);
        read_line(line, sizeof(line));

        /* Same `strtol` pattern as `prompt_int`: reject half-numeric input. */
        char *end = NULL;
        long choice = strtol(line, &end, 10);
        while (end != NULL && *end != '\0' && isspace((unsigned char)*end)) {
            ++end;
        }

        if (end != NULL && *end == '\0' && choice >= 1 && choice <= player->handCount) {
            return (int)choice - 1;
        }

        printf("Please choose a valid card number.\n");
    }
}

/*
    choose_player_index
    -------------------

    Similar to card selection, but the numbered list contains candidate player
    indexes. The caller decides who is eligible; this helper only displays and
    validates the choice.

    Example:
    -> candidates = {2, 4}
    -> user chooses displayed option [1]
    -> function returns actual player index 2
*/
static int choose_player_index(const Game *game, const int *candidates, int candidateCount, const char *prompt) {
    char line[128];

    for (;;) {
        printf("%s\n", prompt);
        for (int i = 0; i < candidateCount; ++i) {
            printf("  [%d] %s\n", i + 1, game->players[candidates[i]].name);
        }

        read_line(line, sizeof(line));
        char *end = NULL;
        long choice = strtol(line, &end, 10);
        while (end != NULL && *end != '\0' && isspace((unsigned char)*end)) {
            ++end;
        }

        if (end != NULL && *end == '\0' && choice >= 1 && choice <= candidateCount) {
            return candidates[choice - 1];
        }

        printf("Please choose one of the listed numbers.\n");
    }
}

/*
    subject_order_clockwise
    -----------------------

    Builds the list of subjects who act during a reign, starting with the player
    clockwise after the king. The king is skipped because the king does not take
    subject turns during their own reign.

    !!! CALLER-PROVIDED ARRAY !!!
    -> `order` must have room for PLAYER_COUNT - 1 integers.
    -> This file passes a PLAYER_COUNT-sized array, so there is spare room.
*/
static void subject_order_clockwise(int kingIndex, int *order, int *count) {
    *count = 0;
    for (int step = 1; step < PLAYER_COUNT; ++step) {
        order[*count] = (kingIndex + step) % PLAYER_COUNT;
        ++(*count);
    }
}

/*
    discard_card
    ------------

    Tiny naming helper around `push_card` so gameplay code reads like the rules:
    "discard this card" instead of "push into discardPile".
*/
static void discard_card(Game *game, Card card) {
    push_card(game->discardPile, &game->discardCount, card);
}

static void resolve_parlay(Game *game,
                           int kingIndex,
                           int donorIndex,
                           int interceptorIndex,
                           Card donationCard,
                           bool rebellionActive,
                           int rebelLeader) {
    Player *interceptor = &game->players[interceptorIndex];
    Player *defender = &game->players[donorIndex];
    Player *king = &game->players[kingIndex];

    printf("\nParlay declared by %s against %s's donation to %s.\n",
           interceptor->name, defender->name, king->name);

    int interceptorRoll = (rand() % 6) + 1;
    int defenderRoll = (rand() % 6) + 1;

    printf("%s rolls %d.\n", interceptor->name, interceptorRoll);
    printf("%s rolls %d.\n", defender->name, defenderRoll);

    if (defenderRoll > interceptorRoll) {
        printf("%s has the higher roll and may cancel the parlay by discarding one card.\n",
               defender->name);
        char prompt[160];
        snprintf(prompt, sizeof(prompt), "%s, do you want to discard one card to cancel the parlay? [y/N] ",
                 defender->name);
        if (defender->handCount > 0 && prompt_yes_no(prompt, false)) {
            snprintf(prompt, sizeof(prompt), "%s, choose a card to discard: ", defender->name);
            int discardIndex = choose_card_index_from_hand(defender, prompt);
            Card discarded = take_card_at(defender->hand, &defender->handCount, discardIndex);
            discard_card(game, discarded);
            push_card(defender->hand, &defender->handCount, donationCard);

            char cardText[32];
            card_to_string(discarded, cardText, sizeof(cardText));
            printf("%s cancels the parlay by discarding %s.\n", defender->name, cardText);
            return;
        }
        printf("%s declines the cancel option or cannot pay it.\n", defender->name);
    }

    int bustPoint = interceptorRoll + defenderRoll;
    printf("Bust point is %d.\n", bustPoint);

    if (interceptor->handCount == 0 || defender->handCount == 0) {
        printf("A player lacks a required parlay card. The donation is returned to the defender.\n");
        push_card(defender->hand, &defender->handCount, donationCard);
        return;
    }

    char prompt[160];
    snprintf(prompt, sizeof(prompt), "%s, choose a card for the parlay: ", interceptor->name);
    int interceptorCardIndex = choose_card_index_from_hand(interceptor, prompt);

    snprintf(prompt, sizeof(prompt), "%s, play it face up? [y/N] ", interceptor->name);
    bool interceptorFaceUp = prompt_yes_no(prompt, false);
    Card interceptorCard = take_card_at(interceptor->hand, &interceptor->handCount, interceptorCardIndex);

    snprintf(prompt, sizeof(prompt), "%s, choose a face-down parlay card: ", defender->name);
    int defenderCardIndex = choose_card_index_from_hand(defender, prompt);
    Card defenderCard = take_card_at(defender->hand, &defender->handCount, defenderCardIndex);

    char interceptorText[32];
    char defenderText[32];
    card_to_string(interceptorCard, interceptorText, sizeof(interceptorText));
    card_to_string(defenderCard, defenderText, sizeof(defenderText));

    printf("%s plays %s %s.\n", interceptor->name, interceptorText,
           interceptorFaceUp ? "face up" : "face down");
    printf("%s plays a face-down card.\n", defender->name);
    printf("Reveal: %s -> %s, %s -> %s.\n",
           interceptor->name, interceptorText, defender->name, defenderText);

    int interceptorValue = card_value(interceptorCard);
    int defenderValue = card_value(defenderCard);
    bool interceptorOver = interceptorValue > bustPoint;
    bool defenderOver = defenderValue > bustPoint;

    if (interceptorValue == defenderValue) {
        printf("It is a tie on card value. Both parlay cards are discarded and the donation returns to the defender.\n");
        discard_card(game, interceptorCard);
        discard_card(game, defenderCard);
        push_card(defender->hand, &defender->handCount, donationCard);
        return;
    }

    if (interceptorOver && defenderOver) {
        printf("Both players busted. The donation and both parlay cards are discarded.\n");
        discard_card(game, interceptorCard);
        discard_card(game, defenderCard);
        discard_card(game, donationCard);
        return;
    }

    if (!interceptorOver && !defenderOver) {
        printf("Both players are under the bust point. Both parlay cards return to hand, then each draws one.\n");
        push_card(interceptor->hand, &interceptor->handCount, interceptorCard);
        push_card(defender->hand, &defender->handCount, defenderCard);

        Card draw;
        if (draw_one(game, &draw)) {
            char cardText[32];
            push_card(interceptor->hand, &interceptor->handCount, draw);
            card_to_string(draw, cardText, sizeof(cardText));
            printf("%s draws %s.\n", interceptor->name, cardText);
        } else {
            printf("%s cannot draw because no cards remain.\n", interceptor->name);
        }

        if (draw_one(game, &draw)) {
            char cardText[32];
            push_card(defender->hand, &defender->handCount, draw);
            card_to_string(draw, cardText, sizeof(cardText));
            printf("%s draws %s.\n", defender->name, cardText);
        } else {
            printf("%s cannot draw because no cards remain.\n", defender->name);
        }

        push_card(defender->hand, &defender->handCount, donationCard);
        return;
    }

    if (interceptorOver && !defenderOver) {
        printf("%s wins the parlay.\n", interceptor->name);
        discard_card(game, interceptorCard);
        discard_card(game, defenderCard);
        if (rebellionActive && rebelLeader >= 0) {
            printf("Rebellion is active. Give the intercepted donation to %s instead of keeping it?\n",
                   game->players[rebelLeader].name);
            if (prompt_yes_no("  Give it to the Rebel Leader? [y/N] ", false)) {
                push_card(game->players[rebelLeader].hand, &game->players[rebelLeader].handCount, donationCard);
                printf("%s receives the intercepted donation.\n", game->players[rebelLeader].name);
                return;
            }
        }
        push_card(interceptor->hand, &interceptor->handCount, donationCard);
        printf("%s takes the donated card into hand.\n", interceptor->name);
        return;
    }

    if (!interceptorOver && defenderOver) {
        printf("%s wins the parlay.\n", defender->name);
        discard_card(game, interceptorCard);
        discard_card(game, defenderCard);
        push_card(game->favorPile, &game->favorCount, donationCard);
        printf("The donation survives and goes to the King's favor pile.\n");
        return;
    }

    push_card(defender->hand, &defender->handCount, donationCard);
}

static void resolve_donation(Game *game, int kingIndex, int donorIndex, bool rebellionActive, int rebelLeader) {
    Player *donor = &game->players[donorIndex];
    Player *king = &game->players[kingIndex];

    if (donor->handCount == 0) {
        printf("%s has no cards to donate.\n", donor->name);
        return;
    }

    show_private_hand(donor);
    char prompt[160];
    snprintf(prompt, sizeof(prompt), "%s, choose a card to donate: ", donor->name);
    int donationIndex = choose_card_index_from_hand(donor, prompt);
    Card donationCard = take_card_at(donor->hand, &donor->handCount, donationIndex);

    printf("%s places a donation face down before %s.\n", donor->name, king->name);

    int parlayOrder[PLAYER_COUNT];
    int parlayCount = 0;
    for (int step = 1; step < PLAYER_COUNT; ++step) {
        int candidate = (donorIndex + step) % PLAYER_COUNT;
        if (candidate != kingIndex) {
            parlayOrder[parlayCount++] = candidate;
        }
    }

    for (int i = 0; i < parlayCount; ++i) {
        int challengerIndex = parlayOrder[i];
        Player *challenger = &game->players[challengerIndex];
        if (challenger->handCount == 0) {
            continue;
        }

        snprintf(prompt, sizeof(prompt), "%s, do you want to Parlay? [y/N] ", challenger->name);
        if (prompt_yes_no(prompt, false)) {
            resolve_parlay(game, kingIndex, donorIndex, challengerIndex, donationCard,
                           rebellionActive, rebelLeader);
            return;
        }
    }

    push_card(game->favorPile, &game->favorCount, donationCard);
    printf("No one parlayed. The donation goes to %s's favor pile.\n", king->name);
}

static void resolve_petition(Game *game, int kingIndex, int subjectIndex, bool *accusedTyranny) {
    Player *subject = &game->players[subjectIndex];
    Player *king = &game->players[kingIndex];

    printf("\n%s petitions %s.\n", subject->name, king->name);
    printf("1. Accuse tyranny\n");
    printf("2. Ask for a tax\n");
    printf("3. General petition / speech\n");

    int choice = prompt_int("Choose a petition: ", 1, 3, 3);
    if (choice == 1) {
        accusedTyranny[subjectIndex] = true;
        printf("%s publicly accuses %s of tyranny.\n", subject->name, king->name);
    } else if (choice == 2) {
        printf("%s asks for a tax, but the King is not forced to act.\n", subject->name);
    } else {
        printf("%s makes a general petition. Political meaning is left to the table.\n", subject->name);
    }
}

static void resolve_tax(Game *game, int kingIndex) {
    Player *king = &game->players[kingIndex];
    int validTargets[PLAYER_COUNT];
    int validCount = 0;

    for (int i = 0; i < PLAYER_COUNT; ++i) {
        if (i != kingIndex && game->players[i].handCount > 0) {
            validTargets[validCount++] = i;
        }
    }

    if (validCount == 0) {
        printf("No subject has a card available to tax.\n");
        return;
    }

    char prompt[160];
    snprintf(prompt, sizeof(prompt), "%s, choose a subject to tax:", king->name);
    int targetIndex = choose_player_index(game, validTargets, validCount, prompt);
    Player *target = &game->players[targetIndex];

    snprintf(prompt, sizeof(prompt), "%s, choose a card to pay the tax: ", target->name);
    int discardIndex = choose_card_index_from_hand(target, prompt);
    Card discarded = take_card_at(target->hand, &target->handCount, discardIndex);
    discard_card(game, discarded);

    char cardText[32];
    card_to_string(discarded, cardText, sizeof(cardText));
    printf("%s taxes %s for %s.\n", king->name, target->name, cardText);
}

static int resolve_throne_entry(Game *game, int kingIndex) {
    Player *king = &game->players[kingIndex];

    if (king->handCount == 0) {
        printf("%s arrives at the throne with no hand to manage.\n", king->name);
        return 0;
    }

    printf("\n%s is about to begin a reign and currently holds a hand.\n", king->name);
    show_private_hand(king);
    printf("1. Bank the Hand\n");
    printf("2. Open the Hand\n");

    char prompt[160];
    snprintf(prompt, sizeof(prompt), "%s, choose how to handle your throne hand: ", king->name);
    int choice = prompt_int(prompt, 1, 2, 1);
    if (choice == 1) {
        int handValue = total_card_value(king->hand, king->handCount);
        int bonus = handValue / 2;
        printf("%s banks the hand for a Royal Coffers bonus of %d.\n", king->name, bonus);
        for (int i = 0; i < king->handCount; ++i) {
            discard_card(game, king->hand[i]);
        }
        king->handCount = 0;
        return bonus;
    }

    int nextPlayerIndex = (kingIndex + 1) % PLAYER_COUNT;
    Player *nextPlayer = &game->players[nextPlayerIndex];

    printf("%s opens the hand publicly.\n", king->name);
    printf("%s may take any number of cards, limited by both hands.\n", nextPlayer->name);
    printf("Current hand: ");
    print_hand_summary(king);
    printf("\n%s's hand: ", nextPlayer->name);
    print_hand_summary(nextPlayer);
    printf("\n");

    int maxTake = king->handCount < nextPlayer->handCount ? king->handCount : nextPlayer->handCount;
    snprintf(prompt, sizeof(prompt), "%s, how many cards do you want to take? ", nextPlayer->name);
    int takeCount = prompt_int(prompt, 0, maxTake, 0);

    for (int i = 0; i < takeCount; ++i) {
        printf("\n%s's revealed hand: ", king->name);
        print_hand_summary(king);
        printf("\n");

        snprintf(prompt, sizeof(prompt), "%s, choose a card to take: ", nextPlayer->name);
        int takeIndex = choose_card_index_from_hand(king, prompt);
        Card taken = take_card_at(king->hand, &king->handCount, takeIndex);
        push_card(nextPlayer->hand, &nextPlayer->handCount, taken);

        char cardText[32];
        card_to_string(taken, cardText, sizeof(cardText));
        printf("%s takes %s.\n", nextPlayer->name, cardText);

        if (nextPlayer->handCount == 0) {
            printf("%s has no cards left to discard as payment.\n", nextPlayer->name);
            break;
        }

        snprintf(prompt, sizeof(prompt), "%s, choose a card to discard in payment: ", nextPlayer->name);
        int discardIndex = choose_card_index_from_hand(nextPlayer, prompt);
        Card discarded = take_card_at(nextPlayer->hand, &nextPlayer->handCount, discardIndex);
        discard_card(game, discarded);
        card_to_string(discarded, cardText, sizeof(cardText));
        printf("%s discards %s as payment.\n", nextPlayer->name, cardText);
    }

    for (int i = 0; i < king->handCount; ++i) {
        discard_card(game, king->hand[i]);
    }
    king->handCount = 0;
    printf("Any unclaimed throne cards are discarded.\n");
    return 0;
}

static void score_favor_pile(Game *game, int kingIndex, int reignScore) {
    Player *king = &game->players[kingIndex];
    reignScore += total_card_value(game->favorPile, game->favorCount);
    king->totalScore += reignScore;
    if (king->reignScoreCount < MAX_REIGNS) {
        king->reignScores[king->reignScoreCount++] = reignScore;
    }

    printf("\n%s scores this reign:\n", king->name);
    printf("  Favor pile total: %d\n", reignScore);
    printf("  Total score now: %d\n", king->totalScore);
}

static void clear_favor_pile_to_discard(Game *game) {
    for (int i = 0; i < game->favorCount; ++i) {
        discard_card(game, game->favorPile[i]);
    }
    game->favorCount = 0;
}

static int count_true(const bool *flags) {
    int count = 0;
    for (int i = 0; i < PLAYER_COUNT; ++i) {
        if (flags[i]) {
            ++count;
        }
    }
    return count;
}

static void play_reign(Game *game, int kingIndex, int reignNumber, int reignsPerCycle) {
    Player *king = &game->players[kingIndex];
    printf("\n============================================================\n");
    printf("Reign %d of the cycle: %s is King.\n", reignNumber, king->name);
    printf("============================================================\n");

    int reignScore = resolve_throne_entry(game, kingIndex);
    bool accusedTyranny[PLAYER_COUNT] = {false};
    bool rebellionActive = false;
    int rebelLeader = -1;
    bool rebelFlags[PLAYER_COUNT] = {false};

    for (int round = 1; round <= reignsPerCycle; ++round) {
        printf("\n--- Subject round %d ---\n", round);

        bool taxUsedThisRound = false;
        int order[PLAYER_COUNT];
        int orderCount = 0;
        subject_order_clockwise(kingIndex, order, &orderCount);

        for (int o = 0; o < orderCount; ++o) {
            int subjectIndex = order[o];
            Player *subject = &game->players[subjectIndex];

            printf("\nIt is %s's turn.\n", subject->name);
            printf("Hand size: %d\n", subject->handCount);

            if (rebelFlags[subjectIndex]) {
                printf("%s is part of the rebellion this reign.\n", subject->name);
            }

            printf("1. Donate\n");
            printf("2. Draw\n");
            printf("3. Petition\n");
            printf("4. Declare Rebellion\n");
            printf("5. Pass\n");

            bool turnComplete = false;
            while (!turnComplete) {
                char prompt[160];
                snprintf(prompt, sizeof(prompt), "%s, choose an action: ", subject->name);
                int choice = prompt_int(prompt, 1, 5, 5);

                if (choice == 1) {
                    if (rebelFlags[subjectIndex]) {
                        printf("Rebels may no longer donate to the current King.\n");
                        continue;
                    }
                    resolve_donation(game, kingIndex, subjectIndex, rebellionActive, rebelLeader);
                    turnComplete = true;
                } else if (choice == 2) {
                    Card drawn;
                    if (draw_one(game, &drawn)) {
                        char cardText[32];
                        push_card(subject->hand, &subject->handCount, drawn);
                        card_to_string(drawn, cardText, sizeof(cardText));
                        printf("%s draws %s.\n", subject->name, cardText);
                    } else {
                        printf("No cards can be drawn right now.\n");
                    }
                    turnComplete = true;
                } else if (choice == 3) {
                    resolve_petition(game, kingIndex, subjectIndex, accusedTyranny);
                    turnComplete = true;
                } else if (choice == 4) {
                    if (count_true(accusedTyranny) < 3) {
                        printf("Rebellion is not yet legal. At least three subjects must publicly accuse tyranny this reign.\n");
                    } else {
                        rebellionActive = true;
                        rebelFlags[subjectIndex] = true;
                        printf("%s declares rebellion against %s!\n", subject->name, king->name);
                        if (rebelLeader < 0) {
                            int leaderCandidates[PLAYER_COUNT];
                            int leaderCount = 0;
                            for (int i = 0; i < PLAYER_COUNT; ++i) {
                                if (i != kingIndex) {
                                    leaderCandidates[leaderCount++] = i;
                                }
                            }

                            if (prompt_yes_no("Name a Rebel Leader now? [y/N] ", false)) {
                                rebelLeader = choose_player_index(game, leaderCandidates, leaderCount,
                                                                  "Choose the Rebel Leader from the subjects:");
                                printf("%s is named Rebel Leader.\n", game->players[rebelLeader].name);
                            }
                        }
                        turnComplete = true;
                    }
                } else {
                    printf("%s passes.\n", subject->name);
                    turnComplete = true;
                }
            }
        }

        if (!taxUsedThisRound) {
            char prompt[160];
            snprintf(prompt, sizeof(prompt), "%s, do you want to levy a tax this round? [y/N] ", king->name);
            if (prompt_yes_no(prompt, false)) {
                resolve_tax(game, kingIndex);
                taxUsedThisRound = true;
            }
        }

        show_public_table_state(game, kingIndex);

        if (count_true(accusedTyranny) >= 3) {
            printf("\nThe court considers %s Tyrannical this reign.\n", king->name);
        }
    }

    score_favor_pile(game, kingIndex, reignScore);
    clear_favor_pile_to_discard(game);

    if (king->handCount == 0) {
        draw_cards(game, king, 5);
        printf("%s draws a new 5-card hand for the next time they are a subject.\n", king->name);
    }
}

static int find_top_scorers(const Game *game, int *tied) {
    int topScore = game->players[0].totalScore;
    for (int i = 1; i < PLAYER_COUNT; ++i) {
        if (game->players[i].totalScore > topScore) {
            topScore = game->players[i].totalScore;
        }
    }

    int count = 0;
    for (int i = 0; i < PLAYER_COUNT; ++i) {
        if (game->players[i].totalScore == topScore) {
            tied[count++] = i;
        }
    }
    return count;
}

static int resolve_tie_by_best_reign(const Game *game, const int *tiedPlayers, int tiedCount, int *best) {
    int bestReign = -1;
    int bestCount = 0;

    for (int i = 0; i < tiedCount; ++i) {
        int playerIndex = tiedPlayers[i];
        int playerBest = highest_single_reign_score(&game->players[playerIndex]);
        if (playerBest > bestReign) {
            bestReign = playerBest;
            bestCount = 0;
            best[bestCount++] = playerIndex;
        } else if (playerBest == bestReign) {
            best[bestCount++] = playerIndex;
        }
    }

    return bestCount;
}

static int run_final_parlay_duel(Game *game, int *tiedPlayers, int tiedCount) {
    printf("\nFinal tie remains. Starting the optional Kingless Parlay duel.\n");
    int duelists[PLAYER_COUNT];
    int duelistCount = tiedCount;
    for (int i = 0; i < tiedCount; ++i) {
        duelists[i] = tiedPlayers[i];
    }

    for (;;) {
        for (int i = 0; i < duelistCount; ++i) {
            Player *player = &game->players[duelists[i]];
            for (int c = 0; c < player->handCount; ++c) {
                discard_card(game, player->hand[c]);
            }
            player->handCount = 0;
            draw_cards(game, player, 5);
            printf("%s draws 5 duel cards.\n", player->name);
        }

        Card chosenCards[PLAYER_COUNT];
        for (int i = 0; i < duelistCount; ++i) {
            Player *player = &game->players[duelists[i]];
            show_private_hand(player);
            char prompt[160];
            snprintf(prompt, sizeof(prompt), "%s, choose one card for the duel: ", player->name);
            int idx = choose_card_index_from_hand(player, prompt);
            chosenCards[i] = take_card_at(player->hand, &player->handCount, idx);
        }

        int bustPoint = ((rand() % 6) + 1) + ((rand() % 6) + 1);
        printf("Bust point for the duel is %d.\n", bustPoint);

        int bestOverIndex = -1;
        int bestOverDistance = INT_MAX;
        int bestUnderIndex = -1;
        int bestUnderValue = INT_MIN;
        bool hasOver = false;
        bool hasUnder = false;

        for (int i = 0; i < duelistCount; ++i) {
            int value = card_value(chosenCards[i]);
            char cardText[32];
            card_to_string(chosenCards[i], cardText, sizeof(cardText));
            printf("%s reveals %s (%d).\n", game->players[duelists[i]].name, cardText, value);

            if (value > bustPoint) {
                hasOver = true;
                int distance = value - bustPoint;
                if (distance < bestOverDistance) {
                    bestOverDistance = distance;
                    bestOverIndex = i;
                } else if (distance == bestOverDistance) {
                    bestOverIndex = -2;
                }
            } else {
                hasUnder = true;
                if (value > bestUnderValue) {
                    bestUnderValue = value;
                    bestUnderIndex = i;
                } else if (value == bestUnderValue) {
                    bestUnderIndex = -2;
                }
            }
        }

        if (hasOver && bestOverIndex >= 0) {
            return duelists[bestOverIndex];
        }
        if (!hasOver && hasUnder && bestUnderIndex >= 0) {
            return duelists[bestUnderIndex];
        }

        printf("The duel is still tied. Repeat.\n");
    }
}

static Game create_game(void) {
    Game game;
    memset(&game, 0, sizeof(game));
    game.config.cycles = 1;
    game.config.reignTurnsPerSubject = 3;
    build_deck(&game);

    printf("Enter the five player names. Press Enter to use the G-IDLE defaults.\n");
    for (int i = 0; i < PLAYER_COUNT; ++i) {
        char prompt[64];
        char name[MAX_NAME_LEN];
        snprintf(prompt, sizeof(prompt), "Player %d name [%s]: ", i + 1, DEFAULT_PLAYER_NAMES[i]);
        printf("%s", prompt);
        read_line(name, sizeof(name));
        if (name[0] == '\0') {
            snprintf(game.players[i].name, sizeof(game.players[i].name), "%s", DEFAULT_PLAYER_NAMES[i]);
        } else {
            snprintf(game.players[i].name, sizeof(game.players[i].name), "%s", name);
        }
    }

    game.config.cycles = prompt_int("How many cycles do you want to play? [1-3, default 1] ", 1, 3, 1);
    game.config.reignTurnsPerSubject =
        prompt_int("How many turns should each subject get per reign? [1-6, default 3] ", 1, 6, 3);

    shuffle_cards(game.drawDeck, game.drawCount);
    for (int i = 0; i < PLAYER_COUNT; ++i) {
        draw_cards(&game, &game.players[i], 5);
    }

    return game;
}

static int choose_initial_king(void) {
    return rand() % PLAYER_COUNT;
}

int main(void) {
    srand((unsigned int)time(NULL));

    Game game = create_game();
    int kingIndex = choose_initial_king();

    printf("\nA random first King has been chosen: %s.\n", game.players[kingIndex].name);
    printf("King order proceeds clockwise from there.\n");

    int reignNumber = 0;
    for (int cycle = 1; cycle <= game.config.cycles; ++cycle) {
        printf("\n==================== Cycle %d ====================\n", cycle);
        for (int reignInCycle = 1; reignInCycle <= PLAYER_COUNT; ++reignInCycle) {
            ++reignNumber;
            play_reign(&game, kingIndex, reignNumber, game.config.reignTurnsPerSubject);
            kingIndex = (kingIndex + 1) % PLAYER_COUNT;
        }
    }

    printf("\n==================== Final Scores ====================\n");
    for (int i = 0; i < PLAYER_COUNT; ++i) {
        printf("%s: total %d, best reign %d\n",
               game.players[i].name,
               game.players[i].totalScore,
               highest_single_reign_score(&game.players[i]));
    }

    int topScorers[PLAYER_COUNT];
    int topCount = find_top_scorers(&game, topScorers);
    if (topCount == 1) {
        printf("\nWinner: %s\n", game.players[topScorers[0]].name);
        return 0;
    }

    int bestReignTies[PLAYER_COUNT];
    int bestCount = resolve_tie_by_best_reign(&game, topScorers, topCount, bestReignTies);
    if (bestCount == 1) {
        printf("\nWinner by best single reign: %s\n", game.players[bestReignTies[0]].name);
        return 0;
    }

    printf("\nThere is still a tie after the single-reign tiebreaker.\n");
    if (prompt_yes_no("Play the optional final Kingless Parlay duel? [y/N] ", false)) {
        int winnerIndex = run_final_parlay_duel(&game, bestReignTies, bestCount);
        printf("\nWinner: %s\n", game.players[winnerIndex].name);
    } else {
        printf("Shared victory between:\n");
        for (int i = 0; i < bestCount; ++i) {
            printf("  %s\n", game.players[bestReignTies[i]].name);
        }
    }

    return 0;
}
