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

#ifndef _PBS_BASE_OBJ_H
#define _PBS_BASE_OBJ_H
#ifdef  __cplusplus
extern "C" {
#endif

#include "attribute.h"

enum obj_type {
	OBJ_NONE = -1,
	OBJ_SERVER,		/* Server	*/
	OBJ_QUEUE,		/* Queue	*/
	OBJ_JOB,		/* Job		*/
	OBJ_NODE,		/* Vnode  	*/
	OBJ_RESV,		/* Reservation	*/
	OBJ_RSC,		/* Resource	*/
	OBJ_SCHED,		/* Scheduler	*/
	OBJ_HOOK,		/* Hook         */
	OBJ_LAST		/* Last entry	*/
};
typedef enum obj_type obj_type_t;

void __reset_attr_flag(void *pobj, int attr_idx, int flag, obj_type_t obj_type);
void __set_attr_flag(void *pobj, int attr_idx, int flag, obj_type_t obj_type);
void __unset_attr_flag(void *pobj, int attr_idx, int flag, obj_type_t obj_type);
int __is_attr_flag_set(const void *pobj, int attr_idx, int flag, obj_type_t obj_type);
char *__get_attr_str(const void *pobj, int attr_idx, obj_type_t obj_type);
svrattrl *__get_attr_usr_encoded(const void *pobj, int attr_idx, obj_type_t obj_type);
svrattrl *__get_attr_priv_encoded(const void *pobj, int attr_idx, obj_type_t obj_type);
int __get_attr_flag(const void *pobj, int attr_idx, obj_type_t obj_type);
long __get_attr_long(const void *pobj, int attr_idx, obj_type_t obj_type);
int __set_attr_generic(void *pobj, int attr_idx, char *val, char *rscn, enum batch_op op, obj_type_t obj_type);
int __set_attr_str_light(void *pobj, int attr_idx, char *val, char *rscn, obj_type_t obj_type);
int __set_attr(void *pobj, int attr_idx, void *val, enum batch_op op, obj_type_t obj_type);
void __free_attr(void *pobj, int attr_idx, obj_type_t obj_type);

#ifdef	__cplusplus
}
#endif
#endif	/* _PBS_JOB_H */