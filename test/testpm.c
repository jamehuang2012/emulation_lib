#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "logger.h"
#include "pm.h"


int main(int argc , char **argv)
{

	char ver[20];
	int i = 10;
	fprintf(stderr,"log init\n");
	logger_init();
	pm_init();

	sleep(10);
	while (i--) {
		
//		pdm_get_firmware_version(ver);
		sleep(20);
	}

		
}

