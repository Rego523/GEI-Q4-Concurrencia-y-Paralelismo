#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <openssl/evp.h>
#include <threads.h>

#include "options.h"
#include "queue.h"


#define MAX_PATH 1024
#define BLOCK_SIZE (10*1024*1024)
#define MAX_LINE_LENGTH (MAX_PATH * 2)


struct file_md5 {
    char *file;
    unsigned char *hash;
    unsigned int hash_size;
};

typedef struct args {
    char *dir;
    queue q;
    queue *out_q;
    int num_threads;
    char *file;
} arguments;

struct thrd_return{
    thrd_t thr;
    arguments arg;
};

/*############################################################################*/

void get_entries(char *dir, queue q);
int get_entries_thread(void *dir);

/**
 * Printea el hash por el terminal
 */
void print_hash(struct file_md5 *md5) {
    for(int i = 0; i < md5->hash_size; i++) {
        printf("%02hhx", md5->hash[i]);
    }
}

void read_hash_file(char *file, char *dir, queue q) {
    FILE *fp;
    char line[MAX_LINE_LENGTH];
    char *file_name, *hash;
    int hash_len;

    if((fp = fopen(file, "r")) == NULL) {
        printf("Could not open %s : %s\n", file, strerror(errno));
        exit(0);
    }

    while(fgets(line, MAX_LINE_LENGTH, fp) != NULL) {
        char *field_break;
        struct file_md5 *md5 = malloc(sizeof(struct file_md5));

        if((field_break = strstr(line, ": ")) == NULL) {
            printf("Malformed md5 file\n");
            exit(0);
        }
        *field_break = '\0';

        file_name = line;
        hash      = field_break + 2;
        hash_len  = strlen(hash);

        md5->file      = malloc(strlen(file_name) + strlen(dir) + 2);
        sprintf(md5->file, "%s/%s", dir, file_name);
        md5->hash      = malloc(hash_len / 2);
        md5->hash_size = hash_len / 2;


        for(int i = 0; i < hash_len; i+=2)
            sscanf(hash + i, "%02hhx", &md5->hash[i / 2]);

        q_insert(q, md5);
    }

    fclose(fp);
}

void sum_file(struct file_md5 *md5) {
    EVP_MD_CTX *mdctx;
    int nbytes;
    FILE *fp;
    char *buf;

    if((fp = fopen(md5->file, "r")) == NULL) {
        printf("Could not open %s\n", md5->file);
        return;
    }

    buf = malloc(BLOCK_SIZE);
    const EVP_MD *md = EVP_get_digestbyname("md5");

    mdctx = EVP_MD_CTX_create();
    EVP_DigestInit_ex(mdctx, md, NULL);

    while((nbytes = fread(buf, 1, BLOCK_SIZE, fp)) >0)
        EVP_DigestUpdate(mdctx, buf, nbytes);

    md5->hash = malloc(EVP_MAX_MD_SIZE);
    EVP_DigestFinal_ex(mdctx, md5->hash, &md5->hash_size);

    EVP_MD_CTX_destroy(mdctx);
    free(buf);
    fclose(fp);
}

void recurse(char *entry, void *arg) {
    queue q = * (queue *) arg;
    struct stat st;

    stat(entry, &st);

    if(S_ISDIR(st.st_mode)) {
        get_entries(entry, q);
    }
}

void add_files(char *entry, void *arg) {
    queue q = * (queue *) arg;
    struct stat st;

    stat(entry, &st);

    if(S_ISREG(st.st_mode))
        q_insert(q, strdup(entry));
}

void walk_dir(char *dir, void (*action)(char *entry, void *arg), void *arg) {
    DIR *d;
    struct dirent *ent;
    char full_path[MAX_PATH];

    if((d = opendir(dir)) == NULL) {
        printf("Could not open dir %s\n", dir);
        return;
    }

    while((ent = readdir(d)) != NULL) {
        if(strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") ==0)
            continue;

        snprintf(full_path, MAX_PATH, "%s/%s", dir, ent->d_name);

        action(full_path, arg);
    }

    closedir(d);
}

/*############################################################################*/

int read_hash_file_thread(void *dir) {
    arguments *args = dir;

    read_hash_file(args->file, args->dir, args->q);

    if(q_getThreadTerminated(args->q) == 0) {
        q_liberar(args->q);
    }

    free(args);

    return 0;
}

int calculoHash2(void *p) {
    arguments *args = p;
    struct file_md5 *md5_in, md5_file;

    while((md5_in = q_remove(args->q))) {
        md5_file.file = md5_in->file;

        sum_file(&md5_file);

        if(memcmp(md5_file.hash, md5_in->hash, md5_file.hash_size)!=0) {
            printf("File %s doesn't match.\nFound:    ", md5_file.file);
            print_hash(&md5_file);
            printf("\nExpected: ");
            print_hash(md5_in);
            printf("\n");
        }

        free(md5_file.hash);
        free(md5_in->file);
        free(md5_in->hash);
        free(md5_in);
    }
    q_threadTerminated(args->q);

    free(args);

    return 0;
}

/*############################################################################*/

struct thrd_return createThreadReadHashFile(char *file, char *dir, queue in_q) {
    arguments *args = malloc(sizeof(arguments));
    thrd_t thr;

    args->q = in_q;
    args->dir = dir;
    args->file = file;

    struct thrd_return ret;
    ret.arg = *args;

    thrd_create(&thr, read_hash_file_thread, args);

    ret.thr = thr; // Guardamos el identificador

    return ret;
}

struct thrd_return createThreadCalculoHash2(queue in_q,  int num_threads) {
    arguments *args = malloc(sizeof(arguments));
    thrd_t thr;

    args->q = in_q;
    args->num_threads = num_threads;

    struct thrd_return ret;
    ret.arg = *args;

    thrd_create(&thr, calculoHash2, args);

    ret.thr = thr; // Guardamos el identificador

    return ret;
}

/*############################################################################*/

void check(struct options opt) {
    queue in_q;

    in_q = q_create(opt.queue_size);

    struct thrd_return threadRHF;
    struct thrd_return threadCalculoHash[opt.num_threads];

    threadRHF = createThreadReadHashFile(opt.file, opt.dir, in_q);

    for(int i = 0; i < opt.num_threads; i++) { // Calculamos los hashes
        threadCalculoHash[i] = createThreadCalculoHash2(in_q, opt.num_threads);
    }
    for(int i = 0; i < opt.num_threads; i++) { // Esperamos a que se calculen todos
        thrd_join(threadCalculoHash[i].thr, NULL);
    }

    thrd_join(threadRHF.thr, NULL);

    q_destroy(in_q);
}

/*############################################################################*/

int escrituraHash(void *p) {
    arguments *args = p;
    struct file_md5 *md5;
    int dirname_len;
    FILE *out;

    if((out = fopen(args->file, "w")) == NULL) {
        printf("Could not open output file\n");
        exit(0);
    }

    dirname_len = strlen(args->dir) + 1; // length of dir + /

    while((md5 = q_remove(*args->out_q)) != NULL) {

        fprintf(out, "%s: ", md5->file + dirname_len);

        for(int i = 0; i < md5->hash_size; i++)
            fprintf(out, "%02hhx", md5->hash[i]);
        fprintf(out, "\n");

        free(md5->file);
        free(md5->hash);
        free(md5);
    }

    fclose(out);

    free(args);

    return 0;
}

int calculoHash(void *p) {
    arguments *args = p;
    char *ent;
    struct file_md5 *md5;

    while((ent = q_remove(args->q)) != NULL) {
        md5 = malloc(sizeof(struct file_md5));
        md5->file = ent;
        sum_file(md5);
        q_insert(*args->out_q, md5);
    }
    q_threadTerminated(args->q);

    // Cuando calcula el último hash libera el thread de salida
    if(q_getThreadTerminated(args->q) == args->num_threads * -1) {
        q_liberar(*args->out_q);
    }

    free(args);

    return 0;
}

/**
 * Recorre la carpeta que le pasamos como parámetro y guarda los paths en la cola
 * @param dir
 * @param q
 */
void get_entries(char *dir, queue q) {
    walk_dir(dir, add_files, &q);
    walk_dir(dir, recurse, &q);
}

int get_entries_thread(void *dir) {
    arguments *args = dir;

    get_entries(args->dir, args->q);

    if(q_getThreadTerminated(args->q) == 0) {
        q_liberar(args->q);
    }

    free(args);

    return 0;
}

/*############################################################################*/

struct thrd_return createThreadEscrituraHash(queue *q, char *file, char *dir) {
    arguments *args = malloc(sizeof(arguments));
    thrd_t thr;

    args->out_q = q;
    args->dir = dir;
    args->file = file;

    struct thrd_return ret;
    ret.arg = *args;

    thrd_create(&thr, escrituraHash, args);

    ret.thr = thr; // Guardamos el identificador

    return ret;
}

struct thrd_return createThreadCalculoHash(queue in_q, queue *out_q, int num_threads) {
    arguments *args = malloc(sizeof(arguments));
    thrd_t thr;

    args->q = in_q;
    args->out_q = out_q;
    args->num_threads = num_threads;

    struct thrd_return ret;
    ret.arg = *args;

    thrd_create(&thr, calculoHash, args);

    ret.thr = thr; // Guardamos el identificador

    return ret;
}

struct thrd_return createThreadGetEntries(char *dir, queue in_q) {
    thrd_t thr;
    arguments *args = malloc(sizeof(arguments));

    args->q = in_q;
    args->dir = dir;

    struct thrd_return ret;
    ret.arg = *args;

    thrd_create(&thr, get_entries_thread, args);

    ret.thr = thr; // Guardamos el identificador

    return ret;
}

/*############################################################################*/

void sum(struct options opt) {
    queue in_q, out_q;

    in_q  = q_create(opt.num_threads);
    out_q = q_create(opt.num_threads);

    struct thrd_return threadGetEntries;
    struct thrd_return threadCalculoHash[opt.num_threads];
    struct thrd_return threadEscrituraHash;

////////////////////////////////////////////////////////////

    // CREACIÓN DE HILOS
    threadGetEntries = createThreadGetEntries(opt.dir, in_q);
    for(int i = 0; i < opt.num_threads; i++) { // Calculamos los hashes
        threadCalculoHash[i] = createThreadCalculoHash(in_q, &out_q, opt.num_threads);
    }
    threadEscrituraHash = createThreadEscrituraHash(&out_q, opt.file, opt.dir);

    // JOINS
    thrd_join(threadGetEntries.thr, NULL);
    for(int i = 0; i < opt.num_threads; i++) { // Esperamos a que se calculen todos
        thrd_join(threadCalculoHash[i].thr, NULL);
    }
    thrd_join(threadEscrituraHash.thr, NULL);

    q_destroy(in_q);
    q_destroy(out_q);
}

/*############################################################################*/

int main(int argc, char *argv[]) {
    struct options opt;
    opt.num_threads = 5;
    opt.queue_size = 1000;
    opt.check = true;
    opt.file = NULL;
    opt.dir = NULL;

    read_options(argc, argv, &opt);

    if(opt.check)
        check(opt);
    else
        sum(opt);
}