#include "codes/qos.h"
#include <stdbool.h>


#define BW_MONITOR 1

int get_vcg_from_category(char * category)
{
   int vcg;

   if(strcmp(category, "high") == 0)
       vcg = Q_LEVEL_0;
   else if(strcmp(category, "medium") == 0)
       vcg = Q_LEVEL_1;
   else if(strcmp(category, "low") == 0)
       vcg = Q_LEVEL_2;
   else if(strcmp(category, "class3") == 0)
       vcg = Q_LEVEL_3;
   else if(strcmp(category, "class4") == 0)
       vcg = Q_LEVEL_4;
   else if(strcmp(category, "class5") == 0)
       vcg = Q_LEVEL_5;
   else
       tw_error(TW_LOC, "\n priority needs to be specified with qos_levels>1 (catetory: %s)", category);

   assert(vcg >= Q_LEVEL_0 && vcg <= Q_LEVEL_5);
   return vcg;
}

void qos_state_init(const qos_params * qp, qos_state * q, int radix)
{
    int num_qos_levels = qp->num_qos_levels;

    for(int i=0; i < radix; i++){

        q[i].last_lvl = -1;

        q[i].data = (int*)calloc(num_qos_levels, sizeof(int));
        q[i].status = (int*)calloc(num_qos_levels, sizeof(int));
        q[i].min_token_count = (float*)calloc(num_qos_levels, sizeof(float));
        q[i].max_token_count = (float*)calloc(num_qos_levels, sizeof(float));
        q[i].min_update_time = (tw_stime*)calloc(num_qos_levels, sizeof(tw_stime));
        q[i].max_update_time = (tw_stime*)calloc(num_qos_levels, sizeof(tw_stime));
        #if DEBUG_QOS == 1
        q[i].green_total = (int*)calloc(num_qos_levels, sizeof(int));
        q[i].green_sent  = (int*)calloc(num_qos_levels, sizeof(int));
        q[i].yellow_total = (int*)calloc(num_qos_levels, sizeof(int));
        q[i].yellow_sent  = (int*)calloc(num_qos_levels, sizeof(int));
        q[i].red_total = (int*)calloc(num_qos_levels, sizeof(int));
        q[i].red_sent  = (int*)calloc(num_qos_levels, sizeof(int));
        #endif

        for(int j = 0; j < num_qos_levels; j++)
        {
            // TODO: consider call the qos_state_reset fucntion for some of these variables
            q[i].data[j] = 0;
            q[i].status[j] = Q_ACTIVE_UNSATED;
            q[i].min_token_count[j] = 0;
            q[i].max_token_count[j] = 0;
            q[i].min_update_time[j] = 0.0;
            q[i].max_update_time[j] = 0.0;
            #if DEBUG_QOS == 1
            q[i].green_total[j] = 0;
            q[i].green_sent[j] = 0;
            q[i].yellow_total[j] = 0;
            q[i].yellow_sent[j] = 0;
            q[i].red_total[j] = 0;
            q[i].red_sent[j] = 0;
            #endif
        }
    }
}

void qos_state_free(qos_state * q, int radix)
{
    for(int i=0; i < radix; i++){
        free(q[i].data);
        free(q[i].status);
        free(q[i].min_token_count);
        free(q[i].max_token_count);
        free(q[i].min_update_time);
        free(q[i].max_update_time);
        #if DEBUG_QOS == 1
        free(q[i].green_total);
        free(q[i].green_sent);
        free(q[i].yellow_total);
        free(q[i].yellow_sent);
        free(q[i].red_total);
        free(q[i].red_sent);
        #endif
    }
}

void qos_state_reset(const qos_params * qp, qos_state * q, int radix)
{
    for(int i = 0; i < radix; i++)
    {
        for(int j = 0; j < qp->num_qos_levels; j++)
        {
            q[i].status[j] = Q_ACTIVE_UNSATED;
            q[i].data[j] = 0;

            #if DEBUG_QOS == 1
            q[i].green_total[j] = 0;
            q[i].green_sent[j] = 0;
            q[i].yellow_total[j] = 0;
            q[i].yellow_sent[j] = 0;
            q[i].red_total[j] = 0;
            q[i].red_sent[j] = 0;
            #endif
            /*
            (*q)[i].qos_status[j] = Q_ACTIVE_UNSATED;
            (*q)[i].qos_data[j] = 0;

            #if DEBUG_QOS == 1
            (*q)[i].qos_green_total[j] = 0;
            (*q)[i].qos_green_sent[j] = 0;
            (*q)[i].qos_yellow_total[j] = 0;
            (*q)[i].qos_yellow_sent[j] = 0;
            (*q)[i].qos_red_total[j] = 0;
            (*q)[i].qos_red_sent[j] = 0;
            #endif
            */
        }
    }
}

//static void update_rtr_accumulated_tokens(tw_stime now, router_state * s, int qos_lvl, int port)
void qos_update_accumulated_tokens(const qos_params *qp, qos_state *q, int qos_lvl, 
        #if DEBUG_QOS_X == 1
        int port, 
        #endif
        double bandwidth, int chunk_size, tw_stime now)
{

    //printf("================================== UPDATING =======================================\n");
    if (q->min_token_count[qos_lvl] == qp->qos_bucket_max && q->max_token_count[qos_lvl] == qp->qos_bucket_max)
    {
        q->max_update_time[qos_lvl] = now;
        return;
    }

    double bw_bytes = bandwidth * 1024.0 * 1024.0 * 1024.0;
    double bytes_per_ns = bw_bytes / (1000.0 * 1000.0 * 1000.0);

    tw_stime min_elapsed_time = now - q->min_update_time[qos_lvl];
    tw_stime max_elapsed_time = now - q->max_update_time[qos_lvl];

    // QOS - is it better to add these to the qos_state or compute them every time?
    double qos_min_bytes_per_ns = bytes_per_ns * qp->qos_min_bws[qos_lvl] / 100;
    double qos_max_bytes_per_ns = bytes_per_ns * qp->qos_max_bws[qos_lvl] / 100;
    
    /* Calculations based on the cost of sending 1 flit = 1 token */
    double min_accum_tokens = (qos_min_bytes_per_ns / chunk_size) * min_elapsed_time;
    double max_accum_tokens = (qos_max_bytes_per_ns / chunk_size) * max_elapsed_time;
    //printf("================================== VALSSSS =======================================hw: %f - min: %f - max: %f\n",bandwidth, min_accum_tokens, max_accum_tokens);

    if(min_accum_tokens >= 1.0f)
    {
        int whole_tokens = (int)min_accum_tokens;
        double part_token = min_accum_tokens - whole_tokens;

        if(q->min_token_count[qos_lvl] + whole_tokens >= qp->qos_bucket_max)
        {
            q->min_token_count[qos_lvl] = qp->qos_bucket_max; // Any truncation?
        }else
        {
            q->min_token_count[qos_lvl] += whole_tokens;
        }
        assert(q->min_token_count[qos_lvl] >= 0.0 && q->min_token_count[qos_lvl] <= qp->qos_bucket_max);

        // Record updated time as time for last whole token /
        double part_token_time = (part_token * chunk_size) / qos_min_bytes_per_ns;
        q->min_update_time[qos_lvl] = now - part_token_time; 
    }
    if(max_accum_tokens >= 1.0f)
    {
        int whole_tokens = (int)max_accum_tokens;
        double part_token = max_accum_tokens - whole_tokens;

        if(q->max_token_count[qos_lvl] + whole_tokens >= qp->qos_bucket_max)
        {
            q->max_token_count[qos_lvl] = qp->qos_bucket_max; // Any truncation?
        }else
        {
            q->max_token_count[qos_lvl] += whole_tokens;
        }
        assert(q->max_token_count[qos_lvl] >= 0.0);
        assert(q->max_token_count[qos_lvl] <= qp->qos_bucket_max);

        // Record updated time as time for last whole token /
        double part_token_time = (part_token * chunk_size) / qos_max_bytes_per_ns;
        q->max_update_time[qos_lvl] = now - part_token_time; 
    }

    return;
}

int qos_token_get_next_vcg(const qos_params * qp, qos_state * q, int vcs_per_qos, int vc_size, double bandwidth, int chunk_size, int * vc_occupancy,
    #if DEBUG_QOS_X == 1
    char node_type,
    int node_id,
    int port,
    #endif
    const void ** pending_msgs, short * last_saved_qos, tw_bf * bf, tw_lp * lp)
{
    int vcg = 0;
    int base_limit = 0;
    int num_qos_levels = qp->num_qos_levels;

    //printf("================================== GETTING ========================================\n");
    /* First make sure the bandwidth consumptions are up to date. */
    if(BW_MONITOR == 1 && num_qos_levels > 1)
    {
        int first_green = -1;       // Marks the class that can send next
        int first_yellow = -1;      // If no classes are marked green, this class will send next

        for(int i = 0; i < num_qos_levels; i++)
        {
            int green = false;
            int yellow = false;
            int red = false;

            // Update token buckets with newly accumulated tokens /
            qos_update_accumulated_tokens(qp, q, i, 
                    #if DEBUG_QOS_X == 1
                    port, 
                    #endif
                    bandwidth, chunk_size, tw_now(lp));

            base_limit = i * vcs_per_qos;
            for(int k = base_limit; k < base_limit + vcs_per_qos; k ++)
            {
                //if(pending_msgs[k] != NULL && vc_occupancy[k] + chunk_size <= vc_size)
                if(pending_msgs[k] != NULL)
                {
                    /* Check if this is a yellow class: it is not green and within its peak rate. */
                    if(q->max_token_count[i] < 1.0f)
                    {
                        red = true;
                        break;
                    }
                    /* Check if this is a green class: it is within its assured rate */
                    if(q->min_token_count[i] < 1.0f)
                    {
                        yellow = true;
                        if(first_yellow < 0)
                        {
                            first_yellow = k;
                        }
                        break;
                    }
                    /* If the class is neither green nor yellow, it is red */
                    else
                    {
                        green = true;
                        if(first_green < 0 )
                        {
                            first_green = k;
                        }
                        break;
                    }
                }
            }

            #if DEBUG_QOS == 1
            if(green == true)
            {
                q->green_total[i]++;
            }
            else if(yellow == true)
            {
                q->yellow_total[i]++;
            }
            else if(red == true)
            {
                q->red_total[i]++;
            }
            #endif

            #if DEBUG_QOS_X == 1
            printf("[%.0lf] qos_token_accumulate %c:%d port:%d class:%d min_tokens:%.2f max_tokens:%.2f\n", 
                    tw_now(lp), node_type, node_id, port, i,
                    q->min_token_count[i],
                    q->max_token_count[i]);
            #endif
        }

        /* The loops before and after the following debug section could be combined,
         * I wanted to get the status of all buffers and buckets before each send.
         * Combining the loops would give slightly better performance since
         * tokens for lower priority classs don't have to be updated if a
         * higher priorty class is sending. */
        
        // This line may be needed so that we don't try to send from a class that doesn't have credit downstream
            ///if(s->terminal_msgs[k] != NULL && s->vc_occupancy[k] + s->params->chunk_size <= s->params->cn_vc_size)
            
        // Return the first VC with traffic from the green class
        if(first_green >= 0)
        {
            int i = first_green / vcs_per_qos;
            q->min_token_count[i] -= 1.0f;
            if(q->max_token_count[i] >= 1.0f)
                q->max_token_count[i] -= 1.0f;

            #if DEBUG_QOS == 1
            q->green_sent[i]++;
            #endif
            #if DEBUG_QOS_X == 1
            printf("[%.0lf] qos_send %c:%d port:%d class:%d vc:%d (sent_GREEN)\n", tw_now(lp),
                    node_type, node_id, port, i, first_green);

            #endif

            assert(q->min_token_count[i] >= 0.0);
            assert(q->max_token_count[i] >= 0.0);

    //printf("================================== GOTTEN ========================================\n");
            return first_green;
        }
        else if(first_yellow >= 0)
        {
            int i = first_yellow / vcs_per_qos;
            q->max_token_count[i] -= 1.0f;

            #if DEBUG_QOS == 1
            q->yellow_sent[i]++;
            #endif
            #if DEBUG_QOS_X == 1
            printf("[%.0lf] qos_send %c:%d port:%d class:%d vc:%d (sent_YELLOW)\n", tw_now(lp),
                    node_type, node_id, port, i, first_yellow);
            #endif

            assert(q->max_token_count[i] >= 0.0);
    //printf("================================== GOTTEN ========================================\n");

            return first_yellow;
        }
    }
        
    /* All vcgs are exceeding their bandwidth limits*/
    *last_saved_qos = q->last_lvl;
    int next_rr_vcg = (q->last_lvl + 1) % num_qos_levels;

    for(int i = 0; i < num_qos_levels; i++)
    {
        base_limit = next_rr_vcg * vcs_per_qos; 
        for(int k = base_limit; k < base_limit + vcs_per_qos; k++)
        {
            #if DEBUG_QOS_X == 1
            printf("[%.0lf] qos_send_excess %c:%d port:%d class:%d vc:%d (checked)\n", tw_now(lp), 
                    node_type, node_id, port, next_rr_vcg, k);
            #endif
            //if(pending_msgs[k] != NULL && vc_occupancy[k] + chunk_size <= vc_size)
            if(pending_msgs[k] != NULL)
            {
                #if DEBUG_QOS_X == 1
                printf("[%.0lf] qos_send_excess %c:%d port:%d class:%d vc:%d (sent-RED)\n", tw_now(lp), 
                        node_type, node_id, port, next_rr_vcg, k);
                #endif

                #if DEBUG_QOS == 1 
                q->red_sent[next_rr_vcg]++;
                #endif

                if(*last_saved_qos < 0)
                    *last_saved_qos = q->last_lvl;  // Is this correct for RC KBEDIT

                q->last_lvl = next_rr_vcg;
    //printf("================================== GOTTEN ========================================\n");
                return k;
            }
        }
        next_rr_vcg = (next_rr_vcg + 1) % num_qos_levels;
        assert(next_rr_vcg < num_qos_levels);
    }
    #if DEBUG_QOS_X == 1
    printf("[%.0lf] qos_send_excess %c:%d port:%d ----  (no data to send)\n", tw_now(lp), 
            node_type, node_id, port);
    #endif
    //printf("================================== GOTTEN ========================================\n");

    return -1;
}


/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 * End:
 *
 * vim: ft=c ts=4 sts=4 sw=4 expandtab
 */
