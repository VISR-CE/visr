#!/usr/bin/env python3
"""tvOS's text dialog must be reaped when the system removes it with no handler run (host_tv.m)."""
import pathlib, re, sys

src = pathlib.Path(__file__).resolve().parents[1].joinpath('port/ios/host/host_tv.m').read_text()
up = re.search(r'static bool text_prompt_up\(void\) \{(.*?)\n\}', src, re.S)
reap = re.search(r'static void text_prompt_reap\(void\) \{(.*?)\n\}', src, re.S)
assert up and 'text_prompt_reap()' in up.group(1), 'text_prompt_up must reap a removed dialog first'
assert reap, 'text_prompt_reap missing'
body = reap.group(1)
assert 'text_prompt_seen' in body and 'text_prompt=nil' in body and 'text_prompt_state=2' in body
assert 'text_prompt_seen=false;\n    UIAlertController' in src, 'a new dialog must reset text_prompt_seen'
print('tvos prompt reap: ok')
sys.exit(0)
