/**
 * @file srpo_uci.h
 * @author Luka Paulic <luka.paulic@sartura.hr>
 * @brief srpo_uci - Sysrepo plugin OpenWrt UCI header file for setting/getting data to/from UCI from/to Sysrepo.
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

#ifndef SRPO_UCI_H_ONCE
#define SRPO_UCI_H_ONCE

#include <stdbool.h>
#include <stdlib.h>

typedef enum {
#define SRPO_UCI_ERROR_TABLE                                                  \
	XM(SRPO_UCI_ERR_OK, 0, "Success")                                         \
	XM(SRPO_UCI_ERR_ARGUMENT, -1, "Invalid argumnet")                         \
	XM(SRPO_UCI_ERR_NOT_FOUND, -2, "Entry not found in table")                \
	XM(SRPO_UCI_ERR_UCI, -3, "Internal UCI error")                            \
	XM(SRPO_UCI_ERR_XPATH, -4, "Internal XPath error")                        \
	XM(SRPO_UCI_ERR_TABLE_ENTRY, -5, "Table doesn't contain a path template") \
	XM(SRPO_UCI_ERR_SECTION_NAME, -6, "UCI section name is missing")          \
	XM(SRPO_UCI_ERR_TRANSFORM_CB, -7, "Tranform data callback error")         \
	XM(SRPO_UCI_ERR_UCI_FILE, -8, "Error opening uci config file")

#define XM(ENUM, CODE, DESCRIPTION) ENUM = CODE,
	SRPO_UCI_ERROR_TABLE
#undef XM
} srpo_uci_error_e;

typedef char *(*srpo_uci_transform_data_cb)(const char *value, void *private_data);

typedef struct {
	const char *xpath_template;
	const char *ucipath_template;
	const char *uci_section_type;
	srpo_uci_transform_data_cb transform_sysrepo_data_cb;
	srpo_uci_transform_data_cb transform_uci_data_cb;
	bool has_transform_sysrepo_data_private;
	bool has_transform_uci_data_private;
} srpo_uci_xpath_uci_template_map_t;

int srpo_uci_init(void);
void srpo_uci_cleanup(void);

const char *srpo_uci_error_description_get(srpo_uci_error_e error);

int srpo_uci_ucipath_list_get(const char *uci_config, const char **uci_section_list, size_t uci_section_list_size, char ***ucipath_list, size_t *ucipath_list_size, bool convert_to_extended);

int srpo_uci_xpath_to_ucipath_convert(const char *xpath, srpo_uci_xpath_uci_template_map_t *xpath_uci_template_map, size_t xpath_uci_template_map_size, char **ucipath);
int srpo_uci_ucipath_to_xpath_convert(const char *ucipath, srpo_uci_xpath_uci_template_map_t *uci_xpath_template_map, size_t uci_xpath_template_map_size, char **xpath);
int srpo_uci_sublist_ucipath_to_xpath_convert(const char *ucipath, const char *xpath_parent_template, const char *ucipath_parent_template, srpo_uci_xpath_uci_template_map_t *uci_xpath_template_map, size_t uci_xpath_template_map_size, char **xpath);

char *srpo_uci_section_name_get(const char *ucipath);

int srpo_uci_transform_sysrepo_data_cb_get(const char *xpath, srpo_uci_xpath_uci_template_map_t *xpath_uci_template_map, size_t xpath_uci_template_map_size, srpo_uci_transform_data_cb *transform_sysrepo_data_cb);
int srpo_uci_transform_uci_data_cb_get(const char *ucipath, srpo_uci_xpath_uci_template_map_t *uci_xpath_template_map, size_t uci_xpath_template_map_size, srpo_uci_transform_data_cb *transform_uci_data_cb);
int srpo_uci_section_type_get(const char *ucipath, srpo_uci_xpath_uci_template_map_t *uci_xpath_template_map, size_t uci_xpath_template_map_size, const char **uci_section_type);
int srpo_uci_has_transform_sysrepo_data_private_get(const char *xpath, srpo_uci_xpath_uci_template_map_t *xpath_uci_template_map, size_t xpath_uci_template_map_size, bool *has_transform_sysrepo_data_private);
int srpo_uci_has_transform_uci_data_private_get(const char *ucipath, srpo_uci_xpath_uci_template_map_t *uci_xpath_template_map, size_t uci_xpath_template_map_size, bool *has_transform_uci_data_private);

int srpo_uci_section_create(const char *ucipath, const char *uci_section_type);
int srpo_uci_section_delete(const char *ucipath);
int srpo_uci_option_set(const char *ucipath, const char *value, srpo_uci_transform_data_cb transform_sysrepo_data_cb, void *private_data);
int srpo_uci_option_remove(const char *ucipath);
int srpo_uci_list_set(const char *ucipath, const char *value, srpo_uci_transform_data_cb transform_sysrepo_data_cb, void *private_data);
int srpo_uci_list_remove(const char *ucipath, const char *value);
int srpo_uci_element_value_get(const char *ucipath, srpo_uci_transform_data_cb transform_uci_data_cb, void *private_data, char ***value_list, size_t *value_list_size);

int srpo_uci_revert(const char *uci_config);
int srpo_uci_commit(const char *uci_config);

#endif /* SRPO_UCI_H_ONCE */
