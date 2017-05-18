#include "chess.h"

#define DEPTH 3

//#define AUTOMATCH
#define UCI

int count_material(const struct chess_ctx *ctx, int color)
{
    int total = 0;
    static const int values[] = { 0,
                                  1, /* pawn */
                                  8, /* rook */
                                  3, /* knight */
                                  3, /* bishop */
                                  20, /* queen */
                                  5  /* king */
    };
    for(int y = 0; y < 8; ++y)
    {
        for(int x = 0; x < 8; ++x)
        {
            if(ctx->board[y][x].color == color)
                total += values[ctx->board[y][x].type];

            /* pawn near promotion */
            if(ctx->board[y][x].type == PAWN)
            {
                if((y >= 5 && ctx->board[y][x].color == WHITE) ||
                   (y <= 2 && ctx->board[y][x].color == BLACK))
                    total += 4;

                /* pawns are more valuable the further they have advanced */
                total += ctx->board[y][x].color == WHITE ? y : 7 - y;
            }
            if(ctx->board[y][x].type == KING && (y == 2 || y == 6)) /* castled king */
                total += 20;
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
                for_each_move(ctx, y, x, count_space_cb, &space, true, true);
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

    score += count_material(ctx, color) * 4;
    score -= count_material(ctx, inv_player(color)) * 2;
    score += count_space(ctx, color);
    score -= count_space(ctx, inv_player(color));

    if(can_castle(ctx, color, QUEENSIDE) || can_castle(ctx, color, KINGSIDE))
        score += 25;

    if(king_in_check(ctx, color, NULL))
    {
        score -= 9;
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

#define enemy_occupied(ctx, y, x, c) (valid_coords((y),(x))?((ctx)->board[(y)][(x)].color == NONE ? false : ((ctx)->board[(y)][(x)].color == inv_player(c))):false)

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

#define occupied(ctx, y, x) (valid_coords((y),(x))?((ctx)->board[(y)][(x)].color != NONE):false)

struct check_info {
    int color;
    bool checked;
    struct coordinates king;
};

bool detect_check_cb(void *data, const struct chess_ctx *ctx, struct move_t move)
{
    struct check_info *info = data;
    int x, y;
    if(move.type == NORMAL)
    {
        x = move.data.normal.to.x;
        y = move.data.normal.to.y;
    }
    else if(move.type == PROMOTION)
    {
        x = move.data.promotion.to.x;
        y = move.data.promotion.to.y;
    }
    else
        return true;

    if(ctx->board[y][x].type == KING && ctx->board[y][x].color == info->color)
    {
        info->king.y = y;
        info->king.x = x;
        info->checked = true;
        return false;
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
                for_each_move(ctx, y, x, detect_check_cb, &info, false, false);
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
    (void) ctx;
    struct threatened_info *info = data;

    int x, y;
    if(move.type == NORMAL)
    {
        x = move.data.normal.to.x;
        y = move.data.normal.to.y;
    }
    else if(move.type == PROMOTION)
    {
        x = move.data.promotion.to.x;
        y = move.data.promotion.to.y;
    }
    else
        return true;

    if(y == info->y && x == info->x)
    {
        info->threatened = true;
        return false;
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
                for_each_move(ctx, y, x, threatened_cb, &info, false, false);
                if(info.threatened)
                    return true;
            }
        }
    }
    return false;
}

struct move_t construct_move(int color, int y, int x, int dy, int dx)
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

    /* move is to castle, must be before next if(), as it relies on it
     * to make sure it's legal */
    if(ctx->board[y][x].type == KING && ABS(dx) == 2)
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
        for(unsigned int i = 0; i < ARRAYLEN(promote_pieces); ++i)
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
                   void *data, bool enforce_check, bool consider_castle)
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

        /* en passant */
        if((piece->color == WHITE && y == 4) ||
           (piece->color == BLACK && y == 3))
        {
            /* piece to right */
            int opp = piece->color == WHITE ? 1 : 0;
            if(valid_coords(y + dy, x + 1) && ctx->en_passant[opp][x + 1])
            {
                if(!gen_and_call(ctx, y + dy, x, dy, 1, cb, data, enforce_check))
                    return;
            }
            if(valid_coords(y + dy, x - 1) && ctx->en_passant[opp][x - 1])
            {
                if(!gen_and_call(ctx, y + dy, x, dy, -1, cb, data, enforce_check))
                    return;
            }
        }

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

        /* castling */
        if(piece->type == KING && consider_castle)
        {
            if(x == 4 && (y == 0 || y == 7))
            {
                if(can_castle(ctx, piece->color, QUEENSIDE))
                    if(!gen_and_call(ctx, y, x, 0, -2, cb, data, false))
                        return;
                if(can_castle(ctx, piece->color, KINGSIDE))
                    if(!gen_and_call(ctx, y, x, 0, 2, cb, data, false))
                        return;
            }
        }

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

        (void) to;
        (void) from;

        char name[3];
        name[0] = 'a' + move.data.normal.to.x;
        name[1] = '1' + move.data.normal.to.y;
        name[2] = '\0';

#ifdef UCI
        char fromname[3];
        fromname[0] = 'a' + move.data.normal.from.x;
        fromname[1] = '1' + move.data.normal.from.y;
        fromname[2] = '\0';
        printf("%s%s\n", fromname, name);
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
        char piecename[2];
        piecename[0] = "  rnbq"[move.data.promotion.type];
        piecename[1] = '\0';
        printf("%s%s%s\n", fromname, name, piecename);
#else
        printf("pawn promoted\n");
#endif
        break;
    }
    case CASTLE:
    {
#ifdef UCI
        const char *castle_lan[2][2] = { { "e1c1", "e1g1" },
                                         { "e8c8", "e8g8" } };
        printf("%s\n", castle_lan[move.color == WHITE ? 0 : 1][move.data.castle_style]);
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
    memset(&ctx->en_passant[move.color == WHITE ? 0 : 1], 0, sizeof(ctx->en_passant[0]));
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

        if(from->type == PAWN)
        {
            /* see if we've moved two squares ahead */
            if(ABS(move.data.normal.to.y - move.data.normal.from.y) == 2)
                ctx->en_passant[move.color == WHITE ? 0 : 1][move.data.normal.to.x] = true;
            else if(move.data.normal.to.x - move.data.normal.from.x != 0 && to->type == EMPTY)
            {
                /* en passant capture */
                ctx->board[move.data.normal.from.y][move.data.normal.to.x].type = EMPTY;
                ctx->board[move.data.normal.from.y][move.data.normal.to.x].color = NONE;
            }
        }
        if(from->type == KING)
        {
            ctx->king_moved[move.color == WHITE ? 0 : 1] = true;
        }
        else if(from->type == ROOK && (move.data.normal.from.x == 0 || move.data.normal.from.x == 7))
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
        ctx->king_moved[move.color == WHITE ? 0 : 1] = true;
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
    (void) ctx;
    struct legal_data *info = data;
    switch(info->move.type)
    {
    case PROMOTION:
    case CASTLE:
    {
        if(move.type == info->move.type &&
           move.type == CASTLE ? move.data.castle_style == info->move.data.castle_style : true)
        {
            info->legal = true;
            return false;
        }
        break;
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
    default:
        assert(false);
    }
    return true;
}

bool can_castle(const struct chess_ctx *ctx, int color, int style)
{
    int k_idx = color == WHITE ? 0 : 1;
    if(ctx->king_moved[k_idx])
        return false;

    int r_idx = style == QUEENSIDE ? 0 : 1;
    if(ctx->rook_moved[k_idx][r_idx])
        return false;

    int start = (style == QUEENSIDE ? 1 : 5);
    int end = (style == QUEENSIDE ? 3 : 6);

    int y = color == WHITE ? 0 : 7;
    bool clear = true;
    for(int i = start; i <= end; ++i)
        if(ctx->board[y][i].type != EMPTY)
            clear = false;

    if(clear)
    {
        int dx = style == QUEENSIDE ? -1 : 1;
        if(!square_threatened(ctx, y, 4 + dx, color) &&
           !square_threatened(ctx, y, 4 + 2*dx, color))
            if(!king_in_check(ctx, color, NULL))
                return true;
    }
    return false;
}

bool legal_move(const struct chess_ctx *ctx, struct move_t move)
{
    switch(move.type)
    {
    case NORMAL:
    {
        struct coordinates from = move.data.normal.from;

        if(move.color != ctx->board[from.y][from.x].color) /* moving a different piece */
            return false;

        struct legal_data data;
        data.legal = false;
        data.move = move;
        for_each_move(ctx, move.data.normal.from.y, move.data.normal.from.x, legal_cb, &data, true, true);
        return data.legal;
    }
    case CASTLE:
    {
        return can_castle(ctx, move.color, move.data.castle_style);
    }
    default:
        return true;
    }
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

struct move_t move_from_str(const struct chess_ctx *ctx, char **line, int color)
{
    struct move_t ret;

    int x = (*line)[0] - 'a';
    int y = (*line)[1] - '1';
    int dx = (*line)[2] - 'a' - x;
    int dy = (*line)[3] - '1' - y;
    if(valid_coords(y, x) && valid_coords(y + dy, x + dx) && (dx || dy))
        ret = construct_move(color, y, x, dy, dx);
    char piece = (*line)[4];
    *line += 5;
    if(piece && piece != ' ' && piece != '\n')
    {
        /* promotion */
        (*line)++;

        ret.type = PROMOTION;
        ret.data.promotion.from = (struct coordinates) { y, x  };
        ret.data.promotion.to = (struct coordinates) { y + dy, x + dx };
        switch(toupper(piece))
        {
        case 'R':
            ret.data.promotion.type = ROOK;
            break;
        case 'N':
            ret.data.promotion.type = KNIGHT;
            break;
        case 'B':
            ret.data.promotion.type = BISHOP;
            break;
        case 'Q':
            ret.data.promotion.type = QUEEN;
            break;
        default:
            ret.type = NOMOVE;
            break;
        }
    }

    if(valid_coords(y, x) && ctx->board[y][x].type == PAWN &&
       (y + dy == 0 || y + dy == 7) && ret.type != PROMOTION)
    {
        ret.type = NOMOVE; /* we don't allow pawns on the 8th rank,
                            * they must be promoted */
    }

    if(valid_coords(y, x) && x == 4 && y == (color == WHITE ? 0 : 7) &&
       ctx->board[y][x].color == color && ctx->board[y][x].type == KING &&
       ABS(dx) == 2 && dy == 0)
    {
        /* castle */
        ret.type = CASTLE;
        ret.data.castle_style = dx > 0 ? KINGSIDE : QUEENSIDE;
    }
    return ret;
}

struct chess_ctx get_uci_ctx(void)
{
    while(1)
    {
        char *ptr = NULL;
        size_t sz = 0;
        ssize_t len = getline(&ptr, &sz, stdin);
        char *line = ptr;

        if(!strncasecmp(line, "uci", 3))
        {
            printf("id name chessengine\n");
            printf("id author Franklin Wei\n");
            printf("uciok\n");
            fflush(stdout);
        }
        else if(!strncasecmp(line, "isready", 7))
        {
            printf("readyok\n");
            fflush(stdout);
        }
        else if(!strncasecmp(line, "position startpos moves ", 24))
        {
            struct chess_ctx ctx = new_game();

            line += 24;
            len -= 24;
            while(len > 0)
            {
                char *before = line;
                struct move_t move = move_from_str(&ctx, &line, ctx.to_move);
                execute_move(&ctx, move);
                len -= line - before;
            }
            free(ptr);
            print_ctx(&ctx);
            return ctx;
        }
        else if(!strncasecmp(line, "position startpos",  17))
        {
            free(ptr);
            return new_game();
        }

        free(ptr);
    }
}

struct move_t get_move(const struct chess_ctx *ctx, enum player color)
{
    struct move_t ret;
    ret.type = NOMOVE;

    char *ptr = NULL;
    size_t sz = 0;
    ssize_t len = getline(&ptr, &sz, stdin);
    char *line = ptr;

    if(!strncasecmp(line, "0-0-0", 5))
    {
        ret.color = color;
        ret.type = CASTLE;
        ret.data.castle_style = QUEENSIDE;
        goto done;
    }
    else if(!strncasecmp(line, "0-0", 3))
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
        fflush(stdout);
    }

    if(!strncasecmp(line, "isready", 7))
    {
        printf("readyok\n");
        fflush(stdout);
    }

    if(!strncasecmp(line, "help", 4))
    {
        best_move_negamax(ctx, DEPTH, -999999, 999999, color, &ret);
        goto done;
    }

    if(len < 5)
    {
        goto done;
    }

    ret = move_from_str(ctx, &line, color);

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

uint64_t pondered;
int moveno;

bool negamax_cb(void *data, const struct chess_ctx *ctx, struct move_t move)
{
    struct negamax_info *info = data;
    struct chess_ctx local = *ctx;
    if(info->depth == DEPTH)
    {
#if defined(UCI) || DEPTH > 4
        printf("info currmove ");
        print_move(ctx, move);
        printf("info currmovenumber %d\n", ++moveno);
        fflush(stdout);
#endif
    }
#if DEPTH > 5
    else if(info->depth == DEPTH - 1)
    {
        printf("submove ");
        print_move(ctx, move);
        fflush(stdout);
    }
#endif
    ++pondered;
    execute_move(&local, move);
    int v = -best_move_negamax(&local, info->depth - 1, -info->b, -info->a, local.to_move, NULL);
    if(v > info->best || (v == info->best && rand() % 2 == 0))
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
    struct negamax_info info;
    info.best = -99999999;
    info.move.type = NOMOVE;
    info.depth = depth;
    info.a = a;
    info.b = b;
    if(depth > 0)
    {
        for(int y = 0; y < 8; ++y)
        {
            for(int x = 0; x < 8; ++x)
            {
                if(ctx->board[y][x].color == ctx->to_move)
                {
                    /* recurse */
                    for_each_move(ctx, y, x, negamax_cb, &info, true, true);
                }
            }
        }
        if(best)
            *best = info.move;
    }
    if(!depth || info.move.type == NOMOVE) /* terminal node */
        return eval_position(ctx, color);

    return info.best;
}

void print_status(const struct chess_ctx *ctx)
{
    (void) ctx;
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
#ifndef UCI
    struct chess_ctx ctx = new_game();
    print_ctx(&ctx);
#endif
    for(;;)
    {
#ifndef AUTOMATCH
#ifndef UCI
        struct move_t player = get_move(&ctx, ctx.to_move);
        if(player.type == NOMOVE)
        {
            printf("Illegal\n");
            continue;
        }

        if(!legal_move(&ctx, player))
        {
            printf("Illegal\n");
            continue;
        }

        execute_move(&ctx, player);

        print_ctx(&ctx);
        print_status(&ctx);
#else
        struct chess_ctx ctx = get_uci_ctx();
#endif
#endif

        printf("info Thinking...\n");
        struct move_t best;
        pondered = 0;
        moveno = 0;
        clock_t start = clock();
        best_move_negamax(&ctx, DEPTH, -999999, 999999, ctx.to_move, &best);
        clock_t end = clock();
        float time = (float)(end - start) / CLOCKS_PER_SEC;

        if(time)
        {
            printf("info pondered %"PRIu64" moves in %.2f seconds (%.1f/sec)\n", pondered,
                   time, pondered / time);
        }
        printf("bestmove ");
        print_move(&ctx, best);
        fflush(stdout);

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
