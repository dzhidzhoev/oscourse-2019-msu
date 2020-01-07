#include <inc/lib.h>
#include <inc/x86.h>

extern void (*_start)(void);

#define MOD 20
#define COUNT 100

void
umain(int argc, char **argv)
{
	int counts[MOD] = {};
	int max_pgnum = 0;
	int min_pgnum = 100000000;
	for (int i = 0; i < COUNT; i++) {
		int fd[2];
		if (pipe(fd) < 0) {
			panic("unable to pipe");
		}
		int pid = fork();
		if (pid < 0) {
			panic("unable to fork");
		}
		if (!pid) {
			int r;
			
			if ((r = close(fd[0])) < 0 || (r = dup(fd[1], 1)) < 0 || (r = close(fd[1])) < 0){
				panic("unable to setup pipe");
			}
			const char *args[] = {
				"testentry",
				"quiet",
				NULL
			};
			if (spawn("testentry", args) < 0) {
				panic("unable to spawn");
			}
			return;
		}
		if (close(fd[1]) < 0) {
			panic("unable to close write pipe on parent");
		}
		unsigned eip;
		if (read(fd[0], &eip, sizeof(eip)) != sizeof(eip) || eip < 0) {
			panic("unable to read eip");
		}
		if (close(fd[0]) < 0) {
			panic("unable to close read pipe on parent");
		}
		cprintf("%x\n", PGNUM(eip));
		max_pgnum = MAX(max_pgnum, PGNUM(eip));
		min_pgnum = MIN(min_pgnum, PGNUM(eip));
		counts[PGNUM(eip) % MOD]++;
	}
	int min  = counts[0], max = counts[0], sum = 0;
	for (int i = 0; i < MOD; i++) {
		min = MIN(min, counts[i]);
		max = MAX(max, counts[i]);
		sum += counts[i];
	}
	if (sum != COUNT || max < min || max - min > 2 * COUNT / MOD) {
		cprintf("BAD ASLR sum %d max count %d min count %d!!!\n", sum, max, min);
		for (int i = 0; i < MOD; i++) {
			cprintf("%d - %d\n", i, counts[i]);
		}
	}
	cprintf("Min page num - %x, max page num - %x\n", min_pgnum, max_pgnum);
}
