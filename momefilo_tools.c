#include <stdlib.h>
#include <errno.h>
#include <time.h>

void msort(int *array, int size){
	int i;
	int j;
	int tmp;
	for(i=0; i<size - 1; i++){
		j = i + 1;
		tmp = array[j];
		while(j>0 && array[j - 1]>tmp){
			array[j] = array[j - 1];
			--j;
		}
		array[j] = tmp;
	}
}

/* Sleepfunktion
 * https://qnaplus.com/c-program-to-sleep-in-milliseconds/
 */
int mmillisleep(long millisec){
	struct timespec ts;
	int ret;

	if(millisec < 0){
		errno = EINVAL;
		return - 1;
	}
	ts.tv_sec = millisec / 1000;
	ts.tv_nsec = (millisec % 1000) * 1000000;

	do{
		ret = nanosleep(&ts, &ts);
	}while( ret && errno == EINTR);

	return ret;
}
