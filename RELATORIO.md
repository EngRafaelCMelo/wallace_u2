# Atividade FreeRTOS para ESP32

## 1. Introdução

O ESP32 é um microcontrolador com recursos para executar tarefas concorrentes e, em muitos modelos, possui dois núcleos de processamento. O ESP-IDF utiliza o FreeRTOS para organizar essas tarefas, controlar prioridades e fornecer mecanismos de sincronização.

Uma tarefa é uma unidade independente de execução. Ela possui uma função, uma prioridade e uma área de stack. O escalonador escolhe a tarefa pronta de maior prioridade. Quando duas tarefas compatíveis estão prontas, pode ocorrer uma troca de contexto por preempção ou por uma cessão voluntária, como a chamada `taskYIELD()`.

As três atividades foram separadas em arquivos C. A seleção de qual atividade será iniciada é feita por uma opção de `menuconfig`, mantendo o mesmo projeto compilável para os três experimentos.

## 2. Atividade 1 — Monitoramento das tarefas

### 2.1 Objetivo

O objetivo é observar as tarefas existentes durante a execução do ESP32, registrando nome, estado, prioridade, informação de stack e núcleo ou afinidade.

### 2.2 Estratégia

Foi criada a tarefa `MonitorTarefas`. Ela executa imediatamente e depois a cada cinco segundos usando a API de atraso absoluto (`vTaskDelayUntil()` em versões anteriores do IDF e `xTaskDelayUntil()` no IDF 6, que removeu a função de compatibilidade). A função `uxTaskGetSystemState()` copia as informações das tarefas para um vetor estático de `TaskStatus_t`. O vetor é alocado uma única vez, evitando `malloc` periódico.

O tamanho total da stack não é inventado. O campo público utilizado é `usStackHighWaterMark`, exibido como `Stack HWM`. A aplicação não apresenta esse valor como se fosse o tamanho total alocado.

### 2.3 Implementação

Em cada ciclo, a tarefa consulta a quantidade de tarefas e chama:

```c
uxTaskGetSystemState(vetor, capacidade, NULL);
```

Depois, percorre os elementos copiados e converte `eCurrentState` para texto. Em uma configuração SMP, a afinidade é obtida por `vTaskCoreAffinityGet()` e interpretada como máscara; nas configurações tradicionais do ESP-IDF, usa-se `xTaskGetCoreID()`. Uma máscara com mais de um núcleo permitido, ou o retorno `tskNO_AFFINITY`, é apresentada como `ANY`.

### 2.4 APIs utilizadas

- `xTaskCreate()` cria a tarefa de monitoramento.
- `uxTaskGetNumberOfTasks()` consulta uma quantidade inicial de tarefas.
- `uxTaskGetSystemState()` preenche o vetor de `TaskStatus_t`.
- `vTaskCoreAffinityGet()` consulta a máscara de afinidade em SMP; `xTaskGetCoreID()` é usado nas configurações tradicionais do ESP-IDF.
- `vTaskDelayUntil()` ou sua sucessora `xTaskDelayUntil()` mantém o período aproximado de cinco segundos.

Os estados significam:

| Estado | Significado |
|---|---|
| `RUNNING` | A tarefa está usando a CPU naquele momento, quando a API registra o estado. |
| `READY` | A tarefa pode executar, mas está aguardando sua vez no escalonador. |
| `BLOCKED` | A tarefa aguarda tempo, fila, semáforo, notificação ou outro evento. |
| `SUSPENDED` | A tarefa foi suspensa explicitamente. |
| `DELETED` | A tarefa foi excluída e ainda pode aparecer durante a coleta. |
| `INVALID` | Estado não reconhecido ou inválido. |

A prioridade é um valor numérico usado pelo escalonador. Em geral, uma tarefa pronta com prioridade maior pode executar antes de uma tarefa pronta com prioridade menor. A prioridade não representa o percentual de CPU utilizado.

### 2.5 Resultado esperado

A saída deve ser semelhante a esta, mas os valores devem ser substituídos pela saída real:

```text
===============================================
TAREFAS FREERTOS
Quantidade de tarefas: [INSERIR VALOR REAL]
===============================================
Nome             Estado     Prioridade  Stack HWM    Core
---------------------------------------------------------------
[INSERIR SAÍDA REAL DO MONITOR SERIAL]
===============================================
```

Não foi conectado um ESP32 neste ambiente. Portanto, não foram registrados nomes, prioridades, stacks ou núcleos reais.

### 2.6 Funcionalidade das tarefas observadas

A tabela abaixo apresenta tarefas comuns que podem aparecer em um projeto ESP-IDF. Ela não é uma lista garantida: a presença depende do alvo, da versão do IDF e dos componentes habilitados. A tabela final da atividade deve ser atualizada com a saída real do monitor serial.

| Tarefa que pode aparecer | Função provável | Condição/observação |
|---|---|---|
| `main` | Tarefa que inicia `app_main()` e configura a aplicação. | Pode desaparecer ou ser finalizada depois da inicialização, conforme o projeto. |
| `IDLE0` | Tarefa ociosa associada ao Core 0. | Normalmente aparece em configuração multicore. |
| `IDLE1` | Tarefa ociosa associada ao Core 1. | Pode não existir em configuração unicore. |
| `esp_timer` | Processa callbacks do componente de temporização, quando a configuração usa uma tarefa para isso. | Depende da versão e da configuração do ESP-IDF. |
| `ipc0` | Atende chamadas de comunicação entre núcleos relacionadas ao Core 0. | Associada a configurações multicore e ao suporte IPC. |
| `ipc1` | Atende chamadas de comunicação entre núcleos relacionadas ao Core 1. | Pode não aparecer em configuração unicore. |

Tarefas de Wi-Fi, Bluetooth, TCP/IP e outros componentes não devem ser declaradas como presentes sem que o respectivo componente esteja habilitado e apareça na saída observada.

`Core 0` e `Core 1` identificam os núcleos físicos usados pelo escalonador. Uma tarefa com afinidade fica restrita a um núcleo. Uma tarefa sem afinidade pode ser escalonada em qualquer núcleo permitido, por isso a saída usa `ANY`.

## 3. Atividade 2 — Medição da troca de contexto

### 3.1 Objetivo

O objetivo é estimar experimentalmente o intervalo entre a cessão da CPU por uma tarefa e a execução da outra, além de comparar esse valor com o período do tick e com o tempo total do experimento.

### 3.2 Metodologia

Foram criadas `Task1` e `Task2`. As duas são fixadas no mesmo núcleo e recebem a mesma prioridade, definida como `configMAX_PRIORITIES - 2`. Essa prioridade é alta, mas deixa uma faixa acima para reduzir o risco de bloquear tarefas de sistema.

As tarefas permanecem prontas e alternam por meio de `taskYIELD()`. A sincronização inicial é feita antes da medição com um grupo de eventos. Depois do início:

1. `Task1` registra `esp_timer_get_time()` imediatamente antes de ceder.
2. `Task2` registra o instante logo depois de assumir a CPU.
3. `Task2` registra seu instante antes de ceder.
4. `Task1` registra o instante depois de assumir a CPU.

Esse processo é repetido até obter 10.000 amostras no total, sendo aproximadamente 5.000 em cada direção. Não há `printf`, fila ou mutex dentro do intervalo medido. A chamada `taskYIELD()` é apropriada para esse teste porque provoca uma oportunidade explícita de escalonamento entre tarefas prontas de mesma prioridade. Ainda assim, interrupções e tarefas de sistema podem influenciar a medição.

### 3.3 Uso do `esp_timer_get_time()`

O código inclui `esp_timer.h` e usa `esp_timer_get_time()`, que retorna o tempo em microssegundos. O custo da própria chamada é medido antes do experimento fazendo 10.000 pares:

```text
t1 = esp_timer_get_time()
t2 = esp_timer_get_time()
overhead = t2 - t1
```

A média desse overhead é subtraída da média bruta. Se a subtração produzir um valor negativo, a média corrigida é limitada a zero. Essa correção é aproximada, pois o custo pode variar conforme o estado do processador e as interrupções.

### 3.4 Número de medições

Foi escolhido `NUM_MEDICOES = 10000`. O valor é suficientemente grande para reduzir a influência de uma amostra isolada, sem exigir um vetor de 10.000 elementos: o programa acumula soma, mínimo, máximo e soma dos quadrados durante a execução.

### 3.5 Cálculo da média

Para cada intervalo `d_i`, a média bruta é:

```text
média_bruta = (Σ d_i) / N
```

A média corrigida usada no relatório é:

```text
média_corrigida = máximo(0, média_bruta - overhead_médio_do_timer)
```

O desvio padrão bruto, também calculado pelo programa, é estimado por:

```text
desvio = raiz( (Σ d_i² / N) - média_bruta² )
```

### 3.6 Cálculo percentual

O período do tick é obtido a partir da configuração real do FreeRTOS:

```text
período_tick_us = 1.000.000 / configTICK_RATE_HZ
```

A primeira métrica é a fração equivalente de um tick ocupada por uma troca:

```text
percentual_por_troca = média_corrigida / período_tick_us × 100
```

Esse resultado não é o uso global real do kernel. Ele apenas compara uma troca com um período de tick.

A segunda métrica usa o experimento observado:

```text
tempo_total_experimento = instante_final - instante_inicial
overhead_estimado = Σ d_i
percentual_experimento = overhead_estimado / tempo_total_experimento × 100
```

O teste força as tarefas a trocar de contexto continuamente. Por isso, esse percentual descreve o experimento sintético e não uma aplicação comum executando uma carga representativa.

### 3.7 Fontes de erro experimental

As principais fontes de variação são interrupções, tarefas de maior prioridade, acesso ao contador de tempo, comportamento do escalonador e configuração do tick. A chamada `taskYIELD()` garante uma oportunidade de troca, mas não elimina o tempo gasto pelo hardware e pelo restante do sistema.

Os valores reais devem ser colados abaixo:

```text
===== TESTE DE TROCA DE CONTEXTO =====
[INSERIR RESULTADO REAL DO MONITOR SERIAL]
```

Não foi possível executar essa medição sem uma placa ESP32 conectada.

## 4. Atividade 3 — Semáforos

### 4.1 Objetivo

O objetivo é proteger um buffer compartilhado acessado por cinco produtoras e duas leitoras. Cada produtora escreve seu próprio nome cinco vezes, totalizando 25 itens.

### 4.2 Problema produtor-consumidor

As produtoras geram itens e as leitoras retiram itens. Como o buffer tem dez posições, uma produtora precisa esperar quando não há espaço e uma leitora precisa esperar quando não há item. Os semáforos bloqueiam as tarefas nesses casos, evitando busy waiting.

O programa usa:

- `mutexBuffer`: protege os índices, o contador e os textos do buffer;
- `semEspacosLivres`: semáforo contador iniciado em 10;
- `semItensDisponiveis`: semáforo contador iniciado em 0.

### 4.3 Buffer compartilhado

O buffer é uma matriz global `char buffer[10][20]`. `writeIndex` indica onde a próxima produtora escreverá, `readIndex` indica de onde a próxima leitora retirará e `bufferCount` informa quantas posições estão ocupadas.

Quando um índice chega ao final, ele volta para zero:

```text
índice seguinte = (índice atual + 1) % BUFFER_SIZE
```

Esse comportamento caracteriza uma fila circular. Os índices são necessários para separar a posição de escrita da posição de leitura e para reutilizar as dez posições sem mover todos os textos.

### 4.4 Mutex

Uma condição de corrida ocorreria se duas tarefas lessem e alterassem `writeIndex`, `readIndex` ou `bufferCount` ao mesmo tempo. O trecho que acessa esses dados é a região crítica. O mutex garante que apenas uma tarefa por vez execute esse trecho.

O mutex também evita que uma leitora copie uma string enquanto uma produtora está escrevendo a mesma posição. As mensagens são impressas fora da região crítica para reduzir o tempo de retenção do mutex.

### 4.5 Semáforos contadores

Um semáforo contador representa uma quantidade de recursos iguais. `semEspacosLivres` representa as posições que ainda podem receber dados; a produtora faz `take` antes de escrever e a leitora faz `give` depois de retirar um item. `semItensDisponiveis` representa os itens prontos; a leitora faz `take` antes de ler e a produtora faz `give` depois de escrever.

A diferença principal para um mutex é que o mutex protege a propriedade de um recurso por uma tarefa e possui semântica de posse. O semáforo contador representa várias unidades disponíveis e não é usado para proteger diretamente a região crítica do buffer.

### 4.6 Estratégia de finalização

Uma leitora não pode ser excluída somente porque encontrou o buffer vazio. O buffer pode estar temporariamente vazio enquanto as produtoras ainda trabalham. Por isso, cada produtora incrementa `produtoresFinalizados` depois das cinco escritas, com o mutex protegido.

Depois de um timeout controlado de 100 ms ao esperar por um item, a leitora verifica, também sob o mutex, se:

```text
produtoresFinalizados == 5 e bufferCount == 0
```

Somente essa combinação indica que não haverá novos itens e que todos os itens já foram consumidos. O timeout resolve o caso em que a última produtora terminou sem haver uma leitora permanentemente bloqueada. Não há busy-loop.

### 4.7 Resultado esperado

Cada produtora deve informar sua conclusão. As leitoras devem imprimir os itens e, no fim, informar que a leitura foi finalizada. A última leitora imprime os contadores:

```text
===== VALIDAÇÃO DA ATIVIDADE 3 =====
Total produzido: 25
Total consumido: 25
Resultado: OK
=====================================
```

O fluxo lógico é:

```text
PRODUTOR
  ↓
espera espaço
  ↓
obtém mutex
  ↓
escreve uma posição
  ↓
libera mutex
  ↓
sinaliza item disponível

CONSUMIDOR
  ↓
espera item
  ↓
obtém mutex
  ↓
lê e remove uma posição
  ↓
libera mutex
  ↓
sinaliza espaço disponível
```

Como uma produtora só incrementa `totalProduzido` dentro da região protegida e uma leitora só incrementa `totalConsumido` depois de retirar um item, cada item é contabilizado uma vez. A combinação entre os semáforos e a fila circular impede sobrescrita, desaparecimento e leitura duplicada.

O resultado real da execução deve ser colado aqui:

```text
[INSERIR RESULTADO REAL DO MONITOR SERIAL]
```

## 5. Conclusão

As atividades mostram três aspectos complementares do FreeRTOS. A primeira permite observar o estado interno das tarefas sem inventar o tamanho da stack. A segunda transforma a troca de contexto em uma medição experimental com timer de microssegundos e deixa explícita a diferença entre comparação com o tick e percentual do teste sintético. A terceira aplica sincronização clássica de produtor-consumidor com um mutex, dois semáforos contadores e uma fila circular.

Os valores que dependem do hardware ainda precisam ser coletados no ESP32. A compilação e a execução do firmware não foram realizadas neste ambiente porque o ESP-IDF e as ferramentas de build não estavam disponíveis, e não havia uma placa conectada para gerar resultados reais.
