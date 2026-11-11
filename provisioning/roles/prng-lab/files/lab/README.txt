SEED Labs - Pseudo Random Number Generation Lab (version CyberRangeCZ)
======================================================================

Ce répertoire contient tout ce qu'il faut pour les 5 tâches de l'énoncé SEED.
Les fichiers des tâches 1 et 2 sont PERSONNALISÉS : vos dates, vos clés et vos
flags sont différents de ceux de vos camarades. Recopier le flag d'un autre
étudiant ne fonctionne pas (et la plateforme enregistre les réponses fausses).

  task1/   Générer une clé de la mauvaise façon (srand(time(NULL)))
           -> le flag est dans task1/secret.enc, à déchiffrer
  task2/   Deviner la clé d'Alice par force brute
           -> le flag est dans le PDF déchiffré
  task3/   Mesurer l'entropie du noyau          -> seedlab check 3
  task4/   /dev/random (bloquant ?)             -> seedlab check 4
  task5/   /dev/urandom, ent, clé de 256 bits   -> seedlab check 5

Outils installés : gcc, make, openssl (+ libssl-dev), ent, hexdump, watch,
nano, vim.

Conseils
  - Travaillez avec DEUX terminaux (ou deux connexions SSH) : les tâches 3 et 4
    demandent d'observer l'entropie pendant qu'une autre commande tourne.
  - Les heures des fichiers sont en UTC : utilisez « date -u ».
  - Notez vos observations et faites des captures d'écran au fur et à mesure :
    elles sont nécessaires pour le compte rendu.
