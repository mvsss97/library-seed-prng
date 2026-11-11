#!/usr/bin/env python3
"""Génère les réponses de la sandbox comme backend-ansible-runner (manage_answers.py) :
même bibliothèque APG, même graine (pool_id + allocation_unit_id) * 31.

Usage : generate_answers.py variables.yml POOL_ID ALLOCATION_UNIT_ID answers.json flags.txt
"""
import json
import sys

from generator.var_generator import generate
from generator.var_parser import parser_var_file

variables_path, pool_id, allocation_unit_id, answers_path, flags_path = sys.argv[1:6]
with open(variables_path, encoding="utf-8") as f:
    variables = parser_var_file(f)
answers = generate(variables, (int(pool_id) + int(allocation_unit_id)) * 31)

with open(answers_path, "w", encoding="utf-8") as f:
    json.dump(answers, f)
with open(flags_path, "w", encoding="utf-8") as f:
    f.write("\n".join(answers.values()) + "\n")

for value in answers.values():          # masque les flags dans les journaux GitHub Actions
    print(f"::add-mask::{value}")
print(f"{len(answers)} réponses générées : {', '.join(sorted(answers))}")
