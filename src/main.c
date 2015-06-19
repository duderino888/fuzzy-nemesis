#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>

#include "log.h"


#define SERVER_PORT         12345
#define SERVER_ROOT         "./www/"
#define MAX_HEADER_LEN      512
#define MAX_FILENAME_LEN    256


#define streq(A,B)      (strcmp((A), (B)) == 0)


static void *thread_func(void *);
static char *strtrim(char *);
static char *strsplit(char *, const char *);
static bool parse_request(char *, char **, char **, char **);
static bool map_path_to_filesystem(char *, size_t, const char *);
static char *get_mime_type(const char *);


int
main(void)
{
    int srv_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (srv_fd == -1) {
        log_error("Failed to create socket: %d", errno);
        exit(EXIT_FAILURE);
    }

    setsockopt(srv_fd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
    setsockopt(srv_fd, SOL_SOCKET, SO_REUSEPORT, &(int){1}, sizeof(int));

    struct sockaddr_in sa = {
        .sin_family = AF_INET,
        .sin_port = htons(SERVER_PORT),
        .sin_addr.s_addr = INADDR_ANY
    };

    if (bind(srv_fd, (struct sockaddr*)&sa, sizeof(sa)) != 0) {
        log_error("Failed to bind socket: %d", errno);
        exit(EXIT_FAILURE);
    }

    if (listen(srv_fd, 64) != 0) {
        log_error("Failed to listen on socket: %d", errno);
        exit(EXIT_FAILURE);
    }

    log_info("Server listening on port %d", SERVER_PORT);

    for (;;) {
        int cli_fd = accept(srv_fd, NULL, NULL);
        if (cli_fd == -1) {
            continue;
        }

        FILE *fp = fdopen(cli_fd, "r+b");
        if (!fp) {
            log_error("Failed to open stream for socket: %d", errno);
            close(cli_fd);
            continue;
        }

        pthread_t thr;
        if (pthread_create(&thr, NULL, thread_func, fp) != 0) {
            log_error("Failed to create thread: %d", errno);
            fclose(fp);
            continue;
        }

        pthread_detach(thr);
    }
    return 0;
}


static void *
thread_func(void *thread_arg)
{
    FILE *fp = thread_arg;
    char line[MAX_HEADER_LEN];
    char *request = NULL;
    char file_name[MAX_FILENAME_LEN];
    int file = -1;
    char *method, *path, *version;


    if (!fgets(line, sizeof(line), fp)) {
        log_debug("Failed to receive request");
        goto cleanup;
    }

    request = strdup(strtrim(line));
    if (!parse_request(request, &method, &path, &version)) {
        log_debug("Bad request");
        goto cleanup;
    }

    while (fgets(line, sizeof(line), fp) && strlen(strtrim(line)) > 0) {
        /* TODO: handle headers */
        // char *name = line;
        // char *value = strtrim(strsplit(name, ":"));
    }

    log_info("Request: %s %s %s", method, path, version);

    if (!map_path_to_filesystem(file_name, sizeof(file_name), path)) {
        /* TODO: Return a 404. */
        log_debug("Failed to map path to filesystem");
        goto cleanup;
    }

    file = open(file_name, O_RDONLY);
    if (file == -1) {
        /* TODO: Return a 404. */
        log_debug("Failed to open file '%s'", file_name);
        goto cleanup;
    }
    struct stat st;
    if (fstat(file, &st) != 0) {
        /* TODO: Return a 500 or something. */
        log_debug("Failed to fstat file");
        goto cleanup;
    }

    fprintf(fp, "HTTP/1.1 200 OK\r\n");
    fprintf(fp, "Server: fuzzy-nemesis\r\n");
    fprintf(fp, "Content-Type: %s\r\n", get_mime_type(file_name));
    fprintf(fp, "Content-Length: %"PRIu32"\r\n", (uint32_t)st.st_size);
    fprintf(fp, "\r\n");

    char buf[512];
    ssize_t buf_len = 0;
    while ((buf_len = read(file, buf, sizeof(buf))) > 0) {
        fwrite(buf, 1, buf_len, fp);
    }

cleanup:
    free(request);
    if (file != -1) close(file);
    fclose(fp);
    return 0;
}


static char *
strtrim(char *str)
{
    size_t len = strlen(str);
    char *tmp = str;

    while (len > 0 && isspace(str[len - 1])) {
        str[--len] = '\0';
    }
    while (len > 0 && isspace(*tmp)) {
        ++tmp;
        --len;
    }
    if (tmp != str) {
        memmove(str, tmp, len + 1);
    }
    return str;
}


static char *
strsplit(char *str, const char *sep)
{
    char *token = strstr(str, sep);
    if (token) {
        *token = '\0';
        token += strlen(sep);
    }
    else {
        token = strchr(str, '\0');
    }

    return token;
}


static bool
parse_request(char *request, char **method, char **path, char **ver)
{
    *method = request;
    *path = strsplit(*method, " ");
    *ver = strsplit(*path, " ");

    if (strlen(*method) == 0 || strlen(*path) == 0 || strlen(*ver) == 0) {
        return false;
    }

    if (!streq(*ver, "HTTP/1.0") && !streq(*ver, "HTTP/1.1")) {
        log_debug("Bad version: '%s'", *ver);
        return false;
    }

    return true;
}


static bool
map_path_to_filesystem(char *filename, size_t max, const char *path)
{
    int len = snprintf(filename, max, "%s/%s", SERVER_ROOT, path);
    if (len < 0) {
        return false;
    }

    if ((size_t)len >= max) {
        log_debug("Filename too long");
        return false;
    }

    char *pos;
    while ((pos = strstr(filename, "..")) != NULL) {
        pos[0] = '_';
        pos[1] = '_';
    }

    struct stat st;
    if (stat(filename, &st) != 0) {
        return false;
    }

    if (st.st_mode & S_IFDIR) {
        strncat(filename, "/index.htm", max);
    }

    return true;
}


static char *
get_mime_type(const char *filename)
{
    char *str = strrchr(filename, '.');
    if (str) {
        char ext[32];
        strncpy(ext, str, sizeof(ext));
        ext[sizeof(ext)-1] = '\0';
        for (str = ext; *str; ++str) *str = tolower(*str);

        if (streq(ext, ".htm") || streq(ext, ".html")) {
            return "text/html";
        }

        if (streq(ext, ".jpg") || streq(ext, ".jpeg")) {
            return "image/jpeg";
        }

        if (streq(ext, ".png")) {
            return "image/png";
        }
    }
    return "application/octet-stream";
}

