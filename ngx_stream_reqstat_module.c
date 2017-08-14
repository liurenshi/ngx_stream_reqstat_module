#include <ngx_http.h>
#include <assert.h>
#include "ngx_stream_reqstat.h"

typedef struct {
    ngx_uint_t                   recv;
    ngx_uint_t                   sent;
    ngx_array_t                  monitor_index;
    ngx_flag_t                   bypass;
} ngx_stream_reqstat_store_t;

#if 0
static ngx_stream_input_body_filter_pt  ngx_http_next_input_body_filter;
static ngx_stream_output_body_filter_pt ngx_http_next_output_body_filter;

ngx_int_t  (*ngx_http_top_header_filter) (ngx_http_request_t *r);
ngx_int_t  (*ngx_http_top_body_filter) (ngx_http_request_t *r, ngx_chain_t *ch);

ngx_int_t  (*ngx_http_top_input_body_filter) (ngx_http_request_t *r,
    ngx_buf_t *buf);
#endif

off_t  ngx_stream_reqstat_fields[12] = {
    NGX_STREAM_REQSTAT_BYTES_IN,
    NGX_STREAM_REQSTAT_BYTES_OUT,
    NGX_STREAM_REQSTAT_CONN_TOTAL,
    NGX_STREAM_REQSTAT_2XX,
    NGX_STREAM_REQSTAT_3XX,
    NGX_STREAM_REQSTAT_4XX,
    NGX_STREAM_REQSTAT_5XX,
    NGX_STREAM_REQSTAT_OTHER_STATUS,
    NGX_STREAM_REQSTAT_RT,
    NGX_STREAM_REQSTAT_UPS_REQ,
    NGX_STREAM_REQSTAT_UPS_RT,
    NGX_STREAM_REQSTAT_UPS_TRIES
};

static void *ngx_stream_reqstat_create_main_conf(ngx_conf_t *cf);
static ngx_int_t ngx_stream_reqstat_init(ngx_conf_t *cf);
static char *ngx_stream_reqstat_show(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_stream_reqstat_zone(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_stream_reqstat(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static void ngx_stream_reqstat_count(void *data, off_t offset,
    ngx_int_t incr);
static ngx_int_t ngx_stream_reqstat_init_zone(ngx_shm_zone_t *shm_zone,
    void *data);

static ngx_int_t ngx_stream_reqstat_log_handler(ngx_stream_session_t *s);
static ngx_int_t ngx_stream_reqstat_show_handler(ngx_http_request_t *r);

static ngx_stream_reqstat_rbnode_t *
    ngx_stream_reqstat_rbtree_lookup(ngx_shm_zone_t *shm_zone,
    ngx_str_t *val);
static void ngx_stream_reqstat_rbtree_insert_value(ngx_rbtree_node_t *temp,
    ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel);
static ngx_stream_reqstat_store_t *
    ngx_stream_reqstat_create_store(ngx_stream_session_t *s,
    ngx_stream_reqstat_conf_t *slcf);

static void *
ngx_stream_reqstat_create_main_conf(ngx_conf_t *cf);

static void *
ngx_stream_reqstat_create_srv_conf(ngx_conf_t *cf);

static char *
ngx_stream_reqstat_merge_srv_conf(ngx_conf_t *cf, void *parent,
    void *child);
//static ngx_int_t ngx_stream_reqstat_input_body_filter(ngx_stream_session_t *s,
//    ngx_buf_t *buf);
//static ngx_int_t ngx_stream_reqstat_output_body_filter(ngx_stream_session_t *s,
//    ngx_chain_t *in);

static ngx_command_t   ngx_stream_reqstat_commands[] = {

    { ngx_string("req_status_zone"),
      NGX_STREAM_MAIN_CONF|NGX_CONF_TAKE3,
      ngx_stream_reqstat_zone,
      0,
      0,
      NULL },

    { ngx_string("req_status"),
      NGX_STREAM_MAIN_CONF|NGX_STREAM_SRV_CONF|NGX_CONF_1MORE,
      ngx_stream_reqstat,
      NGX_STREAM_SRV_CONF_OFFSET,
      0,
      NULL },

      ngx_null_command
};



static ngx_stream_module_t  ngx_stream_reqstat_module_ctx = {
    NULL,                                  /* preconfiguration */
    ngx_stream_reqstat_init,                 /* postconfiguration */

    ngx_stream_reqstat_create_main_conf,     /* create main configuration */
    NULL,                                  /* init main configuration */

    ngx_stream_reqstat_create_srv_conf,      /* create server configuration */
    ngx_stream_reqstat_merge_srv_conf        /* merge server configuration */
};

ngx_module_t  ngx_stream_reqstat_module = {
    NGX_MODULE_V1,
    &ngx_stream_reqstat_module_ctx,          /* module context */
    ngx_stream_reqstat_commands,             /* module directives */
    NGX_STREAM_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};

static ngx_command_t ngx_stream_show_reqstatus_commands[] =
{
		{
				ngx_string("stream_req_status_show"),
				NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1|NGX_CONF_NOARGS,
				ngx_stream_reqstat_show,
				0,
				0,
				NULL
		},

		ngx_null_command
};

static ngx_http_module_t ngx_stream_show_reqstatus_module_ctx =
{
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL
};

ngx_module_t  ngx_stream_reqstat_show_module = {
    NGX_MODULE_V1,
    &ngx_stream_show_reqstatus_module_ctx, /* module context */
	ngx_stream_show_reqstatus_commands,    /* module directives */
    NGX_HTTP_MODULE,                       	   /* module type */
    NULL,                                  					   /* init master */
    NULL,                                  					   /* init module */
    NULL,                                  					   /* init process */
    NULL,                                  					   /* init thread */
    NULL,                                  					   /* exit thread */
    NULL,                                 					  /* exit process */
    NULL,                                  					  /* exit master */
    NGX_MODULE_V1_PADDING
};

ngx_stream_reqstat_conf_t *stream_reqstat_main_conf = NULL;

static void *
ngx_stream_reqstat_create_main_conf(ngx_conf_t *cf)
{
	assert(stream_reqstat_main_conf == NULL);
	stream_reqstat_main_conf = ngx_pcalloc(cf->pool, sizeof(ngx_stream_reqstat_conf_t));
	return ((void *)stream_reqstat_main_conf);
}

static void *
ngx_stream_reqstat_create_srv_conf(ngx_conf_t *cf)
{
	ngx_stream_reqstat_conf_t  *conf;

    conf = ngx_palloc(cf->pool, sizeof(ngx_stream_reqstat_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    conf->monitor = NGX_CONF_UNSET_PTR;

    return conf;
}

static char *
ngx_stream_reqstat_merge_srv_conf(ngx_conf_t *cf, void *parent,
    void *child)
{
	ngx_stream_reqstat_conf_t *conf = child;
	ngx_stream_reqstat_conf_t *prev = parent;

    ngx_conf_merge_ptr_value(conf->monitor, prev->monitor, NULL);

    return NGX_CONF_OK;
}

static ngx_int_t
ngx_stream_reqstat_init(ngx_conf_t *cf)
{
    ngx_stream_handler_pt          *h;
    ngx_stream_core_main_conf_t    *cmcf;

    cmcf = ngx_stream_conf_get_module_main_conf(cf, ngx_stream_core_module);

    h = ngx_array_push(&cmcf->phases[NGX_STREAM_LOG_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = ngx_stream_reqstat_log_handler;

//    ngx_http_next_input_body_filter = ngx_http_top_input_body_filter;
//    ngx_http_top_input_body_filter = ngx_http_reqstat_input_body_filter;

//    ngx_http_next_output_body_filter = ngx_http_top_body_filter;
//    ngx_http_top_body_filter = ngx_http_reqstat_output_body_filter;

    return NGX_OK;
}

static char *
ngx_stream_reqstat_show(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t     *clcf;

    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    clcf->handler = ngx_stream_reqstat_show_handler;

    return NGX_CONF_OK;
}


static char *
ngx_stream_reqstat_zone(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ssize_t                            size;
    ngx_str_t                         *value;
    ngx_shm_zone_t                    *shm_zone;
    ngx_stream_reqstat_ctx_t            *ctx;
    ngx_stream_compile_complex_value_t ccv;

    value = cf->args->elts;

    size = ngx_parse_size(&value[3]);
    if (size == NGX_ERROR) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid zone size \"%V\"", &value[3]);
        return NGX_CONF_ERROR;
    }

    if (size < (ssize_t) (8 * ngx_pagesize)) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "zone \"%V\" is too small", &value[1]);
        return NGX_CONF_ERROR;
    }

    ctx = ngx_pcalloc(cf->pool, sizeof(ngx_stream_reqstat_ctx_t));
    if (ctx == NULL) {
        return NGX_CONF_ERROR;
    }

    if (ngx_stream_script_variables_count(&value[2]) == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "the value \"%V\" is a constant",
                           &value[2]);
        return NGX_CONF_ERROR;
    }

    ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));

    ccv.cf = cf;
    ccv.value = &value[2];
    ccv.complex_value = &ctx->value;

    if (ngx_stream_compile_complex_value(&ccv) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    ctx->val = ngx_palloc(cf->pool, sizeof(ngx_str_t));
    if (ctx->val == NULL) {
        return NGX_CONF_ERROR;
    }
    *ctx->val = value[2];

    shm_zone = ngx_shared_memory_add(cf, &value[1], size,
                                     &ngx_stream_reqstat_module);
    if (shm_zone == NULL) {
        return NGX_CONF_ERROR;
    }

    if (shm_zone->data) {
        ctx = shm_zone->data;

        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "%V \"%V\" is already bound to value \"%V\"",
                           &cmd->name, &value[1], ctx->val);
        return NGX_CONF_ERROR;
    }

    shm_zone->init = ngx_stream_reqstat_init_zone;
    shm_zone->data = ctx;

    return NGX_CONF_OK;
}

static char *
ngx_stream_reqstat(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_str_t                    *value;
    ngx_uint_t                    i, j;
    ngx_shm_zone_t               *shm_zone, **z;
    ngx_stream_reqstat_conf_t      *smcf;
    ngx_stream_reqstat_conf_t      *sscf = conf;

    value = cf->args->elts;
    smcf = ngx_stream_conf_get_module_main_conf(cf, ngx_stream_reqstat_module);
    ngx_log_error(NGX_LOG_EMERG, cf->log, 0, "req status value: %s", value[1].data);

    if (sscf->monitor != NGX_CONF_UNSET_PTR) {
        return "is duplicate";
    }

    if (smcf->monitor == NULL) {
        smcf->monitor = ngx_array_create(cf->pool, cf->args->nelts - 1,
                                         sizeof(ngx_shm_zone_t *));
        if (smcf->monitor == NULL) {
            return NGX_CONF_ERROR;
        }
    }

    sscf->monitor = ngx_array_create(cf->pool, cf->args->nelts - 1,
                                     sizeof(ngx_shm_zone_t *));
    if (sscf->monitor == NULL) {
        return NGX_CONF_ERROR;
    }

    for (i = 1; i < cf->args->nelts; i++) {
        shm_zone = ngx_shared_memory_add(cf, &value[i], 0,
                                         &ngx_stream_reqstat_module);
        if (shm_zone == NULL) {
            return NGX_CONF_ERROR;
        }

        z = ngx_array_push(sscf->monitor);
        *z = shm_zone;

        z = smcf->monitor->elts;
        for (j = 0; j < smcf->monitor->nelts; j++) {
            if (!ngx_strcmp(value[i].data, z[j]->shm.name.data)) {
                break;
            }
        }

        if (j == smcf->monitor->nelts) {
            z = ngx_array_push(smcf->monitor);
            if (z == NULL) {
                return NGX_CONF_ERROR;
            }
            *z = shm_zone;
        }
    }

    return NGX_CONF_OK;
}

static ngx_int_t
ngx_stream_reqstat_log_handler(ngx_stream_session_t *s)
{
    ngx_uint_t                    i, j, status, utries;
    ngx_time_t                   *tp;
    ngx_msec_int_t                ms, total_ms;
    ngx_stream_reqstat_conf_t      *sscf;
    ngx_stream_reqstat_rbnode_t    *fnode, **fnode_store;
    ngx_stream_upstream_state_t    *state;
    ngx_stream_reqstat_store_t     *store;

   	sscf = ngx_stream_get_module_srv_conf(s, ngx_stream_reqstat_module);

    if (sscf->monitor == NULL) {
        return NGX_OK;
    }

   	store = ngx_stream_get_module_ctx(s, ngx_stream_reqstat_module);

    if (store == NULL) {
    	store = ngx_stream_reqstat_create_store(s, sscf);
        if (store == NULL) {
            return NGX_ERROR;
        }
        ngx_stream_set_ctx(s, store, ngx_stream_reqstat_module);
    }

    if (store->bypass) {
        return NGX_OK;
    }

    fnode_store = store->monitor_index.elts;
    for (i = 0; i < store->monitor_index.nelts; i++) {
        fnode = fnode_store[i];
        ngx_stream_reqstat_count(fnode, NGX_STREAM_REQSTAT_CONN_TOTAL, 1);
        ngx_stream_reqstat_count(fnode, NGX_STREAM_REQSTAT_BYTES_IN, s->received);
        ngx_stream_reqstat_count(fnode, NGX_STREAM_REQSTAT_BYTES_OUT, s->connection->sent);

        if (s->status) {
            status = s->status;
        } else {
            status = 0;
        }

        if (status >= 200 && status < 300) {
            ngx_stream_reqstat_count(fnode, NGX_STREAM_REQSTAT_2XX, 1);
        } else if (status >= 300 && status < 400) {
            ngx_stream_reqstat_count(fnode, NGX_STREAM_REQSTAT_3XX, 1);
        } else if (status >= 400 && status < 500) {
            ngx_stream_reqstat_count(fnode, NGX_STREAM_REQSTAT_4XX, 1);
        } else if (status >= 500 && status < 600) {
            ngx_stream_reqstat_count(fnode, NGX_STREAM_REQSTAT_5XX, 1);
        } else {
            ngx_stream_reqstat_count(fnode, NGX_STREAM_REQSTAT_OTHER_STATUS, 1);
        }

        tp = ngx_timeofday();

        ms = (ngx_msec_int_t)
             ((tp->sec - s->start_sec) * 1000 + (tp->msec - s->start_msec));
        ms = ngx_max(ms, 0);
        ngx_stream_reqstat_count(fnode, NGX_STREAM_REQSTAT_RT, ms);

        if (s->upstream_states != NULL && s->upstream_states->nelts > 0) {
            ngx_stream_reqstat_count(fnode, NGX_STREAM_REQSTAT_UPS_REQ, 1);

            j = 0;
            total_ms = 0;
            utries = 0;
            state = s->upstream_states->elts;

            for ( ;; ) {

                utries++;

               #if nginx_version <= 1009000
                ms = (ngx_msec_int_t) (state[j].response_sec * 1000
                                               + state[j].response_msec);
               #else
                ms = (ngx_msec_int_t) state[j].response_time;
               #endif
                ms = ngx_max(ms, 0);
                total_ms += ms;

                if (++j == s->upstream_states->nelts) {
                    break;
                }

                if (state[j].peer == NULL) {
                    if (++j == s->upstream_states->nelts) {
                        break;
                    }
                }
            }

            ngx_stream_reqstat_count(fnode, NGX_STREAM_REQSTAT_UPS_RT,
                                   total_ms);
            ngx_stream_reqstat_count(fnode, NGX_STREAM_REQSTAT_UPS_TRIES,
                                   utries);
        }
    }

    return NGX_OK;
}

//add by newcent@Aug 11, 2017
#include <assert.h>
static ngx_int_t ngx_stream_req_show_header_line(ngx_http_request_t *r, ngx_chain_t **free, ngx_chain_t **busy)
{
	ngx_chain_t *tl = NULL;
	ngx_buf_t	*b = NULL;
	static u_char header[] ="key\t\t\tbytes_in\tbytes_out\tconn_total\t"
	            "stat_2xx\tstatx_3xx\tstat_4xx\tstat_5xx\tstat_other\tstat_rt\tstat_ups_req\tstat_ups_rt\tstat_ups_tries\n";

	assert(*free == NULL);
	assert(*busy == NULL);

	tl = ngx_chain_get_free_buf(r->pool, free);
	if (tl == NULL)
	{
		return NGX_ERROR;
	}
	b = tl->buf;

	if (b->start == NULL)
	{
		b->start = ngx_pcalloc(r->pool, 512);
	    if (b->start == NULL)
	    {
	    	return NGX_ERROR;
	    }
	    b->end = b->start + 512;
     }

	b->last = b->pos = b->start;
    b->memory = 1;
    b->temporary = 1;

    b->last = ngx_cpymem(b->last, header, sizeof(header) - 1);

    if (ngx_http_output_filter(r, tl) == NGX_ERROR)
    {
    	return NGX_STREAM_INTERNAL_SERVER_ERROR;
    }
#if nginx_version >= 1002000
  ngx_chain_update_chains(r->pool, free, busy, &tl,
                                    (ngx_buf_tag_t) &ngx_stream_reqstat_show_module);
#else
            ngx_chain_update_chains(&free, &busy, &tl,
                                    (ngx_buf_tag_t) &ngx_stream_reqstat_show_module);
#endif
     return NGX_OK;
}

static ngx_int_t
ngx_stream_reqstat_show_handler(ngx_http_request_t *r)
{
    ngx_int_t                     rc;
    ngx_buf_t                    *b;
    ngx_uint_t                    i, j;
    ngx_array_t                  *display;
    ngx_chain_t                  *tl, *free = NULL, *busy = NULL;
    ngx_queue_t                  *q;
    ngx_shm_zone_t              **shm_zone;
    ngx_stream_reqstat_ctx_t *ctx;
    ngx_stream_reqstat_conf_t      *smcf;
    ngx_stream_reqstat_rbnode_t *node;

    smcf = stream_reqstat_main_conf;
    assert(smcf != NULL);
    display = smcf->monitor;
    if (display == NULL) {
        r->headers_out.status = NGX_HTTP_NO_CONTENT;
        return ngx_http_send_header(r);
    }

    r->headers_out.status = NGX_HTTP_OK;
    ngx_http_clear_content_length(r);

    rc = ngx_http_send_header(r);
    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
        return rc;
    }

    shm_zone = display->elts;

    if (ngx_stream_req_show_header_line(r, &free, &busy) == NGX_ERROR)
    {
    	return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    for (i = 0; i < display->nelts; i++) {

        ctx = shm_zone[i]->data;

        for (q = ngx_queue_head(&ctx->sh->queue);
             q != ngx_queue_sentinel(&ctx->sh->queue);
             q = ngx_queue_next(q))
        {
            node = ngx_queue_data(q, ngx_stream_reqstat_rbnode_t, queue);

            tl = ngx_chain_get_free_buf(r->pool, &free);
            if (tl == NULL) {
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
            }

            b = tl->buf;
            if (b->start == NULL) {
                b->start = ngx_pcalloc(r->pool, 512);
                if (b->start == NULL) {
                    return NGX_HTTP_INTERNAL_SERVER_ERROR;
                }

                b->end = b->start + 512;
            }

            b->last = b->pos = b->start;
            b->memory = 1;
            b->temporary = 1;

            b->last = ngx_slprintf(b->last, b->end, "%*s\t",
                                   (size_t) node->len, node->data);

            for (j = 0;
                 j < sizeof(ngx_stream_reqstat_fields) / sizeof(off_t);
                 j++)
            {
                b->last = ngx_slprintf(b->last, b->end, "%uA\t",
                                       *REQ_FIELD(node,
                                                  ngx_stream_reqstat_fields[j]));
            }

            *(b->last - 1) = '\n';

            if (ngx_http_output_filter(r, tl) == NGX_ERROR) {
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
            }

#if nginx_version >= 1002000
            ngx_chain_update_chains(r->pool, &free, &busy, &tl,
                                    (ngx_buf_tag_t) &ngx_stream_reqstat_show_module);
#else
            ngx_chain_update_chains(&free, &busy, &tl,
                                    (ngx_buf_tag_t) &ngx_stream_reqstat_show_module);
#endif
        }
    }

    tl = ngx_chain_get_free_buf(r->pool, &free);
    if (tl == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    b = tl->buf;
    b->last_buf = 1;

    return ngx_http_output_filter(r, tl);
}


void
ngx_stream_reqstat_count(void *data, off_t offset, ngx_int_t incr)
{
    ngx_stream_reqstat_rbnode_t    *node = data;

    (void) ngx_atomic_fetch_add(REQ_FIELD(node, offset), incr);
}

static ngx_stream_reqstat_rbnode_t *
ngx_stream_reqstat_rbtree_lookup(ngx_shm_zone_t *shm_zone, ngx_str_t *val)
{
    size_t                        size;
    uint32_t                      hash;
    ngx_int_t                     rc;
    ngx_rbtree_node_t            *node, *sentinel;
    ngx_stream_reqstat_ctx_t       *ctx;
    ngx_stream_reqstat_rbnode_t    *rs;

    ctx = shm_zone->data;

    hash = ngx_crc32_short(val->data, val->len);

    node = ctx->sh->rbtree.root;
    sentinel = ctx->sh->rbtree.sentinel;
    ngx_shmtx_lock(&ctx->shpool->mutex);

    while (node != sentinel) {

        if (hash < node->key) {
            node = node->left;
            continue;
        }

        if (hash > node->key) {
            node = node->right;
            continue;
        }

        /* hash == node->key */

        rs = (ngx_stream_reqstat_rbnode_t *) &node->color;

        rc = ngx_memn2cmp(val->data, rs->data, val->len, (size_t) rs->len);

        if (rc == 0) {
            ngx_shmtx_unlock(&ctx->shpool->mutex);
            return rs;
        }

        node = (rc < 0) ? node->left : node->right;
    }

    size = offsetof(ngx_rbtree_node_t, color)
         + offsetof(ngx_stream_reqstat_rbnode_t, data)
         + val->len;

    node = ngx_slab_alloc_locked(ctx->shpool, size);
    if (node == NULL) {
        ngx_shmtx_unlock(&ctx->shpool->mutex);
        return NULL;
    }

    node->key = hash;

    rs = (ngx_stream_reqstat_rbnode_t *) &node->color;

    rs->len = val->len;

    ngx_memcpy(rs->data, val->data, val->len);

    ngx_rbtree_insert(&ctx->sh->rbtree, node);

    ngx_queue_insert_head(&ctx->sh->queue, &rs->queue);

    ngx_shmtx_unlock(&ctx->shpool->mutex);

    return rs;
}


static ngx_int_t
ngx_stream_reqstat_init_zone(ngx_shm_zone_t *shm_zone, void *data)
{
    ngx_stream_reqstat_ctx_t       *ctx, *octx;

    octx = data;
    ctx = shm_zone->data;

    if (octx != NULL) {
        if (ngx_strcmp(ctx->val->data, octx->val->data) != 0) {
            ngx_log_error(NGX_LOG_EMERG, shm_zone->shm.log, 0,
                          "reqstat \"%V\" uses the value str \"%V\" "
                          "while previously it used \"%V\"",
                          &shm_zone->shm.name, ctx->val, octx->val);
            return NGX_ERROR;
        }

        ctx->shpool = octx->shpool;
        ctx->sh = octx->sh;

        return NGX_OK;
    }

    ctx->shpool = (ngx_slab_pool_t *) shm_zone->shm.addr;

    ctx->sh = ngx_slab_alloc(ctx->shpool, sizeof(ngx_stream_reqstat_shctx_t));
    if (ctx->sh == NULL) {
        return NGX_ERROR;
    }

    ctx->shpool->data = ctx->sh;

    ngx_rbtree_init(&ctx->sh->rbtree, &ctx->sh->sentinel,
                    ngx_stream_reqstat_rbtree_insert_value);

    ngx_queue_init(&ctx->sh->queue);

    return NGX_OK;
}


static void
ngx_stream_reqstat_rbtree_insert_value(ngx_rbtree_node_t *temp,
    ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel)
{
    ngx_rbtree_node_t          **p;
    ngx_stream_reqstat_rbnode_t   *rsn, *rsnt;

    for ( ;; ) {

        if (node->key < temp->key) {

            p = &temp->left;

        } else if (node->key > temp->key) {

            p = &temp->right;

        } else { /* node->key == temp->key */

            rsn = (ngx_stream_reqstat_rbnode_t *) &node->color;
            rsnt = (ngx_stream_reqstat_rbnode_t *) &temp->color;

            p = (ngx_memn2cmp(rsn->data, rsnt->data, rsn->len, rsnt->len) < 0)
                ? &temp->left : &temp->right;
        }

        if (*p == sentinel) {
            break;
        }

        temp = *p;
    }

    *p = node;
    node->parent = temp;
    node->left = sentinel;
    node->right = sentinel;
    ngx_rbt_red(node);
}

static ngx_stream_reqstat_store_t *
ngx_stream_reqstat_create_store(ngx_stream_session_t *s,
    ngx_stream_reqstat_conf_t *slcf)
{
    ngx_str_t                     val;
    ngx_uint_t                    i;
    ngx_shm_zone_t              **shm_zone, *z;
    ngx_stream_reqstat_ctx_t       *ctx;
    ngx_stream_reqstat_store_t     *store;
    ngx_stream_reqstat_rbnode_t    *fnode, **fnode_store;

    store = ngx_pcalloc(s->connection->pool, sizeof(ngx_stream_reqstat_store_t));
    if (store == NULL) {
        return NULL;
    }

    if (ngx_array_init(&store->monitor_index, s->connection->pool, slcf->monitor->nelts,
                       sizeof(ngx_stream_reqstat_rbnode_t *)) == NGX_ERROR)
    {
        return NULL;
    }

    shm_zone = slcf->monitor->elts;
    for (i = 0; i < slcf->monitor->nelts; i++) {
        z = shm_zone[i];
        ctx = z->data;

        if (ngx_stream_complex_value(s, &ctx->value, &val) != NGX_OK) {
            ngx_log_error(NGX_LOG_WARN, s->connection->log, 0,
                          "failed to reap the key \"%V\"", ctx->val);
            continue;
        }

        fnode = ngx_stream_reqstat_rbtree_lookup(shm_zone[i], &val);

        if (fnode == NULL) {
            ngx_log_error(NGX_LOG_WARN, s->connection->log, 0,
                          "failed to alloc node in zone \"%V\", "
                          "enlarge it please",
                          &z->shm.name);

        } else {
            fnode_store = ngx_array_push(&store->monitor_index);
            *fnode_store = fnode;
        }
    }

    return store;
}