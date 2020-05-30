/**
 * @file srpo_ubus.h
 * @author Juraj Vijtiuk <juraj.vijtiuk@sartura.hr>
 * @brief srpo_ubus - sysrepo plugin openwrt ubus header file for setting/getting data to/from UBUS from/to sysrepo.
 *
 * @copyright
 * Copyright (C) 2020 Deutsche Telekom AG.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRPO_UBUS_H_ONCE
#define SRPO_UBUS_H_ONCE

#include <stddef.h>

typedef enum {
#define SRPO_UBUS_ERROR_TABLE          \
	XM(SRPO_UBUS_ERR_OK, 0, "Success") \
	XM(SRPO_UBUS_ERR_INTERNAL, -1, "Internal UBUS error")
#define XM(ENUM, CODE, DESCRIPTION) ENUM = CODE,
	SRPO_UBUS_ERROR_TABLE
#undef XM
} srpo_ubus_error_e;

typedef struct {
	char *value;
	char *xpath;
} srpo_ubus_result_value_t;

typedef struct {
	srpo_ubus_result_value_t *values;
	size_t num_values;
} srpo_ubus_result_values_t;

typedef void (*srpo_ubus_transform_data_cb)(const char *ubus_json, srpo_ubus_result_values_t *values);

typedef struct {
	const char *lookup_path;
	const char *method;
	srpo_ubus_transform_data_cb transform_data_cb;
} srpo_ubus_transform_template_t;

srpo_ubus_error_e srpo_ubus_data_get(srpo_ubus_result_values_t *values, srpo_ubus_transform_template_t *transform);

void srpo_ubus_init_result_values(srpo_ubus_result_values_t **values);
srpo_ubus_error_e srpo_ubus_result_values_add(srpo_ubus_result_values_t *values, const char *value, const char *xpath_template, const char *xpath_value);
void srpo_ubus_free_result_values(srpo_ubus_result_values_t *values);

const char *srpo_ubus_error_description_get(srpo_ubus_error_e error);
#endif /*SRPO_UBUS_H_ONCE*/
