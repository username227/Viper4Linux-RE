#!/usr/bin/env python3
"""Prevent CLI commands from being added without matching manual entries."""
import pathlib
import re
repo = pathlib.Path(__file__).resolve().parents[1]
cli = (repo/'src/viper-ctl.cpp').read_text()
man = (repo/'man/viper4linux-ctl.1').read_text()
commands = set(re.findall(r'\bcmd\s*==\s*"([A-Za-z0-9-]+)"', cli))
# Status, legacy names, all effect toggles and parameter names must be listed.
missing = sorted(c for c in commands if not re.search(r'(?<![\w-])'+re.escape(c)+r'(?![\w-])',man))
assert len(commands)>=85, f'CLI parser coverage dropped unexpectedly ({len(commands)})'
assert not missing, f'Undocumented CLI commands: {missing}'
assert '--help' in man and 'round' in man and 'inclusive' in man
print(f'PASS: all {len(commands)} parser commands documented in CLI man page')
