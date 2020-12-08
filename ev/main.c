#include <stdio.h>
#include <libgen.h>
#include <errno.h>
#include "bstr.h"
#include "hiredis_helper.h"

void usage(const char *);

int
main(int argc, char **argv)
{
	char	*execn;
	bstr_t	*val;
	int	err;
	int	ret;

	val = NULL;
	err = 0;

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

	printf("%s\n", bget(val));


end_label:
	
	buninit(&val);
	hiredis_uninit();

	return err;

}


void
usage(const char *execn)
{
	printf("usage: %s <key>\n", execn);
}
