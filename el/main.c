#include <stdio.h>
#include <libgen.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include "bstr.h"
#include "hiredis_helper.h"

void usage(const char *);

#define EDITOR		"/usr/bin/vi"
#define KEY_PREF	"list:"

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
	char	*listn;
	bstr_t	*key;
	bstr_t	*tmpkey;
	int	c;
	int	docreate;
	barr_t	*newelems;

	val = NULL;
	err = 0;
	filen = NULL;
	key = NULL;
	tmpkey = NULL;
	cmd = NULL;
	newval = NULL;
	elems = NULL;
	newelems = NULL;

	execn = basename(argv[0]);
	if(xstrempty(execn)) {
		fprintf(stderr, "Can't get executable name\n");
		err = -1;
		goto end_label;
	}

	docreate = 0;
	opterr = 0;

	while ((c = getopt (argc, argv, "c")) != -1) {
		switch (c) {
		case 'c':
			++docreate;
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
	

	ret = hiredis_init();
	if(ret != 0) {
		fprintf(stderr, "Could not connect to redis\n");
		err = -1;
		goto end_label;
	}

	key = binit();
	if(key == NULL) {
		fprintf(stderr, "Can't allocate key\n");
		err = -1;
		goto end_label;
	}
	bprintf(key, "%s%s", KEY_PREF, listn);

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

	ret = hiredis_lrange(bget(key), 0, - 1, elems);
	if(ret != 0) {
		fprintf(stderr, "Couldn't lrange: %s\n", strerror(ret));
		err = -1;
		goto end_label;
	}

	if(barr_cnt(elems) == 0 && !docreate) {
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
	
	if(!bstrempty(val)) { /* OK to be empty if we are creating */
		ret = btofile(bget(filen), val);
		if(ret != 0) {
			fprintf(stderr, "Couldn't write value to filen\n");
			err = -1;
			goto end_label;
		}
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
	if(ret != 0 && !docreate) {
		/* OK for file to not exist if we are creating. Just means
		 * "unchanged". */
		fprintf(stderr, "Couldn't load file\n");
		err = -1;
		goto end_label;
	}

	if(!bstrcmp(val, bget(newval))) {
		if(docreate && bstrempty(newval))
			printf("List not created.\n");
		else
			printf("List not changed.\n");
		goto end_label;
	}

	ret = bstrsplit(newval, "\n", 0, &newelems);
	if(ret != 0 || newelems == NULL) {
		fprintf(stderr, "Couldn't split new value\n");
		err = -1;
		goto end_label;
	}
	if(barr_cnt(newelems) == 0) {
		fprintf(stderr, "New list can't be empty, use -d to delete"
		    " a list\n");
		err = -1;
		goto end_label;
	}

	tmpkey = binit();
	if(tmpkey == NULL) {
		fprintf(stderr, "Can't allocate tmpkey\n");
		err = -1;
		goto end_label;
	}
	bprintf(tmpkey, "%s_tmp_%s_%d_%d", KEY_PREF, execn, getpid(),
	    time(NULL));

	for(elem = (bstr_t *) barr_begin(newelems);
	    elem < (bstr_t *) barr_end(newelems); ++elem) {
		if(bstrempty(elem))
			continue;
		ret = hiredis_rpush(bget(tmpkey), bget(elem));
		if(ret != 0) {
			fprintf(stderr, "Could not rpush element.\n");
			err = -1;
			goto end_label;
		}
	}

	ret = hiredis_rename(bget(tmpkey), bget(key));
	if(ret != 0) {
		fprintf(stderr, "Could not rename.\n");
		err = -1;
		goto end_label;
	}

	printf("List updated successfully.\n");


end_label:
	
	buninit(&val);

	if(!bstrempty(filen)) {
		(void) unlink(bget(filen));
	}
	buninit(&filen);
	buninit(&cmd);
	buninit(&newval);
	buninit(&key);

	if(err != 0 && !bstrempty(tmpkey)) {
		(void) hiredis_del(bget(tmpkey), NULL);
	}
	buninit(&tmpkey);

	if(elems) {
		for(elem = (bstr_t *) barr_begin(elems);
		    elem < (bstr_t *) barr_end(elems); ++elem) {
			buninit_(elem);
		}
		barr_uninit(&elems);
	}
	if(newelems) {
		for(elem = (bstr_t *) barr_begin(newelems);
		    elem < (bstr_t *) barr_end(newelems); ++elem) {
			buninit_(elem);
		}
		barr_uninit(&newelems);
	}

	hiredis_uninit();

	return err;

}


void
usage(const char *execn)
{
	printf("usage: %s [-c] <key>\n", execn);
}
