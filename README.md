# Atividade FreeRTOS para ESP32

Este projeto contém três atividades acadêmicas em C usando as APIs nativas do FreeRTOS e do ESP-IDF. O workspace estava vazio, por isso foi criada uma estrutura mínima de projeto ESP-IDF.

## 1. Estrutura dos arquivos

```text
.
├── CMakeLists.txt
├── sdkconfig.defaults
├── README.md
├── RELATORIO.md
└── main/
    ├── CMakeLists.txt
    ├── Kconfig.projbuild
    ├── main.c
    ├── atividade1.c
    ├── atividade2.c
    └── atividade3.c
```

`main.c` chama uma única atividade por vez. A escolha é feita em `menuconfig`, no menu **Atividade acadêmica FreeRTOS**. Os três arquivos de atividade são compilados no firmware, mas somente a atividade selecionada é iniciada.

## 2. Como compilar

Abra um terminal do ESP-IDF com o ambiente já exportado e execute:

```powershell
cd C:\Users\Rafael\Desktop\wallace_u2
idf.py set-target esp32
idf.py menuconfig
```

No menu, selecione uma opção em **Atividade acadêmica FreeRTOS → Atividade a executar**, salve e então compile:

```powershell
idf.py build
```

Depois de trocar a atividade no `menuconfig`, execute novamente `idf.py build`.

## 3. Como gravar no ESP32

Substitua `COM3` pela porta serial real da placa:

```powershell
idf.py -p COM3 flash
```

O comando combinado abaixo grava e abre o monitor:

```powershell
idf.py -p COM3 flash monitor
```

Para sair do monitor serial, use `Ctrl+]`.

## 4. Configurações relevantes

O arquivo `sdkconfig.defaults` habilita:

```text
CONFIG_FREERTOS_USE_TRACE_FACILITY=y
```

Essa opção é necessária para `uxTaskGetSystemState()` e `TaskStatus_t`, usados na Atividade 1. `CONFIG_FREERTOS_USE_STATS_FORMATTING_FUNCTIONS` não é necessário, pois o código não usa `vTaskList()` nem `vTaskGetRunTimeStats()`.

O código não fixa o valor do tick: a Atividade 2 imprime o `configTICK_RATE_HZ` real do firmware. Também não há dependência de Wi-Fi ou Bluetooth. Se o projeto for configurado como unicore, a Atividade 2 usa o Core 0; em uma configuração multicore, fixa as duas tarefas no Core 1.

## 5. O que observar

### Atividade 1

O monitor imprime uma tabela a cada cinco segundos. A coluna `Stack HWM` é o *High Water Mark*, isto é, a menor quantidade de stack livre observada, em unidades de `StackType_t`. Ela não é o tamanho total originalmente alocado. A lista de tarefas depende dos componentes e da configuração do firmware. O código usa `vTaskDelayUntil()` em versões anteriores do IDF e `xTaskDelayUntil()` no IDF 6, onde a função antiga foi removida.

### Atividade 2

O monitor deve mostrar os números reais obtidos pela placa: overhead do `esp_timer_get_time()`, média bruta e corrigida, mínimo, máximo, desvio padrão e os dois percentuais solicitados. Não há impressão dentro do loop de 10.000 medições.

### Atividade 3

As cinco produtoras escrevem cinco vezes cada uma. As duas leitoras retiram os itens sem busy waiting. Ao final, a saída deve indicar `Total produzido: 25`, `Total consumido: 25` e `Resultado: OK`.

## 6. Limites experimentais

Os valores das Atividades 1 e 2 dependem do modelo do ESP32, da versão do ESP-IDF, da configuração do FreeRTOS e das interrupções presentes no momento do teste. O relatório contém espaços para colar a saída real do monitor serial. Nenhum resultado de hardware foi inventado.

Não foi gerado `Relatorio_FreeRTOS.pdf` porque Pandoc, LaTeX e CMake/ESP-IDF não estavam disponíveis neste ambiente. O PDF pode ser gerado posteriormente a partir de `RELATORIO.md` quando uma ferramenta já instalada estiver disponível, por exemplo:

```powershell
pandoc RELATORIO.md -o Relatorio_FreeRTOS.pdf
```
