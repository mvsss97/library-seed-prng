/*
 * seedlab - validateur du SEED Lab « Pseudo Random Number Generation » (CyberRangeCZ)
 *
 * Tâches 3, 4 et 5 : ces tâches sont des expériences d'observation. Le validateur
 * vérifie EN DIRECT, sur la sandbox de l'étudiant, qu'il a réellement fait l'expérience,
 * puis affiche le flag propre à cette sandbox.
 *
 * Ce fichier est compilé pendant le provisioning avec « flags.h » (généré par
 * build_challenges.py à partir des variables APG, puis supprimé). Le binaire est
 * installé root:root en mode 0711 : l'étudiant (sans sudo) peut l'exécuter mais ni le
 * lire, ni le copier, ni l'inspecter avec un débogueur.
 *
 * Usage : seedlab check 3|4|5
 */
#define _GNU_SOURCE
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <openssl/evp.h>

#include "flags.h"
/* flags.h définit : SEEDLAB_XK[32], SEEDLAB_F3[], SEEDLAB_F4[], SEEDLAB_F5[]
 * et SEEDLAB_F3_LEN, SEEDLAB_F4_LEN, SEEDLAB_F5_LEN (flags XOR-és avec SEEDLAB_XK). */

#define ENTROPY_AVAIL "/proc/sys/kernel/random/entropy_avail"
#define POOLSIZE "/proc/sys/kernel/random/poolsize"
#define INPUT_TIMEOUT 300   /* secondes avant abandon d'une question */
#define ENTROPY_TOLERANCE 64 /* bits de marge pour une valeur lue sur watch */
#define TASK5_SIZE 1048576L /* head -c 1M = 1 Mio */

/* ------------------------------------------------------------------ utilitaires */

static void deobfuscate(const unsigned char *obf, size_t len, char *out)
{
    for (size_t i = 0; i < len; i++)
        out[i] = (char)(obf[i] ^ SEEDLAB_XK[i % sizeof SEEDLAB_XK]);
    out[len] = '\0';
}

/* Décrit chaque caractère pour éviter les confusions l/I/1 et O/0 en recopiant. */
static void print_spelling(const char *flag)
{
    printf("      Lecture  : ");
    for (const char *p = flag; *p; p++) {
        if (isdigit((unsigned char)*p))
            printf("%c(chiffre) ", *p);
        else if (isupper((unsigned char)*p))
            printf("%c(MAJ) ", *p);
        else
            printf("%c(min) ", *p);
    }
    printf("\n");
}

static int reveal(int task, const unsigned char *obf, size_t len)
{
    char flag[128];
    if (len >= sizeof flag)
        return 1;
    deobfuscate(obf, len, flag);
    printf("\n  ============================================================\n");
    printf("   Tâche %d validée !\n", task);
    printf("   Flag à soumettre dans CyberRangeCZ : %s\n", flag);
    print_spelling(flag);
    printf("  ============================================================\n\n");
    memset(flag, 0, sizeof flag);
    return 0;
}

static int fail(const char *msg)
{
    printf("\n  [ÉCHEC] %s\n\n", msg);
    return 1;
}

static int read_long(const char *path, long *value)
{
    FILE *f = fopen(path, "r");
    if (!f)
        return -1;
    int ok = (fscanf(f, "%ld", value) == 1);
    fclose(f);
    return ok ? 0 : -1;
}

static void trim(char *s)
{
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1]))
        s[--len] = '\0';
    size_t start = 0;
    while (s[start] && isspace((unsigned char)s[start]))
        start++;
    if (start > 0)
        memmove(s, s + start, len - start + 1);
}

static int parse_long(const char *s, long *out)
{
    char *end;
    errno = 0;
    long v = strtol(s, &end, 10);
    if (errno || end == s || *end != '\0')
        return -1;
    *out = v;
    return 0;
}

struct sampler {
    int enabled;
    long min;
    long max;
};

static void sample_entropy(struct sampler *s)
{
    long v;
    if (!s || !s->enabled)
        return;
    if (read_long(ENTROPY_AVAIL, &v) == 0) {
        if (v < s->min)
            s->min = v;
        if (v > s->max)
            s->max = v;
    }
}

/*
 * Pose une question et lit une ligne au clavier. Si « s » est fourni, l'entropie du
 * noyau est échantillonnée toutes les 50 ms pendant que l'étudiant répond : la valeur
 * qu'il recopie depuis watch doit tomber dans l'intervalle observé.
 */
static int ask(const char *question, char *buf, size_t size, struct sampler *s)
{
    size_t len = 0;
    time_t start = time(NULL);

    if (s) {
        s->min = LONG_MAX;
        s->max = LONG_MIN;
    }
    printf("%s", question);
    fflush(stdout);

    for (;;) {
        sample_entropy(s);
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(STDIN_FILENO, &rfds);
        struct timeval tv = {0, 50000};
        int r = select(STDIN_FILENO + 1, &rfds, NULL, NULL, &tv);
        if (r < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (r > 0) {
            char c;
            ssize_t k = read(STDIN_FILENO, &c, 1);
            if (k <= 0)
                return -1;
            if (c == '\n')
                break;
            if (len + 1 < size)
                buf[len++] = c;
        }
        if (time(NULL) - start > INPUT_TIMEOUT)
            return -1;
    }
    buf[len] = '\0';
    sample_entropy(s);
    trim(buf);
    return 0;
}

static const char *home_dir(void)
{
    struct passwd *pw = getpwuid(getuid());
    if (pw && pw->pw_dir)
        return pw->pw_dir;
    const char *h = getenv("HOME");
    return h ? h : "/tmp";
}

static void print_kernel_info(void)
{
    struct utsname u;
    long e = -1, p = -1;
    read_long(ENTROPY_AVAIL, &e);
    read_long(POOLSIZE, &p);
    if (uname(&u) == 0)
        printf("  Noyau Linux : %s\n", u.release);
    printf("  Entropie disponible actuellement : %ld bits (taille du pool : %ld bits)\n", e, p);
}

/* ------------------------------------------------------------------ tâche 3 */

static int task3(void)
{
    char buf[64];
    long value, poolsize;
    struct sampler s = {1, 0, 0};

    printf("\n=== Tâche 3 : mesurer l'entropie du noyau ===\n\n");
    printf("  Avant de répondre, lancez dans UN AUTRE terminal :\n");
    printf("      watch -n .1 cat /proc/sys/kernel/random/entropy_avail\n\n");

    if (ask("  [1/2] Valeur affichée par watch en ce moment (en bits) : ", buf, sizeof buf, &s) < 0)
        return fail("Pas de réponse.");
    if (parse_long(buf, &value) < 0)
        return fail("Un nombre entier est attendu.");
    if (value < s.min - ENTROPY_TOLERANCE || value > s.max + ENTROPY_TOLERANCE)
        return fail("Cette valeur ne correspond pas à l'entropie mesurée sur votre machine "
                    "pendant votre réponse. Lisez la valeur ACTUELLE affichée par watch.");

    if (read_long(POOLSIZE, &poolsize) < 0)
        return fail("Impossible de lire " POOLSIZE ".");
    if (ask("  [2/2] Taille maximale du pool d'entropie en bits (fichier poolsize du même répertoire) : ",
            buf, sizeof buf, NULL) < 0)
        return fail("Pas de réponse.");
    if (parse_long(buf, &value) < 0 || value != poolsize)
        return fail("Valeur incorrecte : lisez /proc/sys/kernel/random/poolsize.");

    return reveal(3, SEEDLAB_F3, SEEDLAB_F3_LEN);
}

/* ------------------------------------------------------------------ tâche 4 */

/* Cherche un processus de l'utilisateur courant qui a /dev/random ouvert. */
static int find_random_reader(pid_t *who, char *comm, size_t comm_size)
{
    DIR *proc = opendir("/proc");
    struct dirent *e;
    uid_t me = getuid();
    pid_t self = getpid();
    char path[PATH_MAX], link[600], target[PATH_MAX];

    if (!proc)
        return 0;
    while ((e = readdir(proc)) != NULL) {
        if (!isdigit((unsigned char)e->d_name[0]))
            continue;
        pid_t pid = (pid_t)atol(e->d_name);
        if (pid == self)
            continue;
        struct stat st;
        snprintf(path, sizeof path, "/proc/%s", e->d_name);
        if (stat(path, &st) != 0 || st.st_uid != me)
            continue;
        snprintf(path, sizeof path, "/proc/%s/fd", e->d_name);
        DIR *fds = opendir(path);
        if (!fds)
            continue;
        struct dirent *f;
        int found = 0;
        while ((f = readdir(fds)) != NULL) {
            if (f->d_name[0] == '.')
                continue;
            snprintf(link, sizeof link, "/proc/%s/fd/%s", e->d_name, f->d_name);
            ssize_t k = readlink(link, target, sizeof target - 1);
            if (k < 0)
                continue;
            target[k] = '\0';
            if (strcmp(target, "/dev/random") == 0) {
                found = 1;
                break;
            }
        }
        closedir(fds);
        if (found) {
            *who = pid;
            snprintf(path, sizeof path, "/proc/%s/comm", e->d_name);
            FILE *c = fopen(path, "r");
            comm[0] = '\0';
            if (c) {
                if (fgets(comm, (int)comm_size, c))
                    comm[strcspn(comm, "\n")] = '\0';
                fclose(c);
            }
            closedir(proc);
            return 1;
        }
    }
    closedir(proc);
    return 0;
}

static int task4(void)
{
    char buf[128], comm[64];
    long value;
    pid_t pid;
    struct sampler s = {1, 0, 0};

    printf("\n=== Tâche 4 : nombres pseudo-aléatoires depuis /dev/random ===\n\n");

    if (!find_random_reader(&pid, comm, sizeof comm))
        return fail("Aucun de vos processus ne lit /dev/random.\n"
                    "           Dans un autre terminal, lancez et LAISSEZ TOURNER :\n"
                    "               cat /dev/random | hexdump\n"
                    "           puis relancez : seedlab check 4");
    printf("  Lecteur de /dev/random détecté : processus %ld (%s)\n", (long)pid, comm);
    print_kernel_info();
    printf("\n  Rappel : avant Linux 5.6, /dev/random bloque quand l'entropie estimée est épuisée.\n"
           "  Depuis Linux 5.6, il ne bloque plus qu'au démarrage (avant l'initialisation du CRNG).\n"
           "  Expliquez dans votre compte rendu ce que VOUS avez observé.\n\n");

    if (ask("  [1/2] Pendant que cat /dev/random tourne, quelle valeur affiche watch ? ", buf,
            sizeof buf, &s) < 0)
        return fail("Pas de réponse.");
    if (parse_long(buf, &value) < 0)
        return fail("Un nombre entier est attendu.");
    if (value < s.min - ENTROPY_TOLERANCE || value > s.max + ENTROPY_TOLERANCE)
        return fail("Cette valeur ne correspond pas à l'entropie mesurée sur votre machine. "
                    "Surveillez l'entropie avec watch PENDANT que cat /dev/random tourne.");

    if (ask("  [2/2] Un serveur qui tire ses clés de session de /dev/random peut subir un DoS.\n"
            "        Quel fichier spécial (chemin complet) doit-il utiliser à la place ? ",
            buf, sizeof buf, NULL) < 0)
        return fail("Pas de réponse.");
    for (char *p = buf; *p; p++)
        *p = (char)tolower((unsigned char)*p);
    if (strcmp(buf, "/dev/urandom") != 0)
        return fail("Réponse incorrecte (chemin complet attendu, ex. /dev/xxxx).");

    return reveal(4, SEEDLAB_F4, SEEDLAB_F4_LEN);
}

/* ------------------------------------------------------------------ tâche 5 */

/* Entropie de Shannon en bits par octet : même formule que l'outil ent. */
static int file_entropy(const char *path, long *size, double *entropy)
{
    unsigned char block[65536];
    unsigned long long counts[256] = {0};
    long total = 0;
    size_t k;
    FILE *f = fopen(path, "rb");

    if (!f)
        return -1;
    while ((k = fread(block, 1, sizeof block, f)) > 0) {
        for (size_t i = 0; i < k; i++)
            counts[block[i]]++;
        total += (long)k;
    }
    fclose(f);
    double h = 0.0;
    for (int i = 0; i < 256; i++) {
        if (counts[i]) {
            double p = (double)counts[i] / (double)total;
            h -= p * log2(p);
        }
    }
    *size = total;
    *entropy = h;
    return 0;
}

/* Exécute le programme de l'étudiant (5 s max) et capture sa sortie standard. */
static int run_capture(const char *prog, char *out, size_t size)
{
    int p[2];
    if (pipe(p) < 0)
        return -1;
    pid_t pid = fork();
    if (pid < 0)
        return -1;
    if (pid == 0) {
        int devnull = open("/dev/null", O_RDWR);
        if (devnull >= 0)
            dup2(devnull, STDIN_FILENO);
        dup2(p[1], STDOUT_FILENO);
        close(p[0]);
        close(p[1]);
        alarm(5);
        execl(prog, prog, (char *)NULL);
        _exit(127);
    }
    close(p[1]);
    size_t len = 0;
    ssize_t k;
    while (len + 1 < size && (k = read(p[0], out + len, size - 1 - len)) > 0)
        len += (size_t)k;
    out[len] = '\0';
    close(p[0]);
    int status;
    if (waitpid(pid, &status, 0) < 0)
        return -1;
    if (WIFEXITED(status) && WEXITSTATUS(status) == 127)
        return -1;
    if (WIFSIGNALED(status))
        return -2;
    return 0;
}

/*
 * Extrait une clé de 256 bits affichée en hexadécimal : soit une suite d'exactement
 * 64 chiffres hexadécimaux, soit une ligne ne contenant que 64 chiffres hexadécimaux
 * séparés par des espaces, « : » ou « - ».
 */
static int extract_key256(const char *s, char key[65])
{
    size_t run = 0;
    for (const char *p = s;; p++) {
        if (*p && isxdigit((unsigned char)*p)) {
            run++;
            continue;
        }
        if (run == 64) {
            for (size_t i = 0; i < 64; i++)
                key[i] = (char)tolower((unsigned char)(p - 64)[i]);
            key[64] = '\0';
            return 1;
        }
        run = 0;
        if (!*p)
            break;
    }
    const char *line = s;
    while (*line) {
        size_t n = 0;
        int ok = 1;
        const char *p = line;
        for (; *p && *p != '\n'; p++) {
            if (isxdigit((unsigned char)*p)) {
                if (n < 64)
                    key[n] = (char)tolower((unsigned char)*p);
                n++;
            } else if (!(*p == ' ' || *p == '\t' || *p == ':' || *p == '-' || *p == '\r')) {
                ok = 0;
            }
        }
        if (ok && n == 64) {
            key[64] = '\0';
            return 1;
        }
        line = *p ? p + 1 : p;
    }
    return 0;
}

static int file_contains(const char *path, const char *needle)
{
    char buf[65536];
    FILE *f = fopen(path, "r");
    if (!f)
        return -1;
    size_t k = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[k] = '\0';
    return strstr(buf, needle) != NULL;
}

static int task5(void)
{
    char bin[PATH_MAX], src[PATH_MAX], prog[PATH_MAX], buf[64], msg[PATH_MAX + 512];
    char out1[4096], out2[4096], key1[65], key2[65];
    long size;
    double h, given;
    const char *home = home_dir();

    snprintf(bin, sizeof bin, "%s/lab/task5/output.bin", home);
    snprintf(src, sizeof src, "%s/lab/task5/genkey.c", home);
    snprintf(prog, sizeof prog, "%s/lab/task5/genkey", home);

    printf("\n=== Tâche 5 : nombres aléatoires depuis /dev/urandom ===\n\n");

    /* Partie A : 1 Mio issu de /dev/urandom, analysé avec ent */
    if (file_entropy(bin, &size, &h) < 0) {
        snprintf(msg, sizeof msg, "Fichier introuvable : %s\n"
                 "           Créez-le avec : head -c 1M /dev/urandom > ~/lab/task5/output.bin", bin);
        return fail(msg);
    }
    if (size != TASK5_SIZE) {
        snprintf(msg, sizeof msg, "output.bin fait %ld octets ; 1 Mio (%ld octets) est attendu "
                 "(head -c 1M).", size, TASK5_SIZE);
        return fail(msg);
    }
    if (h < 7.99)
        return fail("output.bin ne ressemble pas à des données aléatoires : "
                    "générez-le à partir de /dev/urandom.");
    printf("  output.bin : 1 Mio trouvé.\n");
    if (ask("  [1/2] Valeur « Entropy = ... bits per byte » donnée par ent pour output.bin : ",
            buf, sizeof buf, NULL) < 0)
        return fail("Pas de réponse.");
    for (char *p = buf; *p; p++)
        if (*p == ',')
            *p = '.';
    char *end;
    given = strtod(buf, &end);
    if (end == buf || *end != '\0')
        return fail("Un nombre décimal est attendu (ex. 7.999812).");
    if (fabs(given - h) > 0.0001)
        return fail("Valeur différente de celle de votre fichier : lancez « ent output.bin » "
                    "et recopiez au moins 4 décimales.");

    /* Partie B : clé de 256 bits lue dans /dev/urandom */
    printf("\n  [2/2] Vérification de votre programme %s\n", prog);
    int has = file_contains(src, "/dev/urandom");
    if (has < 0) {
        snprintf(msg, sizeof msg, "Code source introuvable : %s", src);
        return fail(msg);
    }
    if (!has)
        return fail("genkey.c doit lire la clé dans /dev/urandom.");
    if (access(prog, X_OK) != 0)
        return fail("Programme ~/lab/task5/genkey introuvable : compilez avec\n"
                    "               gcc genkey.c -o genkey   (dans ~/lab/task5)");
    if (run_capture(prog, out1, sizeof out1) != 0 || run_capture(prog, out2, sizeof out2) != 0)
        return fail("genkey ne s'exécute pas correctement (plantage ou plus de 5 secondes).");
    if (!extract_key256(out1, key1) || !extract_key256(out2, key2))
        return fail("genkey doit afficher la clé de 256 bits en hexadécimal : 64 caractères "
                    "(32 octets affichés avec %.2x).");
    if (strcmp(key1, key2) == 0)
        return fail("Deux exécutions de genkey ont donné la même clé : elle n'est pas aléatoire.");
    printf("  Exécution 1 : %s\n  Exécution 2 : %s\n", key1, key2);
    printf("  Deux clés de 256 bits différentes : OK\n");

    return reveal(5, SEEDLAB_F5, SEEDLAB_F5_LEN);
}

/* ------------------------------------------------------------------ auto-test */

/* Utilisé par Ansible (root) pour vérifier que les bons flags ont été compilés.
 * N'affiche que des empreintes SHA-256, jamais les flags. */
static int selftest(void)
{
    const unsigned char *obf[3] = {SEEDLAB_F3, SEEDLAB_F4, SEEDLAB_F5};
    size_t len[3] = {SEEDLAB_F3_LEN, SEEDLAB_F4_LEN, SEEDLAB_F5_LEN};

    if (getuid() != 0) {
        fprintf(stderr, "seedlab: --selftest est réservé à root\n");
        return 1;
    }
    for (int i = 0; i < 3; i++) {
        char flag[128];
        unsigned char md[EVP_MAX_MD_SIZE];
        unsigned int mdlen = 0;
        if (len[i] >= sizeof flag)
            return 1;
        deobfuscate(obf[i], len[i], flag);
        if (!EVP_Digest(flag, len[i], md, &mdlen, EVP_sha256(), NULL))
            return 1;
        for (unsigned int j = 0; j < mdlen; j++)
            printf("%02x", md[j]);
        printf("\n");
        memset(flag, 0, sizeof flag);
    }
    return 0;
}

/* ------------------------------------------------------------------ main */

static void usage(void)
{
    printf("Validateur du SEED Lab « Pseudo Random Number Generation »\n\n"
           "  seedlab check 3   tâche 3 : entropie du noyau\n"
           "  seedlab check 4   tâche 4 : /dev/random\n"
           "  seedlab check 5   tâche 5 : /dev/urandom\n\n"
           "Tâches 1 et 2 : le flag se trouve dans le fichier que vous déchiffrez.\n"
           "Consignes : ~/lab/README.txt\n");
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc == 2 && strcmp(argv[1], "--selftest") == 0)
        return selftest();
    if (argc == 3 && strcmp(argv[1], "check") == 0) {
        switch (atoi(argv[2])) {
        case 1:
        case 2:
            printf("Tâches 1 et 2 : pas de validateur, le flag est dans le fichier que vous "
                   "déchiffrez (voir ~/lab/task%s/README.txt).\n", argv[2]);
            return 0;
        case 3:
            return task3();
        case 4:
            return task4();
        case 5:
            return task5();
        default:
            break;
        }
    }
    usage();
    return 2;
}
