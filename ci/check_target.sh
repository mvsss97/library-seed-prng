#!/usr/bin/env bash
# Vérifications après provisioning : permissions, compte étudiant seed / dees sans droits
# d'administration, compte de gestion intact, absence de flag en clair.
set -uo pipefail
FAIL=0
ok() { echo "  [OK]   $1"; }
ko() { echo "  [FAIL] $1"; FAIL=1; }
check() { if eval "$2"; then ok "$1"; else ko "$1"; fi; }
R() { docker exec seed bash -c "$1"; }
S() { docker exec -u seed seed bash -c "$1"; }

echo "Fichiers et permissions"
check "seedlab : root:root, mode 0711" '[ "$(R "stat -c \"%U:%G %a\" /usr/local/bin/seedlab")" = "root:root 711" ]'
check "sel secret : root, mode 0600" '[ "$(R "stat -c \"%U %a\" /root/.prng-lab-salt")" = "root 600" ]'
check "répertoire de construction supprimé" 'R "test ! -e /root/.prng-build"'
for f in README.txt task1/keygen.c task1/keygen.log task1/secret.enc task2/alice_tax_return.pdf.enc \
         task2/bob_notes.txt task3/README.txt task4/README.txt task5/genkey.c; do
  check "~/lab/$f appartient à seed" '[ "$(R "stat -c %U /home/seed/lab/$f")" = seed ]'
done
check "date du fichier chiffré = date de création des notes de Bob" \
  'R "cd /home/seed/lab/task2 && [ \"\$(date -u -r alice_tax_return.pdf.enc \"+%Y-%m-%d %H:%M:%S\")\" = \"\$(sed -n \"s/^Date de création *: \\(.*\\) (UTC)\$/\\1/p\" bob_notes.txt)\" ]"'
check "outils : gcc, openssl, ent, hexdump, watch, en-têtes OpenSSL" \
  'R "command -v gcc openssl ent hexdump watch > /dev/null && test -f /usr/include/openssl/evp.h"'

echo "Compte étudiant seed / dees"
check "mot de passe de seed = dees" \
  'R "python3 -W ignore -c \"import crypt, spwd, sys; h = spwd.getspnam(\\\"seed\\\").sp_pwdp; sys.exit(0 if crypt.crypt(\\\"dees\\\", h) == h else 1)\""'
check "seed n'a plus aucun droit sudo" 'R "LC_ALL=C sudo -l -U seed" | grep -q "is not allowed to run sudo"'
check "seed retiré des groupes sudo, adm, docker, lxd" '! R "id -nG seed" | grep -Eqw "sudo|adm|docker|lxd"'
check "session ouverte avant le provisioning fermée" '! R "pgrep -u seed -x sleep > /dev/null"'
check "compte de gestion ubuntu : sudo intact" 'R "LC_ALL=C sudo -l -U ubuntu" | grep -q "NOPASSWD: ALL"'
check "seed ne peut pas lire seedlab" '! S "cat /usr/local/bin/seedlab > /dev/null 2>&1"'
check "seed ne peut pas copier seedlab" '! S "cp /usr/local/bin/seedlab /tmp/copie 2> /dev/null"'
check "seed peut exécuter seedlab" 'S "seedlab > /dev/null; [ \$? -eq 2 ]"'
check "seed ne peut pas lire le sel" '! S "cat /root/.prng-lab-salt > /dev/null 2>&1"'
check "seed : sudo refusé même avec le mot de passe dees" '! S "echo dees | sudo -S -k true 2> /dev/null"'
check "auto-test du validateur refusé à seed" '! S "seedlab --selftest > /dev/null 2>&1"'

echo "Aucun flag en clair"
docker cp flags.txt seed:/root/ci-flags.txt
check "aucun fichier de la VM ne contient un flag en clair" \
  '! R "grep -rlaF -f /root/ci-flags.txt / --exclude-dir=proc --exclude-dir=sys --exclude-dir=dev --exclude-dir=run --exclude=ci-flags.txt 2> /dev/null | grep -q ."'
check "journal système (sudo, syslog) sans flag" '! R "journalctl --no-pager 2> /dev/null | grep -qF -f /root/ci-flags.txt"'
check "sortie du provisioning Ansible (-vv) sans flag" '! grep -qF -f flags.txt provision-1.log'
R "rm -f /root/ci-flags.txt"

exit $FAIL
