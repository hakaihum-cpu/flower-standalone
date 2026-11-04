#!/usr/bin/env python3
"""Single MIYAKO FM-1 C dependency policy for preflight and post-patch.

Quoted strings and comments are never executable dependencies. The JUCE
header is caught separately, including angle-bracket includes.
"""
import re

_LEX = re.compile(
    r'(?P<quoted>"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\')'
    r'|(?P<comment>/\*[\s\S]*?\*/|//[^\n]*)'
)
_FORBIDDEN = (
    ('malloc', re.compile(r'\bmalloc\s*\(')),
    ('calloc', re.compile(r'\bcalloc\s*\(')),
    ('realloc', re.compile(r'\brealloc\s*\(')),
    ('free', re.compile(r'\bfree\s*\(')),
    ('new', re.compile(r'\bnew\b')),
    ('float', re.compile(r'\bfloat\b')),
    ('double', re.compile(r'\bdouble\b')),
)
_JUCE_INCLUDE = re.compile(r'(?m)^\s*#\s*include\s*[<"]\s*JuceHeader(?:\.h)?[>"]')


def _mask(segment):
    return ''.join('\n' if ch == '\n' else ' ' for ch in segment)


def code_only(source):
    """Mask comments and string/character literals, preserving line positions."""
    return _LEX.sub(lambda m: _mask(m.group(0)), source)


def _without_comments(source):
    return _LEX.sub(
        lambda m: m.group('quoted') if m.group('quoted') is not None else _mask(m.group(0)),
        source
    )


def forbidden_dependencies(source):
    code = code_only(source)
    matches = [name for name, pattern in _FORBIDDEN if pattern.search(code)]
    if _JUCE_INCLUDE.search(_without_comments(source)):
        matches.append('JuceHeader')
    return matches


def selftest():
    assert forbidden_dependencies('/* fade the new one */\n// new slice\nvoid ok(void) {}') == []
    assert forbidden_dependencies('char *s = "http://example.org/new";\nvoid ok(void) {}') == []
    assert forbidden_dependencies('void bad(void) { malloc (4); }') == ['malloc']
    assert forbidden_dependencies('void bad(void) { float x = 1; }') == ['float']
    assert forbidden_dependencies('#include "JuceHeader.h"') == ['JuceHeader']
    assert forbidden_dependencies('#include <JuceHeader.h>') == ['JuceHeader']
    assert forbidden_dependencies('/* #include "JuceHeader.h" */\nint x;') == []
    assert forbidden_dependencies('/* malloc(4) */\nint x;') == []
    assert forbidden_dependencies('void x() { new Item; }') == ['new']
    assert forbidden_dependencies('void x() { // new\n int y = 1; }') == []
    assert forbidden_dependencies('int x = 1; const char *s="realloc(4)";') == []


if __name__ == '__main__':
    selftest()
    print('PASS: native dependency scanner regression fixtures')
