#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <uci.h>
#include <libuci2.h>
#include <sysrepo/xpath.h>

#include "srpo_uci.h"
#include "utils/memory.h"

#ifndef SRPO_UCI_CONFIG_DIR
#define SRPO_UCI_CONFIG_DIR "/etc/config"
#endif

#define UCI2_IS_ANYNYMOUS_SECTION(node) (uci2_nc((node)) && (node)->ch[0]->nt != UCI2_NT_SECTION_NAME)

typedef struct srpo_uci_ctx srpo_uci_ctx_t;
typedef struct srpo_uci_path srpo_uci_path_t;

struct srpo_uci_ctx {
	uci2_parser_ctx_t *parser_ctx;

	const char *config_dir;
	char *current_file;
	char config_path[PATH_MAX];
};

struct srpo_uci_path {
	char *package;
	char *section;
	char *option;
	char *value;
};

static srpo_uci_ctx_t *uci_context = NULL;

// helper functions
static char *path_from_template_get(const char *template, const char *data);
static uci2_n_t *uci_get_last_type(uci2_n_t *cfg, const char *type_name);

// path functions
static void uci_path_init(srpo_uci_path_t *path);
static void uci_path_print(srpo_uci_path_t *path);
static int uci_path_parse(srpo_uci_path_t *path, const char *ucipath);
static void uci_path_free(srpo_uci_path_t *path);

// context functions
static srpo_uci_ctx_t *uci_context_alloc(void);
static void uci_context_set_config_dir(srpo_uci_ctx_t *ctx, const char *dir);
static int uci_context_load(srpo_uci_ctx_t *ctx, const char *config);
static int uci_context_create_config_path(srpo_uci_ctx_t *ctx, const char *config);
static int uci_context_revert(srpo_uci_ctx_t *ctx, const char *config);
static int uci_context_commit(srpo_uci_ctx_t *ctx, const char *config);
static void uci_context_free(srpo_uci_ctx_t *ctx);

int srpo_uci_init(void)
{
	int error = SRPO_UCI_ERR_OK;

	uci_context = uci_context_alloc();
	if (uci_context == NULL) {
		error = SRPO_UCI_ERR_UCI;
		goto error_out;
	}

	uci_context_set_config_dir(uci_context, SRPO_UCI_CONFIG_DIR);
	goto out;

error_out:
	srpo_uci_cleanup();

out:

	return error;
}

void srpo_uci_cleanup(void)
{
	if (uci_context) {
		uci_context_free(uci_context);
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

int srpo_uci_ucipath_list_get(const char *uci_config, const char **uci_section_list, size_t uci_section_list_size, char ***ucipath_list, size_t *ucipath_list_size, bool convert_to_extended)
{
	int error = 0;
	char path_buffer[PATH_MAX] = {0};
	char sec_buffer[256] = {0};
	char list_items_buffer[256] = {0};
	uci2_n_t *node_sec = NULL;
	uci2_n_t *node_type = NULL;
	bool anonym_sec = false;

	struct {
		char **list;
		size_t size;
	} path_list = {NULL, 0};

	error = uci_context_load(uci_context, uci_config);

	if (error != SRPO_UCI_ERR_OK) {
		return error;
	} else {
		uci2_n_t *root = UCI2_CFG_ROOT(uci_context->parser_ctx);
		for (size_t iter = 0; iter < uci_section_list_size; iter++) {
			node_sec = NULL;
			node_type = NULL;
			anonym_sec = false;

			for (int i = 0; i < root->ch_nr; i++) {
				uci2_n_t *type = root->ch[i];

				if (UCI2_IS_ANYNYMOUS_SECTION(type) && convert_to_extended) {
					if (strcmp(type->name, uci_section_list[iter]) == 0) {
						node_sec = type;
						node_type = type;
						anonym_sec = true;
						break;
					}
				} else {
					// if not, iterate through sections and if name found set the tmp variable to it
					for (int j = 0; j < type->ch_nr; j++) {
						uci2_n_t *sec = type->ch[j];
						if (strcmp(sec->name, uci_section_list[iter]) == 0) {
							node_sec = sec;
							node_type = type;
							break;
						}
					}
				}

				if (node_sec != NULL) {
					// node found => break
					break;
				}
			}

			if (node_sec == NULL) {
				// wanted node not found -> error
				goto error_out;
			}

			if (anonym_sec) {
				snprintf(sec_buffer, sizeof(sec_buffer), "@%s[%d]", uci2_get_name(node_sec), node_sec->id - 1);
			} else {
				snprintf(sec_buffer, sizeof(sec_buffer), "%s", uci2_get_name(node_sec));
			}
			// write path in the list
			snprintf(path_buffer, sizeof(path_buffer), "%s.%s=%s", uci_config, sec_buffer, uci2_get_name(node_type));
			path_list.list = xrealloc(path_list.list, sizeof(char *) * (++path_list.size));
			path_list.list[path_list.size - 1] = xstrdup(path_buffer);

			// iterate options and lists and write them to the path
			for (int i = 0; i < node_sec->ch_nr; i++) {
				uci2_n_t *child = node_sec->ch[i];
				path_list.list = xrealloc(path_list.list, sizeof(char *) * (++path_list.size));
				if (child->nt == UCI2_NT_LIST) {
					// if its not an option -> list
					size_t byte_cnt = 0;
					for (int j = 0; j < child->ch_nr; j++) {
						uci2_n_t *li = child->ch[j];
						if (sizeof(list_items_buffer) - byte_cnt > 0) {
							snprintf(list_items_buffer + byte_cnt, sizeof(list_items_buffer) - byte_cnt, "\'%s\' ", uci2_get_name(li));
							byte_cnt += strlen(li->name);
						} else {
							// err
							error = SRPO_UCI_ERR_UCI;
							break;
						}
					}
					// remove last space
					list_items_buffer[byte_cnt - 2] = 0;
				}
				snprintf(path_buffer, sizeof(path_buffer), "%s.%s.%s=%s", uci_config, sec_buffer, uci2_get_name(child), child->nt == UCI2_NT_LIST ? list_items_buffer : uci2_get_value(child));
				path_list.list[path_list.size - 1] = xstrdup(path_buffer);
			}
		}
	}
	goto out;
error_out:
	if (path_list.list) {
		for (size_t i = 0; i < path_list.size; i++) {
			free(path_list.list[i]);
		}
		free(path_list.list);
		path_list.list = NULL;
		path_list.size = 0;
	}
out:
	*ucipath_list = path_list.list;
	*ucipath_list_size = path_list.size;
	return error;
}

int srpo_uci_xpath_to_ucipath_convert(const char *xpath, srpo_uci_xpath_uci_template_map_t *xpath_uci_template_map, size_t xpath_uci_template_map_size, char **ucipath)
{
	char *ucipath_tmp = NULL;
	int error = SR_ERR_OK;

	if (xpath == NULL || ucipath == NULL || xpath_uci_template_map == NULL) {
		return SRPO_UCI_ERR_ARGUMENT;
	}

	*ucipath = NULL;

	// find the table entry that matches the xpath for the found xpath list key
	for (size_t i = 0; i < xpath_uci_template_map_size; i++) {
		error = srpo_uci_path_get(xpath,
								  xpath_uci_template_map[i].xpath_template, xpath_uci_template_map[i].ucipath_template,
								  xpath_uci_template_map[i].transform_path_cb, SRPO_UCI_PATH_DIRECTION_UCI, &ucipath_tmp);
		if (error == SRPO_UCI_ERR_NOT_FOUND) {
			continue;
		} else if (error == SR_ERR_OK) {
			break;
		} else {
			return SRPO_UCI_ERR_ARGUMENT;
		}
	}

	*ucipath = ucipath_tmp;

	return *ucipath ? SRPO_UCI_ERR_OK : SRPO_UCI_ERR_NOT_FOUND;
}

int srpo_uci_ucipath_to_xpath_convert(const char *ucipath, srpo_uci_xpath_uci_template_map_t *uci_xpath_template_map, size_t uci_xpath_template_map_size, char **xpath)
{
	char *xpath_tmp = NULL;
	int error = SR_ERR_OK;

	if (ucipath == NULL || xpath == NULL || uci_xpath_template_map == NULL) {
		return SRPO_UCI_ERR_ARGUMENT;
	}

	*xpath = xpath_tmp;

	// find the table entry that matches the uci path for the found uci section
	for (size_t i = 0; i < uci_xpath_template_map_size; i++) {
		error = srpo_uci_path_get(ucipath,
								  uci_xpath_template_map[i].ucipath_template, uci_xpath_template_map[i].xpath_template,
								  uci_xpath_template_map[i].transform_path_cb, SRPO_UCI_PATH_DIRECTION_XPATH, &xpath_tmp);
		if (error == SRPO_UCI_ERR_NOT_FOUND) {
			continue;
		} else if (error == SR_ERR_OK) {
			break;
		} else {
			return SRPO_UCI_ERR_ARGUMENT;
		}
	}

	*xpath = xpath_tmp;

	return *xpath ? SRPO_UCI_ERR_OK : SRPO_UCI_ERR_NOT_FOUND;
}

int srpo_uci_path_get(const char *target, const char *from_template, const char *to_template, srpo_uci_transform_path_cb transform_path_cb, srpo_uci_path_direction_t direction, char **path)
{
	int error = SRPO_UCI_ERR_ARGUMENT;
	char *path_key_value = NULL;
	char *path_tmp = NULL;

	if (from_template == NULL || to_template == NULL) {
		goto cleanup;
	}

	/* redirect to custom path handler */
	if (transform_path_cb) {
		error = transform_path_cb(target, from_template, to_template, direction, path);
		goto cleanup;
	}

	/* assume simple path constructor */
	if (direction == SRPO_UCI_PATH_DIRECTION_XPATH) {
		path_key_value = srpo_uci_section_name_get(target);
	} else if (direction == SRPO_UCI_PATH_DIRECTION_UCI) {
		path_key_value = srpo_uci_xpath_key_value_get(target, 1);
	}

	path_tmp = path_from_template_get(from_template, path_key_value);
	if (strcmp(target, path_tmp) == 0) {
		*path = path_from_template_get(to_template, path_key_value);

		error = SRPO_UCI_ERR_OK;
		goto cleanup;
	}

	error = SRPO_UCI_ERR_NOT_FOUND;

cleanup:
	FREE_SAFE(path_tmp);
	FREE_SAFE(path_key_value);

	return error;
}

int srpo_uci_transform_sysrepo_data_cb_get(const char *xpath, srpo_uci_xpath_uci_template_map_t *xpath_uci_template_map, size_t xpath_uci_template_map_size, srpo_uci_transform_data_cb *transform_sysrepo_data_cb)
{
	int error = SRPO_UCI_ERR_OK;
	srpo_uci_transform_data_cb transform_sysrepo_data_cb_tmp = NULL;
	char *ucipath_tmp = NULL;

	if (xpath == NULL || xpath_uci_template_map == NULL) {
		return SRPO_UCI_ERR_ARGUMENT;
	}

	// find the table entry that matches the xpath for the found xpath list key
	for (size_t i = 0; i < xpath_uci_template_map_size; i++) {
		error = srpo_uci_path_get(xpath,
								  xpath_uci_template_map[i].xpath_template, xpath_uci_template_map[i].ucipath_template,
								  xpath_uci_template_map[i].transform_path_cb, SRPO_UCI_PATH_DIRECTION_UCI, &ucipath_tmp);
		if (error == SRPO_UCI_ERR_NOT_FOUND) {
			FREE_SAFE(ucipath_tmp);
			continue;
		} else if (error == SRPO_UCI_ERR_OK) {
			transform_sysrepo_data_cb_tmp = xpath_uci_template_map[i].transform_sysrepo_data_cb;
			break;
		} else {
			FREE_SAFE(ucipath_tmp);
			return SRPO_UCI_ERR_ARGUMENT;
		}
	}

	*transform_sysrepo_data_cb = transform_sysrepo_data_cb_tmp;

	FREE_SAFE(ucipath_tmp);

	return SRPO_UCI_ERR_OK;
}

int srpo_uci_transform_uci_data_cb_get(const char *ucipath, srpo_uci_xpath_uci_template_map_t *uci_xpath_template_map, size_t uci_xpath_template_map_size, srpo_uci_transform_data_cb *transform_uci_data_cb)
{
	int error = SRPO_UCI_ERR_OK;
	srpo_uci_transform_data_cb transform_uci_data_cb_tmp = NULL;
	char *xpath_tmp = NULL;

	if (ucipath == NULL || uci_xpath_template_map == NULL) {
		return SRPO_UCI_ERR_ARGUMENT;
	}

	// find the table entry that matches the uci path for the found uci section
	for (size_t i = 0; i < uci_xpath_template_map_size; i++) {
		error = srpo_uci_path_get(ucipath,
								  uci_xpath_template_map[i].ucipath_template, uci_xpath_template_map[i].xpath_template,
								  uci_xpath_template_map[i].transform_path_cb, SRPO_UCI_PATH_DIRECTION_XPATH, &xpath_tmp);
		if (error == SRPO_UCI_ERR_NOT_FOUND) {
			FREE_SAFE(xpath_tmp);
			continue;
		} else if (error == SR_ERR_OK) {
			transform_uci_data_cb_tmp = uci_xpath_template_map[i].transform_uci_data_cb;
			break;
		} else {
			FREE_SAFE(xpath_tmp);
			return SRPO_UCI_ERR_ARGUMENT;
		}
	}

	*transform_uci_data_cb = transform_uci_data_cb_tmp;

	FREE_SAFE(xpath_tmp);

	return SRPO_UCI_ERR_OK;
}

int srpo_uci_section_type_get(const char *ucipath, srpo_uci_xpath_uci_template_map_t *uci_xpath_template_map, size_t uci_xpath_template_map_size, const char **uci_section_type)
{
	int error = SRPO_UCI_ERR_OK;
	const char *uci_section_type_tmp = NULL;
	char *xpath_tmp = NULL;

	if (ucipath == NULL || uci_xpath_template_map == NULL) {
		return SRPO_UCI_ERR_ARGUMENT;
	}

	// find the table entry that matches the uci path for the found uci section
	for (size_t i = 0; i < uci_xpath_template_map_size; i++) {
		error = srpo_uci_path_get(ucipath,
								  uci_xpath_template_map[i].ucipath_template, uci_xpath_template_map[i].xpath_template,
								  uci_xpath_template_map[i].transform_path_cb, SRPO_UCI_PATH_DIRECTION_XPATH, &xpath_tmp);
		if (error == SRPO_UCI_ERR_NOT_FOUND) {
			continue;
		} else if (error == SR_ERR_OK) {
			uci_section_type_tmp = uci_xpath_template_map[i].uci_section_type;
			break;
		} else {
			return SRPO_UCI_ERR_ARGUMENT;
		}
	}

	*uci_section_type = uci_section_type_tmp;

	FREE_SAFE(xpath_tmp);

	return SRPO_UCI_ERR_OK;
}

int srpo_uci_has_transform_sysrepo_data_private_get(const char *xpath, srpo_uci_xpath_uci_template_map_t *xpath_uci_template_map, size_t xpath_uci_template_map_size, bool *has_transform_sysrepo_data_private)
{
	int error = SRPO_UCI_ERR_OK;
	bool has_transform_sysrepo_data_private_tmp = false;
	char *ucipath_tmp = NULL;

	if (xpath == NULL || xpath_uci_template_map == NULL) {
		return SRPO_UCI_ERR_ARGUMENT;
	}

	// find the table entry that matches the xpath for the found xpath list key
	for (size_t i = 0; i < xpath_uci_template_map_size; i++) {
		error = srpo_uci_path_get(xpath,
								  xpath_uci_template_map[i].xpath_template, xpath_uci_template_map[i].ucipath_template,
								  xpath_uci_template_map[i].transform_path_cb, SRPO_UCI_PATH_DIRECTION_UCI, &ucipath_tmp);
		if (error == SRPO_UCI_ERR_NOT_FOUND) {
			FREE_SAFE(ucipath_tmp);
			continue;
		} else if (error == SRPO_UCI_ERR_OK) {
			has_transform_sysrepo_data_private_tmp = xpath_uci_template_map[i].has_transform_sysrepo_data_private;
			break;
		} else {
			FREE_SAFE(ucipath_tmp);
			return SRPO_UCI_ERR_ARGUMENT;
		}
	}

	*has_transform_sysrepo_data_private = has_transform_sysrepo_data_private_tmp;

	FREE_SAFE(ucipath_tmp);

	return SRPO_UCI_ERR_OK;
}

int srpo_uci_has_transform_uci_data_private_get(const char *ucipath, srpo_uci_xpath_uci_template_map_t *uci_xpath_template_map, size_t uci_xpath_template_map_size, bool *has_transform_uci_data_private)
{
	int error = SRPO_UCI_ERR_OK;
	bool has_transform_uci_data_private_tmp = false;
	char *xpath_tmp = NULL;

	if (ucipath == NULL || uci_xpath_template_map == NULL) {
		return SRPO_UCI_ERR_ARGUMENT;
	}

	// find the table entry that matches the uci path for the found uci section
	for (size_t i = 0; i < uci_xpath_template_map_size; i++) {
		error = srpo_uci_path_get(ucipath,
								  uci_xpath_template_map[i].ucipath_template, uci_xpath_template_map[i].xpath_template,
								  uci_xpath_template_map[i].transform_path_cb, SRPO_UCI_PATH_DIRECTION_XPATH, &xpath_tmp);
		if (error == SRPO_UCI_ERR_NOT_FOUND) {
			FREE_SAFE(xpath_tmp);
			continue;
		} else if (error == SR_ERR_OK) {
			has_transform_uci_data_private_tmp = uci_xpath_template_map[i].has_transform_uci_data_private;
			break;
		} else {
			FREE_SAFE(xpath_tmp);
			return SRPO_UCI_ERR_ARGUMENT;
		}
	}

	*has_transform_uci_data_private = has_transform_uci_data_private_tmp;

	FREE_SAFE(xpath_tmp);

	return SRPO_UCI_ERR_OK;
}

char *srpo_uci_section_name_get(const char *ucipath)
{
	int error = 0;
	srpo_uci_path_t uci_path;
	char *value_tmp = NULL;

	uci_path_init(&uci_path);

	error = uci_path_parse(&uci_path, ucipath);

	if (error)
		goto out;

	// check for empty section
	if (!uci_path.section) {
		error = SRPO_UCI_ERR_ARGUMENT;
		goto out;
	}

	value_tmp = xstrdup(uci_path.section);
	uci_path_print(&uci_path);

out:
	uci_path_free(&uci_path);
	return value_tmp;
}

char *srpo_uci_xpath_key_value_get(const char *xpath, int level)
{
	sr_xpath_ctx_t sr_xpath_ctx = {0};
	int xpath_level = 0;
	char *xpath_node = NULL;
	char *xpath_key_name = NULL;
	char *xpath_key_value = NULL;

	xpath_node = sr_xpath_next_node((char *) xpath, &sr_xpath_ctx);
	if (xpath_node) {
		do {
			xpath_key_name = sr_xpath_next_key_name(NULL, &sr_xpath_ctx);
			if (xpath_key_name) {
				xpath_level++;

				if (xpath_level >= level) {
					xpath_key_value = strdup(sr_xpath_next_key_value(NULL, &sr_xpath_ctx));
					break;
				}
			}
		} while (sr_xpath_next_node(NULL, &sr_xpath_ctx));
	}

	sr_xpath_recover(&sr_xpath_ctx);

	return xpath_key_value;
}

int srpo_uci_section_create(const char *ucipath, const char *uci_section_type)
{
	int error = SRPO_UCI_ERR_OK;
	srpo_uci_path_t uci_path;
	uci2_n_t *last_type = NULL;

	uci_path_init(&uci_path);

	if (ucipath == NULL) {
		return SRPO_UCI_ERR_ARGUMENT;
	}

	if (uci_section_type == NULL) {
		return SRPO_UCI_ERR_ARGUMENT;
	}

	error = uci_path_parse(&uci_path, ucipath);

	last_type = uci_get_last_type(UCI2_CFG_ROOT(uci_context->parser_ctx), uci_section_type);

	if (!last_type) {
		error = SRPO_UCI_ERR_UCI;
		goto out;
	}

	uci2_add_S(uci_context->parser_ctx, last_type, uci_path.section);

out:
	uci_path_free(&uci_path);
	return error;
}

int srpo_uci_section_delete(const char *ucipath)
{
	int error = SRPO_UCI_ERR_OK;
	srpo_uci_path_t uci_path;
	uci2_n_t *lookup_node = NULL;

	uci_path_init(&uci_path);

	if (ucipath == NULL) {
		return SRPO_UCI_ERR_ARGUMENT;
	}

	error = uci_path_parse(&uci_path, ucipath);

	if (error) {
		error = SRPO_UCI_ERR_ARGUMENT;
		goto out;
	}

	if (!uci_path.section || !uci_path.package) {
		error = SRPO_UCI_ERR_ARGUMENT;
		goto out;
	}

	lookup_node = uci2_q(uci_context->parser_ctx, uci_path.section);

	if (!lookup_node) {
		// no such node found
		error = SRPO_UCI_ERR_NOT_FOUND;
		goto out;
	}

	uci2_del(lookup_node);
out:
	uci_path_free(&uci_path);
	return error;
}

int srpo_uci_option_set(const char *ucipath, const char *value, srpo_uci_transform_data_cb transform_sysrepo_data_cb, void *private_data)
{
	int error = SRPO_UCI_ERR_OK;
	char *transform_value = NULL;
	srpo_uci_path_t uci_path;
	uci2_n_t *lookup_node = NULL;

	uci_path_init(&uci_path);

	if (ucipath == NULL) {
		return SRPO_UCI_ERR_ARGUMENT;
	}

	if (value == NULL) {
		return SRPO_UCI_ERR_ARGUMENT;
	}

	transform_value = transform_sysrepo_data_cb ? transform_sysrepo_data_cb(value, private_data) : xstrdup(value);
	if (transform_value == NULL) {
		error = SRPO_UCI_ERR_ARGUMENT;
		goto out;
	}

	uci_path_parse(&uci_path, ucipath);
	if (!uci_path.package || !uci_path.section || !uci_path.option) {
		error = SRPO_UCI_ERR_ARGUMENT;
		goto out;
	}

	lookup_node = uci2_q(uci_context->parser_ctx, uci_path.section, uci_path.option);

	if (!lookup_node) {
		error = SRPO_UCI_ERR_NOT_FOUND;
		goto out;
	}

	uci2_change_value(lookup_node, transform_value);

out:
	uci_path_free(&uci_path);
	FREE_SAFE(transform_value);

	return error;
}

int srpo_uci_option_remove(const char *ucipath)
{
	int error = 0;
	srpo_uci_path_t uci_path;
	uci2_n_t *lookup_node = NULL;

	uci_path_init(&uci_path);

	if (ucipath == NULL) {
		return SRPO_UCI_ERR_ARGUMENT;
	}

	error = uci_path_parse(&uci_path, ucipath);

	if (error) {
		error = SRPO_UCI_ERR_ARGUMENT;
		goto out;
	}

	if (!uci_path.package || !uci_path.section || !uci_path.option) {
		error = SRPO_UCI_ERR_ARGUMENT;
		goto out;
	}

	lookup_node = uci2_q(uci_context->parser_ctx, uci_path.section, uci_path.option);

	if (!lookup_node) {
		error = SRPO_UCI_ERR_NOT_FOUND;
		goto out;
	}

	uci2_del(lookup_node);

out:
	uci_path_free(&uci_path);

	return error;
}

int srpo_uci_list_set(const char *ucipath, const char *value, srpo_uci_transform_data_cb transform_sysrepo_data_cb, void *private_data)
{
	int error = SRPO_UCI_ERR_OK;
	char *transform_value = NULL;
	uci2_n_t *lookup_node = NULL;
	srpo_uci_path_t uci_path;

	uci_path_init(&uci_path);

	if (ucipath == NULL) {
		return SRPO_UCI_ERR_ARGUMENT;
	}

	if (value == NULL) {
		return SRPO_UCI_ERR_ARGUMENT;
	}

	transform_value = transform_sysrepo_data_cb ? transform_sysrepo_data_cb(value, private_data) : xstrdup(value);
	if (transform_value == NULL) {
		error = SRPO_UCI_ERR_ARGUMENT;
		goto out;
	}

	error = uci_path_parse(&uci_path, ucipath);
	if (error) {
		error = SRPO_UCI_ERR_ARGUMENT;
		goto out;
	}

	if (!uci_path.package || !uci_path.section || !uci_path.option) {
		error = SRPO_UCI_ERR_ARGUMENT;
		goto out;
	}

	lookup_node = uci2_q(uci_context->parser_ctx, uci_path.section, uci_path.option);

	if (!lookup_node) {
		error = SRPO_UCI_ERR_NOT_FOUND;
		goto out;
	}

	uci2_add_I(uci_context->parser_ctx, lookup_node, transform_value);

out:
	uci_path_free(&uci_path);
	FREE_SAFE(transform_value);

	return error;
}

int srpo_uci_list_remove(const char *ucipath, const char *value)
{
	int error = SRPO_UCI_ERR_OK;
	uci2_n_t *lookup_node = NULL;
	srpo_uci_path_t uci_path;

	uci_path_init(&uci_path);

	if (ucipath == NULL) {
		return SRPO_UCI_ERR_ARGUMENT;
	}

	if (value == NULL) {
		return SRPO_UCI_ERR_ARGUMENT;
	}

	error = uci_path_parse(&uci_path, ucipath);
	if (error) {
		error = SRPO_UCI_ERR_ARGUMENT;
		goto out;
	}

	if (!uci_path.package || !uci_path.section || !uci_path.option) {
		error = SRPO_UCI_ERR_ARGUMENT;
		goto out;
	}

	lookup_node = uci2_q(uci_context->parser_ctx, uci_path.section, uci_path.option, value);

	if (!lookup_node) {
		error = SRPO_UCI_ERR_NOT_FOUND;
		goto out;
	}

	uci2_del(lookup_node);

out:
	uci_path_free(&uci_path);

	return error;
}

int srpo_uci_element_value_get(const char *ucipath, srpo_uci_transform_data_cb transform_uci_data_cb, void *private_data, char ***value_list, size_t *value_list_size)
{
	int error = 0;
	srpo_uci_path_t uci_path;
	uci2_n_t *tmp_node = NULL;
	struct {
		char **list;
		size_t size;
	} val_list = {0, 0};

	*value_list = NULL;
	*value_list_size = 0;

	uci_path_init(&uci_path);

	if (ucipath == NULL) {
		return SRPO_UCI_ERR_ARGUMENT;
	}

	error = uci_path_parse(&uci_path, ucipath);

	if (error) {
		error = SRPO_UCI_ERR_ARGUMENT;
		goto out;
	}

	// there needs to be an options which is wanted -> no option == noting to return
	if (uci_path.package && uci_path.section && uci_path.option) {
		tmp_node = uci2_q(uci_context->parser_ctx, uci_path.section, uci_path.option);

		if (!tmp_node) {
			error = SRPO_UCI_ERR_NOT_FOUND;
			goto out;
		}

		if (tmp_node->nt == UCI2_NT_LIST) {
			// gather all values
			uci2_iter(tmp_node, li)
			{
				val_list.list = xrealloc(val_list.list, sizeof(char *) * (++val_list.size));
				val_list.list[val_list.size - 1] = transform_uci_data_cb ? transform_uci_data_cb(li->name, private_data) : xstrdup(li->name);
			}
		} else if (tmp_node->nt == UCI2_NT_OPTION) {
			// gather one value
			val_list.list = xrealloc(val_list.list, sizeof(char *) * (++val_list.size));
			val_list.list[val_list.size - 1] = transform_uci_data_cb ? transform_uci_data_cb(tmp_node->value, private_data) : xstrdup(tmp_node->value);
		} else {
			// some internal libuci2 error
			error = SRPO_UCI_ERR_UCI;
			goto out;
		}
	} else {
		error = SRPO_UCI_ERR_ARGUMENT;
		goto out;
	}

	*value_list = val_list.list;
	*value_list_size = val_list.size;
out:
	uci_path_free(&uci_path);
	return error;
}

int srpo_uci_revert(const char *uci_config)
{
	int error = SRPO_UCI_ERR_OK;
	char *uci_config_tmp = NULL;

	if (uci_config == NULL) {
		error = SRPO_UCI_ERR_UCI;
		goto out;
	}

	error = uci_context_revert(uci_context, uci_config);
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

	if (uci_config == NULL) {
		error = SRPO_UCI_ERR_UCI;
		goto out;
	}

	error = uci_context_commit(uci_context, uci_config);
	if (error) {
		error = SRPO_UCI_ERR_UCI;
		goto out;
	}

out:
	FREE_SAFE(uci_config_tmp);
	return error;
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

static uci2_n_t *uci_get_last_type(uci2_n_t *cfg, const char *type_name)
{
	uci2_n_t *ret_node = NULL;
	uci2_iter(cfg, t)
	{
		// check that the node is not deleted
		if (t->parent && strcmp(t->name, type_name) == 0) {
			ret_node = t;
		}
	}
	return ret_node;
}

static void uci_path_init(srpo_uci_path_t *ptr)
{
	ptr->package = NULL;
	ptr->section = NULL;
	ptr->option = NULL;
	ptr->value = NULL;
}

static void uci_path_print(srpo_uci_path_t *ptr)
{
	if (ptr->package)
		printf("ptr->package = %s\n", ptr->package);
	if (ptr->section)
		printf("ptr->section = %s\n", ptr->section);
	if (ptr->option)
		printf("ptr->option = %s\n", ptr->option);
	if (ptr->value)
		printf("ptr->value = %s\n", ptr->value);
}

static int uci_path_parse(srpo_uci_path_t *path, const char *uci_path)
{
	int error = 0;
	size_t opt_pos = 0;
	const char delims[] = ".[]=";
	char *token = NULL;
	char *ucipath = xstrdup(uci_path);

	struct {
		char **list;
		size_t size;
	} parts = {0, 0};

	token = strtok((char *) ucipath, delims);
	while (token != NULL) {
		parts.list = xrealloc(parts.list, sizeof(char *) * (++parts.size));
		parts.list[parts.size - 1] = xstrdup(token);
		token = strtok(NULL, delims);
	}
	if (parts.size > 0) {
		path->package = xstrdup(parts.list[0]);
	}
	if (parts.size > 1) {
		if (parts.list[1][0] == '@' && parts.size > 2) {
			size_t size = strlen(parts.list[1]) + strlen(parts.list[2]) + 1;
			path->section = xmalloc(sizeof(char) * size);
			snprintf(path->section, size, "%s#%d", parts.list[1] + 1, atoi(parts.list[2]) + 1);
			opt_pos = 3;
		} else {
			path->section = xstrdup(parts.list[1]);
			opt_pos = 2;
		}
	}
	if (parts.size > opt_pos) {
		if (parts.size <= opt_pos + 1) {
			path->value = xstrdup(parts.list[opt_pos]);
		} else {
			path->option = xstrdup(parts.list[opt_pos]);
			path->value = xstrdup(parts.list[opt_pos + 1]);
		}
	}

	for (size_t i = 0; i < parts.size; i++) {
		FREE_SAFE(parts.list[i]);
	}
	FREE_SAFE(parts.list);
	FREE_SAFE(ucipath);
	return error;
}

static void uci_path_free(srpo_uci_path_t *ptr)
{
	FREE_SAFE(ptr->package);
	FREE_SAFE(ptr->section);
	FREE_SAFE(ptr->option);
	FREE_SAFE(ptr->value);
}

static srpo_uci_ctx_t *uci_context_alloc(void)
{
	srpo_uci_ctx_t *ctx = xcalloc(1, sizeof(srpo_uci_ctx_t));
	return ctx;
}

static void uci_context_set_config_dir(srpo_uci_ctx_t *ctx, const char *dir)
{
	ctx->config_dir = dir;
}

static int uci_context_load(srpo_uci_ctx_t *ctx, const char *config)
{
	int error = 0;
	ctx->current_file = xstrdup(config);
	error = uci_context_create_config_path(ctx, config);
	ctx->parser_ctx = uci2_parse_file((const char *) ctx->config_path);

	if (!ctx->parser_ctx) {
		error = SRPO_UCI_ERR_UCI_FILE;
	}
	return error;
}

static int uci_context_create_config_path(srpo_uci_ctx_t *ctx, const char *config)
{
	int error = 0;
	size_t path_len = strlen(ctx->config_dir);
	size_t config_len = strlen(config);

	if (path_len + config_len + 1 > sizeof(ctx->config_path)) {
		error = SRPO_UCI_ERR_FILE_PATH_SIZE;
		goto out;
	}

	snprintf(ctx->config_path, sizeof(ctx->config_path), "%s", ctx->config_dir);
	if (ctx->config_path[path_len - 1] != '/') {
		// no trailing '/' -> add one
		ctx->config_path[path_len] = '/';
		ctx->config_path[++path_len] = 0;
	}
	snprintf(ctx->config_path + path_len, sizeof(ctx->config_path) - path_len, "%s", config);
out:
	return error;
}

static int uci_context_revert(srpo_uci_ctx_t *ctx, const char *config)
{
	int error = 0;
	if (strcmp(config, ctx->current_file) == 0) {
		// reload the file
		uci2_free_ctx(ctx->parser_ctx);
		error = uci_context_load(ctx, config);
	}
	return error;
}
static int uci_context_commit(srpo_uci_ctx_t *ctx, const char *config)
{
	int error = 0;
	if (strcmp(config, ctx->current_file) == 0) {
		// write to file
		error = uci2_export_ctx_fsync(ctx->parser_ctx, ctx->config_path);
	}
	return error;
}

static void uci_context_free(srpo_uci_ctx_t *ctx)
{
	if (ctx) {
		if (ctx->current_file) {
			FREE_SAFE(ctx->current_file);
		}
		uci2_free_ctx(ctx->parser_ctx);
		free(ctx);
	}
}