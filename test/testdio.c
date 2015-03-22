#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>
#include <unistd.h>



#include "logger.h"
#include "dio.h"

#define false  0
#define true   1

#define bool unsigned char

int callback(uint8_t msg_id, uint8_t dev_num, uint8_t *payload, uint16_t len)
{
    bool door_opened = false;
    bool tech_switch_enabled = false;

    if (*payload & DOOR_OPEN)
        door_opened = true;
    else
        door_opened = false;

    if (*payload & TPB_ACTIVE)
        tech_switch_enabled = true;
    else
        tech_switch_enabled = false;

    printf("payload = %02x\n", *payload);
    printf("door status: %s\n", door_opened ? "opened":"closed");
    printf("technician switch: %s\n", tech_switch_enabled ? "enabled":"disabled");
    return 0;
}


int main(int argc , char **argv)
{

	int status;
	char ver[20];
	int i = 10;
	logger_init();
	dio_init();
		
	register_dio_callback(callback);
#if  0 
	dio_5v_set(1);
	dio_bdp_bus_a_set(1);
	dio_bdp_bus_b_set(1);
	dio_spare_power_set(1);
	status = dio_5v_get();
	printf("dio 5v get =%d\n",status);
	dio_modem_power_set(1);

	status =dio_modem_power_get();
	printf("dio modem get =%d\n",status);
#endif 
	while(1) {
		sleep(20);
	}
	
}

