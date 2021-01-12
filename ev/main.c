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

	val = NULL;
	err = 0;
	filen = NULL;
	cmd = NULL;
	newval = NULL;

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

	val = binit();
	if(val == NULL) {
		fprintf(stderr, "Can't allocate val\n");
		err = -1;
		goto end_label;
	}

	ret = hiredis_get(argv[1], val);
        if(ret == ENOENT) {
                fprintf(stderr, "Key not found: %s\n", argv[1]);
		err = -1;
		goto end_label;
        } else
        if(ret != 0) {
                fprintf(stderr, "Error while calling hiredis_get: %s",
		    strerror(ret));
                err = ret;
                goto end_label;
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
		printf("Value unchanged.\n");
		goto end_label;
	}

	ret = hiredis_set(argv[1], newval);
        if(ret != 0) {
                fprintf(stderr, "Could not update value in redis.\n");
		err = -1;
		goto end_label;
        } else {
                printf("Update successful.\n");
	}


end_label:
	
	buninit(&val);

	if(!bstrempty(filen)) {
		(void) unlink(bget(filen));
	}
	buninit(&filen);
	buninit(&cmd);
	buninit(&newval);

	hiredis_uninit();

	return err;

}


void
usage(const char *execn)
{
	printf("usage: %s <key>\n", execn);
}
