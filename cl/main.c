#include <stdio.h>
#include <libgen.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include "bstr.h"
#include "hiredis_helper.h"

void usage(const char *);

#define EDITOR		"/usr/bin/vi"
#define KEY_PREFIX	"list:"


int
main(int argc, char **argv)
{
	char	*execn;
	int	err;
	int	ret;
	barr_t	*elems;
	bstr_t	*elem;
	bstr_t	*key;
	char	*listn;
	int	c;
	int	silent;

	err = 0;
	elems = NULL;
	key = NULL;

	execn = basename(argv[0]);
	if(xstrempty(execn)) {
		fprintf(stderr, "Can't get executable name\n");
		err = -1;
		goto end_label;
	}

	silent = 0;
	opterr = 0;

	while ((c = getopt (argc, argv, "s")) != -1) {
		switch (c) {
		case 's':
			++silent;
			break;
		case '?':
			fprintf (stderr, "Unknown option `-%c'\n", optopt);
			err = -1;
			goto end_label;
		default:
			fprintf (stderr, "Error while parsing options");
			err = -1;
			goto end_label;
		}
	}

	if(argc != optind + 1 || xstrempty(argv[optind])) {
		usage(execn);
		err = -1;
		goto end_label;
	}

	listn = argv[optind];

	key = binit();
	if(key == NULL) {
		fprintf(stderr, "Could not initialize key\n");
		err = -1;
		goto end_label;
	}
	bprintf(key, "%s%s", KEY_PREFIX, listn);

	ret = hiredis_init();
	if(ret != 0) {
		fprintf(stderr, "Could not connect to redis\n");
		err = -1;
		goto end_label;
	}

	elems = barr_init(sizeof(bstr_t));
	if(elems == NULL) {
		fprintf(stderr, "Couldn't allocate elems\n");
		err = -1;
		goto end_label;
	}

	ret = hiredis_lrange(bget(key), 0, - 1, elems);
	if(ret != 0) {
		fprintf(stderr, "Couldn't lrange: %s\n", strerror(ret));
		err = -1;
		goto end_label;
	}

	if(barr_cnt(elems) == 0) {
		if(!silent) {
			fprintf(stderr, "Key doesn't exist\n");
			err = -1;
			goto end_label;
		}
	} else {
			for(elem = (bstr_t *) barr_begin(elems);
		    elem < (bstr_t *) barr_end(elems); ++elem) {
			printf("%s\n", bget(elem));
		}
	}

end_label:
	
	if(elems) {
		for(elem = (bstr_t *) barr_begin(elems);
		    elem < (bstr_t *) barr_end(elems); ++elem) {
			buninit_(elem);
		}
		barr_uninit(&elems);
	}

	buninit(&key);

	hiredis_uninit();

	return err;

}


void
usage(const char *execn)
{
	printf("usage: %s [-s] <key>\n", execn);
}
