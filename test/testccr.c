#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "logger.h"
#include "cmgrapi.h"


int main(int argc , char **argv)
{

	char ver[20];
	int i = 10;
	logger_init();
	pm_init();
	sleep(10);

	ccr_power_on();	
	ccr_init();

	sleep(2);
	while (i--) {
		
		ccr_power_on();	
		start_ccr();
		sleep(10);
		stop_ccr();
///		ccr_power_off();
		sleep(5);
	}

		
}

