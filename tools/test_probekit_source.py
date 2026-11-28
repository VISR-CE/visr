#!/usr/bin/env python3
"""tools/probekit/source.py: finding production source by name, on fixture C and Objective-C text."""
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent))
from probekit import source

C = r'''/* present_frame() is described here; a { in a comment does not count */
#define LOOP(x) for (;;) { x }
static int helper(int value);

static int helper(int value)
{
	const char *text = "{ not a brace }";
	char brace = '}';
	return value + 1;
}

// wrapped(void) { in a line comment }
int present_frame(int value)
{
	return helper(value) + present_inner(value);
}

static int present_inner(int value)
{
	if (value)
		return deep(value);
	return 0;
}

static int deep(int value)
{
	return value * 2;
}

void annotated(void) API_AVAILABLE(ios(16.0), visionos(1.0))
{
	first();
	second();
}

__attribute__((unused)) static int attributed(void)
{
	return 1;
}

API_AVAILABLE(ios(26.0)) static void finish(void)
{
	attributed();
}

#if PLATFORM_A
static int variant(void)
{
	return 1; /* a */
}
#else
static int variant(void)
{
	return 2; /* b */
}
#endif

static int uses_variant(void)
{
	/* was: retired(); */
	return variant() + attributed();
}

struct pair {
	struct { int a; } inner;
	int b;
};

/*
int commented_out(void)
{
	return 0;
}
*/
'''

OBJC = '''@implementation Thing
- (void)drawFrame:(int)frame withScale:(float)scale {
    [self other];
}
- (void)drawFrame:(int)frame {
    [self single];
}
+ (instancetype)shared {
    return nil;
}
@end
'''


def test_a_function_is_found_by_name_with_braces_in_strings_and_comments():
    text = source.function_in(C, 'helper')
    assert text.startswith('static int helper(int value)\n{') and text.endswith('return value + 1;\n}')
    assert source.body_in(C, 'helper').startswith('\tconst char *text') and source.body_in(C, 'helper').endswith('+ 1;')


def test_annotations_between_signature_and_body_are_skipped():
    assert source.function_in(C, 'annotated').startswith('void annotated(void) API_AVAILABLE')


def test_prototypes_comments_and_macros_are_not_definitions():
    names = [name for name, *_ in source.definitions(C)]
    assert names == ['helper', 'present_frame', 'present_inner', 'deep', 'annotated', 'attributed', 'finish',
                     'variant', 'variant', 'uses_variant']
    assert source.defined_in(C, 'helper') and not source.defined_in(C, 'commented_out')


def test_a_missing_name_says_renamed_wrapped_or_split_with_the_closest_names():
    with pytest.raises(LookupError, match='renamed, wrapped or split') as failure:
        source.function_in(C, 'present_frames', 'port/x.c')
    assert '`present_frame`' in str(failure.value) and 'port/x.c' in str(failure.value)


def test_objective_c_methods_are_found_by_their_first_selector_part():
    assert source.function_in(OBJC, 'drawFrame:withScale:').startswith('- (void)drawFrame:(int)frame withScale')
    assert '[self single]' in source.function_in(OBJC, 'drawFrame:')
    with pytest.raises(LookupError, match='defined 2 times'):
        source.function_in(OBJC, 'drawFrame')
    assert source.body_in(OBJC, 'shared').strip() == 'return nil;'


def test_reach_follows_same_file_calls_so_a_wrap_keeps_a_wiring_test_green():
    reached = source.reach_in(C, 'present_frame')
    assert list(reached) == ['present_frame', 'helper', 'present_inner', 'deep']
    assert any('value * 2' in body for body in reached.values())
    assert list(source.reach_in(C, 'present_frame', depth=1)) == ['present_frame', 'helper', 'present_inner']
    wrapped = C.replace('int present_frame(int value)\n{\n\treturn helper(value) + present_inner(value);\n}',
                        'int present_frame(int value)\n{\n\treturn present_frame_inner(value);\n}\n\n'
                        'static int present_frame_inner(int value)\n{\n\treturn helper(value) + present_inner(value);\n}')
    assert any('present_inner(value)' in body for body in source.reach_in(wrapped, 'present_frame').values())


def test_calls_in_order_within_one_body():
    body = source.body_in(C, 'annotated')
    assert source.calls_in_order(body, 'first();', 'second();')
    assert not source.calls_in_order(body, 'second();', 'first();')
    assert not source.calls_in_order(body, 'first();', 'third();')


def test_structure_takes_nested_braces_through_the_semicolon():
    assert source.structure_in(C, 'pair').endswith('int b;\n};')
    with pytest.raises(LookupError, match='renamed, wrapped or split'):
        source.structure_in(C, 'pairs')


def test_region_markers_and_files_outside_port_are_refused(tmp_path):
    text = 'a\n/* probe-region: setup */\nint x;\n/* probe-region-end: setup */\nb\n'
    assert source.region_in(text, 'setup') == '\nint x;\n'
    with pytest.raises(LookupError):
        source.region_in(text, 'other')
    with pytest.raises(ValueError, match='only in files under port/'):
        source.region('source/main/main.c', 'setup')


def test_mutating_applies_one_substitution_to_every_helper_and_restores_it(tmp_path):
    path = tmp_path / 'fixture.c'
    path.write_text(C, encoding='latin-1')
    original = source.read(path)
    name = 'helper'
    before = source.function(path, name)
    with source.mutating(path, before, before + '\n/* mutated */') as text:
        assert '/* mutated */' in text and '/* mutated */' in source.read(path)
    assert source.read(path) == original
    with pytest.raises(LookupError, match='no longer applies'):
        source.mutated(original, 'text that is not there', 'x')


def test_a_leading_attribute_on_the_same_line_does_not_hide_a_definition():
    assert source.body_in(C, 'attributed').strip() == 'return 1;'
    assert source.body_in(C, 'finish').strip() == 'attributed();'


def test_a_name_defined_twice_needs_which_and_reach_takes_every_variant():
    with pytest.raises(LookupError, match=r'defined 2 times in the text \(lines \d+, \d+'):
        source.function_in(C, 'variant')
    assert '/* b */' in source.function_in(C, 'variant', which='return 2;')
    assert '/* a */' in source.function_in(C, 'variant', which=0)
    with pytest.raises(LookupError, match='contain'):
        source.function_in(C, 'variant', which='return')
    reached = source.reach_in(C, 'uses_variant')
    assert 'return 1;' in reached['variant'] and 'return 2;' in reached['variant']
    assert list(reached) == ['uses_variant', 'variant', 'attributed']


def test_reach_and_call_order_ignore_comments_but_keep_strings():
    assert not any('retired();' in text for text in source.reach_in(C, 'uses_variant').values())
    assert any('{ not a brace }' in text for text in source.reach_in(C, 'helper').values())
    assert not source.calls_in_order('/* second(); */ first(); second_();', 'second();')
    assert source.calls_in_order('first("x;"); second();', 'first("x;");', 'second();')


def test_assert_reaches_names_the_functions_it_searched(tmp_path, monkeypatch):
    path = tmp_path / 'x.c'
    path.write_text(C)
    source.assert_reaches(path, 'uses_variant', 'return 2;')
    with pytest.raises(AssertionError, match='searched: uses_variant, variant, attributed'):
        source.assert_reaches(path, 'uses_variant', 'retired();')


def test_unbalanced_braces_name_the_file():
    with pytest.raises(LookupError, match='port/odd.c has unbalanced braces'):
        source.function_in('int f(void)\n{\n#if A\n{\n#endif\n', 'f', 'port/odd.c')
    with pytest.raises(LookupError, match='port/odd.h has unbalanced braces'):
        source.structure_in('struct pair {\n#if A\n{\n#endif\n', 'pair', 'port/odd.h')


SPAN = r'''#define SLOTS 4
/* a #define SLOTS in a comment is not the macro */
static short shifts[SLOTS];

static short first_slot(void)
{
	return shifts[0];
}

static short last_slot(void)
{
	return shifts[SLOTS - 1];
}

static void after(void) { }
'''


def test_span_runs_from_a_macro_or_function_through_a_function_with_what_lies_between():
    text = source.span_in(SPAN, 'SLOTS', 'last_slot')
    assert text.startswith('#define SLOTS 4\n') and 'static short shifts[SLOTS];' in text
    assert text.endswith('return shifts[SLOTS - 1];\n}') and 'after' not in text
    assert source.span_in(SPAN, 'first_slot', 'last_slot').startswith('static short first_slot(void)')


def test_span_names_what_is_gone_or_out_of_order():
    with pytest.raises(LookupError, match='renamed, wrapped or split.*`last_slot`'):
        source.span_in(SPAN, 'SLOTS', 'last_slots')
    with pytest.raises(LookupError, match='renamed, wrapped or split'):
        source.span_in(SPAN, 'SLOT', 'last_slot')
    with pytest.raises(LookupError, match='comes before'):
        source.span_in(SPAN, 'last_slot', 'first_slot')
    with pytest.raises(LookupError, match='#defined 2 times'):
        source.span_in(SPAN + '#define SLOTS 5\n', 'SLOTS', 'last_slot')
