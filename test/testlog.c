#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "logger.h"


int main(int argc , char **argv)
{

	char ver[20];
	int i = 10;
	fprintf(stderr,"log init\n");
	logger_init();

	sleep(2);
	while (i--) {
		
		ALOGD("TST","Helo world!");
		sleep(1);
	}
	logger_close();

		
}

