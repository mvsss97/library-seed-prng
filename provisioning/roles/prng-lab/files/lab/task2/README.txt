Tâche 2 - Deviner la clé
========================

Le 17 avril 2018 (dans votre sandbox, la date exacte est différente), Alice a chiffré sa
déclaration de revenus (un PDF) avec une clé produite par le programme de la tâche 1.
Bob a copié le fichier chiffré et a pris des notes.

Fichiers
  alice_tax_return.pdf.enc   le PDF chiffré en aes-128-cbc (date de modification = date de création)
  bob_notes.txt              date de création, IV, premier bloc clair et premier bloc chiffré

Travail demandé
  1. Calculez la fenêtre de recherche : les 2 heures qui précèdent la création du fichier
     (date -u -d "AAAA-MM-JJ HH:MM:SS" +%s).
  2. Écrivez un programme qui essaie toutes les graines de la fenêtre :
     srand(t), génération de la clé comme keygen.c, chiffrement AES-128-CBC du bloc clair
     connu avec l'IV, comparaison avec le bloc chiffré connu.
     (C + OpenSSL : gcc brute.c -o brute -lcrypto)
  3. Avec la clé trouvée, déchiffrez TOUT le fichier :
     openssl enc -d -aes-128-cbc -K <clé> -iv <iv> -in alice_tax_return.pdf.enc -out alice.pdf
  4. Le flag est écrit dans le PDF (strings alice.pdf, ou ouvrez-le avec un lecteur PDF).
