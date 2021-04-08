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


/*	pbsD_modify_resv.c
 *
 *	The Modify Reservation request.
 */

#include <pbs_config.h>   /* the master config generated by configure */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include "libpbs.h"
#include "pbs_ecl.h"

/**
 * @brief
 *	Passes modify reservation request to PBSD_modify_resv( )
 *
 * @param[in]   c - socket on which connected
 * @param[in]   attrib - the list of attributes for batch request
 * @param[in]   extend - extension of batch request
 *
 * @return char*
 * @retval SUCCESS returns the response from the server.
 * @retval ERROR NULL
 */
char *
pbs_modify_resv(int c, char *resv_id, struct attropl *attrib, char *extend)
{
	struct attropl *pal = NULL;
	int rc = 0;
	char *ret = NULL;
	int i;
	svr_conn_t **svr_conns = get_conn_svr_instances(c);
	int start = 0;
	int ct;

	for (pal = attrib; pal; pal = pal->next)
		pal->op = SET;

	/* first verify the attributes, if verification is enabled */
	rc = pbs_verify_attributes(random_srv_conn(c, svr_conns), PBS_BATCH_ModifyResv,
		MGR_OBJ_RESV, MGR_CMD_NONE, attrib);
	if (rc)
		return NULL;

	if (svr_conns) {
		/*
		 * For a single server cluster, instance fd and cluster fd are the same. 
		 * Hence breaking the loop.
		 */
		if (svr_conns[0]->sd == c)
			/* initiate the modification of the reservation  */
			return PBSD_modify_resv(c, resv_id, attrib, extend);

		if ((start = get_obj_location_hint(resv_id, MGR_OBJ_RESV)) == -1)
			start = 0;

		for (i = start, ct = 0; ct < NSVR; i = (i + 1) % NSVR, ct++) {

			if (!svr_conns[i] || svr_conns[i]->state != SVR_CONN_STATE_UP)
				continue;
			
			/* initiate the modification of the reservation  */
			ret = PBSD_modify_resv(svr_conns[i]->sd, resv_id, attrib, extend);
			if (ret != NULL)
				break;
		
			else if (pbs_errno != PBSE_UNKRESVID)
				break;
		}

		return ret;
	}

	/* Not a cluster fd. Treat it as an instance fd */
	return PBSD_modify_resv(c, resv_id, attrib, extend);

}
