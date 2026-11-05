# WebVTT for Zed

Syntax highlighting for WebVTT (`.vtt`) and SubRip (`.srt`) subtitles in [Zed](https://zed.dev), with each speaker in a colour of their own, so you can follow a meeting transcript at a glance.

It highlights:

- speakers, written as voice tags (`<v Alice>`, `<v.loud Bob>`, as in Teams transcripts) or as name prefixes at the start of a line (`Alice:`, `- Alice:`, `>> Alice:`, as in Zoom transcripts and SRT files). Each speaker's label and words share one colour.
- cue numbers and identifiers, timestamps (`00:00:01.000`, `00:01,000`) and cue settings (`align:start`)
- tags (`<i>`, `</v>`, `<c.yellow>`, `<00:00:01.500>`)
- the `WEBVTT` header and its metadata (`Kind: captions`), `NOTE` blocks as comments, `REGION` blocks, and `STYLE` blocks as CSS

Speakers are numbered in order of first appearance, and the same name always gets the same number, whichever way it is written and ignoring case and extra spaces. The first eight speakers get distinct colours; after that, names share colours.

## Installation

The extension isn't in the Zed extension registry yet. To install it as a dev extension:

1. Clone this repository.
2. In Zed, run `zed: install dev extension` from the command palette and select the cloned folder.

If it fails to load, run `zed: open log` to see the error.

## Colours

Speaker *n* is captured with a different base for each *n*, so any theme already shows eight distinct colours:

| Speaker | Capture | Label capture |
|---|---|---|
| 0 | `function.speaker.0` | `function.speaker.0.name` |
| 1 | `keyword.speaker.1` | `keyword.speaker.1.name` |
| 2 | `type.speaker.2` | `type.speaker.2.name` |
| 3 | `string.speaker.3` | `string.speaker.3.name` |
| 4 | `constant.speaker.4` | `constant.speaker.4.name` |
| 5 | `number.speaker.5` | `number.speaker.5.name` |
| 6 | `property.speaker.6` | `property.speaker.6.name` |
| 7 | `attribute.speaker.7` | `attribute.speaker.7.name` |

To pick the colours yourself, set the full keys in `theme_overrides` in your Zed `settings.json`. The `.name` keys style the speaker labels and fall back to the speaker colour when unset. Cue numbers use `label.cue_id`.

```jsonc
"theme_overrides": {
  "One Dark": {
    "syntax": {
      "function.speaker.0": { "color": "#5CC98A" },
      "function.speaker.0.name": { "color": "#5CC98A", "font_style": "italic" },
      "keyword.speaker.1": { "color": "#D38BD0" },
      "keyword.speaker.1.name": { "color": "#D38BD0", "font_style": "italic" },
      // … up to attribute.speaker.7
      "label.cue_id": { "color": "#858B97", "font_weight": 700 }
    }
  }
}
```

## Limitations

- After `</v>`, the rest of the line stays in that speaker's colour.
- A `Name:` prefix counts only at the very start of a line, with at most four words and no punctuation other than `. ' - _ ( ) &`. A line such as `Note: bring snacks` is still read as a speaker.
- With more than eight speakers, colours repeat.
- Cues must be separated by blank lines, as both formats require.

## Development

```
extension.toml               extension manifest; pins the grammar commit
grammar/grammar.js           tree-sitter grammar
grammar/src/scanner.c        external scanner: line breaks, cue ids, speaker numbering
grammar/src/                 generated parser (committed; Zed builds from it)
grammar/test/corpus/         grammar tests
languages/webvtt/            language config, highlight and injection queries
examples/                    sample .vtt and .srt files
```

Tools are pinned in `.tool-versions` for [mise](https://mise.jdx.dev); `just` lists the recipes. Run `just hooks` once after cloning so pushes run `just check`.

```sh
just generate   # regenerate the parser from grammar.js
just test       # corpus tests, and the examples parse without errors
just check      # lint (parser in sync, queries load) and test
```

Changes to files in `languages/webvtt/` only need **Rebuild** in Zed's Extensions panel.

Zed builds the grammar from the commit named in `extension.toml`, not from your working tree. After changing the grammar or scanner, commit and push, set `rev` in `extension.toml` to the new commit hash, and rebuild the extension in Zed.

## License

[MIT](LICENSE)
