/*
 * Copyright (C) 1994-2020 Altair Engineering, Inc.
 * For more information, contact Altair at www.altair.com.
 *
 * This file is part of both the OpenPBS software ("OpenPBS")
 * and the PBS Professional ("PBS Pro") software.
 *
 * Open Source License Information:
 *
 * OpenPBS is free software. You can redistribute it and/or modify it under
 * the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * OpenPBS is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Commercial License Information:
 *
 * PBS Pro is commercially licensed software that shares a common core with
 * the OpenPBS software.  For a copy of the commercial license terms and
 * conditions, go to: (http://www.pbspro.com/agreement.html) or contact the
 * Altair Legal Department.
 *
 * Altair's dual-license business model allows companies, individuals, and
 * organizations to create proprietary derivative works of OpenPBS and
 * distribute them - whether embedded or bundled with other software -
 * under a commercial license agreement.
 *
 * Use of Altair's trademarks, including but not limited to "PBS™",
 * "OpenPBS®", "PBS Professional®", and "PBS Pro™" and Altair's logos is
 * subject to Altair's trademark licensing policies.
 */

/**
 * @file	pbsD_selectj.c
 * @brief
 *	This file contines two main library entries:
 *		pbs_selectjob()
 *		pbs_selstat()
 *
 *
 *	pbs_selectjob() - the SelectJob request
 *		Return a list of job ids that meet certain selection criteria.
 */

#include <pbs_config.h>   /* the master config generated by configure */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "libpbs.h"
#include "dis.h"
#include "pbs_ecl.h"

struct reply_list {
	struct batch_reply *reply;
	struct reply_list *next;
	struct reply_list *last;
};

/**
 * @brief
 *	-reads selectjob reply from stream
 *
 * @param[in] c - communication handle
 * @param[in] jlist - structure which holds job list and related counters.
 *
 * @return	string list
 * @retval	list of strings		success
 * @retval	NULL			error
 *
 */
static void
PBSD_select_get(int c, struct reply_list **rlist)
{
	struct batch_reply *reply;

	/* read reply from stream */

	reply = PBSD_rdrpy(c);
	if (reply == NULL) {
		pbs_errno = PBSE_PROTOCOL;
	} else if (reply->brp_choice != BATCH_REPLY_CHOICE_NULL &&
		   reply->brp_choice != BATCH_REPLY_CHOICE_Text &&
		   reply->brp_choice != BATCH_REPLY_CHOICE_Select) {
		pbs_errno = PBSE_PROTOCOL;
	} else if (get_conn_errno(c) == 0) {
		if (reply->brp_un.brp_select == NULL) {
			PBSD_FreeReply(reply);
			return;
		}

		struct reply_list *new = malloc(sizeof(struct reply_list));
		new->reply = reply;
		new->next = NULL;
		new->last = new;
		if (*rlist) {
			(*rlist)->last->next = new;
			(*rlist)->last = new;
		} else
			*rlist = new;
	}
}

char **
reply_to_jobarray(struct reply_list *rlist) {
	int i;
	int   njobs;
	char *sp;
	int   stringtot;
	size_t totsize;
	char **retval = NULL;
	struct reply_list *cur;
	struct brp_select *sr = NULL;

	if (!rlist)
		return NULL;

	stringtot = 0;
	njobs = 0;
	cur = rlist;
	sr = cur->reply->brp_un.brp_select;
	while (sr) {
		stringtot += strlen(sr->brp_jobid) + 1;
		njobs++;
		sr = sr->brp_next;
		if (!sr && cur->next) {
			cur = cur->next;
			sr = cur->reply->brp_un.brp_select;
		}
	}

	totsize = stringtot + (njobs + 1) * (sizeof(char *));
	retval = (char **)malloc(totsize);
	if (retval == NULL) {
		pbs_errno = PBSE_SYSTEM;
		return NULL;
	}
	cur = rlist;
	sr = cur->reply->brp_un.brp_select;
	sp = (char *)retval + (njobs + 1) * sizeof(char *);
	for (i = 0; i < njobs; i++) {
		retval[i] = sp;
		strcpy(sp, sr->brp_jobid);
		sp += strlen(sp) + 1;
		sr = sr->brp_next;
		if (!sr && cur->next) {
			cur = cur->next;
			sr = cur->reply->brp_un.brp_select;
		}
	}
	retval[i] = NULL;

	return retval;
}

/**
 * @brief
 *	-the SelectJob request
 *	Return a list of job ids that meet certain selection criteria.
 *
 * @param[in] c - communication handle
 * @param[in] attrib - pointer to attropl structure(selection criteria)
 * @param[in] extend - extend string to encode req
 *
 * @return	string
 * @retval	job ids		success
 * @retval	NULL		error
 *
 */
char **
__pbs_selectjob(int c, struct attropl *attrib, char *extend)
{
	char **ret = NULL;
	int i;
	struct reply_list *rlist = NULL;
	struct reply_list *cur;
	svr_conn_t *svr_connections = get_conn_servers();
	int num_cfg_svrs = get_num_servers();
	int *failed_conn;
	int rc = 0;

	if (!svr_connections)
		return NULL;

	failed_conn = calloc(num_cfg_svrs, sizeof(int));

	if (pbs_client_thread_init_thread_context() != 0)
		return NULL;

	if (pbs_verify_attributes(random_srv_conn(svr_connections), PBS_BATCH_SelectJobs, MGR_OBJ_JOB, MGR_CMD_NONE, attrib) != 0)
		return NULL;

	if (pbs_client_thread_lock_connection(c) != 0)
		return NULL;

	for (i = 0; i <num_cfg_svrs; i++) {

		if (svr_connections[i].state != SVR_CONN_STATE_UP) {
			pbs_errno = PBSE_NOSERVER;
			continue;
		}

		if (pbs_client_thread_lock_connection(c) != 0)
			goto done;

		if ((rc = PBSD_select_put(svr_connections[i].sd, PBS_BATCH_SelectJobs, attrib, NULL, extend)) != 0) {
			failed_conn[i] = 1;
			continue;
		}
	}

	for (i = 0; i < num_cfg_svrs; i++) {

		if (svr_connections[i].state != SVR_CONN_STATE_UP)
			continue;

		if (failed_conn[i])
			continue;

		PBSD_select_get(svr_connections[i].sd, &rlist);

	}

	if (pbs_client_thread_unlock_connection(c) != 0)
		goto done;

	if (rc)
		pbs_errno = rc;

	free(failed_conn);

done:
	ret = reply_to_jobarray(rlist);
	while (rlist) {
		PBSD_FreeReply(rlist->reply);
		cur = rlist;
		rlist = rlist->next;
		free(cur);
	}

	return ret;
}

/**
 * @brief
 * 	-pbs_selstat() - Selectable status
 *	Return status information for jobs that meet certain selection
 *	criteria.  This is a short-cut combination of pbs_selecljob()
 *	and repeated pbs_statjob().
 *
 * @param[in] c - communication handle
 * @param[in] attrib - pointer to attropl structure(selection criteria)
 * @param[in] extend - extend string to encode req
 * @param[in] rattrib - list of attributes to return
 *
 * @return      structure handle
 * @retval      list of attr	success
 * @retval      NULL		error
 *
 */

struct batch_status *
__pbs_selstat(int c, struct attropl *attrib, struct attrl *rattrib, char *extend)
{
	return PBSD_status_aggregate(c, PBS_BATCH_SelStat, NULL, attrib, extend, MGR_OBJ_JOB, rattrib);
}


/**
 * @brief
 *	-encode and puts selectjob request  data
 *
 * @param[in] c - communication handle
 * @param[in] type - type of request
 * @param[in] attrib - pointer to attropl structure(selection criteria)
 * @param[in] extend - extend string to encode req
 * @param[in] rattrib - list of attributes to return
 *
 * @return      int
 * @retval      0	success
 * @retval      !0	error
 *
 */
int
PBSD_select_put(int c, int type, struct attropl *attrib,
			struct attrl *rattrib, char *extend)
{
	int rc;

	/* setup DIS support routines for following DIS calls */

	DIS_tcp_funcs();

	if ((rc = encode_DIS_ReqHdr(c, type, pbs_current_user)) ||
		(rc = encode_DIS_attropl(c, attrib)) ||
		(rc = encode_DIS_attrl(c, rattrib))  ||
		(rc = encode_DIS_ReqExtend(c, extend))) {
		if (set_conn_errtxt(c, dis_emsg[rc]) != 0) {
			pbs_errno = PBSE_SYSTEM;
		} else {
			pbs_errno = PBSE_PROTOCOL;
		}
		return (pbs_errno);
	}

	/* write data */

	if (dis_flush(c)) {
		return (pbs_errno = PBSE_PROTOCOL);
	}

	return 0;
}
