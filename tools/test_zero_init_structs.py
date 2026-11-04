#!/usr/bin/env python3
"""Structures copied whole into hashed game state must be zeroed at creation.

An AI information packet, a unit speech item and a new-particle record are built
on the stack and filled field by field; padding and unused members would
otherwise keep whatever the stack held and make the objects/particle data differ
between two runs of the same input. Each local of these types must be cleared
(csmemset) or initialized by ai_communication_packet_new before its first use.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
DECL = re.compile(
    r'^[ \t]*struct (unit_speech_item|ai_information_packet|new_particle_data)\s+(\w+);[ \t]*$',
    re.M)
WINDOW = 1500  # characters after the declaration in which the clear must appear


def unzeroed_locals():
    bad = []
    for path in sorted((ROOT / 'source').rglob('*.c')):
        text = path.read_text(encoding='latin-1')
        for m in DECL.finditer(text):
            kind, name = m.groups()
            body = text[m.end():m.end() + WINDOW]
            clears = (rf'csmemset\(\s*&{name}\s*,\s*0\s*,',
                      rf'ai_communication_packet_new\(\s*&{name}\.ai\s*\)')
            first_use = re.search(rf'\b{name}\b', body)
            if not first_use:
                continue
            head = body[:first_use.end() + 80] if False else body
            if not any(re.search(p, head) for p in clears):
                line = text.count('\n', 0, m.start()) + 1
                bad.append(f'{path.relative_to(ROOT)}:{line} {kind} {name}')
            else:
                # the clear must come before the first assignment to a member
                assign = re.search(rf'\b{name}\.\w+', body)
                clear = min((re.search(p, body).start() for p in clears if re.search(p, body)))
                if assign and assign.start() < clear:
                    line = text.count('\n', 0, m.start()) + 1
                    bad.append(f'{path.relative_to(ROOT)}:{line} {kind} {name} cleared after first write')
    return bad


def test_stack_structs_are_zeroed_before_use():
    assert unzeroed_locals() == []


if __name__ == '__main__':
    problems = unzeroed_locals()
    print('\n'.join(problems) or 'ok')
    raise SystemExit(1 if problems else 0)
