/**
 * @file internal_queue.h
 * @authors
 * - Filipe Rodrigues (2021218054)
 * - Joás Silva (2021226149)
 */

#ifndef INTERNALQUEUE_H
#define INTERNALQUEUE_H

#include "functions.h"

typedef struct {
  int type; // 0 is from sensor and 1 from user 
  char content[MAX];
  Command cmd;
} Job;

typedef struct Jobs {
  Job job;
  struct Jobs * next;
} Jobs;

typedef struct {
  int n; // current number of elements
  Jobs *user; // User Console Thread
  Jobs *user_last; // Last element of User Console Thread
  Jobs *sensor; // Sensor Reader Thread
  Jobs *sensor_last; // Last element of Sensor Reader Thread
} InternalQueue;

/**
 * @brief Create the Internal Queue
 * @return InternalQueue* 
 */
InternalQueue * createIQ() {
  InternalQueue * queue = (InternalQueue *) malloc(sizeof(InternalQueue));
  queue->n = 0;
  queue->user = NULL;
  queue->user_last = NULL;
  queue->sensor = NULL;
  queue->sensor_last = NULL;
  return queue;
}

/**
 * @brief Add a job to the queue
 * @param queue 
 * @param job 
 */
void addJob(InternalQueue * queue, Job job) {
  Jobs * new_job = (Jobs *) malloc(sizeof(Jobs));
  new_job->job = job;
  new_job->next = NULL;

  if (job.type == 0) {
    if (queue->sensor == NULL) queue->sensor = new_job;
    else queue->sensor_last->next = new_job;
    queue->sensor_last = new_job;
  } else {
    if (queue->user == NULL) queue->user = new_job;
    else queue->user_last->next = new_job;
    queue->user_last = new_job;
  }
  queue->n++;
}

/**
 * @brief Remove the first job of the queue and return it
 * @param queue 
 * @return Job
 */
Job removeJob(InternalQueue * queue) {
  Job job;
  Jobs * aux;
  if (queue->sensor != NULL) {
    job = queue->sensor->job;
    aux = queue->sensor;
    queue->sensor = queue->sensor->next;
    free(aux);
  } else {
    job = queue->user->job;
    aux = queue->user;
    queue->user = queue->user->next;
    free(aux);
  }
  queue->n--;
  return job;
}

#endif