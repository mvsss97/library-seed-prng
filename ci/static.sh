#!/usr/bin/env bash
# Contrôles statiques : compilation stricte du validateur et syntaxe Ansible.
set -euo pipefail
tmp=$(mktemp -d)
cp provisioning/roles/prng-lab/files/seedlab.c "$tmp/"
python3 - "$tmp/flags.h" << 'EOF'
import sys
out = "static const unsigned char SEEDLAB_XK[] = {%s};\n" % ", ".join(str(b) for b in range(1, 33))
for n in (3, 4, 5):
    out += "static const unsigned char SEEDLAB_F%d[] = {65, 66, 67};\n#define SEEDLAB_F%d_LEN 3\n" % (n, n)
open(sys.argv[1], "w").write(out)
EOF
gcc -O2 -Wall -Wextra -Werror -o "$tmp/seedlab" "$tmp/seedlab.c" -lcrypto -lm
echo "  [OK]   seedlab.c compile sans aucun avertissement (gcc -Wall -Wextra -Werror)"
python3 -m py_compile provisioning/roles/prng-lab/files/build_challenges.py
echo "  [OK]   build_challenges.py : syntaxe Python valide"
cd provisioning
ansible-galaxy install -r requirements.yml -p roles > /dev/null
ansible-playbook playbook.yml -i ../ci/inventory.yml --syntax-check > /dev/null
echo "  [OK]   playbook.yml : syntaxe Ansible valide (avec les rôles de requirements.yml)"
