#include "common.h"
#include <git2.h>
#include <git2/clone.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

typedef struct progress_data {
	git_transfer_progress fetch_progress;
	size_t completed_steps;
	size_t total_steps;
	const char *path;
} progress_data;

static void print_progress(const progress_data *pd)
{
	int network_percent = (100*pd->fetch_progress.received_objects) / pd->fetch_progress.total_objects;
	int index_percent = (100*pd->fetch_progress.indexed_objects) / pd->fetch_progress.total_objects;
	int checkout_percent = pd->total_steps > 0
		? (100 * pd->completed_steps) / pd->total_steps
		: 0.f;
	int kbytes = pd->fetch_progress.received_bytes / 1024;

	printf("net %3d%% (%4d kb, %5d/%5d)  /  idx %3d%% (%5d/%5d)  /  chk %3d%% (%4" PRIuZ "/%4" PRIuZ ") %s\n",
		   network_percent, kbytes,
		   pd->fetch_progress.received_objects, pd->fetch_progress.total_objects,
		   index_percent, pd->fetch_progress.indexed_objects, pd->fetch_progress.total_objects,
		   checkout_percent,
		   pd->completed_steps, pd->total_steps,
		   pd->path);
}

static void fetch_progress(const git_transfer_progress *stats, void *payload)
{
	progress_data *pd = (progress_data*)payload;
	pd->fetch_progress = *stats;
	print_progress(pd);
}
static void checkout_progress(const char *path, size_t cur, size_t tot, void *payload)
{
	progress_data *pd = (progress_data*)payload;
	pd->completed_steps = cur;
	pd->total_steps = tot;
	pd->path = path;
	print_progress(pd);
}

int do_clone(git_repository *repo, int argc, char **argv)
{
	progress_data pd;
	git_repository *cloned_repo = NULL;
	git_checkout_opts checkout_opts;
	const char *url = argv[1];
	const char *path = argv[2];
	int error;

	(void)repo; // unused

	// Validate args
	if (argc < 3) {
		printf ("USAGE: %s <url> <path>\n", argv[0]);
		return -1;
	}

	// Set up options
	memset(&checkout_opts, 0, sizeof(checkout_opts));
	checkout_opts.checkout_strategy = GIT_CHECKOUT_SAFE;
	checkout_opts.progress_cb = checkout_progress;
	memset(&pd, 0, sizeof(pd));
	checkout_opts.progress_payload = &pd;

	// Do the clone
	error = git_clone(&cloned_repo, url, path, &fetch_progress, &pd, &checkout_opts);
	printf("\n");
	if (error != 0) {
		const git_error *err = giterr_last();
		if (err) printf("ERROR %d: %s\n", err->klass, err->message);
		else printf("ERROR %d: no detailed info\n", error);
	}
	else if (cloned_repo) git_repository_free(cloned_repo);
	return error;
}
