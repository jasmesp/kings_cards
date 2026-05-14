/* resume_token: 019e0c39-d4ae-7313-9c41-5c6121857ca3 */
/*
    aespacheckers.c
    ===============

    A build-it challenge ladder for a tiny checkers engine.

    Style:
    -> each challenge function is intentionally wrong
    -> each function is a real piece of a future checkers game
    -> main calls the checks directly and prints SUCCESS/FAIL
    -> no hidden test framework, no mystical harness
    -> aespa jokes because learning C board logic can feel like fighting Black
       Mamba with a debugger and a lukewarm coffee

    Run:

        cmake -S c_version -B c_version/build
        cmake --build c_version/build
        ./c_version/build/aespacheckers

    Board model
    -----------

    Checkers board:
    -> 8 rows
    -> 8 columns
    -> only dark squares are playable

    Coordinates:
    -> row 0 is the top
    -> row 7 is the bottom
    -> col 0 is the left
    -> col 7 is the right

    Pieces:
    -> Red starts near the top and moves down toward larger row numbers.
    -> White starts near the bottom and moves up toward smaller row numbers.
    -> Kings can move both directions.

    This is not a full UI game yet. It is the engine skeleton: the boring little
    functions that later make "click piece, click target, animate capture" work.
*/

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#define BOARD_SIZE 8

#define SECTION(title) printf("\n=== %s ===\n", title)

typedef enum { EMPTY = 0, RED_MAN, RED_KING, WHITE_MAN, WHITE_KING } Piece;

typedef struct {
  int row;
  int col;
} Square;

typedef struct {
  Piece cells[BOARD_SIZE][BOARD_SIZE];
} Board;

static const char *piece_name(Piece piece) {
  switch (piece) {
  case EMPTY:
    return "EMPTY";
  case RED_MAN:
    return "RED_MAN";
  case RED_KING:
    return "RED_KING";
  case WHITE_MAN:
    return "WHITE_MAN";
  case WHITE_KING:
    return "WHITE_KING";
  }
  return "UNKNOWN";
}

static void print_result(const char *name, bool success) {
  printf("%s: %s\n", name, success ? "SUCCESS" : "FAIL");
}

static void clear_board(Board *board) {
  for (int row = 0; row < BOARD_SIZE; ++row) {
    for (int col = 0; col < BOARD_SIZE; ++col) {
      board->cells[row][col] = EMPTY;
    }
  }
}

static void print_board(const Board *board) {
  printf("    0 1 2 3 4 5 6 7\n");
  printf("   -----------------\n");
  for (int row = 0; row < BOARD_SIZE; ++row) {
    printf("%d | ", row);
    for (int col = 0; col < BOARD_SIZE; ++col) {
      char glyph = '.';
      switch (board->cells[row][col]) {
      case EMPTY:
        glyph = ((row + col) % 2 == 1) ? '_' : '.';
        break;
      case RED_MAN:
        glyph = 'r';
        break;
      case RED_KING:
        glyph = 'R';
        break;
      case WHITE_MAN:
        glyph = 'w';
        break;
      case WHITE_KING:
        glyph = 'W';
        break;
      }
      printf("%c ", glyph);
    }
    printf("|\n");
  }
}

/*
    Challenge 1: board bounds
    -------------------------

    Goal:
    Return true only when row and col are both inside the 8x8 board.

    Correct:
    -> 0 <= row < BOARD_SIZE
    -> 0 <= col < BOARD_SIZE

    Broken:
    -> only checks the upper bound
    -> negative indexes are chaos gremlins wearing a Ningning photocard sleeve
*/
static bool challenge_in_bounds(int row, int col) {
  if (row >= 0 && col >= 0) {
    if (row < BOARD_SIZE && col < BOARD_SIZE) {
      return true;
    } else {
      return false;
    } /* TODO: fix me */
  } else
    return false;
}

static bool challenge_in_bounds_works(void) {
  return challenge_in_bounds(0, 0) && challenge_in_bounds(7, 7) &&
         !challenge_in_bounds(-1, 0) && !challenge_in_bounds(0, -1) &&
         !challenge_in_bounds(8, 0) && !challenge_in_bounds(0, 8);
}

/*
    Challenge 2: dark playable squares
    ----------------------------------

    Goal:
    Return true for playable dark squares.

    Convention for this workbook:
    -> dark square when (row + col) is odd

    Broken:
    -> uses even squares instead
*/
static bool challenge_is_dark_square(int row, int col) {
  return (row + col) % 2 == 0; /* TODO: fix me */
}

static bool challenge_is_dark_square_works(void) {
  return challenge_is_dark_square(0, 1) && challenge_is_dark_square(1, 0) &&
         !challenge_is_dark_square(0, 0) && !challenge_is_dark_square(7, 7);
}

/*
    Challenge 3: identify piece color
    ---------------------------------

    Goal:
    Return true for red pieces: RED_MAN or RED_KING.

    Broken:
    -> only recognizes RED_MAN
*/
static bool challenge_is_red(Piece piece) {
  return piece == RED_MAN; /* TODO: fix me */
}

static bool challenge_is_red_works(void) {
  return challenge_is_red(RED_MAN) && challenge_is_red(RED_KING) &&
         !challenge_is_red(WHITE_MAN) && !challenge_is_red(WHITE_KING) &&
         !challenge_is_red(EMPTY);
}

/*
    Challenge 4: identify opposing colors
    -------------------------------------

    Goal:
    Return true when a and b are non-empty pieces owned by opposite players.

    Broken:
    -> says any different piece is an enemy, including EMPTY vs RED_MAN
*/
static bool challenge_are_enemies(Piece a, Piece b) {
  return a != b; /* TODO: fix me */
}

static bool challenge_are_enemies_works(void) {
  return challenge_are_enemies(RED_MAN, WHITE_MAN) &&
         challenge_are_enemies(RED_KING, WHITE_KING) &&
         !challenge_are_enemies(RED_MAN, RED_KING) &&
         !challenge_are_enemies(WHITE_MAN, WHITE_KING) &&
         !challenge_are_enemies(EMPTY, RED_MAN) &&
         !challenge_are_enemies(EMPTY, EMPTY);
}

/*
    Challenge 5: starting board setup
    ---------------------------------

    Goal:
    Fill the standard starting positions.

    Rules for this workbook:
    -> red men on dark squares in rows 0, 1, 2
    -> white men on dark squares in rows 5, 6, 7
    -> all other squares empty

    Broken:
    -> clears the board but places nothing
*/
static void challenge_setup_starting_board(Board *board) {
  clear_board(board);
  /* TODO: place red and white starting pieces */
}

static bool challenge_setup_starting_board_works(void) {
  Board board;
  challenge_setup_starting_board(&board);

  int redCount = 0;
  int whiteCount = 0;
  int badCount = 0;

  for (int row = 0; row < BOARD_SIZE; ++row) {
    for (int col = 0; col < BOARD_SIZE; ++col) {
      Piece piece = board.cells[row][col];
      if (piece == RED_MAN) {
        ++redCount;
        if (!(row <= 2 && (row + col) % 2 == 1)) {
          ++badCount;
        }
      } else if (piece == WHITE_MAN) {
        ++whiteCount;
        if (!(row >= 5 && (row + col) % 2 == 1)) {
          ++badCount;
        }
      } else if (piece != EMPTY) {
        ++badCount;
      }
    }
  }

  return redCount == 12 && whiteCount == 12 && badCount == 0;
}

/*
    Challenge 6: movement direction for men
    ---------------------------------------

    Goal:
    Return the forward row delta for a non-king piece.

    Red moves down:
    -> +1

    White moves up:
    -> -1

    Kings are not directional here; return 0 for kings/empty.

    Broken:
    -> red and white are flipped, which is giving "choreography mirrored in the
       practice room but nobody told the camera operator"
*/
static int challenge_forward_delta(Piece piece) {
  if (piece == RED_MAN) {
    return -1; /* TODO: fix me */
  }
  if (piece == WHITE_MAN) {
    return 1; /* TODO: fix me */
  }
  return 0;
}

static bool challenge_forward_delta_works(void) {
  return challenge_forward_delta(RED_MAN) == 1 &&
         challenge_forward_delta(WHITE_MAN) == -1 &&
         challenge_forward_delta(RED_KING) == 0 &&
         challenge_forward_delta(EMPTY) == 0;
}

/*
    Challenge 7: simple diagonal move
    ---------------------------------

    Goal:
    Return true when from -> to is a one-step diagonal move.

    Correct:
    -> absolute row delta is 1
    -> absolute col delta is 1

    This ignores color/direction. That comes later.

    Broken:
    -> allows straight vertical moves
*/
static bool challenge_is_one_step_diagonal(Square from, Square to) {
  int rowDelta = to.row - from.row;
  int colDelta = to.col - from.col;
  return (rowDelta == 1 || rowDelta == -1) && colDelta == 0; /* TODO: fix me */
}

static bool challenge_is_one_step_diagonal_works(void) {
  return challenge_is_one_step_diagonal((Square){2, 3}, (Square){3, 4}) &&
         challenge_is_one_step_diagonal((Square){2, 3}, (Square){1, 2}) &&
         !challenge_is_one_step_diagonal((Square){2, 3}, (Square){3, 3}) &&
         !challenge_is_one_step_diagonal((Square){2, 3}, (Square){4, 5});
}

/*
    Challenge 8: legal simple move for a man
    ----------------------------------------

    Goal:
    Return true if a non-capturing move is legal for the piece.

    Conditions:
    -> from and to are in bounds
    -> to square is dark
    -> to square is empty
    -> move is one-step diagonal
    -> men move forward only
    -> kings can move one-step diagonal either direction

    Broken:
    -> checks diagonal and empty, but ignores direction and playable squares
*/
static bool challenge_can_simple_move(const Board *board, Square from,
                                      Square to) {
  Piece piece = board->cells[from.row][from.col];
  if (piece == EMPTY) {
    return false;
  }
  if (board->cells[to.row][to.col] != EMPTY) {
    return false;
  }
  return challenge_is_one_step_diagonal(from, to); /* TODO: fix me */
}

static bool challenge_can_simple_move_works(void) {
  Board board;
  clear_board(&board);
  board.cells[2][3] = RED_MAN;
  board.cells[5][4] = WHITE_MAN;
  board.cells[4][1] = RED_KING;

  return challenge_can_simple_move(&board, (Square){2, 3}, (Square){3, 4}) &&
         !challenge_can_simple_move(&board, (Square){2, 3}, (Square){1, 4}) &&
         challenge_can_simple_move(&board, (Square){5, 4}, (Square){4, 3}) &&
         !challenge_can_simple_move(&board, (Square){5, 4}, (Square){6, 3}) &&
         challenge_can_simple_move(&board, (Square){4, 1}, (Square){3, 0}) &&
         challenge_can_simple_move(&board, (Square){4, 1}, (Square){5, 2});
}

/*
    Challenge 9: midpoint of a capture
    ----------------------------------

    Goal:
    For a two-step diagonal jump, return the jumped square.

    Example:
    -> from (2, 3) to (4, 5)
    -> jumped square is (3, 4)

    Broken:
    -> returns destination
*/
static Square challenge_capture_midpoint(Square from, Square to) {
  (void)from;
  return to; /* TODO: fix me */
}

static bool challenge_capture_midpoint_works(void) {
  Square midA = challenge_capture_midpoint((Square){2, 3}, (Square){4, 5});
  Square midB = challenge_capture_midpoint((Square){5, 6}, (Square){3, 4});

  return midA.row == 3 && midA.col == 4 && midB.row == 4 && midB.col == 5;
}

/*
    Challenge 10: legal capture
    ---------------------------

    Goal:
    Return true when a capture is legal.

    Conditions:
    -> from and to in bounds
    -> to is dark and empty
    -> from has a piece
    -> move is exactly two rows and two cols diagonally
    -> midpoint contains an enemy
    -> men capture forward only
    -> kings capture either direction

    Broken:
    -> only checks the two-step shape
*/
static bool challenge_can_capture(const Board *board, Square from, Square to) {
  (void)board;
  int rowDelta = to.row - from.row;
  int colDelta = to.col - from.col;
  return (rowDelta == 2 || rowDelta == -2) &&
         (colDelta == 2 || colDelta == -2); /* TODO: fix me */
}

static bool challenge_can_capture_works(void) {
  Board board;
  clear_board(&board);
  board.cells[2][3] = RED_MAN;
  board.cells[3][4] = WHITE_MAN;
  board.cells[5][6] = WHITE_MAN;
  board.cells[4][5] = RED_MAN;
  board.cells[5][0] = RED_KING;
  board.cells[4][1] = WHITE_MAN;

  return challenge_can_capture(&board, (Square){2, 3}, (Square){4, 5}) &&
         !challenge_can_capture(&board, (Square){2, 3}, (Square){0, 1}) &&
         challenge_can_capture(&board, (Square){5, 6}, (Square){3, 4}) &&
         challenge_can_capture(&board, (Square){5, 0}, (Square){3, 2}) &&
         !challenge_can_capture(&board, (Square){2, 3}, (Square){4, 1});
}

/*
    Challenge 11: apply simple move
    -------------------------------

    Goal:
    Move the piece from source to destination.

    Correct:
    -> copy piece to destination
    -> clear source

    Broken:
    -> copies piece but forgets to clear source, creating a backup dancer clone
*/
static void challenge_apply_simple_move(Board *board, Square from, Square to) {
  board->cells[to.row][to.col] = board->cells[from.row][from.col];
  /* TODO: clear source */
}

static bool challenge_apply_simple_move_works(void) {
  Board board;
  clear_board(&board);
  board.cells[2][3] = RED_MAN;

  challenge_apply_simple_move(&board, (Square){2, 3}, (Square){3, 4});

  return board.cells[2][3] == EMPTY && board.cells[3][4] == RED_MAN;
}

/*
    Challenge 12: apply capture
    ---------------------------

    Goal:
    Move capturing piece, clear source, and remove jumped enemy.

    Broken:
    -> moves and clears source, but does not remove captured piece
*/
static void challenge_apply_capture(Board *board, Square from, Square to) {
  Square mid = challenge_capture_midpoint(from, to);
  (void)mid;
  board->cells[to.row][to.col] = board->cells[from.row][from.col];
  board->cells[from.row][from.col] = EMPTY;
  /* TODO: clear midpoint */
}

static bool challenge_apply_capture_works(void) {
  Board board;
  clear_board(&board);
  board.cells[2][3] = RED_MAN;
  board.cells[3][4] = WHITE_MAN;

  challenge_apply_capture(&board, (Square){2, 3}, (Square){4, 5});

  return board.cells[2][3] == EMPTY && board.cells[3][4] == EMPTY &&
         board.cells[4][5] == RED_MAN;
}

/*
    Challenge 13: promotion
    -----------------------

    Goal:
    Promote men that reach the far side.

    Red promotes on row 7.
    White promotes on row 0.
    Kings stay kings.

    Broken:
    -> never promotes
*/
static Piece challenge_promote_if_needed(Piece piece, int row) {
  (void)row;
  return piece; /* TODO: fix me */
}

static bool challenge_promote_if_needed_works(void) {
  return challenge_promote_if_needed(RED_MAN, 7) == RED_KING &&
         challenge_promote_if_needed(WHITE_MAN, 0) == WHITE_KING &&
         challenge_promote_if_needed(RED_MAN, 6) == RED_MAN &&
         challenge_promote_if_needed(WHITE_KING, 0) == WHITE_KING;
}

/*
    Challenge 14: count pieces
    --------------------------

    Goal:
    Count pieces matching a requested side.

    sideIsRed true:
    -> count RED_MAN and RED_KING

    sideIsRed false:
    -> count WHITE_MAN and WHITE_KING

    Broken:
    -> counts only men, not kings
*/
static int challenge_count_side_pieces(const Board *board, bool sideIsRed) {
  int count = 0;
  for (int row = 0; row < BOARD_SIZE; ++row) {
    for (int col = 0; col < BOARD_SIZE; ++col) {
      Piece piece = board->cells[row][col];
      if (sideIsRed && piece == RED_MAN) {
        ++count;
      }
      if (!sideIsRed && piece == WHITE_MAN) {
        ++count;
      }
    }
  }
  return count; /* TODO: include kings */
}

static bool challenge_count_side_pieces_works(void) {
  Board board;
  clear_board(&board);
  board.cells[0][1] = RED_MAN;
  board.cells[2][3] = RED_KING;
  board.cells[5][4] = WHITE_MAN;
  board.cells[7][6] = WHITE_KING;

  return challenge_count_side_pieces(&board, true) == 2 &&
         challenge_count_side_pieces(&board, false) == 2;
}

/*
    Challenge 15: winner detection
    ------------------------------

    Goal:
    Return the winning side when one side has no pieces.

    Return:
    -> RED_MAN if red has pieces and white has none
    -> WHITE_MAN if white has pieces and red has none
    -> EMPTY if no winner yet

    Broken:
    -> always says no winner
*/
static Piece challenge_winner(const Board *board) {
  (void)board;
  return EMPTY; /* TODO: fix me */
}

static bool challenge_winner_works(void) {
  Board redWins;
  Board whiteWins;
  Board noWinner;
  clear_board(&redWins);
  clear_board(&whiteWins);
  clear_board(&noWinner);

  redWins.cells[0][1] = RED_MAN;
  whiteWins.cells[7][0] = WHITE_MAN;
  noWinner.cells[0][1] = RED_MAN;
  noWinner.cells[7][0] = WHITE_MAN;

  return challenge_winner(&redWins) == RED_MAN &&
         challenge_winner(&whiteWins) == WHITE_MAN &&
         challenge_winner(&noWinner) == EMPTY;
}

static void run_challenges(void) {
  SECTION("aespa checkers challenge ladder");

  print_result("challenge 01 - board bounds", challenge_in_bounds_works());
  print_result("challenge 02 - dark playable squares",
               challenge_is_dark_square_works());
  print_result("challenge 03 - red piece detection", challenge_is_red_works());
  print_result("challenge 04 - enemy detection", challenge_are_enemies_works());
  print_result("challenge 05 - starting board setup",
               challenge_setup_starting_board_works());
  print_result("challenge 06 - forward delta", challenge_forward_delta_works());
  print_result("challenge 07 - one-step diagonal",
               challenge_is_one_step_diagonal_works());
  print_result("challenge 08 - legal simple move",
               challenge_can_simple_move_works());
  print_result("challenge 09 - capture midpoint",
               challenge_capture_midpoint_works());
  print_result("challenge 10 - legal capture", challenge_can_capture_works());
  print_result("challenge 11 - apply simple move",
               challenge_apply_simple_move_works());
  print_result("challenge 12 - apply capture", challenge_apply_capture_works());
  print_result("challenge 13 - promotion", challenge_promote_if_needed_works());
  print_result("challenge 14 - count side pieces",
               challenge_count_side_pieces_works());
  print_result("challenge 15 - winner detection", challenge_winner_works());
}

int main(void) {
  SECTION("aespa checkers");
  printf("A step-by-step C challenge ladder toward a tiny checkers engine.\n");
  printf("If it prints FAIL, that function has not escaped Kwangya yet.\n");

  Board preview;
  clear_board(&preview);
  preview.cells[2][3] = RED_MAN;
  preview.cells[3][4] = WHITE_MAN;
  preview.cells[5][0] = RED_KING;
  printf("\nTiny preview board, not the solved starting board:\n");
  print_board(&preview);

  printf("\nPiece name sanity check: %s, %s, %s\n", piece_name(RED_MAN),
         piece_name(WHITE_KING), piece_name(EMPTY));

  run_challenges();

  SECTION("done");
  printf("Next step: fix challenge 01, rebuild, rerun. One comeback stage at a "
         "time.\n");
  return 0;
}
