# Relatório da integração do irace no DCKP-SBPO

## 1. Objetivo

Este relatório explica como o **irace** foi integrado ao projeto DCKP-SBPO
para configurar automaticamente o algoritmo **ILS com busca local VND**.
Ele descreve:

- o que é o irace e como o processo de racing funciona;
- quais parâmetros do ILS/VND são calibrados;
- como as instâncias foram divididas entre treino e teste;
- como o executável C++ é chamado pelo irace;
- quais critérios garantem reprodutibilidade;
- como executar uma verificação rápida, o tuning definitivo e a avaliação
  final;
- quais arquivos são gerados e como interpretar os resultados;
- quanto tempo cada etapa pode consumir.

O protocolo definitivo está alinhado aos experimentos principais do projeto:
**1.000 segundos por execução e cinco runs por instância na avaliação final**.

---

## 2. O que é o irace

O irace é uma ferramenta de **configuração automática de algoritmos**. Seu
objetivo é encontrar bons valores para os parâmetros de um algoritmo sem
testar exaustivamente todas as possibilidades com o mesmo orçamento.

No projeto, uma configuração é uma combinação como:

~~~text
perturbation_strength = 6
vnd_add               = 1
vnd_swap_1_1          = 0
vnd_swap_2_1          = 1
vnd_swap_1_2          = 1
~~~

O irace executa diversas configurações nas instâncias de treino. Durante cada
race, configurações com desempenho estatisticamente inferior são eliminadas.
As sobreviventes, chamadas de **elites**, alimentam o modelo probabilístico
que gera novas configurações.

Esse processo repete três ideias:

1. amostrar configurações candidatas;
2. comparar as candidatas em instâncias e seeds controladas;
3. eliminar candidatas inferiores e concentrar o orçamento nas regiões mais
   promissoras do espaço de parâmetros.

O irace minimiza um valor chamado **cost**. Como o DCKP é um problema de
maximização de lucro, o projeto retorna:

~~~text
cost = -profit
~~~

Assim, lucro maior produz custo mais negativo e é considerado melhor.

Referências:

- [site oficial do irace](https://mlopez-ibanez.github.io/irace/);
- [manual do pacote](https://mlopez-ibanez.github.io/irace/irace-package.pdf);
- López-Ibáñez et al., *The irace package: Iterated Racing for Automatic
  Algorithm Configuration*, Operations Research Perspectives, 2016,
  [DOI 10.1016/j.orp.2016.09.002](https://doi.org/10.1016/j.orp.2016.09.002).

---

## 3. Visão geral da integração

O fluxo implementado é:

~~~text
scenario.txt + parameters.txt + instâncias de treino
                         |
                         v
                       irace
                         |
                         | configuração + instância + seed
                         v
                 target-runner.sh
                         |
                         | flags da CLI
                         v
                build/dckp_sbpo (C++)
                         |
                         | CSV com profit e valid
                         v
             target-runner devolve -profit
                         |
                         v
             irace elimina ou mantém a configuração
~~~

Os principais arquivos são:

| Arquivo | Responsabilidade |
| --- | --- |
| **scripts/irace/scenario.txt** | Define orçamento, blocos, seed, timeout e runner |
| **scripts/irace/parameters.txt** | Define o espaço de parâmetros e configurações proibidas |
| **scripts/irace/configurations.txt** | Insere a configuração padrão como baseline inicial |
| **scripts/irace/train-instances.txt** | Lista as 80 instâncias usadas pelo tuning |
| **scripts/irace/test-instances.txt** | Lista as 20 instâncias holdout |
| **scripts/irace/target-runner.sh** | Traduz uma avaliação do irace em uma chamada ao C++ |
| **scripts/irace/run.sh** | Localiza o irace e inicia check ou tuning definitivo |
| **scripts/irace/extract_best.R** | Extrai a melhor elite de um log RData |
| **scripts/irace/evaluate_test.sh** | Compara baseline e configuração calibrada no holdout |
| **scripts/irace/analyze_test.R** | Produz a análise pareada e o resumo estatístico |

---

## 4. Parâmetros calibrados

### 4.1 Força de perturbação do ILS

| Nome no irace | Flag do executável | Tipo | Domínio | Padrão |
| --- | --- | --- | --- | --- |
| perturbation_strength | --ils-perturbation-strength | inteiro | 1 a 8 | 4 |

Esse parâmetro controla quantos itens selecionados são removidos em cada
perturbação do ILS. Depois da remoção, o algoritmo executa uma reparação gulosa
e aplica novamente o VND.

Valores pequenos realizam uma busca mais intensiva perto da solução atual.
Valores maiores aumentam a diversificação.

A faixa foi limitada a 1–8 porque as soluções observadas nas instâncias do
projeto frequentemente contêm aproximadamente 10–22 itens. Valores muito
altos poderiam remover toda a solução e criar várias configurações
praticamente equivalentes.

### 4.2 Vizinhanças do VND

| Nome no irace | Flag | Movimento | Domínio |
| --- | --- | --- | --- |
| vnd_add | --vnd-add | Adiciona um item factível | 0 ou 1 |
| vnd_swap_1_1 | --vnd-swap-1-1 | Troca um item por um item | 0 ou 1 |
| vnd_swap_2_1 | --vnd-swap-2-1 | Troca dois itens por um item | 0 ou 1 |
| vnd_swap_1_2 | --vnd-swap-1-2 | Troca um item por dois itens | 0 ou 1 |

O valor 1 ativa a vizinhança e 0 a desativa. As vizinhanças ativas continuam
sendo visitadas na ordem fixa declarada pelo VND.

A combinação 0,0,0,0 é proibida, pois isso deixaria o ILS sem busca local.
A restrição aparece na seção **[forbidden]** de **parameters.txt**.

Com quatro opções binárias existem 16 subconjuntos. Descontando o subconjunto
vazio, o irace pode escolher entre 15 estruturas de VND. Com oito valores para
a perturbação, o espaço discreto total possui:

~~~text
8 × 15 = 120 configurações possíveis
~~~

### 4.3 Configuração padrão

A configuração original participa da corrida como configuração inicial:

~~~text
perturbation_strength = 4
vnd_add               = 1
vnd_swap_1_1          = 1
vnd_swap_2_1          = 1
vnd_swap_1_2          = 1
~~~

Isso garante que o configurador tenha contato direto com o baseline, em vez de
depender de sorte para amostrá-lo.

---

## 5. Instâncias de treino e teste

O conjunto possui 100 instâncias organizadas em 20 famílias, com cinco
réplicas em cada família.

### 5.1 Treino

São utilizadas as variantes I1, I2, I3 e I4 das 20 famílias:

~~~text
20 famílias × 4 réplicas = 80 instâncias de treino
~~~

As instâncias são organizadas em quatro blocos:

- bloco 1: 1I1, 2I1, ..., 20I1;
- bloco 2: 1I2, 2I2, ..., 20I2;
- bloco 3: 1I3, 2I3, ..., 20I3;
- bloco 4: 1I4, 2I4, ..., 20I4.

O cenário utiliza:

~~~text
blockSize = 20
firstTest = 1
eachTest = 1
sampleInstances = 0
~~~

Cada teste de eliminação acontece somente depois de um bloco completo.
Portanto, uma configuração não pode ser eliminada por ter sido avaliada apenas
em instâncias pequenas, grandes, esparsas ou densas. Cada bloco contém uma
instância de todas as famílias.

### 5.2 Teste independente

A variante I5 de cada família é reservada:

~~~text
1I5, 2I5, ..., 20I5
~~~

Essas 20 instâncias não participam do tuning. Elas são usadas somente após a
configuração final ter sido escolhida.

Esse isolamento evita vazamento de informação entre calibração e avaliação.
Os resultados principais do artigo devem vir desse conjunto holdout.

---

## 6. Configuração do cenário

Os principais valores de **scenario.txt** são:

| Opção | Valor | Significado |
| --- | ---: | --- |
| maxExperiments | 1000 | Máximo de avaliações do target runner |
| blockSize | 20 | Uma instância de cada família por bloco |
| firstTest | 1 | Primeiro teste após um bloco |
| eachTest | 1 | Novo teste a cada bloco |
| parallel | 1 | Execução sequencial |
| seed | 20260810 | Seed reprodutível do irace |
| targetRunnerTimeout | 1020 s | Watchdog externo com margem |
| testType | F-test | Teste de Friedman usado pelo racing |

O irace instalado imprime internamente **testType = friedman**. Isso é esperado:
F-test é o nome aceito no arquivo de cenário para o teste de Friedman.

### Por que parallel = 1

O algoritmo é interrompido por tempo de parede. Executar várias configurações
simultaneamente poderia causar competição por CPU, memória e cache. Isso
alteraria quantas iterações cada execução consegue realizar em 1.000 segundos.

A execução sequencial é mais lenta, mas torna as comparações mais controladas.

### Seeds

A seed 20260810 controla o processo de amostragem do irace. Para cada
avaliação, o irace fornece uma seed ao target runner, e essa seed é repassada
ao gerador pseudoaleatório do ILS.

Durante o tuning, uma avaliação do irace corresponde a uma execução estocástica
do algoritmo em um par instância/seed. As cinco repetições do protocolo do
artigo são realizadas posteriormente, na avaliação holdout.

---

## 7. Funcionamento do target runner

O irace chama o script com quatro argumentos fixos:

~~~text
target-runner.sh CONFIG_ID INSTANCE_ID SEED INSTANCE [parâmetros]
~~~

Exemplo:

~~~text
target-runner.sh 35 42 1234567 ../../DCKP/.../10I2 \
  --ils-perturbation-strength 8 \
  --vnd-add 0 \
  --vnd-swap-1-1 0 \
  --vnd-swap-2-1 0 \
  --vnd-swap-1-2 1
~~~

O runner:

1. valida quantidade de argumentos, seed, instância e executável;
2. executa **build/dckp_sbpo** com algoritmo ILS;
3. aplica o limite de 1.000.000 ms;
4. exige uma linha CSV válida;
5. confere instância, seed, lucro, peso, capacidade, tempo, validade e nome do
   algoritmo;
6. devolve somente o lucro negativo.

Erros de infraestrutura, timeout externo, saída malformada ou solução inválida
encerram a corrida. Eles não são transformados silenciosamente em uma
penalidade, pois isso poderia produzir um resultado científico aparentemente
válido a partir de execuções quebradas.

---

## 8. Protocolo de tempo

### 8.1 Tuning definitivo

Cada avaliação usa:

~~~text
1.000.000 ms = 1.000 s = 16 min 40 s
~~~

O orçamento máximo é de 1.000 avaliações:

~~~text
1.000 avaliações × 1.000 s = 1.000.000 s
1.000.000 s ÷ 86.400 = aproximadamente 11,6 dias
~~~

Como parallel = 1, o pior caso de tempo real é próximo de 11,6 dias, acrescido
do overhead do irace e do sistema operacional.

O tempo efetivo pode variar se o irace não consumir todo o orçamento, mas o ILS
normalmente utiliza todo o limite de cada chamada.

### 8.2 Avaliação final

A comparação final executa:

~~~text
20 instâncias
× 5 runs
× 2 configurações
= 200 execuções
~~~

Logo:

~~~text
200 × 1.000 s = 200.000 s
200.000 s ÷ 3.600 = aproximadamente 55,6 horas
~~~

As duas configurações são:

- baseline: perturbação 4 e todas as vizinhanças;
- tuned: configuração vencedora do tuning definitivo.

Os mesmos pares instância/seed são usados nas duas configurações. A ordem de
execução é alternada para reduzir viés temporal.

### 8.3 Teste rápido

O teste rápido utiliza 100 ms por avaliação e orçamento de 2.000 avaliações.
O orçamento mínimo é maior porque o desenho em blocos exige que as
configurações sejam avaliadas em grupos completos de 20 famílias.

Na execução observada:

~~~text
CPU user time  = 224,613 s
CPU system time = 40,661 s
wall-clock time = 342,03 s
~~~

Portanto, o teste de integração levou cerca de 5 minutos e 42 segundos.

Esse resultado serve somente para verificar o pipeline. Ele não pode ser
usado como evidência do artigo, pois uma configuração boa em 100 ms pode ser
ruim após 1.000 segundos.

---

## 9. Instalação

No WSL Ubuntu:

~~~bash
sudo apt update
sudo apt install -y r-base
R -q -e 'install.packages("irace", repos="https://cloud.r-project.org")'
~~~

Na raiz do projeto:

~~~bash
cd /mnt/c/Users/thall/Documents/GitHub/dckp-sbpo
make build
chmod +x scripts/irace/*.sh tests/scripts/*.sh
~~~

O wrapper localiza o irace no PATH ou dentro da biblioteca pessoal do R por
meio de **system.file()**. Isso é necessário porque instalações feitas pelo R
podem colocar o executável fora do PATH do WSL.

---

## 10. Como verificar a instalação

Execute:

~~~bash
./scripts/irace/run.sh --check-fast
~~~

Esse comando:

- lê o cenário e os parâmetros;
- valida a configuração proibida;
- lê o baseline inicial;
- confere os 80 caminhos de treino;
- chama realmente o target runner três vezes;
- usa 100 ms apenas para a verificação.

O final esperado é:

~~~text
Check successful.
~~~

Esse modo não altera o protocolo definitivo. Ao executar o tuning normal, o
wrapper remove as variáveis de teste e restaura obrigatoriamente 1.000 segundos.

---

## 11. Como executar um teste rápido de cinco minutos

Na raiz:

~~~bash
cd /mnt/c/Users/thall/Documents/GitHub/dckp-sbpo
make build
mkdir -p results/irace

IRACE_BIN="$(Rscript -e 'cat(system.file("bin", "irace", package="irace"))')"
cd scripts/irace

DCKP_IRACE_TIME_LIMIT_MS=100 \
DCKP_IRACE_TIMEOUT_SECONDS=2 \
"$IRACE_BIN" \
    --scenario scenario.txt \
    --max-experiments 2000 \
    --log-file ../../results/irace/quick_test.Rdata
~~~

Depois:

~~~bash
cd ../..
Rscript scripts/irace/extract_best.R results/irace/quick_test.Rdata
~~~

O arquivo principal será:

~~~text
results/irace/quick_test.Rdata
~~~

---

## 12. Interpretação do teste rápido executado

O teste rápido encontrou:

~~~text
perturbation_strength = 8
vnd_add               = 0
vnd_swap_1_1          = 0
vnd_swap_2_1          = 0
vnd_swap_1_2          = 1
~~~

As elites foram:

~~~text
ID 35: perturbação 8, somente Swap 1-2
ID 28: perturbação 7, somente Swap 1-2
ID 22: perturbação 6, somente Swap 1-2
~~~

O texto:

~~~text
Best-so-far configuration: 35
mean value: -3391.042857
~~~

significa:

- 35 é o identificador interno da configuração;
- o valor é negativo porque cost = -profit;
- o lucro médio correspondente nas avaliações realizadas por essa configuração
  foi aproximadamente 3391,04;
- essa média não significa necessariamente que a configuração foi executada em
  todas as 80 instâncias, pois o racing elimina candidatas progressivamente.

O campo **PARENT = 22** informa que o modelo do irace gerou a configuração 35
a partir da configuração 22. Não é um parâmetro do ILS.

A mensagem:

~~~text
No test instances, skip testing
~~~

é intencional. O holdout não é entregue ao processo de tuning. A avaliação
independente é feita posteriormente por **evaluate_test.sh**.

A configuração 8,0,0,0,1 prova que o pipeline funciona, mas não deve ser
usada no artigo. O limite artificial de 100 ms muda completamente a dinâmica
da busca.

---

## 13. Como executar o tuning definitivo

Antes do experimento:

1. finalize e revise o código;
2. faça commit das alterações;
3. evite outras cargas pesadas no computador;
4. garanta energia e suspensão desativada durante o experimento;
5. registre as especificações da máquina no artigo.

Execute:

~~~bash
cd /mnt/c/Users/thall/Documents/GitHub/dckp-sbpo
make build
./scripts/irace/run.sh
~~~

O wrapper imprime:

- versão do irace;
- cenário utilizado;
- limite de tempo;
- aviso se o worktree estiver sujo;
- caminho do manifesto;
- caminho do log RData.

Cada execução definitiva gera nomes únicos:

~~~text
results/irace/tuning_<timestamp>.Rdata
results/irace/tuning_<timestamp>_manifest.txt
~~~

O manifesto registra:

- timestamp UTC;
- commit Git;
- indicação de worktree limpo ou sujo;
- versões do irace e R;
- SHA-256 do executável;
- SHA-256 do cenário e dos parâmetros;
- sistema operacional;
- processador;
- orçamento, tempo e paralelismo.

---

## 14. Como extrair a configuração vencedora

Após o tuning:

~~~bash
Rscript scripts/irace/extract_best.R \
    results/irace/tuning_<timestamp>.Rdata
~~~

O script mostra uma tabela e gera o comando exato:

~~~text
Best configuration:
 perturbation_strength vnd_add vnd_swap_1_1 vnd_swap_2_1 vnd_swap_1_2
                     6       1            0            1            1

Final evaluation command:
./scripts/irace/evaluate_test.sh 6 1 0 1 1
~~~

Os valores acima são apenas um exemplo.

---

## 15. Como executar a avaliação final

Execute exatamente o comando produzido pelo extrator:

~~~bash
./scripts/irace/evaluate_test.sh PERTURBACAO ADD SWAP_1_1 SWAP_2_1 SWAP_1_2
~~~

Durante cerca de 55,6 horas, o terminal mostrará progresso semelhante a:

~~~text
[1/200] 1I5 run=1 seed=42 configuration=baseline profit=...
[2/200] 1I5 run=1 seed=42 configuration=tuned profit=...
~~~

O número run reinicia de 1 a 5 em cada instância. Para cada instância, baseline
e tuned usam exatamente a mesma seed.

---

## 16. Arquivos produzidos na avaliação

### 16.1 CSV bruto

~~~text
results/irace/test_<timestamp>.csv
~~~

Contém 200 resultados:

| Campo | Descrição |
| --- | --- |
| configuration | baseline ou tuned |
| instance | Instância holdout |
| run | Repetição de 1 a 5 |
| seed | Seed determinística |
| profit | Lucro da solução |
| weight | Peso selecionado |
| capacity | Capacidade da instância |
| start_time / end_time | Horários da execução |
| time_ms | Tempo medido |
| valid | Validade da solução |
| algorithm | Deve ser ILS |
| parâmetros | Configuração efetivamente usada |

### 16.2 Comparação por instância

~~~text
results/irace/test_<timestamp>_per_instance.csv
~~~

Possui 20 linhas:

~~~text
instance
baseline_mean_profit
baseline_mean_time_ms
tuned_mean_profit
tuned_mean_time_ms
absolute_gain
relative_gain_pct
~~~

Esse arquivo é apropriado para tabelas e gráficos do artigo.

### 16.3 Resumo estatístico

~~~text
results/irace/test_<timestamp>_summary.csv
~~~

Inclui:

- média de lucro de cada configuração;
- ganho percentual médio e mediano;
- quantidade de vitórias, empates e derrotas;
- p-valor do teste de Wilcoxon;
- média de tempo.

---

## 17. Como interpretar a avaliação final

Um exemplo hipotético:

~~~text
mean_relative_gain_pct     = 4.27
median_relative_gain_pct   = 3.81
wins                       = 15
ties                       = 1
losses                     = 4
wilcoxon_p_value           = 0.008
~~~

Interpretação:

- a configuração calibrada melhorou em média 4,27%;
- metade das instâncias teve ganho de pelo menos 3,81%;
- tuned venceu em 15 das 20 instâncias;
- p = 0,008 fornece evidência contra a hipótese de ganho mediano zero, usando
  nível de significância de 5%.

O teste é aplicado aos 20 ganhos percentuais obtidos depois de calcular a média
das cinco runs de cada configuração em cada instância. Assim, a unidade de
análise é a instância, e não cada uma das 200 linhas como se fossem observações
independentes.

Um p-valor pequeno não substitui a análise do tamanho do efeito. O artigo deve
apresentar também ganhos percentuais, vitórias/derrotas, distribuição e tempos.

---

## 18. Como aplicar a configuração em uma instância

A configuração calibrada não é gravada automaticamente como novo padrão do
executável. Isso é intencional: baseline e tuned permanecem identificáveis.

Para uma instância nova:

~~~bash
./build/dckp_sbpo CAMINHO_DA_INSTANCIA \
    --algo ILS \
    --time-limit 1000000 \
    --seed 42 \
    --ils-perturbation-strength 6 \
    --vnd-add 1 \
    --vnd-swap-1-1 0 \
    --vnd-swap-2-1 1 \
    --vnd-swap-1-2 1 \
    --csv
~~~

Os valores devem ser substituídos pela elite do tuning definitivo.

Para repetir cinco vezes, devem ser usadas cinco seeds registradas. Para
comparar dois métodos, ambos precisam receber os mesmos pares instância/seed.

---

## 19. O que deve ser preservado para o artigo

Arquivos mínimos:

- código e commit Git utilizados;
- **scenario.txt**;
- **parameters.txt**;
- listas de treino e teste;
- RData do tuning;
- manifesto do tuning;
- CSV bruto da avaliação;
- CSV por instância;
- CSV de resumo;
- versão do irace e do R;
- especificações de hardware e sistema operacional.

Não se deve:

- usar o resultado de 100 ms como configuração do artigo;
- alterar os parâmetros depois de observar o holdout;
- misturar I5 novamente no tuning;
- comparar métodos com seeds diferentes;
- executar várias buscas em paralelo sem declarar a interferência;
- substituir silenciosamente falhas por resultados penalizados;
- apresentar somente p-valor sem magnitude do ganho.

---

## 20. Resumo para apresentação oral

Uma explicação curta ao professor pode ser:

> O irace foi integrado ao executável C++ por um target runner. Ele recebe uma
> configuração do ILS/VND, uma instância e uma seed, executa o algoritmo por
> 1.000 segundos e devolve o lucro negativo como custo. O espaço possui oito
> forças de perturbação e quinze subconjuntos não vazios das quatro
> vizinhanças, totalizando 120 configurações. O tuning usa 80 instâncias
> organizadas em quatro blocos balanceados com as vinte famílias. As vinte
> instâncias I5 ficam isoladas para o teste final. Depois do tuning, a melhor
> configuração é comparada com o baseline em cinco seeds por instância,
> totalizando 200 execuções. Os resultados são analisados por instância,
> incluindo ganho percentual, vitórias e teste de Wilcoxon.

---

## 21. Checklist operacional

### Antes do tuning

- [ ] Build e 26 testes passando.
- [ ] Check rápido retornando “Check successful”.
- [ ] Código revisado e commitado.
- [ ] Worktree limpo.
- [ ] Suspensão automática desativada.
- [ ] Espaço em disco e energia verificados.

### Depois do tuning

- [ ] RData e manifesto preservados.
- [ ] Elite extraída com extract_best.R.
- [ ] Parâmetros não modificados após observar o holdout.
- [ ] Avaliação final executada.
- [ ] Exatamente 200 linhas válidas no CSV.
- [ ] CSV por instância e resumo gerados.
- [ ] Resultados e hardware descritos no artigo.
