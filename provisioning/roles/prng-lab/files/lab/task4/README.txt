Tâche 4 - Nombres pseudo-aléatoires depuis /dev/random
======================================================

Terminal 1 :  cat /dev/random | hexdump
Terminal 2 :  watch -n .1 cat /proc/sys/kernel/random/entropy_avail

1. Que se passe-t-il si vous ne touchez à rien ? Puis si vous bougez la souris / tapez ?
2. Notez la version du noyau (uname -r) : le comportement de /dev/random a changé dans
   Linux 5.6. Expliquez VOTRE observation.
3. Question : si un serveur utilise /dev/random pour générer les clés de session de ses
   clients, comment lancer une attaque par déni de service (DoS) contre lui ?

Validation : laissez « cat /dev/random | hexdump » tourner et lancez dans un autre terminal :
       seedlab check 4
