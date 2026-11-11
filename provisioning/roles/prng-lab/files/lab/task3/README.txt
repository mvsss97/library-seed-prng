Tâche 3 - Mesurer l'entropie du noyau
=====================================

1. Affichez l'entropie disponible :
       cat /proc/sys/kernel/random/entropy_avail
2. Surveillez-la en continu (toutes les 0,1 s) :
       watch -n .1 cat /proc/sys/kernel/random/entropy_avail
3. Pendant que watch tourne : bougez/cliquez la souris (console graphique), tapez au
   clavier, lisez beaucoup de fichiers (ex. cat /usr/bin/* > /dev/null 2>&1),
   restez inactif... Quelles activités augmentent fortement l'entropie ?
4. Regardez aussi les autres fichiers de /proc/sys/kernel/random/ (poolsize, ...).

Validation : laissez watch tourner dans un terminal et lancez dans un autre :
       seedlab check 3
