; Header
(signature) @keyword
(header (description) @title)
(metadata key: (key) @property)
(metadata value: (value) @string)

; Blocks
(note) @comment
["STYLE" "REGION"] @keyword
(cue_id) @label.cue_id

; Timing and settings
(timestamp) @number
"-->" @operator
(setting name: (setting_name) @property)
(setting value: (setting_value) @constant)
":" @punctuation.delimiter

; Cue text markup
(tag) @tag

; Speakers: each number gets a different base capture, so any theme shows
; distinct colours. Themes can set exact colours with the full keys, e.g.
; "function.speaker.0" in theme_overrides.syntax. Labels (`<v Alice>`, `Alice:`)
; add `.name`, which falls back to the speaker colour unless a theme sets it.

(speaker_0) @function.speaker.0.name
(turn (speaker_0) (text) @function.speaker.0)

(speaker_1) @keyword.speaker.1.name
(turn (speaker_1) (text) @keyword.speaker.1)

(speaker_2) @type.speaker.2.name
(turn (speaker_2) (text) @type.speaker.2)

(speaker_3) @string.speaker.3.name
(turn (speaker_3) (text) @string.speaker.3)

(speaker_4) @constant.speaker.4.name
(turn (speaker_4) (text) @constant.speaker.4)

(speaker_5) @number.speaker.5.name
(turn (speaker_5) (text) @number.speaker.5)

(speaker_6) @property.speaker.6.name
(turn (speaker_6) (text) @property.speaker.6)

(speaker_7) @attribute.speaker.7.name
(turn (speaker_7) (text) @attribute.speaker.7)
