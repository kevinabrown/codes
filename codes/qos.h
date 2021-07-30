#ifndef CODES_QOS_H
#define CODES_QOS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <ross.h>
#include "codes/codes.h"

#define DEBUG_QOS 1
#define DEBUG_QOS_X 1
#define DEBUG_QOS_R 0
#define DEBUG_QOS_T 0

typedef enum{
    Q_TYPE_TIME_WINDOW = 0,
    Q_TYPE_TOKEN_DUAL_RATE,
} qos_type;

typedef struct
{
    int last_lvl;
    int* status;
    int* data;
    float* min_token_count;    // the committed/assured rate bucket
    float* max_token_count;    // the peak/ceil rate bucket
    tw_stime* min_update_time;	// Last time the token was updated
    tw_stime* max_update_time;	// Last time the token was updated
    
#if DEBUG_QOS == 1
    int* green_total;
    int* green_sent;
    int* yellow_total;
    int* yellow_sent;
    int* red_total;
    int* red_sent;
#endif
}qos_state;

typedef struct
{
    qos_type type;
    int num_qos_levels;
    int qos_bucket_max;
    int * qos_min_bws;
    int * qos_max_bws;
}qos_params;

typedef struct
{
    int chunk_size;
    int num_qos_levels;
    int vcs_per_qos;
    int global_vc_size;
    int local_vc_size;
    int cn_vc_size;
} network_params;

typedef enum qos_priority
{
    Q_LEVEL_0 =0,   // Highest priority QoS class
    Q_LEVEL_1,
    Q_LEVEL_2,
    Q_LEVEL_3,
    Q_LEVEL_4,
    Q_LEVEL_5,
    Q_LEVEL_UNKNOWN,
} qos_priority;

typedef enum qos_status
{
    Q_ACTIVE_UNSATED = 1,
    Q_ACTIVE_SATED,
    Q_INACTIVE,
} qos_status;

int get_vcg_from_category(char * category);
void qos_state_init(const qos_params * qp, qos_state * q, int radix);
void qos_state_free(qos_state * q, int radix);
void qos_state_reset(const qos_params * qp, qos_state * q, int radix);
/*
void qos_update_accumulated_tokens(const qos_params *qp, qos_state *q, int qos_lvl, 
        #if DEBUG_QOS_X == 1
        int port, 
        #endif
        double bandwidth, int chunk_size, tw_stime now);
*/
int qos_token_get_next_vcg(const qos_params * qp, qos_state * q, int vcs_per_qos, int vc_size, double bandwidth, int chunk_size, int * vc_occupancy,
    #if DEBUG_QOS_X == 1
    char node_type,
    int node_id, 
    int port,
    #endif
    const void ** pending_msgs, short * last_saved_qos, tw_bf * bf, tw_lp * lp);


#ifdef __cplusplus
}
#endif

#endif /* end of include guard: CODES_QOS_H */

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 * End:
 *
 * vim: ft=c ts=4 sts=4 sw=4 expandtab
 */
