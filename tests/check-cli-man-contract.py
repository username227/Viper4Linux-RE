#!/usr/bin/env python3
"""Keep CLI command coverage and the GUI-order manual structure in sync."""
import pathlib
import re
repo = pathlib.Path(__file__).resolve().parents[1]
cli = (repo / 'src/viper-ctl.cpp').read_text()
man = (repo / 'man/viper4linux-ctl.1').read_text()
commands = set(re.findall(r'\bcmd\s*==\s*"([A-Za-z0-9-]+)"', cli))
# Internal handlers retain historical names, while the manual documents
# canonical scoped operations. No old aliases need to be repeated per effect.
effect_selectors = set(re.findall(
    r'\{"([a-z]+)",\s*"[^"]+"\}',
    (repo / 'src/viper-cli-scope.h').read_text()))
def documented(command):
    if re.search(r'(?<![\w-])' + re.escape(command) + r'(?![\w-])', man):
        return True
    effect, _, option = command.partition('-')
    if effect not in effect_selectors:
        return False
    if option in ('on', 'off'):
        return f'.B "--{effect} on | off"' in man
    return re.search(r'^\.(?:BI|B) "--' + re.escape(effect) +
                     r'\s+' + re.escape(option) + r'=', man, re.M) is not None

missing = sorted(c for c in commands if not documented(c))
assert len(commands) >= 85, f'CLI parser coverage dropped unexpectedly ({len(commands)})'
assert not missing, f'Undocumented CLI commands: {missing}'
assert '--help' in man and 'round' in man and 'inclusive' in man
assert 'bass-gain' in commands and 'bass-freq' in commands
assert not {'gain', 'freq'} & commands
assert '.BI "--bass gain=" VALUE' in man and '.BI "--bass freq=" VALUE' in man
assert 'numerical values must use the scoped' in man
assert '--eq level=BAND:DB' in man
assert '.SH LEGACY NO-ARGUMENT ALIASES' in man
canonical = man.split('.SH LEGACY NO-ARGUMENT ALIASES', 1)[0]
assert not re.search(r'^\.BR [a-z]+-(?:on|off)', canonical, re.M)
assert 'bass-gain 3' not in canonical
assert 'headphone-level 2' not in canonical
assert '.B "--bass on | off"' in canonical
assert 'There are no standalone numerical arguments.' in man
assert 'Legacy commands and the new double-hyphen' in man
assert 'same invocation' in man
assert man.index('.SH LEGACY NO-ARGUMENT ALIASES') > man.index('.SH EXAMPLES')
assert not re.search(r'^\.BI "(gain|freq) "', man, re.M)
assert '.SH COMMAND SEQUENCES' in man and 'not transactions' in man
assert 'headphone-quality' not in cli
assert 'headphone-quality' not in man
gui = (repo / 'src/viper-gui.cpp').read_text()
effects_doc = (repo / 'docs/EFFECTS.md').read_text()
assert 'headphone_note_' not in gui
assert 'Headphone Surround+ details' in effects_doc
expected = [
    ('1. OUTPUT', ['Master Limiter', 'Playback Gain Control (AGC)', 'LUFS Targeting']),
    ('2. BASS', ['ViPER Bass', 'ViPER Bass Mono', 'Psychoacoustic Bass', 'Dynamic System']),
    ('3. EQ & TONE', ['Graphic Equalizer', 'Dynamic EQ', 'ViPER Clarity', 'Spectrum Extension', 'Tube Simulator', 'AnalogX']),
    ('4. SPATIAL', ['Crossfeed (ViPER Cure)', 'Headphone Surround+', 'Differential Surround', 'Field Surround', 'Reverberation', 'Stereo Imager']),
    ('5. DYNAMICS', ['FET Compressor', 'Multiband Compressor']),
    ('6. SOUND CORRECTION', ['ViPER-DDC', 'Speaker Correction', 'Convolver']),
]
sections = re.findall(r'^\.SH "([1-6]\. [^\"]+)"$', man, flags=re.M)
assert sections == [title for title, _ in expected], f'Wrong GUI tab order: {sections}'
parts = re.split(r'^\.SH "[1-6]\. [^\"]+"$', man, flags=re.M)[1:]
assert len(parts) == 6
for (tab, effects), part in zip(expected, parts):
    subsections = re.findall(r'^\.SS "([A-Z])\. ([^\"]+)"$', part, flags=re.M)
    assert subsections == [(chr(65+i), name) for i, name in enumerate(effects)], f'Wrong effect order in {tab}: {subsections}'
assert '0 = Natural,\n1 = Pure Bass+, or 2 = Subwoofer' in man
print(f'PASS: all {len(commands)} parser commands, six GUI tabs, 24 lettered effect sections, and Bass Mono modes documented')
