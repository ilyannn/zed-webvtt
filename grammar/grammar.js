/**
 * Tree-sitter grammar for WebVTT and SubRip (SRT) subtitles.
 *
 * Line structure comes from the external scanner (src/scanner.c): `_nl` is a
 * line break followed by more text, `_blank` is a line break followed by one
 * or more blank lines (or the end of the file), which ends a block.
 *
 * The scanner also recognises speakers, either as a WebVTT voice tag
 * (`<v Alice>`) or as a name prefix at the start of a line (`Alice: hi`,
 * `- Alice: hi`, `>> Alice: hi`). It numbers speakers in order of first
 * appearance and emits `speaker_0` … `speaker_7`, so highlight queries can
 * give each speaker a colour of their own.
 */

const SPEAKERS = 8;
const speakerNames = Array.from({ length: SPEAKERS }, (_, i) => `speaker_${i}`);

module.exports = grammar({
  name: "webvtt",

  extras: (_) => [/[ \t]/],

  externals: ($) => [
    $._nl,
    $._blank,
    $.cue_id,
    ...speakerNames.map((name) => $[name]),
    $._error_sentinel,
  ],

  rules: {
    source_file: ($) =>
      seq(
        optional($._blank),
        optional(
          seq(
            choice($.header, $._block),
            repeat(seq($._blank, $._block)),
            optional($._blank),
          ),
        ),
      ),

    // WEBVTT signature line plus optional `Key: value` metadata lines
    header: ($) =>
      seq(
        $.signature,
        optional(field("description", $.description)),
        repeat(seq($._nl, $.metadata)),
      ),

    signature: (_) => "WEBVTT",

    description: (_) => /[^\s][^\r\n]*/,

    metadata: ($) =>
      seq(
        field("key", $.key),
        ":",
        optional(field("value", alias($.description, $.value))),
      ),

    key: (_) => /[^:\s][^:\r\n]*/,

    _block: ($) => choice($.cue, $.note, $.style, $.region),

    note: ($) =>
      seq("NOTE", optional($._rest), repeat(seq($._nl, $._rest))),

    style: ($) => seq("STYLE", optional(seq($._nl, $.stylesheet))),

    stylesheet: ($) => seq($._rest, repeat(seq($._nl, $._rest))),

    region: ($) => seq("REGION", repeat(seq($._nl, repeat1($.setting)))),

    _rest: (_) => /[^\s][^\r\n]*/,

    cue: ($) =>
      seq(
        optional(seq($.cue_id, $._nl)),
        $.timing,
        optional(seq($._nl, $.payload)),
      ),

    timing: ($) =>
      seq(
        field("start", $.timestamp),
        "-->",
        field("end", $.timestamp),
        repeat($.setting),
      ),

    // hh:mm:ss.ttt or mm:ss.ttt; SRT uses a comma before the milliseconds
    timestamp: (_) => /(\d+:)?\d{1,2}:\d{2}[.,]\d{1,3}/,

    setting: ($) =>
      seq(field("name", $.setting_name), ":", field("value", $.setting_value)),

    setting_name: (_) => /[A-Za-z][A-Za-z0-9-]*/,

    setting_value: (_) => /[^\s:]+/,

    // Unattributed lines first, then one turn per speaker change
    payload: ($) => choice(seq($._plain, repeat($.turn)), repeat1($.turn)),

    _plain: ($) =>
      seq(
        repeat1($._inline),
        repeat(seq($._nl, repeat1($._inline))),
        optional($._nl),
      ),

    // A speaker and their words, up to the next speaker or the end of the cue
    turn: ($) =>
      seq(
        field("speaker", $._speaker),
        repeat($._inline),
        repeat(seq($._nl, repeat1($._inline))),
        optional($._nl),
      ),

    _speaker: ($) => choice(...speakerNames.map((name) => $[name])),

    _inline: ($) => choice($.text, $.tag),

    // A lone `<` that cannot start a tag stays text (`a < b`)
    text: (_) => /([^<\r\n]|<[^A-Za-z0-9/\r\n])+/,

    // <i>, </b>, <c.yellow>, </v>, <lang en>, <00:00:01.000>
    tag: (_) => /<[^>\r\n]*>/,
  },
});
