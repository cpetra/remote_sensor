/**************************************************************************
 Name        : comm.c
 Version     : 0.1

 Copyright (C) 2014 Constantin Petra

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.
***************************************************************************/
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pthread.h>
#include <linux/limits.h>

int tty_fd;
typedef struct __st_data{
    float t;
    float v;
    int relay_on;
} st_data;

enum {
    RUN_CONSOLE,
    RUN_DAEMON
};

#define MAX_READS 200
static st_data reads[MAX_READS];
static char sock_name[PATH_MAX] = "/tmp/comm_socket";
static char tty_name[PATH_MAX] = "/dev/ttyUSB0";
static int read_cnt = 0;
static int read_it = 0;
static int run_state = RUN_CONSOLE;
static int debug_mode = 0;
static pthread_t thr;
static pthread_mutex_t comm_mutex = PTHREAD_MUTEX_INITIALIZER;

static void LOG0(const char* format, ...)
{
    va_list argptr;
    va_start(argptr, format);

    if (run_state == RUN_CONSOLE) {
        vprintf(format, argptr);
    }
    else {
        char log[1024];
        vsnprintf(log, sizeof(log), format, argptr);
        syslog(LOG_NOTICE, "%s", (const char *)log);
    }
    va_end(argptr);
}

static void LOG1(const char* format, ...)
{
    va_list argptr;
    if (debug_mode > 0) {
        va_start(argptr, format);

        if (run_state == RUN_CONSOLE) {
            vprintf(format, argptr);
        }
        else {
            char log[1024];
            vsnprintf(log, sizeof(log), format, argptr);
            syslog (LOG_NOTICE, "%s", log);
        }
        va_end(argptr);
    }
}

static void add_reading(st_data *s)
{
    pthread_mutex_lock(&comm_mutex);
    reads[read_it] = *s;

    if (++read_it >= MAX_READS) {
        read_it = 0;
    }
    if (read_cnt < MAX_READS) {
        read_cnt++;
    }
    pthread_mutex_unlock(&comm_mutex);
}

static void get_reading(st_data *s, int ID)
{
    int v = read_it - 1 - ID;
    if (v < 0) {
        v = MAX_READS + v;
    }
    *s = reads[v];
}

static int init_serial(char *tty_name)
{
    struct termios tio;

    memset(&tio,0,sizeof(tio));
    tio.c_oflag = 0;
    tio.c_cflag = CS8 | CREAD | CLOCAL;           // 8n1, see termios.h for more information
    tio.c_lflag = 0;
    tio.c_cc[VMIN] = 1;
    tio.c_cc[VTIME] = 5;

    tty_fd = open(tty_name, O_RDWR | O_NONBLOCK);
    if (tty_fd < 0) {
        LOG0("Error opening tty %s\n", tty_name);
        return 0;
    }

    /* For now hardcoded to 19200. */
    cfsetospeed(&tio, B19200);
    cfsetispeed(&tio, B19200);

    tcsetattr(tty_fd, TCSANOW, &tio);

    return 1;
}

static int read_string(char *str, int max_size, int tmo)
{
    int ret;
    int cnt = 0;
    char c;
    int sleep_time = 0;

    do {
        ret = read(tty_fd, &c, 1) > 0;
        if (ret) {
            if (c == '\n') {
                break;
            }
            str[cnt] = c;
            if (++cnt >= max_size) {
                break;
            }

        }
        else {
            usleep(1000);
            /* one second no answer (or less) */
            if (++sleep_time > tmo) {
                LOG1("Timeout\n");
                pthread_mutex_unlock(&comm_mutex);
                return 0;
            }
        }
    } while (1);

    return 1;
}

static int read_data(st_data *data)
{
    int ret;
    char res[64];


    /* Send command */

    char c = 'r';
    pthread_mutex_lock(&comm_mutex);
    tcflush(tty_fd, TCOFLUSH);

    if (write(tty_fd, &c, 1) != 1) {;
        return 0;
    }
    /* And read result */
    if (!read_string(res, sizeof(res) / sizeof(res[0]), 1000)) {
        return 0;
    };

    pthread_mutex_unlock(&comm_mutex);
    ret = sscanf(res, "T: %f V: %f R: %d", &data->t, &data->v, &data->relay_on);
    if (ret != 3) {

        return 0;
    }
    tcflush(tty_fd, TCIFLUSH);
    return 1;
}

static void add_data(void)
{
    st_data data;
    if (read_data(&data) != 1) {
        LOG1("Read fail\n");
    }
    else {
        add_reading(&data);
        LOG1("%f %f %d\n", data.t, data.v, data.relay_on);
    }
}

static void relay_on(void)
{
    const char c = 'n';

    pthread_mutex_lock(&comm_mutex);
    if ( write(tty_fd, &c, 1) != 1) {
        ;
    }
    tcflush(tty_fd, TCOFLUSH);
    pthread_mutex_unlock(&comm_mutex);
    add_data();
}

static void relay_off(void)
{
    char c = 'f';

    pthread_mutex_lock(&comm_mutex);
    if (write(tty_fd, &c, 1) != 1) {
        ;
    }
    tcflush(tty_fd, TCOFLUSH);
    pthread_mutex_unlock(&comm_mutex);

    add_data();
}

static int get_cmd(int s, char *str, int len)
{
    char c;
    int i = 0;
    int n = 0;
    int tmo = 50;

    while (1) {
        n = read(s, &c, 1);
        if (n == 0) {
            if (--tmo <= 0) {
                return -2;
            }
            usleep(100000);
            continue;
        }
        if (c == '\n') {
            break;
        }
        if (n < 0) {
            return -1;
        }
        str[i] = c;
        i++;
    }

    str[i] = 0; /* Add NULL terminator */
    if (strcmp(str, "read") == 0) {
        return 0;
    }
    if (strcmp(str, "on") == 0) {
        return 1;
    }
    if (strcmp(str, "off") == 0) {
        return 2;
    }

    return -1;
}


static void* reader_thread (void* arg)
{
    while (1) {
        add_data();
        sleep(5);
    }
    return NULL;
}

static void create_reader(void)
{
    pthread_create(&thr, 0, reader_thread, 0);
}

static int init_socket(const char *sockname)
{
    struct sockaddr_un local;
    int len;

    int s = socket(AF_UNIX, SOCK_STREAM, 0);
    if(s == -1) {
		perror("socket");
        return s;
    }

    memset(&local, 0, sizeof(local));
    local.sun_family = AF_UNIX;
    strcpy(local.sun_path, sockname);
    unlink(local.sun_path);
    len = strlen(local.sun_path) + sizeof(local.sun_family);
    if (bind(s, (struct sockaddr *)&local, len) == -1) {
        perror("bind");
        close(s);
        unlink(local.sun_path);
        return -1;
    }

    chmod( local.sun_path, S_IRWXU | S_IRWXG  | S_IRWXO );

    if (listen(s, 5) == -1) {
        perror("listen");
        close(s);
        unlink(local.sun_path);
        return -1;
    }
    return s;
}

static int parse_args(int argc, char *argv[])
{
    int opt;
    extern char *optarg;
    while ((opt = getopt(argc, argv, "n:bdt:?")) != -1) {
        switch (opt) {
        case 'd':
            run_state = RUN_DAEMON;
            break;
        case 'b':
            debug_mode = 1;
            break;
        case 'n':
            strncpy(sock_name, optarg, PATH_MAX);
            break;
        case 't':
            strncpy(tty_name, optarg, PATH_MAX);
            break;
        case '?':
            fprintf(stderr, "Usage: %s [-d] [-b] [-t tty_name] [-n socket_name]\n",
                    argv[0]);
            return(0);
        }
    }
    return 1;
}

int main(int argc, char *argv[])
{
    struct sockaddr_un remote;
    int s;

    if (!parse_args(argc, argv)) {
        exit(EXIT_FAILURE);
    }

    if (run_state == RUN_DAEMON) {
        if (daemon(0, 0) != 0) {
	    exit(EXIT_FAILURE);
        }
    }

    if (!init_serial(tty_name)) {
        exit(EXIT_FAILURE);
    }

    if ((s = init_socket(sock_name)) < 0) {
        exit(EXIT_FAILURE);
    }

    create_reader();

    while (1) {
        int done;
        int s2;
        socklen_t t;
        char str[100];

        t = sizeof(remote);
        if ((s2 = accept(s, (struct sockaddr *)&remote, &t)) == -1) {
            perror("accept");
            exit(1);
        }

        LOG1("accepted conn\n");

        do {
            int cmd;
            char buf[256];
            int i;
            st_data n_data;
            cmd = get_cmd(s2, str, sizeof(str));
            LOG1("got cmd: %s \n", str);

            switch (cmd) {
            case 0:
                LOG1("client query\n");
                pthread_mutex_lock(&comm_mutex);
                sprintf(buf, "%d\n", read_cnt);
                if (write(s2, buf, strlen(buf)) < 0) {
                    perror("send");
                }

                for (i = 0; i < read_cnt; i++) {
                    get_reading(&n_data, i);
                    sprintf(buf, "%.1f %.1f %d\n", n_data.t, n_data.v, n_data.relay_on);
                    if (write(s2, buf, strlen(buf)) < 0) {
                        perror("send");
                    }
                }

                pthread_mutex_unlock(&comm_mutex);
                done = 1;
                break;
            case 1:
                relay_on();
                LOG1("on\n");
                break;
            case 2:
                relay_off();

                LOG1("off\n");
                break;
            case -2: /* Timeout */
            case -1: /* Unknown command */
            default:
                done = 1;
                LOG0("Command error: %d\n", cmd);
                break;
            }
        } while (!done);

        close(s2);
    }
    close(tty_fd);
    close(s);
    unlink(sock_name);
    closelog();
    exit(EXIT_SUCCESS);
}
