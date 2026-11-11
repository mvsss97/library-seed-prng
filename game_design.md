# SEED Lab : Pseudo Random Number Generation

Les clés de chiffrement sont généralement produites par le logiciel lui-même. Beaucoup de développeurs réutilisent pour cela les méthodes qu'ils emploient en simulation (`srand(time(NULL))` + `rand()`) : la suite obtenue convient à une simulation Monte-Carlo, mais elle est prévisible, donc dangereuse pour une clé. Ce laboratoire montre l'erreur, l'exploite, puis présente la bonne méthode (`/dev/urandom`).

Source : *SEED Labs - Pseudo Random Number Generation Lab*, © 2018 Wenliang Du.

## Principes de conception

* **Fidélité à l'énoncé** : les 5 tâches, le code (listing 1, extrait `/dev/urandom`), le scénario Alice / Bob et le bloc clair connu (`%PDF-1.5…`) sont repris.
* **Un flag unique par étudiant** : chaque flag est une variable APG (`variables.yml`). Les éléments personnalisés (dates, IV, fichiers chiffrés) sont dérivés du flag et d'un sel secret propre à la VM, si bien que le flag ne s'obtient qu'en faisant l'expérience sur sa propre sandbox.
* **Observation vérifiée en direct** : les tâches 3 à 5 n'ont pas de « bonne valeur » fixe. Le validateur `seedlab` compare la réponse de l'étudiant à ce qu'il mesure lui-même au même moment.
* **Compte rendu** : les questions ouvertes de l'énoncé (observations, explications, DoS, analyse de `ent`) sont évaluées dans le compte rendu, pas par la plateforme.

## Barème (200 points = 20/20)

| Niveau | Points | Indices (pénalité) |
|-|-|-|
| Tâche 1 | 30 | 5, 10, 10 |
| Tâche 2 | 60 | 5, 10, 15, 20 |
| Tâche 3 | 30 | 5, 5, 10 |
| Tâche 4 | 40 | 5, 10, 15 |
| Tâche 5 | 40 | 5, 10, 10, 5 |

Pénalité de 5 à 20 points = -0,5 à -2 sur 20. Solution affichée = 0 point pour le niveau.

## Niveau 1 : générer une clé de la mauvaise façon

**Objectifs** : comprendre le rôle de `srand()` et de `time()`, et voir qu'une clé dérivée de l'heure est reconstructible.

**Énoncé** : exécuter `keygen.c` avec et sans `srand(time(NULL))`. Ensuite, l'historique `keygen.log` d'un serveur donne la seconde exacte (UTC) de génération d'une clé et l'IV : il faut reconstruire la clé avec `date -u -d … +%s` et `srand(EPOCH)`, puis déchiffrer `secret.enc` avec `openssl`.

**Flag** : `flag_task1`, dans le texte déchiffré. **Durée** : 15 min.

## Niveau 2 : deviner la clé

**Objectifs** : mener une attaque par force brute sur l'espace des graines à partir d'un couple clair/chiffré connu (AES-128-CBC).

**Énoncé** : scénario de l'énoncé, avec une date de création, un IV et un chiffré propres à la sandbox. La clé a été générée entre 60 s et 7 140 s avant la création du fichier, donc dans la fenêtre de 2 h. L'étudiant écrit un programme (C + OpenSSL EVP) qui teste les 7 201 graines, puis déchiffre le PDF.

**Flag** : `flag_task2`, écrit dans le PDF déchiffré. **Durée** : 35 min.

## Niveau 3 : mesurer l'entropie du noyau

**Objectifs** : connaître les sources d'entropie de Linux et savoir lire `/proc/sys/kernel/random/`.

**Énoncé** : observer `entropy_avail` avec `watch` pendant diverses activités. `seedlab check 3` demande la valeur affichée par `watch` (comparée à l'entropie que le validateur échantillonne toutes les 50 ms pendant la réponse, ±64 bits) et `poolsize`.

**Flag** : `flag_task3`. **Durée** : 10 min.

## Niveau 4 : `/dev/random`

**Objectifs** : observer le caractère bloquant de `/dev/random` et en déduire un déni de service.

**Énoncé** : `cat /dev/random | hexdump` + `watch`. `seedlab check 4` exige qu'un processus de l'étudiant ait `/dev/random` ouvert, demande l'entropie observée pendant l'expérience, puis le périphérique à utiliser à la place (`/dev/urandom`). Le comportement dépend du noyau (< 5.6 : bloquant), et l'étudiant l'explique dans le compte rendu.

**Flag** : `flag_task4`. **Durée** : 12 min.

## Niveau 5 : `/dev/urandom`

**Objectifs** : évaluer un générateur avec `ent`, et générer correctement une clé de 256 bits.

**Énoncé** : `head -c 1M /dev/urandom > output.bin ; ent output.bin`, puis modifier `genkey.c` (LEN 32, affichage hexadécimal). `seedlab check 5` vérifie la taille et l'entropie de `output.bin`, compare la valeur *Entropy* donnée par l'étudiant à son propre calcul (±0,0001), puis exécute deux fois `genkey` : il attend 64 caractères hexadécimaux, différents d'une exécution à l'autre, et un source qui lit `/dev/urandom`.

**Flag** : `flag_task5`. **Durée** : 20 min.
