
/*
 * sophia database
 * sphia.org
 *
 * Copyright (c) Dmitry Simonenko
 * BSD License
*/

#include <libsr.h>
#include <libsv.h>
#include <libsm.h>
#include <libsl.h>
#include <libsd.h>
#include <libsi.h>
#include <libso.h>
#include <sophia.h>

int so_dbctl_init(sodbctl *c, char *name, void *db)
{
	memset(c, 0, sizeof(*c));
	sodb *o = db;
	c->name = sr_strdup(&o->e->a, name);
	if (srunlikely(c->name == NULL))
		return -1;
	c->parent           = db;
	c->logdir_create    = 1;
	c->logdir_write     = 1;
	c->logdir_rotate_wm = 500000;
	c->dir_create       = 1;
	c->dir_write        = 1;
	c->cmp.cmp          = sr_cmpstring;
	c->cmp.cmparg       = NULL;
	c->memory_limit     = 0;
	c->node_size        = 128 * 1024 * 1024; 
	c->node_page_size   = 128 * 1024;
	c->node_branch_wm   = 10 * 1024 * 1024;
	c->node_merge_wm    = 1;
	c->threads_merge    = 2;
	c->threads_branch   = 1;
	c->threads          = 3;
	return 0;
}

int so_dbctl_free(sodbctl *c)
{
	sodb *o = c->parent;
	if (so_dbactive(o))
		return -1;
	if (c->name) {
		sr_free(&o->e->a, c->name);
		c->name = NULL;
	}
	if (c->dir) {
		sr_free(&o->e->a, c->dir);
		c->dir = NULL;
	}
	if (c->logdir) {
		sr_free(&o->e->a, c->logdir);
		c->logdir = NULL;
	}
	return 0;
}

int so_dbctl_validate(sodbctl *c)
{
	if (c->dir == NULL)
		return -1;
	return 0;
}

static int
so_dbctl_branch(srctl *c srunused, void *arg, va_list args srunused)
{
	sodb *db = arg;
	sdc dc;
	sd_cinit(&dc, &db->r);
	int rc;
	while (1) {
		uint64_t lsvn = sm_lsvn(&db->mvcc);
		rc = si_branch(&db->index, &db->r, &dc, lsvn, db->ctl.node_branch_wm);
		if (srunlikely(rc <= 0))
			break;
	}
	sd_cfree(&dc, &db->r);
	return rc;
}

static int
so_dbctl_merge(srctl *c srunused, void *arg, va_list args srunused)
{
	sodb *db = arg;
	sdc dc;
	sd_cinit(&dc, &db->r);
	int rc;
	while (1) {
		uint64_t lsvn = sm_lsvn(&db->mvcc);
		rc = si_merge(&db->index, &db->r, &dc, lsvn, db->ctl.node_merge_wm);
		if (srunlikely(rc <= 0))
			break;
	}
	sd_cfree(&dc, &db->r);
	return rc;
}

static int
so_dbctl_logrotate(srctl *c srunused, void *arg, va_list args srunused)
{
	sodb *db = arg;
	return sl_poolrotate(&db->lp);
}

static int
so_dbctl_cmp(srctl *c srunused, void *arg, va_list args)
{
	sodb *db = arg;
	db->ctl.cmp.cmp = va_arg(args, srcmpf);
	return 0;
}

static int
so_dbctl_cmparg(srctl *c srunused, void *arg, va_list args srunused)
{
	sodb *db = arg;
	db->ctl.cmp.cmparg = va_arg(args, void*);
	return 0;
}

static inline void
so_dbctl_prepare(srctl *t, sodbctl *c)
{
	srctl *p = t;
	p = sr_ctladd(p, "name",           SR_CTLSTRING|SR_CTLRO, &c->name,           NULL);
	p = sr_ctladd(p, "dir",            SR_CTLSTRING,          &c->dir,            NULL);
	p = sr_ctladd(p, "dir_read",       SR_CTLINT,             &c->dir_read,       NULL);
	p = sr_ctladd(p, "dir_write",      SR_CTLINT,             &c->dir_write,      NULL);
	p = sr_ctladd(p, "dir_create",     SR_CTLINT,             &c->dir_create,     NULL);
	p = sr_ctladd(p, "logdir",         SR_CTLSTRING,          &c->logdir,         NULL);
	p = sr_ctladd(p, "logdir_read",    SR_CTLINT,             &c->logdir_read,    NULL);
	p = sr_ctladd(p, "logdir_write",   SR_CTLINT,             &c->logdir_write,   NULL);
	p = sr_ctladd(p, "logdir_create",  SR_CTLINT,             &c->logdir_create,  NULL);
	p = sr_ctladd(p, "node_size",      SR_CTLINT,             &c->node_size,      NULL);
	p = sr_ctladd(p, "node_page_size", SR_CTLINT,             &c->node_page_size, NULL);
	p = sr_ctladd(p, "node_branch_wm", SR_CTLINT,             &c->node_branch_wm, NULL);
	p = sr_ctladd(p, "node_merge_wm",  SR_CTLINT,             &c->node_merge_wm,  NULL);
	p = sr_ctladd(p, "threads",        SR_CTLINT,             &c->threads,        NULL);
	p = sr_ctladd(p, "memory_limit",   SR_CTLU64,             &c->memory_limit,   NULL);
	p = sr_ctladd(p, "cmp",            SR_CTLTRIGGER,         NULL,               so_dbctl_cmp);
	p = sr_ctladd(p, "cmp_arg",        SR_CTLTRIGGER,         NULL,               so_dbctl_cmparg);
	p = sr_ctladd(p, "run_branch",     SR_CTLTRIGGER,         NULL,               so_dbctl_branch);
	p = sr_ctladd(p, "run_merge",      SR_CTLTRIGGER,         NULL,               so_dbctl_merge);
	p = sr_ctladd(p, "run_logrotate",  SR_CTLTRIGGER,         NULL,               so_dbctl_logrotate);
	p = sr_ctladd(p, "profiler",       SR_CTLSUB,             NULL,               NULL);
	p = sr_ctladd(p,  NULL,            0,                     NULL,               NULL);
}

int so_dbctl_set(sodbctl *c, char *path, va_list args)
{
	sodb *db = c->parent;
	srctl ctls[30];
	so_dbctl_prepare(&ctls[0], c);
	srctl *match = NULL;
	int rc = sr_ctlget(&ctls[0], &path, &match);
	if (srunlikely(rc ==  1))
		return 0; /* self */
	if (srunlikely(rc == -1))
		return -1;
	if (so_dbactive(db))
		if (match->type != SR_CTLTRIGGER)
			return -1;
	rc = sr_ctlset(match, db->r.a, db, args);
	if (srunlikely(rc == -1))
		return -1;
	return 0;
}

static inline void
so_dbprofiler_prepare(srctl *t, siprofiler *pf)
{
	srctl *p = t;
	p = sr_ctladd(p, "total_node_count",   SR_CTLU32|SR_CTLRO, &pf->total_node_count,   NULL);
	p = sr_ctladd(p, "total_node_size",    SR_CTLU64|SR_CTLRO, &pf->total_node_size,    NULL);
	p = sr_ctladd(p, "total_branch_count", SR_CTLU32|SR_CTLRO, &pf->total_branch_count, NULL);
	p = sr_ctladd(p, "total_branch_max",   SR_CTLU32|SR_CTLRO, &pf->total_branch_max,   NULL);
	p = sr_ctladd(p, "total_branch_size",  SR_CTLU64|SR_CTLRO, &pf->total_branch_size,  NULL);
	p = sr_ctladd(p, "memory_used",        SR_CTLU64|SR_CTLRO, &pf->memory_used,        NULL);
	p = sr_ctladd(p, "count",              SR_CTLU64|SR_CTLRO, &pf->count,              NULL);
	p = sr_ctladd(p,  NULL,                0,                  NULL,                    NULL);
}

static void*
so_dbprofiler_get(sodb *db, char *path)
{
	siprofiler pf;
	si_profilerbegin(&pf, &db->index);
	si_profiler(&pf);
	si_profilerend(&pf);
	srctl ctls[30];
	so_dbprofiler_prepare(&ctls[0], &pf);
	srctl *match = NULL;
	int rc = sr_ctlget(&ctls[0], &path, &match);
	if (srunlikely(rc ==  1))
		return NULL;
	if (srunlikely(rc == -1))
		return NULL;
	return so_ctlreturn(match, db->e);
}

void *so_dbctl_get(sodbctl *c, char *path, va_list args srunused)
{
	sodb *db = c->parent;
	srctl ctls[30];
	so_dbctl_prepare(&ctls[0], c);
	srctl *match = NULL;
	int rc = sr_ctlget(&ctls[0], &path, &match);
	if (srunlikely(rc ==  1))
		return &db->o; /* self */
	if (srunlikely(rc == -1))
		return NULL;
	int type = match->type & ~SR_CTLRO;
	if (type == SR_CTLSUB) {
		if (strcmp(match->name, "profiler") == 0)
			return so_dbprofiler_get(db, path);
		return NULL;
	}
	return so_ctlreturn(match, db->e);
}