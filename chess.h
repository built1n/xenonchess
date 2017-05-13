#include <unistd.h>
#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define COORD_END 0xdeadbeef
#define ARRAYLEN(x) (sizeof(x)/sizeof((x)[0]))
#define ABS(x) ((x)<0:-(x):(x))

enum player { NONE = 0, WHITE, BLACK };
enum piece { EMPTY = 0, PAWN, ROOK, KNIGHT, BISHOP, QUEEN, KING };

struct piece_t {
    enum piece type;
    enum player color;
};

struct coordinates {
    int y, x; /* 0-indexed */
};

struct promotion_t {
    struct coordinates to, from;
    enum piece type;
};

struct move_t {
    enum player color;
    enum { NORMAL, CASTLE, PROMOTION, NOMOVE } type;
    union {
        struct { struct coordinates to, from; } normal;
        enum { QUEENSIDE, KINGSIDE } castle_style;
        struct promotion_t promotion;
    } data;
};

struct chess_ctx {
    struct piece_t board[8][8]; /* [rank (y)],[file (x)] */
    enum player to_move;
    bool king_moved[2];
    bool rook_moved[2][2]; /* [player][0=first file,1=eighth file] */
};

int eval_position(const struct chess_ctx *ctx, int color, int depth);
struct move_t best_move(const struct chess_ctx *ctx, int *score, int depth);
void execute_move(struct chess_ctx *ctx, struct move_t move);
void gen_and_call(const struct chess_ctx *ctx,
                  int y, int x,
                  int dy, int dx,
                  void (*cb)(void *data, const struct chess_ctx*, struct move_t),
                  void *data, bool enforce, bool in_check);
void for_each_move(const struct chess_ctx *ctx,
                   int y, int x,
                   void (*cb)(void *data, const struct chess_ctx*, struct move_t),
                   void *data, bool enforce_check);
struct move_t worst_move(const struct chess_ctx *ctx, int *score, int depth);
bool king_in_check(const struct chess_ctx *ctx, int color);
void print_ctx(const struct chess_ctx *ctx);
