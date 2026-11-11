Tâche 1 - Générer une clé de chiffrement de la mauvaise façon
============================================================

Fichiers
  keygen.c     programme de l'énoncé SEED (listing 1) : clé de 128 bits avec srand(time(NULL))
  keygen.log   extrait de l'historique root du serveur backup-01 (heures en UTC)
  secret.enc   fichier chiffré avec la clé produite par keygen au moment indiqué dans le log

Partie A (observation, pour le compte rendu)
  1. gcc keygen.c -o keygen ; lancez ./keygen plusieurs fois. Que constatez-vous ?
  2. Commentez la ligne (1) « srand (time(NULL)); », recompilez, relancez plusieurs fois.
  3. Expliquez le rôle de srand() et de time().

Partie B (flag)
  L'administrateur de backup-01 a utilisé keygen pour chiffrer secret.txt.
  keygen.log indique la seconde exacte à laquelle la clé a été générée, ainsi que l'IV.
  Reconstruisez la clé, puis déchiffrez secret.enc avec openssl (aes-128-cbc).
  Le flag est dans le texte déchiffré.
