/*
 * Copyright (C) 1994-2021 Altair Engineering, Inc.
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
 * @file	pbs_sigjob.
 */

#include <pbs_config.h>   /* the master config generated by configure */

#include <string.h>
#include <stdio.h>
#include "libpbs.h"
#include "pbs_ecl.h"

/**
 * @brief
 *	sends and reads signal job batch request.
 *
 * @param[in] c - communication handle
 * @param[in] jobid - job identifier
 * @param[in] sig - signal
 * @param[in] extend - extend string for request
 *
 * @return	int
 * @retval	0	success
 * @retval	!0	error
 *
 */
static int
__pbs_sigjob_inner(int c, char *jobid, char *sig, char *extend)
{
	int rc = 0;
	struct batch_reply *reply;

	/* initialize the thread context data, if not already initialized */
	if (pbs_client_thread_init_thread_context() != 0)
		return pbs_errno;

	/* lock pthread mutex here for this connection */
	/* blocking call, waits for mutex release */
	if (pbs_client_thread_lock_connection(c) != 0)
		return pbs_errno;

	/* send request */
	if ((rc = PBSD_sig_put(c, jobid, sig, extend, PROT_TCP, NULL)) != 0) {
		(void)pbs_client_thread_unlock_connection(c);
		return (rc);
	}

	/* read reply */
	reply = PBSD_rdrpy(c);
	PBSD_FreeReply(reply);

	rc = get_conn_errno(c);

	/* unlock the thread lock and update the thread context data */
	if (pbs_client_thread_unlock_connection(c) != 0)
		return pbs_errno;

	return (rc);
}

/**
 * @brief
 *	sends and reads signal job batch request.
 *
 * @param[in] c - communication handle
 * @param[in] jobid - job identifier
 * @param[in] sig - signal
 * @param[in] extend - extend string for request
 *
 * @return	int
 * @retval	0	success
 * @retval	!0	error
 *
 */
int
__pbs_sigjob(int c, char *jobid, char *sig, char *extend)
{
	int rc = 0;
	svr_conn_t **svr_conns = get_conn_svr_instances(c);
	int i;
	int start = 0;
	int ct;

	if ((jobid == NULL) || (*jobid == '\0') || (sig == NULL))
		return (pbs_errno = PBSE_IVALREQ);

	if (svr_conns) {
		if ((start = get_obj_location_hint(jobid, MGR_OBJ_JOB)) == -1)
			start = 0;

		for (i = start, ct = 0; ct < NSVR; i = (i + 1) % NSVR, ct++) {

			if (!svr_conns[i] || svr_conns[i]->state != SVR_CONN_STATE_UP)
				continue;

			/*
			* For a single server cluster, instance fd and cluster fd are the same. 
			* Hence breaking the loop.
			*/
			if (svr_conns[i]->sd == c)
				return __pbs_sigjob_inner(svr_conns[i]->sd,
							  jobid, sig, extend);

			rc = __pbs_sigjob_inner(svr_conns[i]->sd, jobid, sig, extend);
			if (rc == 0 || pbs_errno != PBSE_UNKJOBID)
				break;
		}

		return pbs_errno;
	}

	/* Not a cluster fd. Treat it as an instance fd */
	return __pbs_sigjob_inner(c, jobid, sig, extend);
}
