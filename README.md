# SEED Lab « Pseudo Random Number Generation » pour CyberRangeCZ

Formation linéaire pour la [plateforme CyberRangeCZ](https://docs.platform.cyberrange.cz/), qui reproduit le [SEED Lab *Pseudo Random Number Generation*](https://seedsecuritylabs.org/Labs_20.04/) (Wenliang Du).

Ce dépôt reprend la structure technique du modèle [library-junior-hacker](https://github.com/cyberrangecz/library-junior-hacker) (topologie, provisioning Ansible, rôles officiels, `training.json`), avec un scénario et une architecture propres au TP PRNG. Chaque étudiant a **ses propres flags** : une valeur différente par sandbox, générée par la plateforme à partir de `variables.yml`.

## Niveaux

| # | Niveau | Flag | Points |
|-|-|-|-|
| 0 | Introduction, barème, règles | - | - |
| 1 | Accès à la sandbox | `start` | - |
| 2 | Tâche 1 : clé générée avec `srand(time(NULL))` | déchiffrer `task1/secret.enc` | 30 |
| 3 | Tâche 2 : retrouver la clé d'Alice par force brute | déchiffrer le PDF d'Alice | 60 |
| 4 | Tâche 3 : entropie du noyau | `seedlab check 3` | 30 |
| 5 | Tâche 4 : `/dev/random` et déni de service | `seedlab check 4` | 40 |
| 6 | Tâche 5 : `/dev/urandom`, `ent`, clé de 256 bits | `seedlab check 5` | 40 |
| 7 | Compte rendu à rendre | - | - |

**Barème : 200 points = 20/20.** Le backend n'accepte que des scores entiers, d'où l'échelle ×10. Chaque indice coûte de 5 à 20 points (-0,5 à -2 sur 20). Afficher une solution vaut 0 point pour le niveau.

## Topologie

| Hôte | Image | Flavor | Réseau | IP |
|-|-|-|-|-|
| seed | `seed` (Ubuntu 20.04 SEED, voir ci-dessous) | m1.medium | lab-switch 10.1.27.0/24 | 10.1.27.10 |
| router | debian-12-x86_64 | m1.medium | lab-switch / WAN | 10.1.27.1 |

Le TP n'utilise qu'une machine. Le routeur reste obligatoire : il est le seul accès à Internet de la sandbox (installation des paquets par `apt`) et la passerelle du réseau accessible par l'utilisateur.

**Prérequis de l'image `seed`** : Ubuntu avec `python3` et cloud-init, et un utilisateur de gestion (`mgmt_user`) disposant de `sudo` sans mot de passe, accessible par la clé SSH injectée par OpenStack. Une image Ubuntu standard (ex. `ubuntu-focal-x86_64`) convient aussi, car tout ce dont le TP a besoin est installé par Ansible. Avec un noyau 5.4 (Ubuntu 20.04), `/dev/random` bloque comme dans l'énoncé original.

## Fonctionnement anti-triche

| Tâche | Ce qui est propre à chaque sandbox | Pourquoi on ne peut pas tricher |
|-|-|-|
| 1, 2 | Dates de génération des clés, IV, fichiers chiffrés, flags | Le flag n'existe que chiffré. Les paramètres sont dérivés (SHA-256) du flag et d'un sel secret tiré sur la VM (`/root/.prng-lab-salt`, illisible pour l'étudiant). Il faut faire l'attaque sur **sa** sandbox. |
| 3, 4, 5 | Flags compilés dans `/usr/local/bin/seedlab` | Binaire `root:root` en mode `0711` : exécutable mais illisible pour `student` (sans sudo). Le validateur vérifie l'expérience **en direct** : entropie mesurée pendant la réponse, processus lisant `/dev/random`, valeur `ent` du fichier de l'étudiant, exécution de son `genkey`. |

En plus : le compte `seed`/`dees` des VM SEED reçoit un mot de passe aléatoire et la connexion graphique automatique est désactivée. Les commandes tapées sont journalisées (rôle `sandbox-logging`), et la plateforme enregistre toutes les réponses soumises, y compris les fausses.

## Structure

```
topology.yml                      topologie (1 VM + 1 routeur)
variables.yml                     5 flags aléatoires par sandbox (APG)
training.json                     définition de la formation (à importer)
game_design.md                    conception pédagogique
provisioning/
  playbook.yml                    hosts -> user-access + prng-lab -> sandbox-logging
  requirements.yml                rôles officiels CyberRangeCZ
  host_vars/seed.yml
  roles/hosts/                    paquets : gcc, libssl-dev, openssl, ent, hexdump, watch, éditeurs
  roles/prng-lab/
    files/lab/                    consignes + keygen.c (listing 1) + squelette genkey.c
    files/build_challenges.py     génère les défis personnalisés (tâches 1-2)
    files/seedlab.c               validateur des tâches 3-5
    tasks/main.yml                déploiement, auto-tests, durcissement anti-triche
```

## Déploiement (résumé)

1. Adapter `image` et `mgmt_user` de l'hôte `seed` dans `topology.yml` à votre instance.
2. Pousser ce dépôt sur GitHub, en dépôt public ou accessible par la plateforme.
3. **Sandbox Agenda > Sandbox Definition** : créer la définition (URL Git, révision `main`).
4. **Sandbox Agenda > Pool** : créer un pool (taille = nombre d'étudiants), puis allouer les sandboxes.
5. **Training Agenda > Training Definition** : importer `training.json`.
6. **Training Agenda > Training Instance** : créer l'instance, lui assigner le pool, puis distribuer le jeton d'accès.

## Licence et crédits

* Code (Ansible, scripts, validateur) : licence MIT, voir [LICENSE](LICENSE).
* Énoncé original : SEED Labs, © 2018 Wenliang Du, libre pour un usage éducatif non commercial. Voir [ATTRIBUTION.txt](ATTRIBUTION.txt).
* Structure technique inspirée de [cyberrangecz/library-junior-hacker](https://github.com/cyberrangecz/library-junior-hacker) (MIT / CC BY 4.0).
