#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "esp_timer.h"

#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>

#define NUM_MEDICOES 10000U
#define NUM_CICLOS (NUM_MEDICOES / 2U)
#define NUM_MEDICOES_TIMER 10000U

/* As duas tarefas ficam abaixo da prioridade máxima para não monopolizar o sistema. */
#define ATV2_PRIORIDADE (configMAX_PRIORITIES - 2U)

#if defined(CONFIG_FREERTOS_UNICORE) && CONFIG_FREERTOS_UNICORE
#define ATV2_CORE 0
#elif defined(portNUM_PROCESSORS) && (portNUM_PROCESSORS > 1)
#define ATV2_CORE 1
#elif defined(CONFIG_FREERTOS_NUMBER_OF_CORES) && (CONFIG_FREERTOS_NUMBER_OF_CORES > 1)
#define ATV2_CORE 1
#else
#define ATV2_CORE 0
#endif

#define BIT_INICIO BIT0
#define BIT_TASK1_PRONTA BIT1
#define BIT_TASK1_FINALIZADA BIT2

static EventGroupHandle_t s_eventos;

/* A alternância é serializada pelo próprio escalonamento no mesmo núcleo. */
static volatile int64_t s_instante_antes_de_ceder;
static int64_t s_inicio_experimento;
static int64_t s_fim_experimento;

static uint32_t s_numero_amostras;
static uint64_t s_soma_us;
static double s_soma_quadrados;
static int64_t s_minimo_us = INT64_MAX;
static int64_t s_maximo_us;
static double s_overhead_timer_us;

static double medir_overhead_esp_timer(void)
{
    uint64_t soma = 0;

    for (uint32_t indice = 0; indice < NUM_MEDICOES_TIMER; ++indice) {
        int64_t inicio = esp_timer_get_time();
        int64_t fim = esp_timer_get_time();
        int64_t delta = fim - inicio;

        if (delta < 0) {
            delta = 0;
        }

        soma += (uint64_t) delta;
    }

    return (double) soma / (double) NUM_MEDICOES_TIMER;
}

static void registrar_amostra(int64_t delta_us)
{
    if (delta_us < 0) {
        delta_us = 0;
    }

    s_soma_us += (uint64_t) delta_us;
    s_soma_quadrados += (double) delta_us * (double) delta_us;
    ++s_numero_amostras;

    if (delta_us < s_minimo_us) {
        s_minimo_us = delta_us;
    }
    if (delta_us > s_maximo_us) {
        s_maximo_us = delta_us;
    }
}

static void imprimir_resultados(void)
{
    double media_bruta = (double) s_soma_us / (double) s_numero_amostras;
    double media_corrigida = media_bruta - s_overhead_timer_us;
    double variancia = (s_soma_quadrados / (double) s_numero_amostras)
                       - (media_bruta * media_bruta);
    double desvio_padrao;
    double periodo_tick_us = 1000000.0 / (double) configTICK_RATE_HZ;
    double percentual_tick;
    int64_t tempo_total_experimento = s_fim_experimento - s_inicio_experimento;
    double percentual_experimento;

    if (media_corrigida < 0.0) {
        media_corrigida = 0.0;
    }
    if (variancia < 0.0) {
        variancia = 0.0;
    }
    desvio_padrao = sqrt(variancia);
    percentual_tick = (media_corrigida / periodo_tick_us) * 100.0;

    if (tempo_total_experimento > 0) {
        percentual_experimento =
            ((double) s_soma_us / (double) tempo_total_experimento) * 100.0;
    } else {
        percentual_experimento = 0.0;
    }

    printf("\n===== TESTE DE TROCA DE CONTEXTO =====\n");
    printf("Core utilizado: %d\n", ATV2_CORE);
    printf("Prioridade das tarefas: %u\n", (unsigned) ATV2_PRIORIDADE);
    printf("NUM_MEDICOES: %u\n", (unsigned) NUM_MEDICOES);
    printf("Tick rate: %u Hz\n", (unsigned) configTICK_RATE_HZ);
    printf("Período do tick: %.3f us\n", periodo_tick_us);
    printf("\nOverhead médio esp_timer_get_time(): %.3f us\n", s_overhead_timer_us);
    printf("\nTroca de contexto:\n");
    printf("Medições efetivamente registradas: %u\n", (unsigned) s_numero_amostras);
    printf("Tempo total medido (soma bruta): %" PRIu64 " us\n", s_soma_us);
    printf("Média bruta: %.3f us\n", media_bruta);
    printf("Média corrigida: %.3f us\n", media_corrigida);
    printf("Mínimo: %" PRId64 " us\n", s_minimo_us);
    printf("Máximo: %" PRId64 " us\n", s_maximo_us);
    printf("Desvio padrão bruto: %.3f us\n", desvio_padrao);
    printf("Tempo total do experimento: %" PRId64 " us\n", tempo_total_experimento);
    printf("\nPercentual equivalente de um tick: %.3f %%\n", percentual_tick);
    printf("Percentual observado no experimento: %.3f %%\n", percentual_experimento);
    printf("=======================================\n");
}

static void tarefa1_contexto(void *argumento)
{
    (void) argumento;
    xEventGroupWaitBits(s_eventos, BIT_INICIO, pdFALSE, pdTRUE, portMAX_DELAY);

    /* Sincronização inicial: garante que a primeira troca parta da Task1. */
    xEventGroupSetBits(s_eventos, BIT_TASK1_PRONTA);
    taskYIELD();

    s_inicio_experimento = esp_timer_get_time();

    for (uint32_t indice = 0; indice < NUM_CICLOS; ++indice) {
        /* O instante fica imediatamente antes da cessão voluntária da CPU. */
        s_instante_antes_de_ceder = esp_timer_get_time();
        taskYIELD();

        /* A Task2 escreveu seu instante antes de ceder a CPU para cá. */
        int64_t fim = esp_timer_get_time();
        registrar_amostra(fim - s_instante_antes_de_ceder);
    }

    s_fim_experimento = esp_timer_get_time();
    xEventGroupSetBits(s_eventos, BIT_TASK1_FINALIZADA);
    imprimir_resultados();
    vTaskDelete(NULL);
}

static void tarefa2_contexto(void *argumento)
{
    (void) argumento;
    xEventGroupWaitBits(s_eventos, BIT_INICIO, pdFALSE, pdTRUE, portMAX_DELAY);
    xEventGroupWaitBits(s_eventos, BIT_TASK1_PRONTA, pdFALSE, pdTRUE, portMAX_DELAY);
    taskYIELD();

    for (uint32_t indice = 0; indice < NUM_CICLOS; ++indice) {
        /* O timestamp foi escrito pela Task1 imediatamente antes do yield. */
        int64_t fim = esp_timer_get_time();
        registrar_amostra(fim - s_instante_antes_de_ceder);

        /* Este é o início da medição no sentido Task2 -> Task1. */
        s_instante_antes_de_ceder = esp_timer_get_time();
        taskYIELD();
    }

    /* A Task1 precisa executar sua última leitura antes de ser finalizada. */
    xEventGroupWaitBits(s_eventos, BIT_TASK1_FINALIZADA, pdFALSE, pdTRUE, portMAX_DELAY);
    vTaskDelete(NULL);
}

void atividade2_start(void)
{
    s_overhead_timer_us = medir_overhead_esp_timer();
    s_eventos = xEventGroupCreate();

    if (s_eventos == NULL) {
        printf("Falha ao criar os eventos da Atividade 2.\n");
        return;
    }

    BaseType_t resultado1 = xTaskCreatePinnedToCore(
        tarefa1_contexto,
        "Task1",
        4096,
        NULL,
        ATV2_PRIORIDADE,
        NULL,
        ATV2_CORE);
    BaseType_t resultado2 = xTaskCreatePinnedToCore(
        tarefa2_contexto,
        "Task2",
        4096,
        NULL,
        ATV2_PRIORIDADE,
        NULL,
        ATV2_CORE);

    if (resultado1 != pdPASS || resultado2 != pdPASS) {
        printf("Falha ao criar as tarefas da Atividade 2.\n");
        return;
    }

    xEventGroupSetBits(s_eventos, BIT_INICIO);
}
