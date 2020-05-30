#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <uci.h>
#include <sysrepo/xpath.h>

#include "srpo_uci.h"
#include "utils/memory.h"

#ifndef SRPO_UCI_CONFIG_DIR
#define SRPO_UCI_CONFIG_DIR "/etc/config"
#endif

static char *path_from_template_get(const char *template, const char *data);
static char *xpath_key_value_get(const char *xpath);
static int uci_element_set(char *uci_data, bool is_uci_list);
static int uci_element_delete(char *uci_data, bool is_uci_list);

static struct uci_context *uci_context;

int srpo_uci_init(void)
{
	int error = SRPO_UCI_ERR_OK;

	uci_context = uci_alloc_context();
	if (uci_context == NULL) {
		error = SRPO_UCI_ERR_UCI;
		goto error_out;
	}

	error = uci_set_confdir(uci_context, SRPO_UCI_CONFIG_DIR);
	if (error) {
		error = SRPO_UCI_ERR_UCI;
		goto error_out;
	}

	goto out;

error_out:
	srpo_uci_cleanup();

out:

	return error;
}

void srpo_uci_cleanup(void)
{
	if (uci_context) {
		uci_free_context(uci_context);
		uci_context = NULL;
	}
}

const char *srpo_uci_error_description_get(srpo_uci_error_e error)
{
	switch (error) {
#define XM(ENUM, CODE, DESCRIPTION) \
	case ENUM:                      \
		return DESCRIPTION;

		SRPO_UCI_ERROR_TABLE
#undef XM

		default:
			return "unknown error code";
	}
}

int srpo_uci_ucipath_list_get(const char *uci_config, const char **uci_section_list, size_t uci_section_list_size, char ***ucipath_list, size_t *ucipath_list_size)
{
	int error = SRPO_UCI_ERR_OK;
	struct uci_package *package = NULL;
	struct uci_element *element_section = NULL;
	struct uci_section *section = NULL;
	char *section_name = NULL;
	size_t section_name_size = 0;
	struct uci_element *element_option = NULL;
	struct uci_option *option = NULL;
	char **uci_path_list_tmp = NULL;
	size_t uci_path_list_size_tmp = 0;
	size_t uci_path_size = 0;
	size_t anonymous_section_index = 0;
	bool anonymous_section_exists = 0;
	size_t anonymous_section_list_size = 0;
	struct anonymous_section {
		char *type;
		size_t index;
	} *anonymous_section_list = NULL;

	if (uci_config == NULL) {
		return SRPO_UCI_ERR_ARGUMENT;
	}

	if (uci_section_list == NULL) {
		return SRPO_UCI_ERR_ARGUMENT;
	}

	if (ucipath_list == NULL) {
		return SRPO_UCI_ERR_ARGUMENT;
	}

	error = uci_load(uci_context, uci_config, &package);
	if (error) {
		return SRPO_UCI_ERR_UCI;
	}

	uci_foreach_element(&package->sections, element_section)
	{
		section = uci_to_section(element_section);
		for (size_t i = 0; i < uci_section_list_size; i++) {
			if (strcmp(section->type, uci_section_list[i]) == 0) {
				if (section->anonymous) { // hande name conversion from cfgXXXXX to @section_type[index] for anonymous sections
					anonymous_section_index = 0;
					anonymous_section_exists = false;
					for (size_t j = 0; j < anonymous_section_list_size; j++) {
						if (strcmp(anonymous_section_list[j].type, section->type) == 0) { // get the next index for the anonymous section
							anonymous_section_index = anonymous_section_list[j].index++;
							anonymous_section_exists = true;
						}
					}

					if (anonymous_section_exists == false) { // add the anonymous section to the list if first occurrence
						anonymous_section_list = xrealloc(anonymous_section_list, (anonymous_section_list_size + 1) * sizeof(struct anonymous_section));
						anonymous_section_list[anonymous_section_list_size].type = strdup(section->type);
						anonymous_section_list[anonymous_section_list_size].index = 0;
						anonymous_section_index = anonymous_section_list[anonymous_section_list_size].index++;
						anonymous_section_list_size++;
					}

					section_name_size = strlen(section->type) + 1 + 2 + 20 + 1;
					section_name = xmalloc(section_name_size);
					snprintf(section_name, section_name_size, "@%s[%zu]", section->type, anonymous_section_index);
				} else {
					section_name = xstrdup(section->e.name);
				}

				uci_foreach_element(&section->options, element_option)
				{
					option = uci_to_option(element_option);

					uci_path_list_tmp = xrealloc(uci_path_list_tmp, (uci_path_list_size_tmp + 1) * sizeof(char *));

					uci_path_size = strlen(uci_config) + 1 + strlen(section_name) + 1 + strlen(option->e.name) + 1;
					uci_path_list_tmp[uci_path_list_size_tmp] = xmalloc(uci_path_size);
					snprintf(uci_path_list_tmp[uci_path_list_size_tmp], uci_path_size, "%s.%s.%s", uci_config, section_name, option->e.name);

					uci_path_list_size_tmp++;
				}

				FREE_SAFE(section_name);

				break;
			}
		}
	}

	*ucipath_list = uci_path_list_tmp;
	*ucipath_list_size = uci_path_list_size_tmp;

	for (size_t i = 0; i < anonymous_section_list_size; i++) {
		FREE_SAFE(anonymous_section_list[i].type);
	}

	FREE_SAFE(anonymous_section_list);

	return SRPO_UCI_ERR_OK;
}

int srpu_xpath_to_uci_path_convert(const char *xpath, srpo_uci_xpath_uci_template_map_t *xpath_uci_template_map, size_t xpath_uci_template_map_size, char **ucipath)
{
	char *xpath_key_value = NULL;
	char *xpath_tmp = NULL;
	char *ucipath_tmp = NULL;

	if (xpath == NULL) {
		return SRPO_UCI_ERR_ARGUMENT;
	}

	if (xpath_uci_template_map == NULL) {
		return SRPO_UCI_ERR_ARGUMENT;
	}

	if (ucipath == NULL) {
		return SRPO_UCI_ERR_ARGUMENT;
	}

	*ucipath = NULL;

	// get the key from the first list element looking from the left
	xpath_key_value = xpath_key_value_get(xpath);

	// find the table entry that matches the xpath for the found xpath list key
	for (size_t i = 0; i < xpath_uci_template_map_size; i++) {
		if (xpath_uci_template_map[i].xpath_template == NULL || xpath_uci_template_map[i].ucipath_template == NULL) {
			FREE_SAFE(xpath_key_value);
			return SRPO_UCI_ERR_TABLE_ENTRY;
		}

		xpath_tmp = path_from_template_get(xpath_uci_template_map[i].xpath_template, xpath_key_value);
		if (strcmp(xpath, xpath_tmp) == 0) {
			ucipath_tmp = path_from_template_get(xpath_uci_template_map[i].ucipath_template, xpath_key_value);
			break;
		}

		FREE_SAFE(xpath_tmp);
	}

	*ucipath = ucipath_tmp;

	FREE_SAFE(xpath_tmp);
	FREE_SAFE(xpath_key_value);

	return *ucipath ? SRPO_UCI_ERR_OK : SRPO_UCI_ERR_NOT_FOUND;
}

int srpu_uci_to_xpath_path_convert(const char *ucipath, srpo_uci_xpath_uci_template_map_t *uci_xpath_template_map, size_t uci_xpath_template_map_size, char **xpath)
{
	char *xpath_tmp = NULL;
	char *uci_section_name = NULL;
	char *ucipath_tmp = NULL;

	if (ucipath == NULL) {
		return SRPO_UCI_ERR_ARGUMENT;
	}

	if (uci_xpath_template_map == NULL) {
		return SRPO_UCI_ERR_ARGUMENT;
	}

	if (xpath == NULL) {
		return SRPO_UCI_ERR_ARGUMENT;
	}

	*xpath = xpath_tmp;

	// get the section name for uci_path
	uci_section_name = srpu_uci_section_name_get(ucipath);

	// find the table entry that matches the uci path for the found uci section
	for (size_t i = 0; i < uci_xpath_template_map_size; i++) {
		if (uci_xpath_template_map[i].xpath_template == NULL || uci_xpath_template_map[i].ucipath_template == NULL) {
			FREE_SAFE(uci_section_name);
			return SRPO_UCI_ERR_TABLE_ENTRY;
		}

		ucipath_tmp = path_from_template_get(uci_xpath_template_map[i].ucipath_template, uci_section_name);
		if (strcmp(ucipath, ucipath_tmp) == 0) {
			xpath_tmp = path_from_template_get(uci_xpath_template_map[i].xpath_template, uci_section_name);
			break;
		}

		FREE_SAFE(ucipath_tmp);
	}

	*xpath = xpath_tmp;

	FREE_SAFE(ucipath_tmp);
	FREE_SAFE(uci_section_name);

	return *xpath ? SRPO_UCI_ERR_OK : SRPO_UCI_ERR_NOT_FOUND;
}

int srpo_uci_transform_sysrepo_data_cb_get(const char *xpath, srpo_uci_xpath_uci_template_map_t *xpath_uci_template_map, size_t xpath_uci_template_map_size, srpo_uci_transform_data_cb *transform_sysrepo_data_cb)
{

	char *xpath_key_value = NULL;
	char *xpath_tmp = NULL;
	srpo_uci_transform_data_cb transform_sysrepo_data_cb_tmp = NULL;

	if (xpath == NULL) {
		return SRPO_UCI_ERR_ARGUMENT;
	}

	if (xpath_uci_template_map == NULL) {
		return SRPO_UCI_ERR_ARGUMENT;
	}

	// get the key from the first list element looking from the left
	xpath_key_value = xpath_key_value_get(xpath);

	// find the table entry that matches the xpath for the found xpath list key
	for (size_t i = 0; i < xpath_uci_template_map_size; i++) {
		if (xpath_uci_template_map[i].xpath_template == NULL) {
			FREE_SAFE(xpath_key_value);
			return SRPO_UCI_ERR_TABLE_ENTRY;
		}

		xpath_tmp = path_from_template_get(xpath_uci_template_map[i].xpath_template, xpath_key_value);
		if (strcmp(xpath, xpath_tmp) == 0) {
			transform_sysrepo_data_cb_tmp = xpath_uci_template_map[i].transform_sysrepo_data_cb;
			break;
		}

		FREE_SAFE(xpath_tmp);
	}

	*transform_sysrepo_data_cb = transform_sysrepo_data_cb_tmp;

	FREE_SAFE(xpath_tmp);
	FREE_SAFE(xpath_key_value);

	return SRPO_UCI_ERR_OK;
}

int srpo_uci_transform_uci_data_cb_get(const char *ucipath, srpo_uci_xpath_uci_template_map_t *uci_xpath_template_map, size_t uci_xpath_template_map_size, srpo_uci_transform_data_cb *transform_uci_data_cb)
{
	char *uci_section_name = NULL;
	char *ucipath_tmp = NULL;
	srpo_uci_transform_data_cb transform_uci_data_cb_tmp = NULL;

	if (ucipath == NULL) {
		return SRPO_UCI_ERR_ARGUMENT;
	}

	if (uci_xpath_template_map == NULL) {
		return SRPO_UCI_ERR_ARGUMENT;
	}

	// get the section name for uci_path
	uci_section_name = srpo_uci_section_name_get(ucipath);

	// find the table entry that matches the uci path for the found uci section
	for (size_t i = 0; i < uci_xpath_template_map_size; i++) {
		if (uci_xpath_template_map[i].ucipath_template == NULL) {
			FREE_SAFE(uci_section_name);
			return SRPO_UCI_ERR_TABLE_ENTRY;
		}

		ucipath_tmp = path_from_template_get(uci_xpath_template_map[i].ucipath_template, uci_section_name);
		if (strcmp(ucipath, ucipath_tmp) == 0) {
			transform_uci_data_cb_tmp = uci_xpath_template_map[i].transform_uci_data_cb;
			break;
		}

		FREE_SAFE(ucipath_tmp);
	}

	*transform_uci_data_cb = transform_uci_data_cb_tmp;

	FREE_SAFE(ucipath_tmp);
	FREE_SAFE(uci_section_name);

	return SRPO_UCI_ERR_OK;
}

int srpo_uci_section_type_get(const char *ucipath, srpo_uci_xpath_uci_template_map_t *uci_xpath_template_map, size_t uci_xpath_template_map_size, const char **uci_section_type)
{
	char *uci_section_name = NULL;
	char *ucipath_tmp = NULL;
	const char *uci_section_type_tmp = NULL;

	if (ucipath == NULL) {
		return SRPO_UCI_ERR_ARGUMENT;
	}

	if (uci_xpath_template_map == NULL) {
		return SRPO_UCI_ERR_ARGUMENT;
	}

	// get the section name for uci_path
	uci_section_name = srpo_uci_section_name_get(ucipath);

	// find the table entry that matches the uci path for the found uci section
	for (size_t i = 0; i < uci_xpath_template_map_size; i++) {
		if (uci_xpath_template_map[i].ucipath_template == NULL) {
			FREE_SAFE(uci_section_name);
			return SRPO_UCI_ERR_TABLE_ENTRY;
		}

		ucipath_tmp = path_from_template_get(uci_xpath_template_map[i].ucipath_template, uci_section_name);
		if (strcmp(ucipath, ucipath_tmp) == 0) {
			uci_section_type_tmp = uci_xpath_template_map[i].uci_section_type;
			break;
		}

		FREE_SAFE(ucipath_tmp);
	}

	*uci_section_type = uci_section_type_tmp;

	FREE_SAFE(ucipath_tmp);
	FREE_SAFE(uci_section_name);

	return SRPO_UCI_ERR_OK;
}

int srpo_uci_has_transform_sysrepo_data_private_get(const char *xpath, srpo_uci_xpath_uci_template_map_t *xpath_uci_template_map, size_t xpath_uci_template_map_size, bool *has_transform_sysrepo_data_private)
{
	char *xpath_key_value = NULL;
	char *xpath_tmp = NULL;
	bool has_transform_sysrepo_data_private_tmp = false;

	if (xpath == NULL) {
		return SRPO_UCI_ERR_ARGUMENT;
	}

	if (xpath_uci_template_map == NULL) {
		return SRPO_UCI_ERR_ARGUMENT;
	}

	// get the key from the first list element looking from the left
	xpath_key_value = xpath_key_value_get(xpath);

	// find the table entry that matches the xpath for the found xpath list key
	for (size_t i = 0; i < xpath_uci_template_map_size; i++) {
		if (xpath_uci_template_map[i].xpath_template == NULL) {
			FREE_SAFE(xpath_key_value);
			return SRPO_UCI_ERR_TABLE_ENTRY;
		}

		xpath_tmp = path_from_template_get(xpath_uci_template_map[i].xpath_template, xpath_key_value);
		if (strcmp(xpath, xpath_tmp) == 0) {
			has_transform_sysrepo_data_private_tmp = xpath_uci_template_map[i].has_transform_sysrepo_data_private;
			break;
		}

		FREE_SAFE(xpath_tmp);
	}

	*has_transform_sysrepo_data_private = has_transform_sysrepo_data_private_tmp;

	FREE_SAFE(xpath_tmp);
	FREE_SAFE(xpath_key_value);

	return SRPO_UCI_ERR_OK;
}
int srpo_uci_has_transform_uci_data_private_get(const char *uci_path, srpo_uci_xpath_uci_template_map_t *uci_xpath_template_map, size_t uci_xpath_template_map_size, bool *has_transform_uci_data_private)
{
	char *uci_section_name = NULL;
	char *uci_path_tmp = NULL;
	bool has_transform_uci_data_private_tmp = false;

	if (uci_path == NULL) {
		return SRPO_UCI_ERR_ARGUMENT;
	}

	if (uci_xpath_template_map == NULL) {
		return SRPO_UCI_ERR_ARGUMENT;
	}

	// get the section name for uci_path
	uci_section_name = srpo_uci_section_name_get(uci_path);

	// find the table entry that matches the uci path for the found uci section
	for (size_t i = 0; i < uci_xpath_template_map_size; i++) {
		if (uci_xpath_template_map[i].ucipath_template == NULL) {
			FREE_SAFE(uci_section_name);
			return SRPO_UCI_ERR_TABLE_ENTRY;
		}

		uci_path_tmp = path_from_template_get(uci_xpath_template_map[i].ucipath_template, uci_section_name);
		if (strcmp(uci_path, uci_path_tmp) == 0) {
			has_transform_uci_data_private_tmp = uci_xpath_template_map[i].has_transform_uci_data_private;
			break;
		}

		FREE_SAFE(uci_path_tmp);
	}

	*has_transform_uci_data_private = has_transform_uci_data_private_tmp;

	FREE_SAFE(uci_path_tmp);
	FREE_SAFE(uci_section_name);

	return SRPO_UCI_ERR_OK;
}

static char *path_from_template_get(const char *template, const char *data)
{
	char *path = NULL;
	size_t path_size = 0;

	if (strstr(template, "%s")) {
		path_size = strlen(template) - 2 + (data ? strlen(data) : 0) + 1;
		path = xmalloc(path_size);
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
		snprintf(path, path_size, template, data ? data : "");
#pragma GCC diagnostic warning "-Wformat-nonliteral"
	} else {
		path = xstrdup(template);
	}

	return path;
}

static char *xpath_key_value_get(const char *xpath)
{
	sr_xpath_ctx_t sr_xpath_ctx = {0};
	char *xpath_node = NULL;
	char *xpath_key_name = NULL;
	char *xpath_key_value = NULL;

	xpath_node = sr_xpath_next_node((char *) xpath, &sr_xpath_ctx);
	if (xpath_node) {
		do {
			xpath_key_name = sr_xpath_next_key_name(NULL, &sr_xpath_ctx);
			if (xpath_key_name) {
				xpath_key_value = strdup(sr_xpath_next_key_value(NULL, &sr_xpath_ctx));
				break;
			}
		} while (sr_xpath_next_node(NULL, &sr_xpath_ctx));
	}

	sr_xpath_recover(&sr_xpath_ctx);

	return xpath_key_value;
}

char *srpo_uci_section_name_get(const char *ucipath)
{
	char *uci_section_name_begin = NULL;
	char *uci_section_name_end = NULL;
	char *uci_section_name = NULL;
	size_t uci_section_name_size = 0;

	if (ucipath == NULL) {
		return NULL;
	}

	uci_section_name_begin = strchr(ucipath, '.');
	if (uci_section_name_begin == NULL) {
		return NULL;
	}

	uci_section_name_begin++;
	uci_section_name_end = strchr(uci_section_name_begin, '.');
	if (uci_section_name_end) {
		uci_section_name_size = (size_t) uci_section_name_end - (size_t) uci_section_name_begin + 1;
	} else {
		uci_section_name_size = strlen(uci_section_name_begin) + 1;
	}

	uci_section_name = xcalloc(1, uci_section_name_size);
	strncpy(uci_section_name, uci_section_name_begin, uci_section_name_size - 1);
	uci_section_name[uci_section_name_size - 1] = '\0';

	return uci_section_name;
}

int srpo_uci_section_create(const char *ucipath, const char *uci_section_type)
{
	int error = SRPO_UCI_ERR_OK;
	size_t uci_data_size = 0;
	char *uci_data = NULL;

	if (ucipath == NULL) {
		return SRPO_UCI_ERR_ARGUMENT;
	}

	if (uci_section_type == NULL) {
		return SRPO_UCI_ERR_ARGUMENT;
	}

	uci_data_size = strlen(ucipath) + 1 + strlen(uci_section_type) + 1;
	uci_data = xcalloc(1, uci_data_size);
	snprintf(uci_data, uci_data_size, "%s=%s", ucipath, uci_section_type);

	error = uci_element_set(uci_data, false);
	if (error) {
		error = SRPO_UCI_ERR_UCI;
		goto out;
	}

out:
	FREE_SAFE(uci_data);

	return error;
}

int srpo_uci_section_delete(const char *ucipath)
{
	int error = SRPO_UCI_ERR_OK;
	char *uci_data = NULL;

	if (ucipath == NULL) {
		return SRPO_UCI_ERR_ARGUMENT;
	}

	uci_data = xstrdup(ucipath);
	error = uci_element_delete(uci_data, false);
	if (error) {
		error = SRPO_UCI_ERR_UCI;
		goto out;
	}

out:
	FREE_SAFE(uci_data);

	return error;
}

int srpo_uci_option_set(const char *ucipath, const char *value, srpo_uci_transform_data_cb transform_sysrepo_data_cb, void *private_data)
{
	int error = SRPO_UCI_ERR_OK;
	char *transform_value = NULL;
	size_t uci_data_size = 0;
	char *uci_data = NULL;

	if (ucipath == NULL) {
		return SRPO_UCI_ERR_ARGUMENT;
	}

	if (value == NULL) {
		return SRPO_UCI_ERR_ARGUMENT;
	}

	transform_value = transform_sysrepo_data_cb ? transform_sysrepo_data_cb(value, private_data) : xstrdup(value);
	if (transform_value == NULL) {
		goto out;
	}

	uci_data_size = strlen(ucipath) + 1 + strlen(transform_value) + 1;
	uci_data = xcalloc(1, uci_data_size);
	snprintf(uci_data, uci_data_size, "%s=%s", ucipath, transform_value);

	error = uci_element_set(uci_data, false);
	if (error) {
		error = SRPO_UCI_ERR_UCI;
		goto out;
	}

out:
	FREE_SAFE(transform_value);
	FREE_SAFE(uci_data);

	return error;
}

int srpo_uci_option_remove(const char *ucipath)
{
	int error = 0;
	char *uci_data = NULL;

	if (ucipath == NULL) {
		return SRPO_UCI_ERR_ARGUMENT;
	}

	uci_data = xstrdup(ucipath);
	error = uci_element_delete(uci_data, false);
	if (error) {
		error = SRPO_UCI_ERR_UCI;
		goto out;
	}

out:
	FREE_SAFE(uci_data);

	return error;
}

int srpo_uci_list_set(const char *ucipath, const char *value, srpo_uci_transform_data_cb transform_sysrepo_data_cb, void *private_data)
{
	int error = SRPO_UCI_ERR_OK;
	char *transform_value = NULL;
	size_t uci_data_size = 0;
	char *uci_data = NULL;

	if (ucipath == NULL) {
		return SRPO_UCI_ERR_ARGUMENT;
	}

	if (value == NULL) {
		return SRPO_UCI_ERR_ARGUMENT;
	}

	transform_value = transform_sysrepo_data_cb ? transform_sysrepo_data_cb(value, private_data) : xstrdup(value);
	if (transform_value == NULL) {
		goto out;
	}

	uci_data_size = strlen(ucipath) + 1 + strlen(transform_value) + 1;
	uci_data = xcalloc(1, uci_data_size);
	snprintf(uci_data, uci_data_size, "%s=%s", ucipath, transform_value);

	error = uci_element_set(uci_data, true);
	if (error) {
		error = SRPO_UCI_ERR_UCI;
		goto out;
	}

out:
	FREE_SAFE(transform_value);
	FREE_SAFE(uci_data);

	return error;
}

int srpo_uci_list_remove(const char *ucipath, const char *value)
{
	int error = SRPO_UCI_ERR_OK;
	size_t uci_data_size = 0;
	char *uci_data = NULL;

	if (ucipath == NULL) {
		return SRPO_UCI_ERR_ARGUMENT;
	}

	if (value == NULL) {
		return SRPO_UCI_ERR_ARGUMENT;
	}

	uci_data_size = strlen(ucipath) + 1 + strlen(value) + 1;
	uci_data = xcalloc(1, uci_data_size);
	snprintf(uci_data, uci_data_size, "%s=%s", ucipath, value);

	error = uci_element_delete(uci_data, true);
	if (error) {
		error = SRPO_UCI_ERR_UCI;
		goto out;
	}

out:
	FREE_SAFE(uci_data);

	return error;
}

static int uci_element_set(char *uci_data, bool is_uci_list)
{
	int error = 0;
	struct uci_ptr uci_ptr = {0};

	error = uci_lookup_ptr(uci_context, &uci_ptr, uci_data, true);
	if (error) {
		return -1;
	}

	error = is_uci_list ? uci_add_list(uci_context, &uci_ptr) : uci_set(uci_context, &uci_ptr);
	if (error) {
		return -1;
	}

	return 0;
}

static int uci_element_delete(char *uci_data, bool is_uci_list)
{
	int error = 0;
	struct uci_ptr uci_ptr = {0};

	error = uci_lookup_ptr(uci_context, &uci_ptr, uci_data, true);
	if (error) {
		return -1;
	}

	if (is_uci_list) {
		uci_del_list(uci_context, &uci_ptr);
	} else {
		uci_delete(uci_context, &uci_ptr);
	}

	return 0;
}

int srpo_uci_element_value_get(const char *ucipath, srpo_uci_transform_data_cb transform_uci_data_cb, void *private_data, char ***value_list, size_t *value_list_size)
{
	int error = 0;
	char *ucipath_tmp = NULL;
	struct uci_ptr uci_ptr = {0};
	char *value = NULL;
	struct uci_element *element = NULL;
	struct uci_option *option = NULL;
	char **value_list_tmp = NULL;
	size_t value_list_size_tmp = 0;

	if (ucipath == NULL) {
		return SRPO_UCI_ERR_ARGUMENT;
	}

	ucipath_tmp = xstrdup(ucipath);
	*value_list = NULL;
	*value_list_size = 0;

	error = uci_lookup_ptr(uci_context, &uci_ptr, (char *) ucipath, true);
	if (error || (uci_ptr.flags & UCI_LOOKUP_COMPLETE) == 0) {
		return SRPO_UCI_ERR_UCI;
	}

	if (uci_ptr.o->type == UCI_TYPE_STRING) {
		value = transform_uci_data_cb ? transform_uci_data_cb(uci_ptr.o->v.string, private_data) : xstrdup(uci_ptr.o->v.string);
		if (value == NULL) {
			goto out;
		}

		value_list_tmp = xrealloc(value_list_tmp, (value_list_size_tmp + 1) * sizeof(char *));
		value_list_tmp[value_list_size_tmp] = value;
		value_list_size_tmp++;

	} else if (uci_ptr.o->type == UCI_TYPE_LIST) {
		uci_foreach_element(&uci_ptr.o->v.list, element)
		{
			option = uci_to_option(element);

			value = transform_uci_data_cb ? transform_uci_data_cb(option->e.name, private_data) : xstrdup(option->e.name);
			if (value == NULL) {
				continue;
			}

			value_list_tmp = xrealloc(value_list_tmp, (value_list_size_tmp + 1) * sizeof(char *));
			value_list_tmp[value_list_size_tmp] = value;
			value_list_size_tmp++;
		}
	}

	*value_list = value_list_tmp;
	*value_list_size = value_list_size_tmp;

out:
	FREE_SAFE(ucipath_tmp);

	return error ? SRPO_UCI_ERR_TRANSFORM_CB : SRPO_UCI_ERR_OK;
}

int srpo_uci_revert(const char *uci_config)
{
	int error = SRPO_UCI_ERR_OK;
	char *uci_config_tmp = NULL;
	struct uci_ptr uci_ptr = {0};

	if (uci_config == NULL) {
		error = SRPO_UCI_ERR_UCI;
		goto out;
	}

	uci_config_tmp = xstrdup(uci_config);

	error = uci_lookup_ptr(uci_context, &uci_ptr, uci_config_tmp, true);
	if (error || (uci_ptr.flags & UCI_LOOKUP_COMPLETE) == 0) {
		error = SRPO_UCI_ERR_UCI;
		goto out;
	}

	error = uci_revert(uci_context, &uci_ptr);
	if (error) {
		error = SRPO_UCI_ERR_UCI;
		goto out;
	}

out:
	FREE_SAFE(uci_config_tmp);
	return error;
}

int srpo_uci_commit(const char *uci_config)
{
	int error = SRPO_UCI_ERR_OK;
	char *uci_config_tmp = NULL;
	struct uci_ptr uci_ptr = {0};

	if (uci_config == NULL) {
		return SRPO_UCI_ERR_ARGUMENT;
	}

	uci_config_tmp = xstrdup(uci_config);

	error = uci_lookup_ptr(uci_context, &uci_ptr, uci_config_tmp, true);
	if (error || (uci_ptr.flags & UCI_LOOKUP_COMPLETE) == 0) {
		error = SRPO_UCI_ERR_UCI;
		goto out;
	}

	error = uci_commit(uci_context, &uci_ptr.p, false);
	if (error) {
		error = SRPO_UCI_ERR_UCI;
		goto out;
	}

out:
	FREE_SAFE(uci_config_tmp);

	return error;
}