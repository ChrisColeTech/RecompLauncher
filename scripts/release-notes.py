#!/usr/bin/env python3
# RecompLauncher - native game library and build launcher.
"""Write release notes from the commits since the previous release: launcher commits, then the engine commits the
submodule pin moved across. Needs full history (actions/checkout fetch-depth: 0, which also unshallows submodules).

Usage: release-notes.py [--previous TAG] [--head REV] > notes.md"""
import argparse
import subprocess
import sys

ENGINE = 'third_party/Switch2RECOMP'
ENGINE_URL = 'https://github.com/ChrisColeTech/Switch2RECOMP-turbo'


def git(*args, cwd='.'):
    return subprocess.run(['git', *args], cwd=cwd, check=True, capture_output=True, text=True).stdout.strip()


def previous_tag(head):
    try: return git('describe', '--tags', '--abbrev=0', '--match', 'launcher-*', head + '^')
    except subprocess.CalledProcessError: return None   # first release: everything


def commits(rev_range, cwd='.'):
    """[(sha, subject, body)] oldest first; Co-Authored-By and similar trailers dropped."""
    out = git('log', '--reverse', '--format=%h%x1f%s%x1f%b%x1e', rev_range, cwd=cwd)
    result = []
    for record in filter(None, (r.strip() for r in out.split('\x1e'))):
        sha, subject, body = (record.split('\x1f') + ['', ''])[:3]
        lines = [l for l in body.strip().splitlines() if not l.lower().startswith(('co-authored-by:', 'signed-off-by:'))]
        result.append((sha, subject.strip(), '\n'.join(lines).strip()))
    return result


def pin(rev):
    """The engine commit the submodule is pinned to at rev, or None."""
    try: return git('rev-parse', '%s:%s' % (rev, ENGINE))
    except subprocess.CalledProcessError: return None


def section(title, entries, link=None):
    out = ['## ' + title, '']
    if not entries: return out + ['No changes.', '']
    for sha, subject, body in entries:
        ref = '[`%s`](%s/commit/%s)' % (sha, link, sha) if link else '`%s`' % sha
        out.append('- **%s** (%s)' % (subject, ref))
        if body: out += ['  ' + l if l else '' for l in body.splitlines()]
    return out + ['']


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--previous', help='previous release tag (default: nearest launcher-* tag before --head)')
    ap.add_argument('--head', default='HEAD')
    args = ap.parse_args()
    prev = args.previous or previous_tag(args.head)
    launcher = commits('%s..%s' % (prev, args.head) if prev else args.head)
    old, new = (pin(prev) if prev else None), pin(args.head)
    if new and old != new:
        engine = commits('%s..%s' % (old, new) if old else new, cwd=ENGINE)
    else:
        engine = []
    lines = ['Changes since %s.' % ('`%s`' % prev if prev else 'the start of the project'), '']
    lines += section('Launcher', launcher)
    lines += section('Engine (Switch2RECOMP-turbo `turbo`)', engine, ENGINE_URL)
    if new: lines += ['Engine pinned at [`%s`](%s/tree/%s).' % (new[:7], ENGINE_URL, new)]
    sys.stdout.write('\n'.join(lines).rstrip() + '\n')


if __name__ == '__main__': main()
