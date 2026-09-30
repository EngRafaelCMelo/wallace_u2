#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void atividade1_start(void);
void atividade2_start(void);
void atividade3_start(void);

void app_main(void)
{
#if CONFIG_APP_ATIVIDADE_1
    atividade1_start();
#elif CONFIG_APP_ATIVIDADE_2
    atividade2_start();
#elif CONFIG_APP_ATIVIDADE_3
    atividade3_start();
#endif

    /* O trabalho de cada atividade continua nas tarefas criadas acima. */
    vTaskDelete(NULL);
}
