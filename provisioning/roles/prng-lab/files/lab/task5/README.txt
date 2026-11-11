Tâche 5 - Nombres aléatoires depuis /dev/urandom
================================================

Partie A
  1. cat /dev/urandom | hexdump      (Ctrl+C pour arrêter) : bouger la souris change-t-il quelque chose ?
  2. Générez 1 Mio et mesurez sa qualité :
         cd ~/lab/task5
         head -c 1M /dev/urandom > output.bin
         ent output.bin
     Analysez chaque ligne du résultat de ent (entropie, chi-deux, moyenne, Monte-Carlo, corrélation).

Partie B
  genkey.c contient l'extrait de code de l'énoncé (clé de 128 bits lue dans /dev/urandom).
  Modifiez-le pour générer une clé de 256 bits et l'afficher en hexadécimal
  (64 caractères, comme keygen.c), puis :
         gcc genkey.c -o genkey
         ./genkey

Validation (output.bin et genkey doivent être dans ~/lab/task5) :
         seedlab check 5
