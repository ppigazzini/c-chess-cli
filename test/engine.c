/*
 * c-chess-cli, a command line interface for UCI chess engines. Copyright 2020 lucasart.
 *
 * c-chess-cli is free software: you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * c-chess-cli is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
 * even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program. If
 * not, see <http://www.gnu.org/licenses/>.
*/
// Stand alone program: minimal UCI engine (random mover) used for testing and benchmarking
#include "gen.h"
#include "util.h"
#include "vec.h"

#define uci_printf(...) printf(__VA_ARGS__), fflush(stdout)
#define uci_puts(str) puts(str), fflush(stdout)

typedef struct {
    int depth;
} Go;

static void parse_go(const char *tail, Go *go)
{
    scope(str_destroy) str_t token = str_init();
    tail = str_tok(tail, &token, " ");
    assert(tail);

    if (!strcmp(token.buf, "depth") && (tail = str_tok(tail, &token, " ")))
        go->depth = atoi(token.buf);
}

static void parse_option(const char *tail, bool *uciChess960)
{
    scope(str_destroy) str_t token = str_init();

    if ((tail = str_tok(tail, &token, " ")) && !strcmp(token.buf, "name")
            && (tail = str_tok(tail, &token, " ")) && !strcmp(token.buf, "UCI_Chess960")
            && (tail = str_tok(tail, &token, " ")) && !strcmp(token.buf, "value")
            && (tail = str_tok(tail, &token, " ")))
        *uciChess960 = !strcmp(token.buf, "true");
}

static void parse_position(const char *tail, Position *pos, bool uciChess960)
{
    scope(str_destroy) str_t token = str_init();
    tail = str_tok(tail, &token, " ");
    assert(tail);

    if (!strcmp(token.buf, "startpos")) {
        pos_set(pos, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", uciChess960, NULL);
        tail = str_tok(tail, &token, " ");
    } else if (!strcmp(token.buf, "fen")) {
        scope(str_destroy) str_t fen = str_init();

        while ((tail = str_tok(tail, &token, " ")) && strcmp(token.buf, "moves"))
            str_push(str_cat(&fen, token), ' ');

        if (!pos_set(pos, fen.buf, uciChess960, NULL))
            DIE("Illegal FEN '%s'\n", fen.buf);
    } else
        assert(false);

    if (!strcmp(token.buf, "moves")) {
        Position p[2];
        int idx = 0;
        p[0] = *pos;

        while ((tail = str_tok(tail, &token, " "))) {
            const move_t m = pos_lan_to_move(&p[idx], token.buf);
            pos_move(&p[1 - idx], &p[idx], m);
            idx = 1 - idx;
        }

        *pos = p[idx];
    }
}

static void random_pv(const Position *pos, uint64_t *seed, int len, str_t *pv)
{
    str_clear(pv);
    Position p[2];
    p[0] = *pos;
    move_t *moves = vec_init_reserve(64, move_t);
    scope(str_destroy) str_t lan = str_init();

    for (int ply = 0; ply < len; ply++) {
        // Generate and count legal moves
        moves = gen_all_moves(&p[ply % 2], moves);
        const uint64_t n = (uint64_t)vec_size(moves);
        if (n == 0)
            break;

        // Choose a random one
        const move_t m = moves[prng(seed) % n];
        pos_move_to_lan(&p[ply % 2], m, &lan);
        str_push(str_cat(pv, lan), ' ');
        pos_move(&p[(ply + 1) % 2], &p[ply % 2], m);
    }

    vec_destroy(moves);
}

static void run_go(const Position *pos, const Go *go, uint64_t *seed)
{
    scope(str_destroy) str_t pv = str_init();

    for (int depth = 1; depth <= go->depth; depth++) {
        random_pv(pos, seed, depth, &pv);
        uci_printf("info depth %d score cp %d pv %s\n", depth, (int)(prng(seed) % 65536) - 32768,
            pv.buf);
    }

    scope(str_destroy) str_t token = str_init();
    str_tok(pv.buf, &token, " ");
    uci_printf("bestmove %s\n", token.buf);
}

int main(int argc, char **argv)
{
    Position pos = {0};
    Go go = {0};
    bool uciChess960 = false;
    uint64_t seed = argc > 1 ? (uint64_t)atoll(argv[1]) : 0;

    scope(str_destroy) str_t line = str_init(), token = str_init();

    while (str_getline(&line, stdin)) {
        const char *tail = str_tok(line.buf, &token, " ");

        if (!strcmp(token.buf, "uci")) {
            uci_puts("id name engine");
            uci_printf("option name UCI_Chess960 type check default %s\n",
                uciChess960 ? "true" : "false");
            uci_puts("uciok");
        } else if (!strcmp(token.buf, "isready"))
            uci_puts("readyok");
        else if (!strcmp(token.buf, "setoption"))
            parse_option(tail, &uciChess960);
        else if (!strcmp(token.buf, "position"))
            parse_position(tail, &pos, uciChess960);
        else if (!strcmp(token.buf, "go")) {
            parse_go(tail, &go);
            run_go(&pos, &go, &seed);
        } else if (!strcmp(token.buf, "quit"))
            break;
    }
}
