/*--------------------------------------------------------------------*/
/* server.c                                                           */
/* Author: Junghan Yoon, KyoungSoo Park                               */
/* Modified by: Jaeun Park                                            */
/*--------------------------------------------------------------------*/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <sys/time.h>
#include "common.h"
#include "skvslib.h"
/*--------------------------------------------------------------------*/
struct thread_args
{
    int listenfd;
    int idx;
    struct skvs_ctx *ctx;

    /*--------------------------------------------------------------------*/
    /* free to use */

    /*--------------------------------------------------------------------*/
};
/*--------------------------------------------------------------------*/
volatile static sig_atomic_t g_shutdown = 0;
static pthread_mutex_t accept_mutex = PTHREAD_MUTEX_INITIALIZER;
/*--------------------------------------------------------------------*/
void *handle_client(void *arg)
{
    TRACE_PRINT();
    struct thread_args *args = (struct thread_args *)arg;
    struct skvs_ctx *ctx = args->ctx;
    int idx = args->idx;
    int listenfd = args->listenfd;
    /*--------------------------------------------------------------------*/
    char rbuf[BUF_SIZE];
    char wbuf[BUF_SIZE];
    int connfd;
    struct sockaddr_in client_addr;
    socklen_t client_len;
    /*--------------------------------------------------------------------*/

    free(args);
    printf("%dth worker ready\n", idx);

    /*--------------------------------------------------------------------*/
    while (!g_shutdown)
    {
        client_len = sizeof(client_addr);

        // accept를 mutex로 보호하여 순서 보장
        pthread_mutex_lock(&accept_mutex);
        connfd = accept(listenfd, (struct sockaddr *)&client_addr, &client_len);
        pthread_mutex_unlock(&accept_mutex);

        if (connfd < 0)
        {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
            {
                continue;
            }
            if (g_shutdown)
                break;
            perror("accept");
            continue;
        }

        // 클라이언트 처리
        while (!g_shutdown)
        {
            memset(rbuf, 0, BUF_SIZE);
            int total_read = 0;

            // recv로 데이터 읽기
            while (1)
            {
                ssize_t n = recv(connfd, rbuf + total_read, BUF_SIZE - total_read - 1, 0);

                if (n < 0)
                {
                    if (errno == EINTR)
                        continue;
                    break;
                }
                else if (n == 0)
                {
                    // 연결 종료
                    goto close_conn;
                }

                total_read += n;
                rbuf[total_read] = '\0';

                // 메시지 완료 확인: \n 또는 \n\n으로 종료
                if (total_read > 0 && rbuf[total_read - 1] == '\n')
                {
                    if (rbuf[0] == '\n' || (total_read >= 2 && rbuf[0] == '\r' && rbuf[1] == '\n'))
                    {
                        // 빈 줄: 연결 종료
                        goto close_conn;
                    }
                    break;
                }

                if (total_read >= BUF_SIZE - 1)
                    break;
            }

            if (total_read == 0)
                break;

            // SKVS 요청 처리
            size_t wlen = 0;
            int ret = skvs_serve(ctx, rbuf, total_read, wbuf, &wlen);

            if (ret < 0)
            {
                break;
            }

            if (ret > 0 && wlen > 0)
            {
                // send로 응답 전송
                size_t total_sent = 0;
                while (total_sent < wlen)
                {
                    ssize_t n = send(connfd, wbuf + total_sent, wlen - total_sent, 0);
                    if (n < 0)
                    {
                        if (errno == EINTR)
                            continue;
                        goto close_conn;
                    }
                    total_sent += n;
                }
            }
        }

    close_conn:
        close(connfd);
    }
    /*--------------------------------------------------------------------*/

    return NULL;
}
/*--------------------------------------------------------------------*/
/* Signal handler for SIGINT */
void handle_sigint(int sig)
{
    TRACE_PRINT();
    printf("\nReceived SIGINT, initiating shutdown...\n");
    g_shutdown = 1;
}
/*--------------------------------------------------------------------*/
int main(int argc, char *argv[])
{
    size_t hash_size = DEFAULT_HASH_SIZE;
    char *ip = DEFAULT_ANY_IP;
    int port = DEFAULT_PORT, opt;
    int num_threads = NUM_THREADS;
    int delay = RWLOCK_DELAY;
    /*--------------------------------------------------------------------*/
    int listenfd, i;
    struct sockaddr_in server_addr;
    pthread_t threads[num_threads];
    struct skvs_ctx *ctx;
    struct sigaction sa;
    /*--------------------------------------------------------------------*/

    /* parse command line options */
    while ((opt = getopt(argc, argv, "p:t:s:d:h")) != -1)
    {
        switch (opt)
        {
        case 'p':
            port = atoi(optarg);
            break;
        case 't':
            num_threads = atoi(optarg);
            break;
        case 's':
            hash_size = atoi(optarg);
            if (hash_size <= 0)
            {
                perror("Invalid hash size");
                exit(EXIT_FAILURE);
            }
            break;
        case 'd':
            delay = atoi(optarg);
            break;
        case 'h':
        default:
            printf("Usage: %s [-p port (%d)] "
                   "[-t num_threads (%d)] "
                   "[-d rwlock_delay (%d)] "
                   "[-s hash_size (%d)]\n",
                   argv[0],
                   DEFAULT_PORT,
                   NUM_THREADS,
                   RWLOCK_DELAY,
                   DEFAULT_HASH_SIZE);
            exit(EXIT_FAILURE);
        }
    }

    /*--------------------------------------------------------------------*/
    // SIGINT 핸들러 등록
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) == -1)
    {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    // SKVS 초기화
    ctx = skvs_init(hash_size, delay);
    if (!ctx)
    {
        fprintf(stderr, "Failed to initialize SKVS\n");
        exit(EXIT_FAILURE);
    }

    // listening socket 생성
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0)
    {
        perror("socket");
        skvs_destroy(ctx, 0);
        exit(EXIT_FAILURE);
    }

    // SO_REUSEADDR 설정
    int reuse = 1;
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
    {
        perror("setsockopt SO_REUSEADDR");
        close(listenfd);
        skvs_destroy(ctx, 0);
        exit(EXIT_FAILURE);
    }

    // 타임아웃 설정
    struct timeval tv;
    tv.tv_sec = TIMEOUT;
    tv.tv_usec = 0;
    if (setsockopt(listenfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
    {
        perror("setsockopt SO_RCVTIMEO");
        close(listenfd);
        skvs_destroy(ctx, 0);
        exit(EXIT_FAILURE);
    }

    // bind
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);
    server_addr.sin_port = htons(port);

    if (bind(listenfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("bind");
        close(listenfd);
        skvs_destroy(ctx, 0);
        exit(EXIT_FAILURE);
    }

    // listen
    if (listen(listenfd, NUM_BACKLOG) < 0)
    {
        perror("listen");
        close(listenfd);
        skvs_destroy(ctx, 0);
        exit(EXIT_FAILURE);
    }

    // 워커 스레드 생성
    for (i = 0; i < num_threads; i++)
    {
        struct thread_args *args = malloc(sizeof(struct thread_args));
        if (!args)
        {
            fprintf(stderr, "Failed to allocate thread args\n");
            g_shutdown = 1;
            break;
        }

        args->listenfd = listenfd;
        args->idx = i;
        args->ctx = ctx;

        if (pthread_create(&threads[i], NULL, handle_client, args) != 0)
        {
            perror("pthread_create");
            free(args);
            g_shutdown = 1;
            break;
        }
    }

    // 스레드 종료 대기
    for (i = 0; i < num_threads; i++)
    {
        pthread_join(threads[i], NULL);
    }

    // 정리
    close(listenfd);
    skvs_destroy(ctx, 1);
    /*--------------------------------------------------------------------*/

    return 0;
}
/*--------------------------------------------------------------------*/