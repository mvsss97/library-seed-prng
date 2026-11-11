#!/usr/bin/env bash
# Provisioning exactement comme backend-ansible-runner :
#   ansible-galaxy install -r requirements.yml -p roles
#   ansible-playbook playbook.yml -i <inventaire> -e @answers.json -vv
set -euo pipefail
LOG="$PWD/$1"
cd provisioning
ansible-galaxy install -r requirements.yml -p roles > /dev/null
if ! ansible-playbook playbook.yml -i ../ci/inventory.yml -e @../answers.json -vv > "$LOG" 2>&1; then
  cat "$LOG"
  exit 1
fi
grep -A 3 "^PLAY RECAP" "$LOG"
