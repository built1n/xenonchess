#include "chess.h"

#define DEPTH 2

//#define AUTOMATCH
#define UCI

int count_material(const struct chess_ctx *ctx, int color)
{
    int total = 0;
    static const int values[] = { 0,
                                  1,
                                  5,
                                  3,
                                  3,
                                  12, /* extra */
                                  5 };
    for(int y = 0; y < 8; ++y)
    {
        for(int x = 0; x < 8; ++x)
        {
            if(ctx->board[y][x].color == color)
                total += values[ctx->board[y][x].type];

            /* pawn near promotion */
            if(ctx->board[y][x].type == PAWN)
            {
                if((y >= 6 && ctx->board[y][x].color == WHITE) ||
                   (y <= 1 && ctx->board[y][x].color == BLACK))
                    total += 4;
            }
        }
    }
    //printf("color %d has %d material\n", color, total);
    return total;
}

bool count_space_cb(void *data, const struct chess_ctx *ctx, struct move_t move)
{
    int *count = data;
    (*count)++;
    if(move.type == NORMAL)
    {
        int x = move.data.normal.to.x, y = move.data.normal.to.y;
        if(ctx->board[y][x].color != NONE) /* threatens an enemy piece */
            (*count)++;
        if((x == 3 || x == 4) && (y == 3 || y == 4)) /* controls center */
            (*count)++;
    }
    return true;
}

/* essentially returns the total of the number of squares each piece
 * can move to */
int count_space(const struct chess_ctx *ctx, int color)
{
    int space = 0;
    for(int y = 0; y < 8; ++y)
    {
        for(int x = 0; x < 8; ++x)
        {
            if(ctx->board[y][x].color == color)
            {
                for_each_move(ctx, y, x, count_space_cb, &space, true);
            }
        }
    }
    //printf("color %d has %d space\n", color, space);
    return space;
}

#define inv_player(p) ((p)==WHITE?BLACK:WHITE)

bool king_in_checkmate(const struct chess_ctx *ctx, int color)
{
    struct coordinates king;
    if(king_in_check(ctx, color, &king))
    {
        /* check that there are no legal moves */
        return count_space(ctx, color) == 0;
    }
    return false;
}

int eval_position(const struct chess_ctx *ctx, int color)
{
    int score = 0;

    score += count_material(ctx, color) * 3;
    score -= count_material(ctx, inv_player(color)) * 2;
    score += count_space(ctx, color);
    score -= count_space(ctx, inv_player(color));

    if(king_in_check(ctx, color, NULL))
    {
        score -= 5;
        if(king_in_checkmate(ctx, color))
            score -= 2000;
    }
    else if(king_in_check(ctx, inv_player(color), NULL))
    {
        score += 5;
        if(king_in_checkmate(ctx, inv_player(color)))
            score += 2000;
    }

    return score;
}

#define valid_coords(y, x) ((0 <= y && y <= 7) && (0 <= x && x <= 7))

const struct coordinates king_moves[] = {
    { 0, 1 },
    { 1, 1 },
    { 1, 0 },
    { 1, -1 },
    { 0, -1 },
    { -1, -1 },
    { -1, 0 },
    { -1, 1 },
    { COORD_END, COORD_END },
};

const struct coordinates rook_directions[] = {
    { 0, 1 },
    { 1, 0 },
    { 0, -1 },
    { -1, 0 },
    { COORD_END, COORD_END },
};

const struct coordinates queen_directions[] = {
    { 0, 1 },
    { 1, 1 },
    { 1, 0 },
    { 1, -1 },
    { 0, -1 },
    { -1, -1 },
    { -1, 0 },
    { -1, 1 },
    { COORD_END, COORD_END },
};

const struct coordinates bishop_directions[] = {
    { 1, 1 },
    { 1, -1 },
    { -1, -1 },
    { -1, 1 },
    { COORD_END, COORD_END },
};

const struct coordinates knight_moves[] = {
    { 1, 2 },
    { 2, 1 },
    { 2, -1 },
    { 1, -2 },
    { -1, 2 },
    { -2, 1 },
    { -2, -1 },
    { -1, -2 },
    { COORD_END, COORD_END }
};

const struct coordinates *line_moves[] = {
    NULL,
    NULL,
    rook_directions,
    NULL,
    bishop_directions,
    queen_directions,
    NULL
};

const struct coordinates *jump_moves[] = {
    NULL,
    NULL,
    NULL,
    knight_moves,
    NULL,
    NULL,
    king_moves,
};

#define friendly_occupied(ctx, y, x, c) ((ctx)->board[(y)][(x)].color == NONE ? false : ((ctx)->board[(y)][(x)].color == (c)))

#if 0
bool friendly_occupied(const struct chess_ctx *ctx, int y, int x, int color)
{
    if(!valid_coords(y, x))
        return false;
    const struct piece_t *piece = &ctx->board[y][x];
    if(piece->type == EMPTY)
        return false;
    switch(piece->color)
    {
    case NONE:
        return false;
    case WHITE:
        return color == WHITE;
    case BLACK:
        return color == BLACK;
    default:
        assert(false);
    }
}
#endif

#define enemy_occupied(ctx, y, x, c) ((ctx)->board[(y)][(x)].color == NONE ? false : ((ctx)->board[(y)][(x)].color == inv_player(c)))

#if 0
bool enemy_occupied(const struct chess_ctx *ctx, int y, int x, int color)
{
    if(!valid_coords(y, x))
        return false;
    const struct piece_t *piece = &ctx->board[y][x];
    if(piece->type == EMPTY)
        return false;
    switch(piece->color)
    {
    case NONE:
        return false;
    case WHITE:
        return color == BLACK;
    case BLACK:
        return color == WHITE;
    default:
        assert(false);
    }
}
#endif

#define occupied(ctx, y, x) ((ctx)->board[(y)][(x)].color != NONE)

struct check_info {
    int color;
    bool checked;
    struct coordinates king;
};

bool detect_check_cb(void *data, const struct chess_ctx *ctx, struct move_t move)
{
    struct check_info *info = data;
    if(move.type == NORMAL)
    {
        int x = move.data.normal.to.x,
            y = move.data.normal.to.y;
        if(ctx->board[y][x].type == KING && ctx->board[y][x].color == info->color)
        {
            info->king.y = y;
            info->king.x = x;
            info->checked = true;
            return false;
        }
    }
    return true;
}

bool king_in_check(const struct chess_ctx *ctx, int color, struct coordinates *king)
{
    struct check_info info;
    info.color = color;
    info.checked = false;
    for(int y = 0; y < 8; ++y)
    {
        for(int x = 0; x < 8; ++x)
        {
            /* check enemy pieces */
            if(ctx->board[y][x].color == inv_player(color))
            {
                for_each_move(ctx, y, x, detect_check_cb, &info, false);
                if(info.checked)
                    goto early;
            }
        }
    }
early:
    if(info.checked)
    {
        if(king)
            *king = info.king;
    }
    return info.checked;
}

struct threatened_info {
    bool threatened;
    int y, x;
};

bool threatened_cb(void *data, const struct chess_ctx *ctx, struct move_t move)
{
    struct threatened_info *info = data;
    if(move.type == NORMAL)
    {
        int x = move.data.normal.to.x,
            y = move.data.normal.to.y;
        if(y == info->y && x == info->x)
        {
            info->threatened = true;
            return false;
        }
    }
    return true;
}

/* color is friendly */
/* checks whether enemy threatens a square */
bool square_threatened(const struct chess_ctx *ctx, int ty, int tx, int color)
{
    struct threatened_info info;
    info.threatened = false;
    info.y = ty;
    info.x = tx;

    for(int y = 0; y < 8; ++y)
    {
        for(int x = 0; x < 8; ++x)
        {
            /* check enemy pieces */
            if(ctx->board[y][x].color == inv_player(color))
            {
                for_each_move(ctx, y, x, threatened_cb, &info, false);
                if(info.threatened)
                    return true;
            }
        }
    }
    return false;
}

inline struct move_t construct_move(int color, int y, int x, int dy, int dx)
{
    struct move_t ret;
    ret.color = color;
    ret.type = NORMAL;
    ret.data.normal.from = (struct coordinates) { y, x };
    ret.data.normal.to = (struct coordinates) { y + dy, x + dx };
    return ret;
}

inline bool gen_and_call(const struct chess_ctx *ctx,
                         int y, int x,
                         int dy, int dx,
                         bool (*cb)(void *data, const struct chess_ctx*, struct move_t),
                         void *data, bool enforce_check)
{
    struct move_t move = construct_move(ctx->board[y][x].color,
                                        y, x,
                                        dy, dx);

    bool promotion = false;
    if(ctx->board[y][x].type == PAWN &&
       (y + dy == 0 || y + dy == 7))
        promotion = true;

    if(ctx->board[y][x].type == KING && ABS(dx) == 2) /* castle, must be before the next if() */
    {
        move.type = CASTLE;
        move.data.castle_style = dx > 0 ? KINGSIDE : QUEENSIDE;
    }

    if(enforce_check)
    {
        struct chess_ctx local = *ctx;
        execute_move(&local, move);
        bool check_after = king_in_check(&local, ctx->board[y][x].color, NULL);

        /* move puts player in check */
        if(check_after)
            return true;
    }

    if(promotion)
    {
        move.type = PROMOTION;
        move.data.promotion.from = (struct coordinates) { y, x  };
        move.data.promotion.to = (struct coordinates) { y + dy, x + dx };

        /* try all possible pieces */
        enum piece promote_pieces[] = { ROOK, KNIGHT, BISHOP, QUEEN };
        for(int i = 0; i < ARRAYLEN(promote_pieces); ++i)
        {
            move.data.promotion.type = promote_pieces[i];
            if(!cb(data, ctx, move))
                return false;
        }
        return true;
    }
    else
    {
        return cb(data, ctx, move);
    }
}

void for_each_move(const struct chess_ctx *ctx,
                   int y, int x,
                   bool (*cb)(void *data, const struct chess_ctx*, struct move_t),
                   void *data, bool enforce_check)
{
    assert(valid_coords(y, x));

    const struct piece_t *piece = &ctx->board[y][x];

    switch(piece->type)
    {
    case EMPTY:
        return;
    case PAWN:
    {
        /* special case */
        int dy = (piece->color == WHITE ? 1 : -1);

        if(!valid_coords(y + dy, x))
            break;

        /* 2 squares on first move */
        if((ctx->board[y][x].color == WHITE && y == 1) ||
           (ctx->board[y][x].color == BLACK && y == 6))
            if(!occupied(ctx, y + 2 * dy, x) && !occupied(ctx, y + dy, x))
                if(!gen_and_call(ctx, y, x, 2 * dy, 0, cb, data, enforce_check))
                    return;

        for(int dx = -1; dx <= 1; dx += 2)
        {
            if(enemy_occupied(ctx, y + dy, x + dx, piece->color))
            {
                if(!gen_and_call(ctx, y, x, dy, dx, cb, data, enforce_check))
                    return;
            }
        }

        if(!occupied(ctx, y + dy, x))
            if(!gen_and_call(ctx, y, x, dy, 0, cb, data, enforce_check))
                return;
        break;
    }
    case ROOK:
    case BISHOP:
    case QUEEN:
    {
        const struct coordinates *dir = line_moves[piece->type];
        while(dir->x != COORD_END)
        {
            bool clear = true;
            for(int i = 1; i < 8 && clear; ++i)
            {
                struct coordinates d = { i * dir->y, i * dir->x };
                clear = valid_coords(y + d.y, x + d.x);
                if(!clear)
                    break;
                if(!occupied(ctx, y + d.y, x + d.x))
                {
                    if(!gen_and_call(ctx, y, x, d.y, d.x, cb, data, enforce_check))
                        return;
                }
                else
                {
                    /* occupied square */
                    if(enemy_occupied(ctx, y + d.y, x + d.x, piece->color))
                        if(!gen_and_call(ctx, y, x, d.y, d.x, cb, data, enforce_check))
                            return;
                    clear = false;
                }
            }
            ++dir;
        }
        break;
    }
    case KNIGHT:
    case KING:
    {
        const struct coordinates *moves = jump_moves[piece->type];

        while(moves->x != COORD_END)
        {
            struct coordinates d = *moves;
            if(valid_coords(y + d.y, x + d.x))
            {
                if(!occupied(ctx, y + d.y, x + d.x))
                {
                    if(!gen_and_call(ctx, y, x, d.y, d.x, cb, data, enforce_check))
                        return;
                }
                else
                {
                    /* occupied square */
                    if(enemy_occupied(ctx, y + d.y, x + d.x, piece->color))
                        if(!gen_and_call(ctx, y, x, d.y, d.x, cb, data, enforce_check))
                            return;
                }
            }
            ++moves;
        }

#if 0
        /* castling */
        if(piece->type == KING)
        {
            /* white = 0, black = 1 */
            int idx = piece->color == WHITE ? 0 : 1;

            if(!ctx->king_moved[idx])
            {
                //if(!king_in_check(ctx, piece->color, NULL))
                {
                    /* handle both queenside and kingside */
                    if(!ctx->rook_moved[idx][0]) /* queenside */
                    {
                        bool clear = true;
                        for(int i = 1; i <= 3; ++i)
                            if(ctx->board[y][i].color != NONE)
                                clear = false;
                        if(clear)
                        {
                            /* check that the two squares the king
                               must pass through aren't threatened */
                            if(!square_threatened(ctx, y, 3, piece->color))
                            {
                                if(!gen_and_call(ctx, y, x, 0, -2, cb, data, enforce_check))
                                    return;
                            }
                        }
                    }
                    if(!ctx->rook_moved[idx][1]) /* kingside */
                    {
                        bool clear = true;
                        for(int i = 5; i <= 6; ++i)
                            if(ctx->board[y][i].color != NONE)
                                clear = false;
                        if(clear)
                        {
                            /* check that the square the king must
                             * pass through isn't threatened */
                            if(!square_threatened(ctx, y, 5, piece->color))
                            {
                                if(!gen_and_call(ctx, y, x, 0, 2, cb, data, enforce_check))
                                    return;
                            }
                        }
                    }
                }
            }
        }
#endif
        break;
    }
    default:
        assert(false);
    }
}

struct best_data {
    int highest_score;
    int depth;
    struct move_t best_found;
};

struct worst_data {
    int lowest_score;
    int depth;
    struct move_t worst_found;
};

void print_ctx(const struct chess_ctx *ctx)
{
    for(int y = 7; y >= 0; --y)
    {
        printf("  +----+----+----+----+----+----+----+----+\n%d ", y + 1);
        for(int x = 0; x < 8; ++x)
        {
            char c = " PRNBQK"[ctx->board[y][x].type];
            if(ctx->board[y][x].color == BLACK)
                printf("| *%c ", c);
            else
                printf("|  %c ", c);
        }
        printf("|\n");
    }
    printf("  +----+----+----+----+----+----+----+----+\n    ");
    for(int i = 0; i < 8; ++i)
    {
        printf("%c    ", 'A' + i);
    }
    printf("\n");
}

const char *piece_name(enum piece type)
{
    static const char *names[] = {
        "no piece",
        "pawn",
        "rook",
        "knight",
        "bishop",
        "queen",
        "king"
    };
    return names[type];
}

void print_move(const struct chess_ctx *ctx, struct move_t move)
{
    switch(move.type)
    {
    case NOMOVE:
#ifndef UCI
        printf("No move.\n");
#endif
        return;
    case NORMAL:
    {
        /* TODO: sanity checks */
        //printf("move from %d, %d to %d, %d",
        //       move.data.normal.from.x, move.data.normal.from.y,
        //       move.data.normal.to.x, move.data.normal.to.y);
        const struct piece_t *to, *from;
        to = &ctx->board[move.data.normal.to.y][move.data.normal.to.x];
        from = &ctx->board[move.data.normal.from.y][move.data.normal.from.x];

        char name[3];
        name[0] = 'a' + move.data.normal.to.x;
        name[1] = '1' + move.data.normal.to.y;
        name[2] = '\0';

#ifdef UCI
        char fromname[3];
        fromname[0] = 'a' + move.data.normal.from.x;
        fromname[1] = '1' + move.data.normal.from.y;
        fromname[2] = '\0';
        printf("bestmove %s%s\n", fromname, name);
#else
        if(to->type != EMPTY)
        {
            printf("%s takes %s at %s\n", piece_name(from->type), piece_name(to->type), name);
        }
        else
        {
            printf("%s to %s\n", piece_name(from->type), name);
        }
#endif
        break;
    }
    case PROMOTION:
    {
#ifdef UCI
        char name[3];
        name[0] = 'a' + move.data.promotion.to.x;
        name[1] = '1' + move.data.promotion.to.y;
        name[2] = '\0';

        char fromname[3];
        fromname[0] = 'a' + move.data.promotion.from.x;
        fromname[1] = '1' + move.data.promotion.from.y;
        fromname[2] = '\0';
#else
        printf("pawn promoted\n");
#endif
        break;
    }
    case CASTLE:
    {
#ifdef UCI
        printf("%s\n", move.data.castle_style == KINGSIDE ? "O-O" : "O-O-O");
#else
        printf("castles %s\n", move.data.castle_style == KINGSIDE ? "kingside" : "queenside");
#endif
        break;
    }
    default:
        assert(false);
    }

}

void execute_move(struct chess_ctx *ctx, struct move_t move)
{
    switch(move.type)
    {
    case NORMAL:
    {
        /* TODO: sanity checks */
        //printf("move from %d, %d to %d, %d",
        //       move.data.normal.from.x, move.data.normal.from.y,
        //       move.data.normal.to.x, move.data.normal.to.y);
        struct piece_t *to, *from;
        to = &ctx->board[move.data.normal.to.y][move.data.normal.to.x];
        from = &ctx->board[move.data.normal.from.y][move.data.normal.from.x];

        if(from->type == KING)
        {
            ctx->king_moved[move.color == WHITE ? 0 : 1] = true;
        }
        if(from->type == ROOK && (move.data.normal.from.x == 0 || move.data.normal.from.x == 7))
        {
            ctx->rook_moved[move.color == WHITE ? 0 : 1][move.data.normal.from.x == 0 ? 0 : 1] = true;
        }

        *to = *from;
        *from = (struct piece_t){ EMPTY, NONE };
        break;
    }
    case PROMOTION:
    {
        struct piece_t *to, *from;
        to = &ctx->board[move.data.promotion.to.y][move.data.promotion.to.x];
        from = &ctx->board[move.data.promotion.from.y][move.data.promotion.from.x];
        *from = (struct piece_t) { EMPTY, NONE };
        *to = (struct piece_t) { move.data.promotion.type, move.color };
        break;
    }
    case CASTLE:
    {
        int y = move.color == BLACK ? 7 : 0;
        int dx = move.data.castle_style == KINGSIDE ? 2 : -2;
        ctx->board[y][4].type = EMPTY;
        ctx->board[y][4].color = NONE;
        ctx->board[y][4+dx].type = KING;
        ctx->board[y][4+dx].color = move.color;
        int rx = move.data.castle_style == QUEENSIDE ? 0 : 7;
        ctx->board[y][rx].type = EMPTY;
        ctx->board[y][rx].color = NONE;
        ctx->board[y][4+dx/2].type = ROOK;
        ctx->board[y][4+dx/2].color = move.color;
        break;
    }
    case NOMOVE:
        return;
    default:
        assert(false);
    }
    ctx->to_move = inv_player(ctx->to_move);
    //print_ctx(ctx);
}

struct legal_data {
    struct move_t move;
    bool legal;
};

bool legal_cb(void *data, const struct chess_ctx *ctx, struct move_t move)
{
    struct legal_data *info = data;
    switch(info->move.type)
    {
    case PROMOTION:
    {
        info->legal = true;
        return false;
    }
    case NORMAL:
    {
        struct coordinates *to = &move.data.normal.to;
        struct coordinates *comp = &info->move.data.normal.to;
        if(to->x == comp->x && to->y == comp->y)
        {
            info->legal = true;
            return false;
        }
        break;
    }
    case CASTLE:
    {
        return true;
    }
    default:
        assert(false);
    }
    return true;
}

bool legal_move(const struct chess_ctx *ctx, struct move_t move)
{
    if(move.type == NORMAL)
    {
        struct coordinates from = move.data.normal.from;

        if(move.color != ctx->board[from.y][from.x].color) /* moving a different piece */
            return false;

        struct legal_data data;
        data.legal = false;
        data.move = move;
        for_each_move(ctx, move.data.normal.from.y, move.data.normal.from.x, legal_cb, &data, true);
        return data.legal;
    }
    else
        return true;
}

struct chess_ctx new_game(void)
{
    struct chess_ctx ret;
    memset(&ret.board, 0, sizeof(ret.board));
    for(int i = 0; i < 8; ++i)
    {
        ret.board[1][i].color = WHITE;
        ret.board[1][i].type = PAWN;
        ret.board[6][i].color = BLACK;
        ret.board[6][i].type = PAWN;
    }

    int types[] = { ROOK, KNIGHT, BISHOP };
    for(int i = 0; i < 3; ++i)
    {
        ret.board[0][i].type = types[i];
        ret.board[0][i].color = WHITE;
        ret.board[0][7 - i].type = types[i];
        ret.board[0][7 - i].color = WHITE;
        ret.board[7][i].type = types[i];
        ret.board[7][i].color = BLACK;
        ret.board[7][7 - i].type = types[i];
        ret.board[7][7 - i].color = BLACK;
    }

    ret.board[0][3].color = WHITE;
    ret.board[0][3].type = QUEEN;
    ret.board[0][4].color = WHITE;
    ret.board[0][4].type = KING;

    ret.board[7][3].color = BLACK;
    ret.board[7][3].type = QUEEN;
    ret.board[7][4].color = BLACK;
    ret.board[7][4].type = KING;
    ret.to_move = WHITE;

    ret.king_moved[0] = false;
    ret.king_moved[1] = false;
    ret.rook_moved[0][0] = false;
    ret.rook_moved[0][1] = false;
    ret.rook_moved[1][0] = false;
    ret.rook_moved[1][1] = false;

    return ret;
}

struct move_t get_move(const struct chess_ctx *ctx, enum player color)
{
    struct move_t ret;
    ret.type = NOMOVE;

    char *ptr = NULL;
    size_t sz = 0;
    ssize_t len = getline(&ptr, &sz, stdin);
    char *line = ptr;

    if(!strncasecmp(line, "o-o-o", 5))
    {
        ret.color = color;
        ret.type = CASTLE;
        ret.data.castle_style = QUEENSIDE;
        goto done;
    }
    else if(!strncasecmp(line, "o-o", 3))
    {
        ret.color = color;
        ret.type = CASTLE;
        ret.data.castle_style = KINGSIDE;
        goto done;
    }

    if(!strncasecmp(line, "uci", 3))
    {
        printf("id name chessengine\n");
        printf("uciok\n");
    }

    if(!strncasecmp(line, "isready", 7))
    {
        printf("readyok\n");
    }

    if(!strncasecmp(line, "help", 4))
    {
        best_move_negamax(ctx, DEPTH, -999999, 999999, color, &ret);
        goto done;
    }

    if(!strncasecmp(line, "position startpos moves ", 24))
    {
        line += 24;
    }

    if(len < 5)
    {
        goto done;
    }

    int x = line[0] - 'a';
    int y = line[1] - '1';
    int dx = line[2] - 'a' - x;
    int dy = line[3] - '1' - y;
    if(valid_coords(y, x) && valid_coords(y + dy, x + dx) && (dx || dy))
        ret = construct_move(color, y, x, dy, dx);

    if(valid_coords(y, x) && ctx->board[y][x].type == PAWN &&
       (y + dy == 0 || y + dy == 7))
    {
        ret.type = PROMOTION;
        ret.data.promotion.from = (struct coordinates) { y, x  };
        ret.data.promotion.to = (struct coordinates) { y + dy, x + dx };
        ret.data.promotion.type = QUEEN;
    }

done:
    free(ptr);

    return ret;
}

struct negamax_info {
    int best;
    int depth;
    int a, b;
    struct move_t move;
};

int pondered;

bool negamax_cb(void *data, const struct chess_ctx *ctx, struct move_t move)
{
    struct negamax_info *info = data;
    struct chess_ctx local = *ctx;
    if(info->depth == DEPTH)
    {
        printf("info pondering top-level move: ");
        print_move(ctx, move);
        printf("info best score so far: %d ", info->best);
        print_move(ctx, info->move);
    }
    ++pondered;
    execute_move(&local, move);
    int v = -best_move_negamax(&local, info->depth - 1, -info->b, -info->a, local.to_move, NULL);
    if(v > info->best || (v == info->best && rand() % 10 == 0))
    {
        info->best = v;
        info->move = move;
    }
    info->a = MAX(info->a, v);
    if(info->a >= info->b)
        return false;
    return true;
}

int best_move_negamax(const struct chess_ctx *ctx, int depth, int a, int b, int color, struct move_t *best)
{
    if(!depth)
        return eval_position(ctx, color);

    struct negamax_info info;
    info.best = -99999999;
    info.move.type = NOMOVE;
    info.depth = depth;
    info.a = a;
    info.b = b;

    for(int y = 0; y < 8; ++y)
    {
        for(int x = 0; x < 8; ++x)
        {
            if(ctx->board[y][x].color == ctx->to_move)
            {
                /* recurse */
                for_each_move(ctx, y, x, negamax_cb, &info, true);
            }
        }
    }
    if(best)
        *best = info.move;
    return info.best;
}

void print_status(const struct chess_ctx *ctx)
{
#ifndef UCI
    if(king_in_checkmate(ctx, WHITE))
    {
        printf("White is in checkmate\n");
        exit(0);
    }
    else if(king_in_check(ctx, WHITE, NULL))
        printf("White is in check\n");

    if(king_in_checkmate(ctx, BLACK))
    {
        printf("Black is in checkmate\n");
        exit(0);
    }
    else if(king_in_check(ctx, BLACK, NULL))
        printf("Black is in check\n");
#endif
}

int main()
{
    printf("Chessengine\n");
    srand(time(0));
    struct chess_ctx ctx = new_game();
#ifndef UCI
    print_ctx(&ctx);
#endif
    while(1)
    {
#ifndef AUTOMATCH
        struct move_t player = get_move(&ctx, ctx.to_move);
        if(player.type == NOMOVE)
        {
#ifndef UCI
            printf("Illegal\n");
#endif
            continue;
        }

        if(!legal_move(&ctx, player))
        {
#ifndef UCI
            printf("Illegal\n");
#endif
            continue;
        }

        execute_move(&ctx, player);

#ifndef UCI
        print_ctx(&ctx);
        print_status(&ctx);
#endif
#endif

        printf("info Thinking...\n");
        struct move_t best;
        pondered = 0;
        clock_t start = clock();
        best_move_negamax(&ctx, DEPTH, -999999, 999999, ctx.to_move, &best);
        clock_t end = clock();
        float time = (float)(end - start) / CLOCKS_PER_SEC;

        if(time)
        {
            printf("info pondered %d moves in %.2f seconds (%.1f/sec)\n", pondered,
                   time, pondered / time);
        }
        print_move(&ctx, best);

        execute_move(&ctx, best);
#ifndef UCI
        print_ctx(&ctx);
        print_status(&ctx);
#endif

        if(best.type == NOMOVE)
        {
            printf("info Stalemate\n");
            return 0;
        }
    }
}
