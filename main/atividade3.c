#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <stdbool.h>
#include <stdio.h>

#define BUFFER_SIZE 10U
#define STRING_SIZE 20U
#define NUM_PRODUTORES 5U
#define PRODUCOES_POR_TAREFA 5U
#define TEMPO_ESPERA_LEITURA_MS 100U

static char buffer[BUFFER_SIZE][STRING_SIZE];
static size_t writeIndex;
static size_t readIndex;
static size_t bufferCount;

static uint32_t totalProduzido;
static uint32_t totalConsumido;
static uint32_t produtoresFinalizados;
static uint32_t leitorasFinalizadas;

static SemaphoreHandle_t mutexBuffer;
static SemaphoreHandle_t semEspacosLivres;
static SemaphoreHandle_t semItensDisponiveis;

typedef struct {
    const char *nome;
} ProdutorArgumento;

static const ProdutorArgumento produtores[NUM_PRODUTORES] = {
    {"Temperatura"},
    {"Umidade"},
    {"Velocidade"},
    {"Peso"},
    {"Distancia"},
};

static void tarefa_produtora(void *argumento)
{
    const ProdutorArgumento *produtor = (const ProdutorArgumento *) argumento;

    for (uint32_t indice = 0; indice < PRODUCOES_POR_TAREFA; ++indice) {
        /* O semáforo impede que uma posição ocupada seja sobrescrita. */
        if (xSemaphoreTake(semEspacosLivres, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        xSemaphoreTake(mutexBuffer, portMAX_DELAY);
        (void) snprintf(buffer[writeIndex], STRING_SIZE, "%s", produtor->nome);
        writeIndex = (writeIndex + 1U) % BUFFER_SIZE;
        ++bufferCount;
        ++totalProduzido;
        xSemaphoreGive(mutexBuffer);

        /* O item só fica disponível depois de a escrita estar concluída. */
        xSemaphoreGive(semItensDisponiveis);
    }

    xSemaphoreTake(mutexBuffer, portMAX_DELAY);
    ++produtoresFinalizados;
    xSemaphoreGive(mutexBuffer);

    printf("%s: Escrita Finalizada.\n", produtor->nome);
    vTaskDelete(NULL);
}

static void tarefa_leitora(void *argumento)
{
    const char *nome_tarefa = (const char *) argumento;
    char item[STRING_SIZE];

    for (;;) {
        BaseType_t obteve_item = xSemaphoreTake(
            semItensDisponiveis,
            pdMS_TO_TICKS(TEMPO_ESPERA_LEITURA_MS));

        if (obteve_item == pdTRUE) {
            bool item_lido = false;

            xSemaphoreTake(mutexBuffer, portMAX_DELAY);
            if (bufferCount > 0U) {
                (void) snprintf(item, STRING_SIZE, "%s", buffer[readIndex]);
                readIndex = (readIndex + 1U) % BUFFER_SIZE;
                --bufferCount;
                ++totalConsumido;
                item_lido = true;
            }
            xSemaphoreGive(mutexBuffer);

            if (item_lido) {
                xSemaphoreGive(semEspacosLivres);
                printf("%s: %s\n", nome_tarefa, item);
            }
            continue;
        }

        /*
         * O timeout evita que uma leitora fique bloqueada para sempre depois
         * do último produtor. A verificação continua protegida pelo mutex.
         */
        bool deve_finalizar = false;
        bool e_ultima_leitora = false;
        uint32_t produzido_final = 0U;
        uint32_t consumido_final = 0U;

        xSemaphoreTake(mutexBuffer, portMAX_DELAY);
        if (produtoresFinalizados == NUM_PRODUTORES && bufferCount == 0U) {
            ++leitorasFinalizadas;
            deve_finalizar = true;
            e_ultima_leitora = (leitorasFinalizadas == 2U);
            produzido_final = totalProduzido;
            consumido_final = totalConsumido;
        }
        xSemaphoreGive(mutexBuffer);

        if (deve_finalizar) {
            printf("%s: Leitura finalizada!\n", nome_tarefa);

            if (e_ultima_leitora) {
                printf("\n===== VALIDAÇÃO DA ATIVIDADE 3 =====\n");
                printf("Total produzido: %u\n", (unsigned) produzido_final);
                printf("Total consumido: %u\n", (unsigned) consumido_final);
                printf("Resultado: %s\n",
                       (produzido_final == 25U && consumido_final == 25U)
                           ? "OK"
                           : "ERRO");
                printf("=====================================\n");
            }

            vTaskDelete(NULL);
        }
    }
}

void atividade3_start(void)
{
    mutexBuffer = xSemaphoreCreateMutex();
    semEspacosLivres = xSemaphoreCreateCounting(BUFFER_SIZE, BUFFER_SIZE);
    semItensDisponiveis = xSemaphoreCreateCounting(BUFFER_SIZE, 0U);

    if (mutexBuffer == NULL || semEspacosLivres == NULL || semItensDisponiveis == NULL) {
        printf("Falha ao criar as primitivas da Atividade 3.\n");
        return;
    }

    for (uint32_t indice = 0; indice < NUM_PRODUTORES; ++indice) {
        BaseType_t resultado = xTaskCreate(
            tarefa_produtora,
            produtores[indice].nome,
            3072,
            (void *) &produtores[indice],
            2,
            NULL);

        if (resultado != pdPASS) {
            printf("Falha ao criar a produtora %s.\n", produtores[indice].nome);
        }
    }

    static const char nome_leitora_1[] = "TaskLeitura1";
    static const char nome_leitora_2[] = "TaskLeitura2";

    if (xTaskCreate(tarefa_leitora, "TaskLeitura1", 3072,
                   (void *) nome_leitora_1, 3, NULL) != pdPASS) {
        printf("Falha ao criar TaskLeitura1.\n");
    }
    if (xTaskCreate(tarefa_leitora, "TaskLeitura2", 3072,
                   (void *) nome_leitora_2, 3, NULL) != pdPASS) {
        printf("Falha ao criar TaskLeitura2.\n");
    }
}
