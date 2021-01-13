#include <stdio.h>
#include <libgen.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include "bstr.h"
#include "hiredis_helper.h"

void usage(const char *);

#define EDITOR	"/usr/bin/vi"

int
main(int argc, char **argv)
{
	char	*execn;
	bstr_t	*val;
	int	err;
	int	ret;
	bstr_t	*filen;
	bstr_t	*cmd;
	bstr_t	*newval;
	barr_t	*elems;
	bstr_t	*elem;

	val = NULL;
	err = 0;
	filen = NULL;
	cmd = NULL;
	newval = NULL;
	elems = NULL;

	execn = basename(argv[0]);
	if(xstrempty(execn)) {
		fprintf(stderr, "Can't get executable name\n");
		err = -1;
		goto end_label;
	}

	if(argc != 2 || xstrempty(argv[1])) {
		usage(execn);
		err = -1;
		goto end_label;
	}

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

	val = binit();
	if(val == NULL) {
		fprintf(stderr, "Can't allocate val\n");
		err = -1;
		goto end_label;
	}

	ret = hiredis_lrange(argv[1], 0, - 1, elems);
	if(ret != 0) {
		fprintf(stderr, "Couldn't lrange: %s\n", strerror(ret));
		err = -1;
		goto end_label;
	}

	if(barr_cnt(elems) == 0) {
		fprintf(stderr, "Key doesn't exist\n");
		err = -1;
		goto end_label;
	}

	for(elem = (bstr_t *) barr_begin(elems);
	    elem < (bstr_t *) barr_end(elems); ++elem) {
		bprintf(val, "%s\n", bget(elem));
	}

	filen = binit();
	if(filen == NULL) {
		fprintf(stderr, "Can't allocate filen\n");
		err = -1;
		goto end_label;
	}
	bprintf(filen, "/tmp/%s_%d_%d", execn, getpid(), time(NULL));

	ret = btofile(bget(filen), val);
	if(ret != 0) {
		fprintf(stderr, "Couldn't write value to filen\n");
		err = -1;
		goto end_label;
	}

	cmd = binit();
	if(cmd == NULL) {
		fprintf(stderr, "Can't allocate cmd\n");
		err = -1;
		goto end_label;
	}
	bprintf(cmd, "%s %s", EDITOR, bget(filen));

	ret = system(bget(cmd));
	if(ret != 0) {
		fprintf(stderr, "Couldn't execute system command\n");
		err = -1;
		goto end_label;
	}

	newval = binit();
	if(newval == NULL) {
		fprintf(stderr, "Can't allocate newval\n");
		err = -1;
		goto end_label;
	}

	ret = bfromfile(newval, bget(filen));
	if(ret != 0) {
		fprintf(stderr, "Couldn't load file\n");
		err = -1;
		goto end_label;
	}

	if(!bstrcmp(val, bget(newval))) {
		printf("Value not changed.\n");
		goto end_label;
	}

#if 0
	ret = hiredis_set(argv[1], newval);
	if(ret != 0) {
		fprintf(stderr, "Could not update value in redis.\n");
		err = -1;
		goto end_label;
	} else {
		printf("Update successful.\n");
	}
#endif

end_label:
	
	buninit(&val);

	if(!bstrempty(filen)) {
		(void) unlink(bget(filen));
	}
	buninit(&filen);
	buninit(&cmd);
	buninit(&newval);

	if(elems) {
		for(elem = (bstr_t *) barr_begin(elems);
		    elem < (bstr_t *) barr_end(elems); ++elem) {
			buninit_(elem);
		}
		barr_uninit(&elems);
	}

	hiredis_uninit();

	return err;

}


void
usage(const char *execn)
{
	printf("usage: %s <key>\n", execn);
}
