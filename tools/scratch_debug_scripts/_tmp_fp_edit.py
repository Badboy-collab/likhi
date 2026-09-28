# -*- coding: utf-8 -*-
"""Targeted line replacements in front-page.php.

Bengali text in the source mixes precomposed and decomposed vowel signs, so
matching on Bengali literals is unreliable. We therefore anchor on unique
ASCII substrings and replace whole lines by index.
"""
import io, sys

PATH = r'E:\Pervez\PC Bangla Typing App\website\wordpress-theme\likhi\front-page.php'

with io.open(PATH, encoding='utf-8', newline='') as fh:
    lines = fh.read().split('\n')

changes = []


def find(marker, start=0):
    """Return index of the single line containing marker."""
    hits = [i for i, l in enumerate(lines) if marker in l]
    if len(hits) != 1:
        raise SystemExit('marker %r matched %d lines' % (marker, len(hits)))
    return hits[0]


def setline(idx, new):
    if lines[idx] == new:
        return
    changes.append((idx + 1, lines[idx].strip()[:60], new.strip()[:60]))
    lines[idx] = new


# 1. hero proof line
i = find('\u26a1 &lt;4ms startup')
setline(i, lines[i].replace('100% Offline', 'Works Offline'))

# 2. Benefit 8 heading + description
i = find('<!-- Benefit 8: 100% Offline Capability -->')
setline(i, '          <!-- Benefit 8: Offline-First Core -->')
setline(i + 5, '                8. Offline-First Core')
setline(i + 8, '              <div class="benefit-desc">\u0987\u099e\u09cd\u099c\u09bf\u09a8, \u09eb\u09e8,\u09e6\u09e6\u09e6+ \u09b6\u09ac\u09cd\u09a6\u09c7\u09b0 \u09a1\u09bf\u0995\u09b6\u09a8\u09be\u09b0\u09bf, \u09ac\u09cd\u09af\u0995\u09cd\u09a4\u09bf\u0997\u09a4 \u0985\u09ad\u09bf\u09a7\u09be\u09a8 \u0993 \u09b0\u09cd\u09af\u09be\u0999\u09cd\u0995\u09bf\u0982\u2014\u09b8\u09ac\u0987 \u0986\u09aa\u09a8\u09be\u09b0 \u09aa\u09bf\u09b8\u09bf\u09a4\u09c7\u0964 \u0987\u09a8\u09cd\u099f\u09be\u09b0\u09a8\u09c7\u099f \u09b8\u0982\u09af\u09cb\u0997 \u099b\u09be\u09a1\u09bc\u09be\u0987 \u099f\u09be\u0987\u09aa\u09bf\u0982 \u09b8\u09ae\u09cd\u09aa\u09c2\u09b0\u09cd\u09a3 \u0995\u09be\u099c \u0995\u09b0\u09c7\u0964 \u0987\u09a8\u09cd\u099f\u09be\u09b0\u09a8\u09c7\u099f \u09a5\u09be\u0995\u09b2\u09c7 \u0990\u099a\u09cd\u099b\u09bf\u0995 \u0985\u09a8\u09b2\u09be\u0987\u09a8 \u09b8\u09be\u099c\u09c7\u09b6\u09a8 \u09af\u09cb\u0997 \u09b9\u09a4\u09c7 \u09aa\u09be\u09b0\u09c7, \u09af\u09be Settings \u09a5\u09c7\u0995\u09c7 \u09af\u09c7\u0995\u09cb\u09a8\u09cb \u09b8\u09ae\u09df \u09ac\u09a8\u09cd\u09a7 \u0995\u09b0\u09be \u09af\u09be\u09df\u0964</div>')

# 3. Benefit 10 privacy description
i = find('Zero telemetry, zero keystroke logging, zero cloud tracking.')
setline(i, '              <div class="benefit-desc">Zero telemetry, zero keystroke logging, zero tracking. \u0986\u09aa\u09a8\u09be\u09b0 \u099f\u09be\u0987\u09aa \u0995\u09b0\u09be \u099f\u09c7\u0995\u09cd\u09b8\u099f\u09c7\u09b0 \u0987\u09a4\u09bf\u09b9\u09be\u09b8 \u0995\u09cb\u09a5\u09be\u0993 \u09b8\u0982\u09b0\u0995\u09cd\u09b7\u09a3 \u09ac\u09be \u09aa\u09be\u09a0\u09be\u09a8\u09cb \u09b9\u09df \u09a8\u09be\u2014\u0990\u099a\u09cd\u099b\u09bf\u0995 \u0985\u09a8\u09b2\u09be\u0987\u09a8 \u09b8\u09be\u099c\u09c7\u09b6\u09a8 \u09b6\u09c1\u09a7\u09c1 \u099f\u09be\u0987\u09aa \u0995\u09b0\u09be \u09b0\u09cb\u09ae\u09be\u09a8 \u09b6\u09ac\u09cd\u09a6\u099f\u09bf \u09b2\u09c1\u0995\u0986\u09aa \u0995\u09b0\u09c7\u0964</div>')

# 4. Brain diagram intro paragraph (line after the <p ...> tag)
i = find('<p style="color:#94A3B8;font-size:0.95rem;max-width:680px')
setline(i + 1, '              \u09ac\u09b0\u09cd\u09a4\u09ae\u09be\u09a8 \u09b0\u09bf\u09b2\u09bf\u099c\u09c7\u09b0 \u09ae\u09c2\u09b2 \u0987\u099e\u09cd\u099c\u09bf\u09a8, \u0985\u09ad\u09bf\u09a7\u09be\u09a8 \u0993 \u09b0\u09cd\u09af\u09be\u0999\u09cd\u0995\u09bf\u0982 \u09b8\u09ae\u09cd\u09aa\u09c2\u09b0\u09cd\u09a3 \u09b2\u09cb\u0995\u09be\u09b2\u2014\u0987\u09a8\u09cd\u099f\u09be\u09b0\u09a8\u09c7\u099f \u099b\u09be\u09a1\u09bc\u09be\u0987 \u099a\u09b2\u09c7\u0964 \u0990\u099a\u09cd\u099b\u09bf\u0995 \u0985\u09a8\u09b2\u09be\u0987\u09a8 \u09b8\u09be\u099c\u09c7\u09b6\u09a8\u09c7 \u09b6\u09c1\u09a7\u09c1 \u099f\u09be\u0987\u09aa \u0995\u09b0\u09be \u09b0\u09cb\u09ae\u09be\u09a8 \u09b6\u09ac\u09cd\u09a6\u099f\u09bf \u09b2\u09c1\u0995\u0986\u09aa \u0995\u09b0\u09be \u09b9\u09df, \u098f\u09ac\u0982 \u09ad\u09ac\u09bf\u09b7\u09cd\u09af\u09a4\u09c7 \u098f\u0987 \u09ac\u09b0\u09cd\u09a7\u09bf\u09a4 \u09ac\u09c1\u09a6\u09cd\u09a7\u09bf\u09ae\u09a4\u09cd\u09a4\u09be \u0986\u09b0\u0993 \u09b8\u09ae\u09cd\u09aa\u09cd\u09b0\u09b8\u09be\u09b0\u09bf\u09a4 \u09b9\u09ac\u09c7\u2014\u09b8\u09ac\u0987 \u09ac\u09cd\u09af\u09ac\u09b9\u09be\u09b0\u0995\u09be\u09b0\u09c0\u09b0 \u09a8\u09bf\u09df\u09a8\u09cd\u09a4\u09cd\u09b0\u09a3\u09c7\u0964</p>')

# 5. "CURRENT RELEASE" column title + its 4th bullet
i = find('CURRENT RELEASE (100% Offline Core)')
setline(i, lines[i].replace('CURRENT RELEASE (100% Offline Core)', 'CURRENT RELEASE (Offline-First Core)'))
setline(i + 6, '                <li>\u2713 \u09ae\u09c2\u09b2 \u099f\u09be\u0987\u09aa\u09bf\u0982 \u09b8\u09ae\u09cd\u09aa\u09c2\u09b0\u09cd\u09a3 \u0985\u09ab\u09b2\u09be\u0987\u09a8; \u0990\u099a\u09cd\u099b\u09bf\u0995 \u0985\u09a8\u09b2\u09be\u0987\u09a8 \u09b8\u09be\u099c\u09c7\u09b6\u09a8 \u0986\u09b2\u09be\u09a6\u09be \u0995\u09b0\u09c7 \u09ac\u09a8\u09cd\u09a7 \u0995\u09b0\u09be \u09af\u09be\u09df</li>')

# 6. privacy pledge line in the brain diagram footer
i = find('\U0001f6e1\ufe0f <strong>')
setline(i, '            \U0001f6e1\ufe0f <strong>\u09aa\u09cd\u09b0\u09be\u0987\u09ad\u09c7\u09b8\u09bf \u0985\u0999\u09cd\u0997\u09c0\u0995\u09be\u09b0:</strong> \u0995\u09cb\u09a8\u09cb telemetry \u09ac\u09be keystroke logging \u09a8\u09c7\u0987\u2014\u0986\u09aa\u09a8\u09be\u09b0 \u09b6\u09ac\u09cd\u09a6\u09ad\u09be\u09a8\u09cd\u09a1\u09be\u09b0, \u099f\u09be\u0987\u09aa\u09bf\u0982 \u0987\u09a4\u09bf\u09b9\u09be\u09b8 \u0993 \u09b8\u09c7\u099f\u09bf\u0982\u09b8 \u09b6\u09c1\u09a7\u09c1 \u0986\u09aa\u09a8\u09be\u09b0 \u09aa\u09bf\u09b8\u09bf\u09a4\u09c7\u0987 \u09a5\u09be\u0995\u09c7\u0964')

# 7. trust strip item
i = find('aria-hidden="true">\U0001f512</span>')
setline(i + 1, '            Works Offline')

# 8. CTA footnote
i = find('Apache 2.0 License \u00b7 100% Offline')
setline(i, lines[i].replace('100% Offline', 'Works Offline'))

with io.open(PATH, 'w', encoding='utf-8', newline='') as fh:
    fh.write('\n'.join(lines))

print('lines changed: %d' % len(changes))
for ln, before, after in changes:
    print('  L%-5d %s  ==>  %s' % (ln, before, after))
