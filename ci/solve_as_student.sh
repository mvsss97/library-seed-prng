#!/usr/bin/env bash
# Résout les 5 tâches comme un étudiant (compte seed, sans droits d'administration),
# avec les commandes et le code des solutions de training.json (/tmp/training.json).
# Les flags obtenus vont dans ~/ci-results.txt ; seul OK / FAIL est affiché.
set -uo pipefail
export HOME=/home/seed LC_ALL=C.UTF-8
FAIL=0
RES="$HOME/ci-results.txt"
: > "$RES"
ok() { echo "  [OK]   $1"; }
ko() { echo "  [FAIL] $1"; FAIL=1; }
check() { if eval "$2"; then ok "$1"; else ko "$1"; fi; }
# refuse "message" "entrée" commande... : la commande doit échouer sans afficher de flag
refuse() {
  local msg="$1" input="$2"; shift 2
  if printf '%b' "$input" | "$@" > /tmp/refuse.out 2>&1 || grep -q "Flag à soumettre" /tmp/refuse.out; then
    ko "$msg"
  else
    ok "$msg"
  fi
}
solution_code() {   # extrait le bloc C de la solution du niveau « Tâche N » de training.json
  python3 - "$1" << 'EOF'
import json, re, sys
t = json.load(open("/tmp/training.json", encoding="utf-8"))
level = next(l for l in t["levels"] if l["title"].startswith("Tâche " + sys.argv[1]))
print(re.search(r"```c\n(.*?)\n```", level["solution"], re.S).group(1))
EOF
}
flag_from() { awk '/Flag à soumettre/ {print $NF}'; }

# ------------------------------------------------------------------ tâche 1
echo "Tâche 1 : srand(time(NULL))"
cd ~/lab/task1
gcc keygen.c -o keygen 2> /dev/null
K1=$(./keygen | tail -1); sleep 1.1; K2=$(./keygen | tail -1)
check "keygen.c (listing 1) : la clé change à chaque seconde" '[ -n "$K1" ] && [ "$K1" != "$K2" ]'
sed 's|srand (time(NULL));|// srand (time(NULL));|' keygen.c > keygen_sans_srand.c
gcc keygen_sans_srand.c -o keygen_sans_srand 2> /dev/null
check "srand commenté : toujours la même clé (graine 1)" \
  '[ "$(./keygen_sans_srand | tail -1)" = 67c6697351ff4aec29cdbaabf2fbe346 ]'
D=$(grep -m1 '\./keygen' keygen.log | awk '{print $1" "$2}')
EPOCH=$(date -u -d "$D" +%s)
IV=$(grep -o -- '-iv [0-9a-f]*' keygen.log | awk '{print $2}')
sed "s/srand (time(NULL));/srand ($EPOCH);/" keygen.c > keygen_fixed.c
gcc keygen_fixed.c -o keygen_fixed 2> /dev/null
KEY=$(./keygen_fixed | tail -1)
F1=$(openssl enc -d -aes-128-cbc -K "$KEY" -iv "$IV" -in secret.enc 2> /dev/null | awk '/^Flag de la/ {print $NF}')
echo "F1=$F1" >> "$RES"
check "clé reconstruite avec date -u et srand(EPOCH) : secret.enc déchiffré" '[ -n "$F1" ]'

# ------------------------------------------------------------------ tâche 2
echo "Tâche 2 : force brute"
cd ~/lab/task2
solution_code 2 > brute.c
check "brute.c (solution de training.json) compile avec -lcrypto" 'gcc brute.c -o brute -lcrypto 2> /dev/null'
SEEDKEY=$(./brute 1524013729 1524020929 255044462d312e350a25d0d4c5d80a34 \
  d06bf9d0dab8e8ef880660d2af65aa82 09080706050403020100A2B2C2D2E2F2 | awk '/^Cl/ {print $NF}')
check "brute.c retrouve la clé de l'énoncé SEED officiel (95fa2030...)" '[ "$SEEDKEY" = 95fa2030e73ed3f8da761b4eb805dfd7 ]'
D=$(sed -n 's/^Date de création *: \(.*\) (UTC)$/\1/p' bob_notes.txt)
END=$(date -u -d "$D" +%s); START=$((END - 7200))
PT=$(awk '/Plaintext/ {print $3}' bob_notes.txt)
CT=$(awk '/Ciphertext/ {print $3}' bob_notes.txt)
IV=$(awk '$1 == "IV" {print $3}' bob_notes.txt)
T0=$(date +%s%N)
KEY=$(./brute "$START" "$END" "$PT" "$CT" "$IV" | awk '/^Cl/ {print $NF}')
echo "         force brute sur 7 201 graines : $(( ($(date +%s%N) - T0) / 1000000 )) ms"
openssl enc -d -aes-128-cbc -K "$KEY" -iv "$IV" -in alice_tax_return.pdf.enc -out alice.pdf 2> /dev/null
F2=$(grep -a -o 'Flag de la tache 2 : [A-Za-z0-9]*' alice.pdf | awk '{print $NF}')
echo "F2=$F2" >> "$RES"
check "PDF d'Alice déchiffré (en-tête %PDF-1.5)" '[ "$(head -c 8 alice.pdf)" = "%PDF-1.5" ]'
check "PDF lisible par un vrai lecteur (pdftotext)" 'pdftotext alice.pdf - 2> /dev/null | grep -q "Flag de la tache 2"'

# ------------------------------------------------------------------ tâche 3
echo "Tâche 3 : entropie"
P=$(cat /proc/sys/kernel/random/poolsize)
refuse "valeur d'entropie inventée refusée" "999999\n$P\n" seedlab check 3
V=$(cat /proc/sys/kernel/random/entropy_avail)
refuse "taille de pool fausse refusée" "$V\n12345\n" seedlab check 3
V=$(cat /proc/sys/kernel/random/entropy_avail)
F3=$(printf '%s\n%s\n' "$V" "$P" | seedlab check 3 2>&1 | flag_from)
echo "F3=$F3" >> "$RES"
check "seedlab check 3 : flag obtenu avec les vraies valeurs" '[ -n "$F3" ]'

# ------------------------------------------------------------------ tâche 4
echo "Tâche 4 : /dev/random"
refuse "refusé si aucun processus ne lit /dev/random" "256\n/dev/urandom\n" seedlab check 4
(cat /dev/random | hexdump > /dev/null) &
sleep 1
V=$(cat /proc/sys/kernel/random/entropy_avail)
refuse "mauvaise contre-mesure (/dev/random) refusée" "$V\n/dev/random\n" seedlab check 4
V=$(cat /proc/sys/kernel/random/entropy_avail)
F4=$(printf '%s\n/dev/urandom\n' "$V" | seedlab check 4 2>&1 | flag_from)
pkill -u seed -x cat; pkill -u seed -x hexdump
echo "F4=$F4" >> "$RES"
check "seedlab check 4 : flag obtenu pendant cat /dev/random | hexdump" '[ -n "$F4" ]'

# ------------------------------------------------------------------ tâche 5
echo "Tâche 5 : /dev/urandom"
cd ~/lab/task5
refuse "refusé sans output.bin" "8\n" seedlab check 5
head -c 1M /dev/urandom > output.bin
E=$(ent output.bin | awk '/^Entropy/ {print $3}')
check "ent : entropie de output.bin > 7.99 bits/octet ($E)" 'awk "BEGIN {exit !($E > 7.99)}"'
gcc genkey.c -o genkey 2> /dev/null
refuse "squelette genkey.c non modifié (128 bits, pas d'affichage) refusé" "$E\n" seedlab check 5
solution_code 5 > genkey.c
gcc genkey.c -o genkey 2> /dev/null
refuse "valeur ent inventée refusée" "7.5\n" seedlab check 5
F5=$(printf '%s\n' "$E" | seedlab check 5 2>&1 | flag_from)
echo "F5=$F5" >> "$RES"
check "seedlab check 5 : flag obtenu avec genkey 256 bits (solution)" '[ -n "$F5" ]'

exit $FAIL
