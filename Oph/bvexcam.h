#ifndef BVEXCAM_H
#define BVEXCAM_H

extern int num_clients;
extern int telemetry_sent;
extern int cancelling_auto_focus;
extern int verbose;
extern void * camera_raw;
extern struct conf_params config;
void * processClient(void* log);
int init_bvexcam(FILE* log);
void * run_bvexcam(void *log);
#endif
