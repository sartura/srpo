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

static struct uci_context *uci_context;

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