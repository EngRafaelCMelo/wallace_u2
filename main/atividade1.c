#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_idf_version.h"

#include <stdio.h>

/*
 * O tamanho é fixo para que o monitor não faça malloc a cada amostra.
 * O valor cobre as tarefas usuais de um projeto ESP-IDF pequeno.
 */
#define ATV1_MAX_TAREFAS 64U
#define ATV1_PERIODO_MS 5000U

#if ESP_IDF_VERSION_MAJOR >= 6
/* vTaskDelayUntil foi removida no IDF 6; xTaskDelayUntil é a API equivalente. */
#define ATV1_DELAY_UNTIL xTaskDelayUntil
#else
#define ATV1_DELAY_UNTIL vTaskDelayUntil
#endif

static TaskStatus_t s_status[ATV1_MAX_TAREFAS];

static const char *estado_para_texto(eTaskState estado)
{
    switch (estado) {
    case eRunning:
        return "RUNNING";
    case eReady:
        return "READY";
    case eBlocked:
        return "BLOCKED";
    case eSuspended:
        return "SUSPENDED";
    case eDeleted:
        return "DELETED";
    case eInvalid:
    default:
        return "INVALID";
    }
}

static const char *afinidade_para_texto(TaskHandle_t tarefa)
{
#if defined(CONFIG_FREERTOS_SMP) && CONFIG_FREERTOS_SMP \
    && defined(CONFIG_FREERTOS_NUMBER_OF_CORES) \
    && (CONFIG_FREERTOS_NUMBER_OF_CORES > 1)
    /* No modo SMP, a API atual retorna uma máscara de afinidade. */
    UBaseType_t mascara = vTaskCoreAffinityGet(tarefa);

    if (mascara == ((UBaseType_t) 1U << 0)) {
        return "0";
    }

    if (mascara == ((UBaseType_t) 1U << 1)) {
        return "1";
    }

    return "ANY";
#else
    /* No modo tradicional do ESP-IDF, xTaskGetCoreID() informa a afinidade. */
    BaseType_t core = xTaskGetCoreID(tarefa);

    if (core == tskNO_AFFINITY || core < 0) {
        return "ANY";
    }

    if (core == 0) {
        return "0";
    }

    if (core == 1) {
        return "1";
    }

    return "?";
#endif
}

static void tarefa_monitoramento(void *argumento)
{
    (void)argumento;
    TickType_t proxima_execucao = xTaskGetTickCount();

    for (;;) {
        UBaseType_t tarefas_existentes = uxTaskGetNumberOfTasks();
        UBaseType_t tarefas_copiadas;
        tarefas_copiadas = uxTaskGetSystemState(
            s_status,
            ATV1_MAX_TAREFAS,
            NULL);

        printf("\n===============================================\n");
        printf("TAREFAS FREERTOS\n");
        printf("Quantidade de tarefas: %u\n", (unsigned) tarefas_existentes);
        printf("===============================================\n");
        printf("%-16s %-10s %-11s %-12s %s\n",
               "Nome", "Estado", "Prioridade", "Stack HWM", "Core");
        printf("---------------------------------------------------------------\n");

        for (UBaseType_t indice = 0; indice < tarefas_copiadas; ++indice) {
            printf("%-16s %-10s %-11u %-12u %s\n",
                   s_status[indice].pcTaskName,
                   estado_para_texto(s_status[indice].eCurrentState),
                   (unsigned) s_status[indice].uxCurrentPriority,
                   (unsigned) s_status[indice].usStackHighWaterMark,
                   afinidade_para_texto(s_status[indice].xHandle));
        }

        if (tarefas_existentes > ATV1_MAX_TAREFAS) {
            printf("Aviso: o vetor comporta %u tarefas; %u foram copiadas.\n",
                   (unsigned) ATV1_MAX_TAREFAS,
                   (unsigned) tarefas_copiadas);
        }

        printf("===============================================\n");
        printf("Stack HWM = menor quantidade de StackType_t livres observada.\n");

        ATV1_DELAY_UNTIL(&proxima_execucao, pdMS_TO_TICKS(ATV1_PERIODO_MS));
    }
}

void atividade1_start(void)
{
    BaseType_t resultado = xTaskCreate(
        tarefa_monitoramento,
        "MonitorTarefas",
        4096,
        NULL,
        2,
        NULL);

    if (resultado != pdPASS) {
        printf("Falha ao criar a tarefa de monitoramento.\n");
    }
}
