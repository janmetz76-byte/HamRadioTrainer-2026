#!/usr/bin/env python3
"""Verteilt die Position der richtigen Antwort auf die vier Antwortplätze.

Der amtliche Fragenkatalog der Bundesnetzagentur stellt in jedem Blatt die
richtige Antwort an erster Stelle. Das ist im gedruckten Katalog in Ordnung,
weil die Pruefungsboegen die Antworten anschliessend mischen. In unserem
Werkzeug darf das Muster aber nicht stehen bleiben: wer die Katalogdatei
oeffnet oder eine Ansicht ohne Mischung sieht, kann die Pruefung durch blosses
Ankreuzen der obersten Zeile bestehen.

Dieses Skript bildet genau die Permutation nach, die tools/catalog-import.cpp
in spreadAnswers() anwendet, damit ein erneuter Import dieselbe Datei
erzeugt. Beide Stellen rechnen mit FNV-1a ueber die Kennung, gefolgt von einem
Fisher-Yates mit xorshift. QRandomGenerator und qHash sind bewusst nicht im
Spiel: deren Seed ist prozessabhaengig, und ein Katalog, der sich bei jedem
Import aendert, laesst sich gegen nichts pruefen.

Aufruf:  python3 tools/catalog-reshuffle-answers.py data/catalogs/*.xml
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

MASK = 0xFFFFFFFF


def spread_answers(question_id: str, count: int) -> list[int]:
    """Liefert die Zielpositionen, wie spreadAnswers() in catalog-import.cpp."""
    h = 2166136261
    for byte in question_id.encode("utf-8"):
        h ^= byte
        h = (h * 16777619) & MASK

    order = list(range(count))
    for i in range(count - 1, 0, -1):
        h ^= (h << 13) & MASK
        h &= MASK
        h ^= h >> 17
        h ^= (h << 5) & MASK
        h &= MASK
        j = h % (i + 1)
        order[i], order[j] = order[j], order[i]
    return order


ANSWER_RE = re.compile(r"\s*<answer\b[^>]*?/>")


def reshuffle(path: Path) -> tuple[int, dict[int, int]]:
    text = path.read_text(encoding="utf-8")
    changed = 0
    tally: dict[int, int] = {}

    def fix_question(match: re.Match[str]) -> str:
        nonlocal changed
        block = match.group(0)
        qid = re.search(r'<question id="([^"]+)"', block).group(1)
        answers = ANSWER_RE.findall(block)
        if not answers:
            return block
        # Die richtige Antwort merken, bevor sie ihre Position wechselt.
        correct_at = next(
            i for i, a in enumerate(answers) if 'correct="true"' in a
        )
        order = spread_answers(qid, len(answers))
        tally[order.index(correct_at) + 1] = tally.get(order.index(correct_at) + 1, 0) + 1

        rebuilt = []
        for position, source in enumerate(order):
            answer = answers[source]
            answer = re.sub(r'(id="[^"]+?)-[a-z](")', rf"\g<1>-{'abcd'[position]}\2", answer, count=1)
            answer = re.sub(r'(correct=")[a-z]+(")', r"\1true\2" if source == correct_at
                            else r"\1false\2", answer, count=1)
            answer = re.sub(r'(order=")[0-9]+(")', rf"\g<1>{position + 1}\2", answer, count=1)
            rebuilt.append(answer)
        if rebuilt != answers:
            changed += 1
        return ANSWER_RE.sub("", block, count=0) and re.sub(
            r"(<answers>).*?(</answers>)",
            lambda m: m.group(1) + "".join(rebuilt) + m.group(2),
            block,
            flags=re.S,
        )

    result = re.sub(r"<question\b.*?</question>", fix_question, text, flags=re.S)
    if result != text:
        path.write_text(result, encoding="utf-8")
    return changed, tally


def main(argv: list[str]) -> int:
    if len(argv) < 2:
        print(__doc__)
        return 2
    for name in argv[1:]:
        path = Path(name)
        changed, tally = reshuffle(path)
        spread = "  ".join(f"{k}:{v}" for k, v in sorted(tally.items()))
        print(f"{path.name}: {changed} Fragen umgestellt   Verteilung {spread}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))