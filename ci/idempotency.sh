#!/usr/bin/env bash
# Re-provisioning (ex. redémarrage de l'étape « user » par la plateforme) : les défis
# doivent rester identiques et le travail de l'étudiant ne doit pas être écrasé.
set -uo pipefail
FAIL=0
R() { docker exec seed bash -c "$1"; }
FILES="/home/seed/lab/task1/secret.enc /home/seed/lab/task1/keygen.log \
/home/seed/lab/task2/alice_tax_return.pdf.enc /home/seed/lab/task2/bob_notes.txt /root/.prng-lab-salt"

R "sha256sum $FILES" > before.txt
R "sha256sum /home/seed/lab/task5/genkey.c" > genkey-before.txt
bash ci/provision.sh provision-2.log || exit 1
R "sha256sum $FILES" > after.txt
R "sha256sum /home/seed/lab/task5/genkey.c" > genkey-after.txt

if diff -q before.txt after.txt > /dev/null; then echo "  [OK]   défis identiques après re-provisioning (sel réutilisé)"
else echo "  [FAIL] défis modifiés par le re-provisioning"; FAIL=1; fi
if diff -q genkey-before.txt genkey-after.txt > /dev/null; then echo "  [OK]   travail de l'étudiant (genkey.c modifié) conservé"
else echo "  [FAIL] genkey.c de l'étudiant écrasé"; FAIL=1; fi
if ! grep -qF -f flags.txt provision-2.log; then echo "  [OK]   sortie du 2e provisioning sans flag"
else echo "  [FAIL] flag visible dans la sortie du 2e provisioning"; FAIL=1; fi
exit $FAIL
