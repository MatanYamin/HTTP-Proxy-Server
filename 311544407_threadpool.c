//Matan Yamin, ID: 311544407
#include "threadpool.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <signal.h>

threadpool *create_threadpool(int num_threads_in_pool)
{
    if (num_threads_in_pool > MAXT_IN_POOL || num_threads_in_pool <= 0)
    { //check if num is good
        perror("illegal thread number");
        return NULL;
    }
    threadpool *tp = (threadpool *)malloc(sizeof(threadpool));
    if (tp == NULL)
    {
        //destroy_threadpool(tp);//matan
        printf("Error. Allocation was unsuccessful. \n");
        return NULL;
    }
    tp->num_threads = 0;
    tp->qsize = 0;
    tp->threads = (pthread_t *)malloc(num_threads_in_pool * sizeof(pthread_t));
    if (tp->threads == NULL)
    {
        //free(tp->threads);
        destroy_threadpool(tp);
        printf("Error. Allocation was unsuccessful. \n");
        return NULL;
    }
    tp->shutdown = 0;
    tp->dont_accept = 0;
    tp->qhead = NULL;
    tp->qtail = NULL;
    pthread_mutex_init(&tp->qlock, NULL);      //check the init
    pthread_cond_init(&tp->q_empty, NULL);     //check the init
    pthread_cond_init(&tp->q_not_empty, NULL); //check the init

    for (int i = 0; i < num_threads_in_pool; i++)
    {
        if (pthread_create(&tp->threads[i], NULL, do_work, tp) != 0)
        {
            break;
        }
        tp->num_threads++;
    }
    return tp;
}

void dispatch(threadpool *from_me, dispatch_fn dispatch_to_here, void *arg)
{
    if(from_me==NULL || dispatch_to_here == NULL || arg==NULL){
        return;
    }
    pthread_mutex_lock(&from_me->qlock);
    if (from_me->dont_accept == 1)
    {                                          //means we r in destruction process
        pthread_mutex_unlock(&from_me->qlock); //put it back to zero
        return;
    }
    work_t *tWork = (work_t *)malloc(sizeof(work_t));
    if (tWork == NULL)
    {
        destroy_threadpool(from_me);
        printf("Error. Allocation was unsuccessful. \n");
        return;
    }
    tWork->routine = dispatch_to_here;
    tWork->arg = arg;
    tWork->next = NULL;
    if (from_me->qhead == NULL)
    {
        from_me->qhead = tWork;
        from_me->qtail = tWork;
    }
    else
    {
        from_me->qtail->next = tWork;
        from_me->qtail = from_me->qtail->next;
        from_me->qtail->next = NULL;
    }
    from_me->qsize++;
    pthread_cond_signal(&from_me->q_not_empty);
    pthread_mutex_unlock(&from_me->qlock);
}

void *do_work(void *p)
{ //will be the thread's job
    if(p == NULL){
        return NULL;
    }
    threadpool *tempPool = (threadpool *)p;
    while (1)
    {
        pthread_mutex_lock(&tempPool->qlock); //lock the queue
        if (tempPool->shutdown == 1)
        { //destruc is in process
            pthread_mutex_unlock(&tempPool->qlock);
            return NULL;
        }
        if (tempPool->qsize == 0)
        {
            pthread_cond_wait(&tempPool->q_not_empty, &tempPool->qlock); //wait untill we have thread
        }
        if (tempPool->shutdown == 1)
        { //destruc is in process
            pthread_mutex_unlock(&tempPool->qlock);
            return NULL;
        }
        work_t *work = tempPool->qhead;
        if (work == NULL)
        {
            pthread_mutex_lock(&tempPool->qlock);
            continue;
        }
        tempPool->qsize--; //decrease the queue size by 1
        if (tempPool->qsize == 0)
        {
            tempPool->qtail = NULL;
            tempPool->qhead = NULL;
            if (tempPool->dont_accept == 1)
            {
                pthread_cond_broadcast(&tempPool->q_empty);
            }
        }
        else
        {
            tempPool->qhead = tempPool->qhead->next; //update the queue
        }
        pthread_mutex_unlock(&tempPool->qlock); //unlcok
        work->routine(work->arg);               //calling the function with arg
        free(work);
        pthread_mutex_lock(&tempPool->qlock);   //lock again after calling the func
        if (tempPool->qsize == 0)
        {
            pthread_cond_signal(&tempPool->q_empty); //the queue is empty now so signal
        }
        pthread_mutex_unlock(&tempPool->qlock);
    }
}

void destroy_threadpool(threadpool *destroyme)
{
    if (destroyme == NULL)
    {
        return;
    }
    pthread_mutex_lock(&destroyme->qlock);
    destroyme->dont_accept = 1;
    if (destroyme->qsize > 0)
    {
        pthread_cond_wait(&destroyme->q_empty, &destroyme->qlock);
    }
    destroyme->shutdown = 1;
    pthread_cond_broadcast(&destroyme->q_not_empty);
    pthread_mutex_unlock(&destroyme->qlock);
    for (int i = 0; i < destroyme->num_threads; i++)
    {
        pthread_join(destroyme->threads[i], NULL);
    }
    free(destroyme->threads);
    free(destroyme);
}