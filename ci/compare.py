#!/usr/bin/env python3
"""Compare les flags obtenus par l'étudiant simulé aux réponses attendues par la plateforme
(sans jamais les afficher). Usage : compare.py answers.json results.txt"""
import json
import sys

answers = json.load(open(sys.argv[1], encoding="utf-8"))
found = {}
for line in open(sys.argv[2], encoding="utf-8"):
    if "=" in line:
        key, value = line.strip().split("=", 1)
        found[key] = value

fail = False
for n in range(1, 6):
    ok = found.get(f"F{n}") == answers[f"flag_task{n}"]
    fail |= not ok
    print(f"  [{'OK' if ok else 'FAIL'}]{'   ' if ok else ' '}tâche {n} : flag obtenu = réponse attendue par CyberRangeCZ")
distinct = len(set(answers.values())) == len(answers)
fail |= not distinct
print(f"  [{'OK' if distinct else 'FAIL'}]{'   ' if distinct else ' '}les 5 flags de la sandbox sont différents")
sys.exit(1 if fail else 0)
