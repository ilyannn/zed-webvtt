// External scanner for the WebVTT grammar.
//
// Line breaks: `_nl` is a line break followed by more text, `_blank` is a line
// break followed by blank lines or the end of the file. Deciding this here
// keeps block boundaries out of the LR grammar.
//
// Cue identifiers: any line at a block start that has no `-->` and is not a
// NOTE, STYLE, REGION or WEBVTT line.
//
// Speakers: `<v Name>` voice tags anywhere in cue text, and `Name:` prefixes
// at the start of a line. Each distinct name (case-insensitive, spaces
// collapsed) gets the next of SPEAKER_COUNT numbers in order of first
// appearance; after that, names fall back to hash % SPEAKER_COUNT. The table
// of seen names lives in the serialized scanner state, so tree-sitter's
// incremental reparsing keeps the numbering consistent.

#include "tree_sitter/alloc.h"
#include "tree_sitter/parser.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define SPEAKER_COUNT 8
#define MAX_NAME_CHARS 40
#define MAX_PREFIX_WORDS 4

enum TokenType {
  NL,
  BLANK,
  CUE_ID,
  SPEAKER_0,
  SPEAKER_LAST = SPEAKER_0 + SPEAKER_COUNT - 1,
  ERROR_SENTINEL,
};

typedef struct {
  uint8_t count;
  uint32_t hashes[SPEAKER_COUNT];
} Scanner;

static inline void advance(TSLexer *lexer) { lexer->advance(lexer, false); }

static inline void skip(TSLexer *lexer) { lexer->advance(lexer, true); }

static inline bool is_hspace(int32_t c) { return c == ' ' || c == '\t'; }

static inline bool is_eol(int32_t c) { return c == '\n' || c == '\r'; }

static inline bool at_line_end(TSLexer *lexer) {
  return is_eol(lexer->lookahead) || lexer->eof(lexer);
}

static inline bool is_ascii_letter(int32_t c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static inline bool is_digit(int32_t c) { return c >= '0' && c <= '9'; }

// Letters in any script start a name; ASCII digits and a few marks may follow
static inline bool is_name_start(int32_t c) {
  return is_ascii_letter(c) || c >= 0x80;
}

static inline bool is_name_char(int32_t c) {
  return is_name_start(c) || is_digit(c) || c == '.' || c == '\'' ||
         c == '-' || c == '_' || c == '(' || c == ')' || c == '&';
}

static inline int32_t to_lower(int32_t c) {
  return (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c;
}

// FNV-1a over the code point's bytes
static uint32_t hash_char(uint32_t hash, int32_t c) {
  uint32_t u = (uint32_t)c;
  do {
    hash ^= u & 0xff;
    hash *= 16777619u;
    u >>= 8;
  } while (u != 0);
  return hash;
}

#define HASH_SEED 2166136261u

// Hashes a name while consuming it, collapsing runs of spaces
typedef struct {
  uint32_t hash;
  int chars;
  int words;
  bool pending_space;
} NameHash;

static void name_init(NameHash *n) {
  n->hash = HASH_SEED;
  n->chars = 0;
  n->words = 0;
  n->pending_space = false;
}

static void name_add(NameHash *n, int32_t c) {
  if (is_hspace(c)) {
    if (n->chars > 0) n->pending_space = true;
    return;
  }
  if (n->pending_space || n->chars == 0) n->words++;
  if (n->pending_space) n->hash = hash_char(n->hash, ' ');
  n->pending_space = false;
  n->hash = hash_char(n->hash, to_lower(c));
  n->chars++;
}

static enum TokenType speaker_token(Scanner *s, uint32_t hash) {
  for (uint8_t i = 0; i < s->count; i++) {
    if (s->hashes[i] == hash) return SPEAKER_0 + i;
  }
  if (s->count < SPEAKER_COUNT) {
    s->hashes[s->count] = hash;
    return SPEAKER_0 + s->count++;
  }
  return SPEAKER_0 + hash % SPEAKER_COUNT;
}

// One line break, then decide whether a blank line or EOF follows. A break
// that ends an empty line (only at the start of the file) is blank already.
static bool scan_line_break(TSLexer *lexer, const bool *valid,
                            bool empty_line) {
  if (lexer->lookahead == '\r') advance(lexer);
  if (lexer->lookahead == '\n') advance(lexer);
  lexer->mark_end(lexer);

  bool blank = empty_line;
  for (;;) {
    while (is_hspace(lexer->lookahead)) advance(lexer);
    if (lexer->eof(lexer)) {
      blank = true;
      lexer->mark_end(lexer);
      break;
    }
    if (!is_eol(lexer->lookahead)) break;
    if (lexer->lookahead == '\r') advance(lexer);
    if (lexer->lookahead == '\n') advance(lexer);
    blank = true;
    lexer->mark_end(lexer);
  }

  if (blank && valid[BLANK]) {
    lexer->result_symbol = BLANK;
    return true;
  }
  if (!blank && valid[NL]) {
    lexer->result_symbol = NL;
    return true;
  }
  return false;
}

// Does `word` follow, ended by a space or the end of the line?
static bool match_keyword(TSLexer *lexer, const char *word, int *matched) {
  for (const char *p = word; *p; p++) {
    if (lexer->lookahead != *p) return false;
    advance(lexer);
    (*matched)++;
  }
  return is_hspace(lexer->lookahead) || at_line_end(lexer);
}

static bool scan_cue_id(TSLexer *lexer) {
  if (at_line_end(lexer)) return false;

  // Block keywords belong to the internal lexer
  int matched = 0;
  int32_t first = lexer->lookahead;
  if ((first == 'N' && match_keyword(lexer, "NOTE", &matched)) ||
      (first == 'S' && match_keyword(lexer, "STYLE", &matched)) ||
      (first == 'R' && match_keyword(lexer, "REGION", &matched)) ||
      (first == 'W' && match_keyword(lexer, "WEBVTT", &matched))) {
    return false;
  }

  int dashes = 0;
  while (!at_line_end(lexer)) {
    int32_t c = lexer->lookahead;
    if (c == '>' && dashes >= 2) return false;  // a timing line
    dashes = c == '-' ? dashes + 1 : 0;
    advance(lexer);
    if (!is_hspace(c)) lexer->mark_end(lexer);
  }
  lexer->result_symbol = CUE_ID;
  return true;
}

// <v Name> or <v.class1.class2 Name>
static bool scan_voice(Scanner *s, TSLexer *lexer) {
  advance(lexer);  // <
  if (lexer->lookahead != 'v') return false;
  advance(lexer);
  if (lexer->lookahead == '.') {
    while (!is_hspace(lexer->lookahead) && lexer->lookahead != '>' &&
           !at_line_end(lexer)) {
      advance(lexer);
    }
  }
  if (!is_hspace(lexer->lookahead)) return false;

  NameHash name;
  name_init(&name);
  while (lexer->lookahead != '>') {
    if (at_line_end(lexer)) return false;
    name_add(&name, lexer->lookahead);
    advance(lexer);
  }
  advance(lexer);  // >
  lexer->mark_end(lexer);
  lexer->result_symbol = speaker_token(s, name.hash);
  return true;
}

// Name: at the start of a line, optionally after a `-` or `>>` dialogue mark
static bool scan_prefix(Scanner *s, TSLexer *lexer) {
  if (lexer->lookahead == '-' || lexer->lookahead == '>') {
    int32_t mark = lexer->lookahead;
    advance(lexer);
    if (mark == '>') {
      if (lexer->lookahead != '>') return false;
      advance(lexer);
    }
    if (!is_hspace(lexer->lookahead)) return false;
    while (is_hspace(lexer->lookahead)) advance(lexer);
  }
  if (!is_name_start(lexer->lookahead)) return false;

  NameHash name;
  name_init(&name);
  while (lexer->lookahead != ':') {
    int32_t c = lexer->lookahead;
    if (at_line_end(lexer)) return false;
    if (!is_hspace(c) && !is_name_char(c)) return false;
    name_add(&name, c);
    if (name.chars > MAX_NAME_CHARS || name.words > MAX_PREFIX_WORDS) {
      return false;
    }
    advance(lexer);
  }
  advance(lexer);  // :
  // `Note:` yes, `http://` and `10:30` no
  if (!is_hspace(lexer->lookahead) && !at_line_end(lexer)) return false;
  lexer->mark_end(lexer);
  lexer->result_symbol = speaker_token(s, name.hash);
  return true;
}

void *tree_sitter_webvtt_external_scanner_create(void) {
  return ts_calloc(1, sizeof(Scanner));
}

void tree_sitter_webvtt_external_scanner_destroy(void *payload) {
  ts_free(payload);
}

unsigned tree_sitter_webvtt_external_scanner_serialize(void *payload,
                                                       char *buffer) {
  Scanner *s = (Scanner *)payload;
  buffer[0] = (char)s->count;
  memcpy(buffer + 1, s->hashes, s->count * sizeof(uint32_t));
  return 1 + s->count * sizeof(uint32_t);
}

void tree_sitter_webvtt_external_scanner_deserialize(void *payload,
                                                     const char *buffer,
                                                     unsigned length) {
  Scanner *s = (Scanner *)payload;
  s->count = 0;
  if (length == 0) return;
  uint8_t count = (uint8_t)buffer[0];
  if (count > SPEAKER_COUNT || length < 1 + count * sizeof(uint32_t)) return;
  s->count = count;
  memcpy(s->hashes, buffer + 1, count * sizeof(uint32_t));
}

bool tree_sitter_webvtt_external_scanner_scan(void *payload, TSLexer *lexer,
                                              const bool *valid) {
  Scanner *s = (Scanner *)payload;
  if (valid[ERROR_SENTINEL]) return false;

  if (valid[NL] || valid[BLANK]) {
    bool line_start = lexer->get_column(lexer) == 0;
    while (is_hspace(lexer->lookahead)) skip(lexer);
    if (is_eol(lexer->lookahead)) {
      return scan_line_break(lexer, valid, line_start);
    }
  }

  if (valid[CUE_ID] && lexer->get_column(lexer) == 0) {
    return scan_cue_id(lexer);
  }

  if (valid[SPEAKER_0]) {
    if (lexer->lookahead == '<') return scan_voice(s, lexer);
    if (lexer->get_column(lexer) == 0) return scan_prefix(s, lexer);
  }

  return false;
}
