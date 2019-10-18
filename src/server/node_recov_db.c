/*
 * Copyright (C) 1994-2018 Altair Engineering, Inc.
 * For more information, contact Altair at www.altair.com.
 *
 * This file is part of the PBS Professional ("PBS Pro") software.
 *
 * Open Source License Information:
 *
 * PBS Pro is free software. You can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option) any
 * later version.
 *
 * PBS Pro is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.
 * See the GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Commercial License Information:
 *
 * For a copy of the commercial license terms and conditions,
 * go to: (http://www.pbspro.com/UserArea/agreement.html)
 * or contact the Altair Legal Department.
 *
 * Altair’s dual-license business model allows companies, individuals, and
 * organizations to create proprietary derivative works of PBS Pro and
 * distribute them - whether embedded or bundled with other software -
 * under a commercial license agreement.
 *
 * Use of Altair’s trademarks, including but not limited to "PBS™",
 * "PBS Professional®", and "PBS Pro™" and Altair’s logos is subject to Altair's
 * trademark licensing policies.
 *
 */


/**
 * @file    node_recov_db.c
 *
 * @brief
 *		node_recov_db.c - This file contains the functions to record a node
 *		data structure to database and to recover it from database.
 *
 * Included functions are:
 *	node_save_db()
 *	db_to_svr_node()
 *	svr_to_db_node()
 *	node_recov_db_raw()
 *	node_delete_db()
 *	node_recov_db()
 */


#include <pbs_config.h>   /* the master config generated by configure */

#include <stdio.h>
#include <sys/types.h>

#ifndef WIN32
#include <sys/param.h>
#endif

#include "pbs_ifl.h"
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include <unistd.h>
#include "server_limits.h"
#include "list_link.h"
#include "attribute.h"

#ifdef WIN32
#include <sys/stat.h>
#include <io.h>
#include <windows.h>
#include "win.h"
#endif

#include "log.h"
#include "attribute.h"
#include "list_link.h"
#include "server_limits.h"
#include "credential.h"
#include "libpbs.h"
#include "batch_request.h"
#include "pbs_nodes.h"
#include "job.h"
#include "resource.h"
#include "reservation.h"
#include "queue.h"
#include "svrfunc.h"
#include <memory.h>
#include "libutil.h"
#include "pbs_db.h"

#ifdef NAS /* localmod 005 */
/* External Functions Called */
extern int recov_attr_db(pbs_db_conn_t *conn,
	void *parent,
	pbs_db_attr_info_t *p_attr_info,
	struct attribute_def *padef,
	struct attribute *pattr,
	int limit,
	int unknown);
extern int recov_attr_db_raw(pbs_db_conn_t *conn,
	pbs_db_attr_info_t *p_attr_info,
	pbs_list_head *phead);
#endif /* localmod 005 */


extern int make_pbs_list_attr_db(void *parent, pbs_db_attr_list_t *attr_list, struct attribute_def *padef, pbs_list_head *phead, int limit, int unknown);
/**
 * @brief
 *		Load a server node object from a database node object
 *
 * @param[out]	pnode - Address of the node in the server
 * @param[in]	pdbnd - Address of the database node object
 *
 * @return	Error code
 * @retval   0 - Success
 * @retval  -1 - Failure
 *
 */
static int
db_to_svr_node(struct pbsnode *pnode, pbs_db_node_info_t *pdbnd)
{
	DBPRT(("Entering %s", __func__))
	if (pdbnd->nd_name && pdbnd->nd_name[0] != 0) {
		pnode->nd_name = strdup(pdbnd->nd_name);
		if (pnode->nd_name == NULL)
			return -1;
	}
	else
		pnode->nd_name = NULL;


	if (pdbnd->nd_hostname && pdbnd->nd_hostname[0]!=0) {
		pnode->nd_hostname = strdup(pdbnd->nd_hostname);
		if (pnode->nd_hostname == NULL)
			return -1;
	}
	else
		pnode->nd_hostname = NULL;

	pnode->nd_ntype = pdbnd->nd_ntype;
	pnode->nd_state = pdbnd->nd_state;
	if (pnode->nd_pque)
		strcpy(pnode->nd_pque->qu_qs.qu_name, pdbnd->nd_pque);

	strcpy(pnode->nd_savetm, pdbnd->nd_savetm);

	if ((decode_attr_db(pnode, &pdbnd->attr_list, node_attr_def,
		pnode->nd_attr, (int) ND_ATR_LAST, 0)) != 0)
		return -1;

	return 0;
}


/**
 * @brief
 *		Recover a node from the database
 *
 * @param[in]	nd_name	- node name
 * @param[in]	pnode	- node object pointer
 * @param[in]	lock	- whether db row has to be locked
 *
 * @return	The recovered node structure
 * @retval	NULL - Failure
 * @retval	!NULL - Success - address of recovered node returned
 */
struct pbsnode *
node_recov_db(char *nd_name, struct pbsnode *pnode, int lock)
{
	pbs_db_obj_info_t obj;
	pbs_db_conn_t *conn = (pbs_db_conn_t *) svr_db_conn;
	pbs_db_node_info_t dbnode;
	int rc = 0;

	DBPRT(("Inside node_recov_db"))

	strcpy(dbnode.nd_name, nd_name);
	dbnode.nd_savetm[0] = '\0';
	dbnode.attr_list.attributes = NULL;

	if (!pnode) {
		pnode = malloc(sizeof(struct pbsnode));
		initialize_pbsnode(pnode, nd_name, NTYPE_PBS);
	} else {
		if (memcache_good(&pnode->trx_status, lock))
			return pnode;
		strcpy(dbnode.nd_savetm, pnode->nd_savetm);
	}
	
	if (pnode == NULL) {
		log_err(errno, "node_recov", "error on recovering node table");
		return NULL;
	}

	obj.pbs_db_obj_type = PBS_DB_NODE;
	obj.pbs_db_un.pbs_db_node = &dbnode;

	if (pbs_db_begin_trx(conn, 0, 0) != 0)
		goto db_err;

	if ((rc = pbs_db_load_obj(conn, &obj, lock)) == -1)
		goto db_err;

	if (rc == -2)
		goto db_commit;

	if (db_to_svr_node(pnode, &dbnode) != 0)
		goto db_err;

db_commit:
	if (lock && pnode) {
		pnode->nd_modified |= NODE_LOCKED;
		memcache_update_state(&pnode->trx_status, lock);
	} else
		pbs_db_end_trx(conn, PBS_DB_COMMIT);

	pbs_db_reset_obj(&obj);

	return pnode;

db_err:
	free(pnode);
	log_err(-1, __func__, "error on recovering node");
	(void) pbs_db_end_trx(conn, PBS_DB_ROLLBACK);
	return NULL;
}

/**
 * @brief
 *		Load a database node object from a server node object
 *
 * @param[in]	pnode - Address of the node in the server
 * @param[out]	pdbnd - Address of the database node object
 *
 * @return 0    Success
 * @return !=0  Failure
 */
static int
svr_to_db_node(struct pbsnode *pnode, pbs_db_node_info_t *pdbnd)
{
	int j, wrote_np = 0;
	svrattrl *psvrl;
	pbs_list_head wrtattr;
	pbs_db_attr_info_t *attrs = NULL;
	int numattr = 0;
	int count = 0;
	int vnode_sharing = 0;

	if (pnode->nd_name)
		strcpy(pdbnd->nd_name, pnode->nd_name);
	else
		pdbnd->nd_name[0]='\0';

	/* node_index is used to sort vnodes upon recovery.
	 * For Cray multi-MoM'd vnodes, we ensure that natural vnodes come
	 * before the vnodes that it manages by introducing offsetting all
	 * non-natural vnodes indices to come after natural vnodes.
	 */
	pdbnd->nd_index = (pnode->nd_nummoms * svr_totnodes) + pnode->nd_index;

	if (pnode->nd_hostname)
		strcpy(pdbnd->nd_hostname, pnode->nd_hostname);
	else
		pdbnd->nd_hostname[0]=0;

	if (pnode->nd_moms && pnode->nd_moms[0])
		pdbnd->mom_modtime = pnode->nd_moms[0]->mi_modtime;
	else
		pdbnd->mom_modtime = 0;

	pdbnd->nd_ntype = pnode->nd_ntype;
	pdbnd->nd_state = pnode->nd_state;
	if (pnode->nd_pque)
		strcpy(pdbnd->nd_pque, pnode->nd_pque->qu_qs.qu_name);
	else
		pdbnd->nd_pque[0]=0;

	/*
	 * node attributes are saved in a different way than attributes of other objects
	 * for other objects we directly call save_attr_db, but for node attributes
	 * we massage some of the attributes. The special ones are pcpus
	 * and resv_enable and sharing.
	 */
	CLEAR_HEAD(wrtattr);

	for (j = 0; j < ND_ATR_LAST; ++j) {
		/* skip certain ones: no-save values */
		if (node_attr_def[j].at_flags & ATR_DFLAG_NOSAVM)
			continue;

		(void) node_attr_def[j].at_encode(&pnode->nd_attr[j],
		                                  &wrtattr,
		                                  node_attr_def[j].at_name,
		                                  NULL,
		                                  ATR_ENCODE_SVR,
		                                  NULL);

		node_attr_def[j].at_flags &= ~ATR_VFLAG_MODIFY;
	}

	vnode_sharing = ((pnode->nd_attr[ND_ATR_Sharing].at_flags & ATR_VFLAG_SET)
				&& (pnode->nd_attr[ND_ATR_Sharing].at_val.at_long != VNS_UNSET));
	numattr = vnode_sharing;
	psvrl = (svrattrl *)GET_NEXT(wrtattr);
	while (psvrl) {
		if ((strcmp(psvrl->al_name, ATTR_rescavail) == 0) &&
				(strcmp(psvrl->al_resc, "ncpus") == 0)) {
			wrote_np = 1;
		}
		psvrl = (svrattrl *)GET_NEXT(psvrl->al_link);
		numattr++;
	}
	if (wrote_np == 0) {
		numattr++;
	}
	pdbnd->attr_list.attributes = calloc(sizeof(pbs_db_attr_info_t), numattr);
	if (!pdbnd->attr_list.attributes)
			return -1;
	pdbnd->attr_list.attr_count = 0;
	attrs = pdbnd->attr_list.attributes;

	while ((psvrl = (svrattrl *) GET_NEXT(wrtattr)) != NULL) {

		if (wrote_np == 0 && strcmp(psvrl->al_name, ATTR_NODE_pcpus) == 0) {
			/* don't write out pcpus at this point, see */
			/* check for pcpus if needed after loop end */
			delete_link(&psvrl->al_link);
			(void) free(psvrl);

			continue;
		}

		/* every attribute to this point we write to database */
		attrs[count].attr_name[sizeof(attrs[count].attr_name) - 1] = '\0';
		strncpy(attrs[count].attr_name, psvrl->al_name, sizeof(attrs[count].attr_name));
		if (psvrl->al_resc) {
			attrs[count].attr_resc[sizeof(attrs[count].attr_resc) - 1] = '\0';
			strncpy(attrs[count].attr_resc, psvrl->al_resc, sizeof(attrs[count].attr_resc));
		}
		else
			strcpy(attrs[count].attr_resc, "");
		attrs[count].attr_value = strdup(psvrl->al_value);
		attrs[count].attr_flags = psvrl->al_flags;
		count++;

		delete_link(&psvrl->al_link);
		(void)free(psvrl);
	}

	/*
	 * Attributes with default values are not general saved to disk.
	 * However to deal with some special cases, things needed for
	 * attaching jobs to the vnodes on recover that we don't have
	 * except after we hear from Mom, i.e. we :
	 * 1. Need number of cpus, if it isn't writen as a non-default, as
	 *    "np", then write "pcpus" which will be treated as a default
	 * 2. Need the "sharing" attribute written even if default
	 *    and not the default value (i.e. it came from Mom).
	 *    so save it as the "special" [sharing] when it is a default
	 */
	if (wrote_np == 0) {
		char pcpu_str[10];
		/* write the default value for the num of cpus */
		attrs[count].attr_name[sizeof(attrs[count].attr_name) - 1] = '\0';
		strncpy(attrs[count].attr_name, ATTR_NODE_pcpus, sizeof(attrs[count].attr_name));
		strcpy(attrs[count].attr_resc, "");
		sprintf(pcpu_str, "%ld", pnode->nd_nsn);
		attrs[count].attr_value = strdup(pcpu_str);
		attrs[count].attr_flags = ATR_VFLAG_SET;
		count++;
	}

	if (vnode_sharing) {
		char *vn_str;
		vn_str = vnode_sharing_to_str((enum vnode_sharing) pnode->nd_attr[ND_ATR_Sharing].at_val.at_long);

		attrs[count].attr_name[sizeof(attrs[count].attr_name) - 1] = '\0';
		strncpy(attrs[count].attr_name, ATTR_NODE_Sharing, sizeof(attrs[count].attr_name));
		strcpy(attrs[count].attr_resc, "");
		attrs[count].attr_value = strdup(vn_str);
		attrs[count].attr_flags = pnode->nd_attr[ND_ATR_Sharing].at_flags;
		count++;
	}
	pdbnd->attr_list.attr_count = count;

	pnode->nd_modified &= ~NODE_UPDATE_OTHERS;

	return 0;
}


int
node_recov_db_raw(void *nd, pbs_list_head *phead)
{
	pbs_db_node_info_t *dbnode =(pbs_db_node_info_t *) nd;

	/* now convert attributes array to pbs list structure */
	if ((make_pbs_list_attr_db(nd, &dbnode->attr_list, node_attr_def,
		phead, (int) ND_ATR_LAST, 0)) != 0)
		return -1;

	return 0;
}


/**
 * @brief
 *	Save a node to the database. When we save a node to the database, delete
 *	the old node information and write the node afresh. This ensures that
 *	any deleted attributes of the node are removed, and only the new ones are
 *	updated to the database.
 *
 * @param[in]	pnode - Pointer to the node to save
 *
 * @return      Error code
 * @retval	0 - Success
 * @retval	-1 - Failure
 *
 */
int
node_save_db(struct pbsnode *pnode)
{
	pbs_db_node_info_t dbnode;
	pbs_db_obj_info_t obj;
	pbs_db_conn_t *conn = (pbs_db_conn_t *) svr_db_conn;

	DBPRT(("Entering %s", __func__))

	svr_to_db_node(pnode, &dbnode);
	obj.pbs_db_obj_type = PBS_DB_NODE;
	obj.pbs_db_un.pbs_db_node = &dbnode;

	if (pbs_db_save_obj(conn, &obj, PBS_UPDATE_DB_FULL) != 0) {
		if (pbs_db_save_obj(conn, &obj, PBS_INSERT_DB) != 0) {
			goto db_err;
		}
	}

	strcpy(pnode->nd_savetm, dbnode.nd_savetm);

	pbs_db_reset_obj(&obj);
	pnode->nd_modified &= ~NODE_UPDATE_OTHERS;
	if (pnode->nd_modified & NODE_LOCKED) {
		if (pbs_db_end_trx(conn, PBS_DB_COMMIT) != 0)
			goto db_err;
		pnode->nd_modified &= ~NODE_LOCKED;
	}

	return (0);
db_err:
	strcpy(log_buffer, "node_save failed ");
	if (conn->conn_db_err != NULL)
		strncat(log_buffer, conn->conn_db_err, LOG_BUF_SIZE - strlen(log_buffer) - 1);
	log_err(-1, "node_save_db", log_buffer);
	panic_stop_db(log_buffer);
	return (-1);
}



/**
 * @brief
 *	Delete a node from the database
 *
 * @param[in]	pnode - Pointer to the node to delete
 *
 * @return      Error code
 * @retval	0 - Success
 * @retval	-1 - Failure
 *
 */
int
node_delete_db(struct pbsnode *pnode)
{
	pbs_db_node_info_t dbnode;
	pbs_db_obj_info_t obj;
	pbs_db_conn_t *conn = (pbs_db_conn_t *) svr_db_conn;

	dbnode.nd_name[sizeof(dbnode.nd_name) - 1] = '\0';
	strncpy(dbnode.nd_name, pnode->nd_name, sizeof(dbnode.nd_name));
	obj.pbs_db_obj_type = PBS_DB_NODE;
	obj.pbs_db_un.pbs_db_node = &dbnode;

	if (pbs_db_delete_obj(conn, &obj) == -1)
		return (-1);
	else
		return (0);	/* "success" or "success but rows deleted" */
}
